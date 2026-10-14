#!/usr/bin/env python3
"""Classify every source outside the focused Maya SDK syntax pass."""

from __future__ import print_function

import os
import re
import sys


REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(REPOSITORY_ROOT, "tools"))

from list_active_cpp_sources import load_manifest  # noqa: E402


EXPECTED_COUNTS = {
    "ALEMBIC": 9,
    "ARNOLD": 10,
    "BULLET": 0,
    "FASTNOISE": 2,
    "JET": 1,
    "OPENVDB": 49,
}

OPENVDB_POINT_SOURCES = {
    "DPAttrSmooth.cpp",
    "DPAttributeVDB.cpp",
    "DPScatter.cpp",
    "DPointVolumeEmit.cpp",
    "DVdbOp.cpp",
    "DPCache.cpp",
    "DPClosestPointVDB.cpp",
    "DPSelfCollision.cpp",
}
ARNOLD_SOURCES = {
    "DArnoldExtentionPlugins.cpp",
    "DPWriteToAi.cpp",
    "DWriteDCurveToAi.cpp",
    "DCurveTranslatorAi.cpp",
    "DMeshTranslatorAi.cpp",
    "DPInstancer.cpp",
    "DPInstancerTranslatorAi.cpp",
    "DPLightTranslatorAi.cpp",
    "DPointTranslatorAi.cpp",
    "DPLightGen.cpp",
}
FASTNOISE_SOURCES = {"DPFastNoise.cpp", "DMFastNoise.cpp"}


def focused_sources():
    path = os.path.join(REPOSITORY_ROOT, "tests", "check_maya_sdk_headers.sh")
    with open(path, "r") as source_file:
        source = source_file.read()
    return set(re.findall(r'"\$repo_root/([^"]+\.cpp)"', source))


def classify(relative_path):
    file_name = os.path.basename(relative_path)
    if relative_path.startswith("src/modules/volume/") or (
        file_name in OPENVDB_POINT_SOURCES
    ):
        return "OPENVDB"
    if file_name in ARNOLD_SOURCES:
        return "ARNOLD"
    if "/DCurve/Cmd/Abc/" in relative_path or file_name in {
        "DDataExportUtil.cpp",
        "DExportAbcCmd.cpp",
    }:
        return "ALEMBIC"
    if "/DBullet/" in relative_path or "/DMesh/Dynamics/Bullet/" in relative_path:
        return "BULLET"
    if file_name in FASTNOISE_SOURCES:
        return "FASTNOISE"
    if file_name == "DJetSmokeSolver.cpp":
        return "JET"
    return None


def verify_default_off_features(categories):
    feature_path = os.path.join(
        REPOSITORY_ROOT, "src", "plugin", "PluginFeatures.h"
    )
    with open(feature_path, "r") as feature_file:
        features = feature_file.read()
    for category in categories:
        pattern = (
            r"^\s*#define\s+DOMMY_WITH_{}\s+0\s*$".format(
                re.escape(category)
            )
        )
        if not re.search(pattern, features, re.MULTILINE):
            raise ValueError(
                "{} coverage exclusions are not disabled by default".format(
                    category
                )
            )


def main():
    manifest_path = os.path.join(
        REPOSITORY_ROOT,
        "config",
        "active_cpp_sources.txt",
    )
    try:
        manifest = load_manifest(manifest_path, REPOSITORY_ROOT)
        project_sources = {source for source in manifest if source.startswith("src/")}
        focused = focused_sources()
        uncovered = sorted(project_sources - focused)

        categorized = {}
        unclassified = []
        for relative_path in uncovered:
            category = classify(relative_path)
            if category is None:
                unclassified.append(relative_path)
                continue
            categorized.setdefault(category, []).append(relative_path)

        if unclassified:
            raise ValueError(
                "unclassified focused-SDK exclusions: {}".format(
                    ", ".join(unclassified)
                )
            )

        actual_counts = {
            category: len(categorized.get(category, []))
            for category in EXPECTED_COUNTS
        }
        if actual_counts != EXPECTED_COUNTS:
            raise ValueError(
                "focused-SDK exclusion counts changed: expected {}, found {}".format(
                    EXPECTED_COUNTS, actual_counts
                )
            )
        verify_default_off_features(categorized)
    except (IOError, OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1

    summary = ", ".join(
        "{}={}".format(category, EXPECTED_COUNTS[category])
        for category in sorted(EXPECTED_COUNTS)
    )
    print(
        "Optional Maya SDK source coverage: OK "
        "({}/{} focused; {} excluded: {})".format(
            len(focused & project_sources),
            len(project_sources),
            len(uncovered),
            summary,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
