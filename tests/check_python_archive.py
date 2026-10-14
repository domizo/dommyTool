#!/usr/bin/env python3
"""Source-only checks for the archived Maya Python package.

The runtime target is Maya 2019 / Python 2.7, so this check deliberately does
not require the modules to import under the host Python interpreter.
"""

from __future__ import print_function

import ast
import builtins
import io
import re
import symtable
import sys
import tokenize
import warnings
from pathlib import Path

try:
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        from lib2to3.refactor import RefactoringTool, get_fixers_from_package
except ImportError:
    RefactoringTool = None
    get_fixers_from_package = None


REPO_ROOT = Path(__file__).resolve().parents[1]
PYTHON_ROOT = REPO_ROOT / "python"
PACKAGE_ROOT = PYTHON_ROOT / "DPyTools"
BUILTIN_NAMES = set(dir(builtins))

IMPORT_RE = re.compile(r"^\s*import\s+(DPyTools(?:\.[A-Za-z_]\w*)+)", re.MULTILINE)
FROM_RE = re.compile(r"^\s*from\s+(DPyTools(?:\.[A-Za-z_]\w*)*)\s+import\s+", re.MULTILINE)
CODING_RE = re.compile(br"coding[:=]\s*([-\w.]+)")
MUTABLE_DEFAULT_RE = re.compile(
    r"def\s+[A-Za-z_]\w*\s*\([^)]*=\s*(?:\[|\{)", re.MULTILINE | re.DOTALL)
BARE_EXCEPT_RE = re.compile(r"^\s*except\s*:\s*(?:#.*)?$", re.MULTILINE)
TRAILING_WHITESPACE_RE = re.compile(r"[ \t]+$", re.MULTILINE)
MUTABLE_FACTORY_NAMES = {"list", "dict", "set", "defaultdict"}


def module_exists(module_name):
    candidate = PYTHON_ROOT.joinpath(*module_name.split("."))
    return candidate.with_suffix(".py").is_file() or (candidate / "__init__.py").is_file()


def literal_value(node):
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.NameConstant):
        return node.value
    return object()


def is_mutable_class_value(node):
    if isinstance(node, (
            ast.List, ast.Dict, ast.Set,
            ast.ListComp, ast.DictComp, ast.SetComp)):
        return True
    if not isinstance(node, ast.Call):
        return False
    function_name = getattr(node.func, "id", None)
    if function_name is None and isinstance(node.func, ast.Attribute):
        function_name = node.func.attr
    return function_name in MUTABLE_FACTORY_NAMES


def main():
    failures = []
    python_files = sorted(PACKAGE_ROOT.rglob("*.py"))
    parser = None
    if RefactoringTool is not None:
        parser = RefactoringTool(get_fixers_from_package("lib2to3.fixes"))

    for path in python_files:
        raw = path.read_bytes()
        relative = path.relative_to(REPO_ROOT)

        if any(byte >= 128 for byte in raw):
            first_two_lines = b"\n".join(raw.splitlines()[:2])
            if not CODING_RE.search(first_two_lines):
                failures.append("Missing Python 2 encoding declaration: {0}".format(relative))

        try:
            source = raw.decode("utf-8")
        except UnicodeDecodeError as error:
            failures.append("Invalid UTF-8 in {0}: {1}".format(relative, error))
            continue

        if parser is None and MUTABLE_DEFAULT_RE.search(source):
            failures.append("Mutable function default in {0}".format(relative))
        if BARE_EXCEPT_RE.search(source):
            failures.append("Bare except clause in {0}".format(relative))
        if TRAILING_WHITESPACE_RE.search(source):
            failures.append("Trailing whitespace in {0}".format(relative))

        try:
            tokens = tokenize.generate_tokens(io.StringIO(source).readline)
            for token in tokens:
                if token.type == tokenize.OP and token.string == ";":
                    failures.append(
                        "Semicolon statement terminator in {0}:{1}".format(
                            relative, token.start[0]))
        except (IndentationError, tokenize.TokenError) as error:
            failures.append("Python tokenization error in {0}: {1}".format(relative, error))

        if parser is not None:
            try:
                converted = str(parser.refactor_string(source + "\n", str(relative)))
            except Exception as error:
                failures.append("Python 2 grammar error in {0}: {1}".format(relative, error))
            else:
                try:
                    compile(converted, str(relative), "exec")
                except (SyntaxError, TabError) as error:
                    failures.append(
                        "2to3 compile blocker in {0}: {1}".format(relative, error))
                else:
                    converted_tree = ast.parse(converted, str(relative))
                    symbol_table = symtable.symtable(
                        converted, str(relative), "exec")
                    module_definitions = {
                        symbol.get_name()
                        for symbol in symbol_table.get_symbols()
                        if (symbol.is_assigned() or symbol.is_imported()
                            or symbol.is_namespace())
                    }
                    child_tables = list(symbol_table.get_children())
                    while child_tables:
                        child_table = child_tables.pop()
                        child_tables.extend(child_table.get_children())
                        for symbol in child_table.get_symbols():
                            name = symbol.get_name()
                            if (symbol.is_referenced() and symbol.is_global()
                                    and name not in module_definitions
                                    and name not in BUILTIN_NAMES):
                                failures.append(
                                    "Undefined global name {0!r} in {1}:{2}".format(
                                        name, relative, child_table.get_lineno()))

                    loaded_names = {
                        node.id for node in ast.walk(converted_tree)
                        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
                    }
                    for statement in converted_tree.body:
                        imports = []
                        if isinstance(statement, ast.Import):
                            imports = [
                                (alias.asname or alias.name.split('.')[0], alias.name)
                                for alias in statement.names
                            ]
                        elif isinstance(statement, ast.ImportFrom):
                            if statement.module == '__future__':
                                continue
                            imports = [
                                (alias.asname or alias.name, alias.name)
                                for alias in statement.names if alias.name != '*'
                            ]
                        for local_name, imported_name in imports:
                            if local_name not in loaded_names:
                                failures.append(
                                    "Unused import {0!r} in {1}:{2}".format(
                                        imported_name, relative, statement.lineno))

                    for function_node in (
                            node for node in ast.walk(converted_tree)
                            if isinstance(node, ast.FunctionDef)):
                        function_body = list(function_node.body)
                        if (function_body
                                and isinstance(function_body[0], ast.Expr)
                                and isinstance(literal_value(function_body[0].value), str)):
                            function_body = function_body[1:]
                        if (len(function_body) == 1
                                and isinstance(function_body[0], ast.Pass)):
                            failures.append(
                                "Pass-only function in {0}:{1} ({2})".format(
                                    relative, function_node.lineno, function_node.name))

                        defaults = list(function_node.args.defaults)
                        defaults.extend(
                            default for default in function_node.args.kw_defaults
                            if default is not None)
                        if any(isinstance(default, (ast.List, ast.Dict, ast.Set))
                               for default in defaults):
                            failures.append(
                                "Mutable function default in {0}:{1} ({2})".format(
                                    relative, function_node.lineno, function_node.name))

                    for class_node in (
                            node for node in ast.walk(converted_tree)
                            if isinstance(node, ast.ClassDef)):
                        for statement in class_node.body:
                            if isinstance(statement, ast.FunctionDef):
                                decorators = {
                                    getattr(decorator, 'id', None)
                                    or getattr(decorator, 'attr', None)
                                    for decorator in statement.decorator_list
                                }
                                if 'staticmethod' not in decorators:
                                    expected = (
                                        'cls' if 'classmethod' in decorators else 'self')
                                    first_arg = (
                                        statement.args.args[0].arg
                                        if statement.args.args else None)
                                    if first_arg != expected:
                                        failures.append(
                                            "Invalid method receiver in {0}:{1} "
                                            "({2}.{3}; expected {4})".format(
                                                relative, statement.lineno,
                                                class_node.name, statement.name, expected))

                            if not isinstance(statement, ast.Assign):
                                continue
                            if is_mutable_class_value(statement.value):
                                failures.append(
                                    "Mutable class state in {0}:{1} ({2})".format(
                                        relative, statement.lineno, class_node.name))

                    for node in ast.walk(converted_tree):
                        if (isinstance(node, ast.Call)
                                and isinstance(node.func, ast.Attribute)
                                and node.func.attr == "has_key"):
                            failures.append(
                                "Legacy dict.has_key call in {0}:{1}".format(
                                    relative, node.lineno))

                        if not isinstance(node, ast.Compare):
                            continue
                        operands = [node.left] + list(node.comparators)
                        for operator, left, right in zip(
                                node.ops, operands, operands[1:]):
                            values = (literal_value(left), literal_value(right))
                            if (isinstance(operator, (ast.Is, ast.IsNot))
                                    and any(value is True or value is False
                                            for value in values)):
                                failures.append(
                                    "Boolean identity comparison in {0}:{1}".format(
                                        relative, node.lineno))
                            if (isinstance(operator, (ast.Eq, ast.NotEq))
                                    and any(value is None for value in values)):
                                failures.append(
                                    "None equality comparison in {0}:{1}".format(
                                        relative, node.lineno))

        for match in IMPORT_RE.finditer(source):
            module_name = match.group(1)
            if not module_exists(module_name):
                failures.append("Unresolved internal import {0!r} in {1}".format(module_name, relative))

        for match in FROM_RE.finditer(source):
            module_name = match.group(1)
            if not module_exists(module_name):
                failures.append("Unresolved internal import {0!r} in {1}".format(module_name, relative))

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    print("Archived Python package check: OK ({0} files)".format(len(python_files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
