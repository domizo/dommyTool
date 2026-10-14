#!/usr/bin/env python3
"""Check DPyTools package boundaries without importing Maya modules."""

from __future__ import print_function

import ast
import sys
import warnings
from pathlib import Path

with warnings.catch_warnings():
    warnings.simplefilter("ignore", DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package


REPO_ROOT = Path(__file__).resolve().parents[1]
PYTHON_ROOT = REPO_ROOT / "python"
PACKAGE_ROOT = PYTHON_ROOT / "DPyTools"
FORBIDDEN_HOST_ROOTS = {"maya", "pymel", "PySide", "PySide2", "shiboken2"}

PURE_INTERNAL_IMPORTS = {
    "DPyTools.ui.table_selection": set(),
    "DPyTools.ui.node_table": {"DPyTools.ui.table_selection"},
    "DPyTools.core.paths": {"DPyTools.core.strings"},
    "DPyTools.core.image_output": {
        "DPyTools.core.components", "DPyTools.core.paths", "DPyTools.core.strings", "DPyTools.core.atomic_file"},
    "DPyTools.core.numeric": set(),
    "DPyTools.core.components": set(),
    "DPyTools.core.geometry": {"DPyTools.core.numeric"},
    "DPyTools.scene.attribute_values": {"DPyTools.core.numeric"},
    "DPyTools.core.diagnostics": set(),
    "DPyTools.core.file_publish": set(),
    "DPyTools.core.bounded_text": set(),
    "DPyTools.core.json_store": {
        "DPyTools.core.atomic_file", "DPyTools.core.file_publish", "DPyTools.core.bounded_text"},
    "DPyTools.core.atomic_file": {"DPyTools.core.file_publish"},
    "DPyTools.core.file_cleanup": {"DPyTools.core.atomic_file", "DPyTools.core.paths"},
    "DPyTools.core.atomic_text": {"DPyTools.core.atomic_file"},
    "DPyTools.core.strings": set(),
    "DPyTools.core.mail_address": {"DPyTools.core.strings"},
    "DPyTools.system.DFileOp": set(),
    "DPyTools.features.animation.format": set(),
    "DPyTools.features.animation.key_plan": {"DPyTools.core.numeric"},
    "DPyTools.features.curves.geometry": {"DPyTools.core.numeric", "DPyTools.core.geometry"},
    "DPyTools.features.animation.parser": {
        "DPyTools.features.animation.format", "DPyTools.core.bounded_text"},
    "DPyTools.features.animation.serializer": {
        "DPyTools.core.bounded_text",
        "DPyTools.features.animation.format",
        "DPyTools.features.animation.parser",
    },
    "DPyTools.features.rigging.face.models": set(),
    "DPyTools.features.rigging.face.lip_edges": {
        "DPyTools.core.atomic_text", "DPyTools.core.bounded_text", "DPyTools.core.components"},
    "DPyTools.features.rigging.face.specs": set(),
    "DPyTools.features.rigging.human.specs": set(),
    "DPyTools.integrations.smtp.client": {"DPyTools.core.mail_address"},
    "DPyTools.pipeline.assets.paths": {"DPyTools.core.paths"},
    "DPyTools.pipeline.assets.creation": {
        "DPyTools.core.atomic_file", "DPyTools.core.paths", "DPyTools.core.strings"},
    "DPyTools.pipeline.assets.settings": {"DPyTools.pipeline.assets.paths"},
    "DPyTools.pipeline.assets.naming": set(),
    "DPyTools.pipeline.assets.export_plan": {
        "DPyTools.core.paths", "DPyTools.core.strings", "DPyTools.core.numeric",
        "DPyTools.pipeline.assets.metadata", "DPyTools.pipeline.assets.versioning"},
    "DPyTools.pipeline.assets.metadata": {
        "DPyTools.core.paths", "DPyTools.pipeline.assets.versioning"},
    "DPyTools.pipeline.assets.rename_plan": {
        "DPyTools.core.strings", "DPyTools.ui.table_selection"},
    "DPyTools.pipeline.assets.user_data": {"DPyTools.core.mail_address"},
    "DPyTools.pipeline.assets.versioning": set(),
    "DPyTools.pipeline.assets.repository": {
        "DPyTools.core.json_store",
        "DPyTools.pipeline.assets.metadata",
        "DPyTools.pipeline.assets.settings",
        "DPyTools.pipeline.assets.user_data",
    },
    "DPyTools.pipeline.io.alembic_job": {"DPyTools.core.numeric"},
    "DPyTools.pipeline.lookdev.texture_plan": {"DPyTools.core.paths", "DPyTools.core.atomic_file"},
}

COMPATIBILITY_TARGETS = {
    "DPyTools.system.DSystemInfo": "DPyTools.features.system_info",
    "DPyTools.ui.DViewOp": "DPyTools.features.view_capture",
    "DPyTools.anim.animOp": "DPyTools.features.animation.operations",
    "DPyTools.anim.animCurveTools": "DPyTools.features.animation.operations",
    "DPyTools.rigging.bottle.bottleRigTool": "DPyTools.features.rigging.bottle",
    "DPyTools.dcurve.convert.DCConvertOp": "DPyTools.features.data_conversion",
    "DPyTools.dmesh.convert.DMConvertOp": "DPyTools.features.data_conversion",
    "DPyTools.dpoint.particle.DPParticleOp": "DPyTools.features.particles",
    "DPyTools.dpoint.display.DPointDisplayOp": "DPyTools.scene.data_display",
    "DPyTools.dcurve.display.DCurveDisplayOp": "DPyTools.scene.data_display",
    "DPyTools.dmesh.display.DMeshDisplayOp": "DPyTools.scene.data_display",
    "DPyTools.anim.DAnimIO": "DPyTools.features.animation.io",
    "DPyTools.create.RigContorller": "DPyTools.create.rig_controller",
    "DPyTools.curve.curveTool": "DPyTools.features.curves.tools",
    "DPyTools.curve.createCurveOp": "DPyTools.features.generators",
    "DPyTools.mesh.meshGen": "DPyTools.features.generators",
    "DPyTools.rigging.DBlendShapeOp": "DPyTools.features.generators",
    "DPyTools.rigging.DConstrainOp": "DPyTools.features.rigging.rivets",
    "DPyTools.defomer.clusterOp": "DPyTools.deformer.cluster",
    "DPyTools.general.attributeActions": "DPyTools.scene.attributes",
    "DPyTools.general.groupActions": "DPyTools.scene.groups",
    "DPyTools.general.DPMayaAPIOp": "DPyTools.features.meshes.api",
    "DPyTools.general.follicleActions": "DPyTools.features.rigging.follicles",
    "DPyTools.general.measureOp": "DPyTools.features.measurement",
    "DPyTools.general.nodeActions": "DPyTools.scene.nodes",
    "DPyTools.general.selectOp": "DPyTools.scene.selection",
    "DPyTools.general.transformActions": "DPyTools.scene.transforms",
    "DPyTools.general.stringActions": "DPyTools.scene.naming",
    "DPyTools.io.DAbcOp": "DPyTools.pipeline.io.alembic",
    "DPyTools.locator.DPTLocator": "DPyTools.plugins.dpt_locator",
    "DPyTools.lookdev.DPMatOp": "DPyTools.pipeline.lookdev.materials",
    "DPyTools.mesh.meshOp": "DPyTools.features.meshes.tools",
    "DPyTools.mesh.uvOp": "DPyTools.features.meshes.uv",
    "DPyTools.network.mailOp": "DPyTools.integrations.smtp.mail",
    "DPyTools.renderman.DPRManCommonOp": "DPyTools.integrations.renderman.common",
    "DPyTools.utility.DAssetOp": "DPyTools.pipeline.assets.operations",
    "DPyTools.utility.DAssetManager": "DPyTools.pipeline.assets.manager",
    "DPyTools.utility.DSceneBuilder": "DPyTools.pipeline.assets.scene_builder",
    "DPyTools.utility.DUserManager": "DPyTools.pipeline.assets.users",
}

CANONICAL_MODULE_PREFIXES = (
    "DPyTools.core",
    "DPyTools.scene",
    "DPyTools.features",
    "DPyTools.pipeline",
    "DPyTools.integrations",
    "DPyTools.plugins",
    "DPyTools.deformer",
)

CANONICAL_MODULES = {
    "DPyTools.bootstrap",
    "DPyTools.create.rig_controller",
    "DPyTools.ui.dialogs",
    "DPyTools.ui.viewport_state",
}

REMOVED_MODULE_FILES = (
    "ui/DComponentUI.py",
    "ui/DPyUI.py",
    "utility/DNodeRenamer.py",
    "rigging/DSkinClusterOp.py",
    "system/DReloadModule.py",
)


def module_name(filename):
    relative = filename.relative_to(PYTHON_ROOT).with_suffix("")
    parts = list(relative.parts)
    if parts[-1] == "__init__":
        parts.pop()
    return ".".join(parts)


def converted_tree(parser, filename):
    source = filename.read_text(encoding="utf-8")
    converted = str(parser.refactor_string(source + "\n", str(filename)))
    return ast.parse(converted, str(filename))


def import_details(tree, known_modules):
    dependencies = set()
    roots = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imported_names = [alias.name for alias in node.names]
        elif isinstance(node, ast.ImportFrom) and node.module:
            imported_names = []
            for alias in node.names:
                candidate = node.module + "." + alias.name
                imported_names.append(
                    candidate if candidate in known_modules else node.module)
        else:
            continue

        for imported_name in imported_names:
            roots.add(imported_name.split(".")[0])
            if imported_name in known_modules:
                dependencies.add(imported_name)
    return dependencies, roots


def find_cycle(graph):
    state = {}
    stack = []

    def visit(module):
        state[module] = "visiting"
        stack.append(module)
        for dependency in sorted(graph[module]):
            if state.get(dependency) == "visiting":
                start = stack.index(dependency)
                return stack[start:] + [dependency]
            if state.get(dependency) is None:
                cycle = visit(dependency)
                if cycle:
                    return cycle
        stack.pop()
        state[module] = "visited"
        return None

    for module in sorted(graph):
        if state.get(module) is None:
            cycle = visit(module)
            if cycle:
                return cycle
    return None


def top_level_symbols(tree):
    symbols = set()
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            symbols.add(node.name)
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    symbols.add(target.id)
    return symbols


def compatibility_exports(tree):
    for statement in tree.body:
        if not isinstance(statement, ast.Assign):
            continue
        if not any(
                isinstance(target, ast.Name) and target.id == "__all__"
                for target in statement.targets):
            continue
        if not isinstance(statement.value, (ast.List, ast.Tuple)):
            return None
        names = [string_literal(element) for element in statement.value.elts]
        if not names or any(name is None for name in names):
            return None
        return set(names)
    return None


def string_literal(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.Str):
        return node.s
    return None


def check_bootstrap_targets(trees, known_modules):
    failures = []
    bootstrap_tree = trees["DPyTools.bootstrap"]

    for node in ast.walk(bootstrap_tree):
        literal = string_literal(node)
        if literal and literal.startswith("DPyTools.") and " " not in literal:
            if literal not in known_modules:
                failures.append("Bootstrap module does not exist: {0}".format(literal))

        if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Name):
            continue
        if node.func.id not in ("_item", "_method_item") or len(node.args) < 3:
            continue
        target_module = string_literal(node.args[1])
        target_symbol = string_literal(node.args[2])
        if not target_module or not target_symbol or target_module not in trees:
            continue
        root_symbol = target_symbol.split(".")[0]
        if root_symbol not in top_level_symbols(trees[target_module]):
            failures.append(
                "Bootstrap symbol does not exist: {0}.{1}".format(
                    target_module, target_symbol))
    return failures


def main():
    failures = []
    parser = RefactoringTool(get_fixers_from_package("lib2to3.fixes"))
    module_files = {
        module_name(filename): filename
        for filename in sorted(PACKAGE_ROOT.rglob("*.py"))
    }
    known_modules = set(module_files)
    trees = {
        module: converted_tree(parser, filename)
        for module, filename in module_files.items()
    }

    graph = {}
    import_roots = {}
    for module, tree in trees.items():
        graph[module], import_roots[module] = import_details(tree, known_modules)
        graph[module].discard(module)

    cycle = find_cycle(graph)
    if cycle:
        failures.append("Internal import cycle: {0}".format(" -> ".join(cycle)))

    for module in sorted(name for name in known_modules if name.startswith("DPyTools.core")):
        forbidden = import_roots[module] & FORBIDDEN_HOST_ROOTS
        if forbidden:
            failures.append(
                "Core module imports host API ({0}): {1}".format(
                    module, ", ".join(sorted(forbidden))))
        external_package_dependencies = {
            dependency for dependency in graph[module]
            if not dependency.startswith("DPyTools.core")
        }
        if external_package_dependencies:
            failures.append(
                "Core module crosses its package boundary ({0}): {1}".format(
                    module, ", ".join(sorted(external_package_dependencies))))

    for module, allowed_dependencies in PURE_INTERNAL_IMPORTS.items():
        forbidden_hosts = import_roots[module] & FORBIDDEN_HOST_ROOTS
        if forbidden_hosts:
            failures.append(
                "Host-free module imports host API ({0}): {1}".format(
                    module, ", ".join(sorted(forbidden_hosts))))
        unexpected = graph[module] - allowed_dependencies
        if unexpected:
            failures.append(
                "Host-free module has unexpected DPyTools dependency ({0}): {1}".format(
                    module, ", ".join(sorted(unexpected))))

    old_asset_prefix = "DPyTools.utility.DAsset"
    for module in sorted(
            name for name in known_modules
            if name.startswith("DPyTools.pipeline.assets")):
        old_dependencies = {
            dependency for dependency in graph[module]
            if (dependency.startswith(old_asset_prefix)
                or dependency == "DPyTools.utility.DUserManager")
        }
        if old_dependencies:
            failures.append(
                "Canonical asset module imports a compatibility path ({0}): {1}"
                .format(module, ", ".join(sorted(old_dependencies))))

    for compatibility_module, canonical_module in COMPATIBILITY_TARGETS.items():
        if canonical_module not in graph[compatibility_module]:
            failures.append(
                "Compatibility module does not delegate to canonical module: "
                "{0} -> {1}".format(compatibility_module, canonical_module))

        compatibility_tree = trees[compatibility_module]
        wildcard_imports = [
            node for node in ast.walk(compatibility_tree)
            if (isinstance(node, ast.ImportFrom)
                and any(alias.name == "*" for alias in node.names))
        ]
        if wildcard_imports:
            failures.append(
                "Compatibility module uses a wildcard export: {0}"
                .format(compatibility_module))

        exports = compatibility_exports(compatibility_tree)
        if exports is None:
            failures.append(
                "Compatibility module requires a literal __all__: {0}"
                .format(compatibility_module))
        else:
            missing_exports = exports - top_level_symbols(trees[canonical_module])
            if missing_exports:
                failures.append(
                    "Compatibility exports are missing from {0}: {1}"
                    .format(canonical_module, ", ".join(sorted(missing_exports))))

    compatibility_modules = set(COMPATIBILITY_TARGETS)
    for module, dependencies in sorted(graph.items()):
        is_canonical = (
            module in CANONICAL_MODULES
            or module.startswith(CANONICAL_MODULE_PREFIXES)
        )
        reverse_dependencies = dependencies & compatibility_modules
        if is_canonical and reverse_dependencies:
            failures.append(
                "Canonical module imports a compatibility path ({0}): {1}"
                .format(module, ", ".join(sorted(reverse_dependencies))))

    for relative in REMOVED_MODULE_FILES:
        if (PACKAGE_ROOT / relative).exists():
            failures.append("Removed module returned: {0}".format(relative))

    failures.extend(check_bootstrap_targets(trees, known_modules))

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("DPyTools architecture check: OK ({0} modules)".format(len(graph)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
