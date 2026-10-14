#!/usr/bin/env python3
"""Validate and list the explicit archival C/C++ source manifest.

Only checked-in implementations belong in this manifest. Optional feature
filtering and external implementation files are managed separately by CMake.
"""

from __future__ import print_function

import argparse
import os
import re
import sys


SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx")
DEFAULT_MANIFEST = "config/active_cpp_sources.txt"
PATH_COMPONENT_RE = re.compile(r"^[A-Za-z0-9_+.-]+$")


class ManifestError(ValueError):
    pass


def load_manifest(manifest_file, repository_root):
    """Return unique repository-relative sources, rejecting unsafe entries."""
    repository_root = os.path.realpath(repository_root)
    sources = set()
    resolved_sources = set()

    with open(manifest_file, "r", encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            source_path = line.rstrip("\r\n")
            if not source_path.strip() or source_path.lstrip().startswith("#"):
                continue
            components = source_path.split("/")
            if (len(components) < 2 or components[0] not in ("src", "third_party")
                    or any(part in ("", ".", "..") or
                           not PATH_COMPONENT_RE.fullmatch(part)
                           for part in components)
                    or not source_path.lower().endswith(SOURCE_SUFFIXES)):
                raise ManifestError(
                    "invalid repository source at line {}: {!r}".format(
                        line_number, source_path
                    )
                )
            absolute_path = os.path.realpath(
                os.path.join(repository_root, *components)
            )
            if os.path.commonpath((repository_root, absolute_path)) != repository_root:
                raise ManifestError(
                    "source resolves outside the repository: {}".format(source_path)
                )
            if not os.path.isfile(absolute_path):
                raise ManifestError("active source is missing: {}".format(source_path))
            if source_path in sources or absolute_path in resolved_sources:
                raise ManifestError("duplicate active source: {}".format(source_path))
            sources.add(source_path)
            resolved_sources.add(absolute_path)

    if not sources:
        raise ManifestError("the active source manifest is empty")
    return sorted(sources)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        default=DEFAULT_MANIFEST,
        help="path to the repository-relative source list",
    )
    parser.add_argument(
        "--repository-root",
        default=None,
        help="repository root (defaults to the parent of tools/)",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="write inventory counts to stderr",
    )
    arguments = parser.parse_args(argv)

    script_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    repository_root = os.path.realpath(arguments.repository_root or script_root)
    manifest_file = arguments.manifest
    if not os.path.isabs(manifest_file):
        manifest_file = os.path.join(repository_root, manifest_file)

    try:
        sources = load_manifest(manifest_file, repository_root)
    except (OSError, UnicodeError, ManifestError) as error:
        print("active source manifest error: {}".format(error), file=sys.stderr)
        return 1

    for source in sources:
        print(source)

    if arguments.summary:
        print(
            "{} unique checked-in repository sources".format(len(sources)),
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
