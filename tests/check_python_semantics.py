#!/usr/bin/env python3
"""Catch source-level callback and callable-usage mistakes in DPyTools."""

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
CALLBACK_NAME_PARTS = ("callback", "delegate")
KNOWN_ATTRIBUTE_TYPOS = {"warninng"}


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


def assignment_names(target):
    if isinstance(target, ast.Name):
        return [target.id]
    if isinstance(target, ast.Attribute):
        return [target.attr]
    if isinstance(target, (ast.List, ast.Tuple)):
        names = []
        for element in target.elts:
            names.extend(assignment_names(element))
        return names
    return []


def is_callback_name(name):
    lowered = name.lower()
    return (
        lowered.startswith("func_")
        or lowered.endswith("_act")
        or any(part in lowered for part in CALLBACK_NAME_PARTS)
    )


def literal_false(node):
    if isinstance(node, ast.Constant):
        return node.value is False
    if isinstance(node, ast.NameConstant):
        return node.value is False
    return False


def literal_string(node):
    if isinstance(node, ast.Constant):
        return node.value if isinstance(node.value, str) else None
    if isinstance(node, ast.Str):
        return node.s
    return None


def main():
    failures = []
    parser = RefactoringTool(get_fixers_from_package("lib2to3.fixes"))
    module_files = {
        module_name(filename): filename
        for filename in sorted(PACKAGE_ROOT.rglob("*.py"))
    }
    trees = {
        module: converted_tree(parser, filename)
        for module, filename in module_files.items()
    }
    module_functions = {
        module: {
            node.name for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        for module, tree in trees.items()
    }

    for module, tree in trees.items():
        relative = module_files[module].relative_to(REPO_ROOT)
        import_aliases = {}
        for statement in tree.body:
            if isinstance(statement, ast.Import):
                for alias in statement.names:
                    if alias.name in trees:
                        import_aliases[alias.asname or alias.name.split(".")[0]] = alias.name

        for node in ast.walk(tree):
            if (isinstance(node, ast.Attribute)
                    and node.attr in KNOWN_ATTRIBUTE_TYPOS):
                failures.append(
                    "Known API attribute typo in {0}:{1} ({2})".format(
                        relative, node.lineno, node.attr))

            if isinstance(node, ast.Expr):
                message = literal_string(node.value)
                if message is not None and message.lstrip().startswith("Warning:"):
                    failures.append(
                        "Ineffective warning string in {0}:{1}".format(
                            relative, node.lineno))

            if isinstance(node, ast.Compare):
                operands = [node.left] + list(node.comparators)
                for operator, left, right in zip(
                        node.ops, operands, operands[1:]):
                    if (isinstance(operator, (ast.Is, ast.IsNot))
                            and (isinstance(left, ast.Subscript)
                                 or isinstance(right, ast.Subscript))):
                        failures.append(
                            "Container element compared by identity in {0}:{1}"
                            .format(relative, node.lineno))

            if (isinstance(node, ast.Assign)
                    and isinstance(node.value, ast.Call)
                    and isinstance(node.value.func, ast.Attribute)
                    and isinstance(node.value.func.value, ast.Name)
                    and node.value.func.value.id == "self"):
                names = []
                for target in node.targets:
                    names.extend(assignment_names(target))
                for name in names:
                    if is_callback_name(name):
                        failures.append(
                            "Callback stores a call result in {0}:{1} ({2})".format(
                                relative, node.lineno, name))

            if isinstance(node, (ast.If, ast.While)):
                condition = node.test
                if isinstance(condition, ast.UnaryOp) and isinstance(condition.op, ast.Not):
                    condition = condition.operand
                if (isinstance(condition, ast.Attribute)
                        and isinstance(condition.value, ast.Name)):
                    imported_module = import_aliases.get(condition.value.id)
                    if (imported_module
                            and condition.attr in module_functions[imported_module]):
                        failures.append(
                            "Imported function tested without a call in {0}:{1} "
                            "({2}.{3})".format(
                                relative, node.lineno, condition.value.id,
                                condition.attr))

            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute):
                continue
            keywords = {keyword.arg: keyword.value for keyword in node.keywords}
            if node.func.attr == "button":
                has_callback = "c" in keywords or "command" in keywords
                is_disabled = any(
                    name in keywords and literal_false(keywords[name])
                    for name in ("enable", "en"))
                if not has_callback and not is_disabled:
                    failures.append(
                        "Enabled button has no callback in {0}:{1}".format(
                            relative, node.lineno))

            if node.func.attr == "menuItem":
                for name in ("c", "command"):
                    if name in keywords and literal_string(keywords[name]) is not None:
                        failures.append(
                            "Menu item uses a string callback in {0}:{1}".format(
                                relative, node.lineno))

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("DPyTools semantic source check: OK ({0} modules)".format(len(trees)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
