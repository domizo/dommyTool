#!/usr/bin/env python3
"""Freeze and validate the serialized Maya MTypeId table."""

from __future__ import print_function

import ast
import hashlib
import os
import re
import sys


REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
EXPECTED_CONTRACT_SHA256 = "c5eeaea3537a16bea7e907f65ef82fe7ac1d976919edd3e239fc6b80033f214c"

DEFINE_PATTERN = re.compile(r"^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+(.+?)\s*$")
TYPE_ID_PATTERN = re.compile(
    r"\bMTypeId\s+([A-Za-z_]\w*(?:::\w+)*)\s*"
    r"\(\s*([A-Z][A-Z0-9_]*|0[xX][0-9A-Fa-f]+[uUlL]*)\s*\)\s*;",
    re.MULTILINE,
)
REGISTRATION_PATTERN = re.compile(
    r"\b([A-Za-z_]\w*)::(?:id|typeId)\b"
)
RESERVED_TYPE_ID_CLASSES = frozenset(
    (
        "BTHingeConstraint",
        "BTSliderConstraint",
        "DOpenVDBCreate",
        "DPAttributePromote",
    )
)


def project_files(suffixes):
    source_root = os.path.join(REPOSITORY_ROOT, "src")
    for directory, subdirectories, filenames in os.walk(source_root):
        subdirectories[:] = sorted(
            name for name in subdirectories if name not in ("old", "backup")
        )
        for filename in sorted(filenames):
            if filename.endswith(suffixes):
                yield os.path.join(directory, filename)


def registration_files():
    roots = (
        os.path.join(REPOSITORY_ROOT, "src", "plugin", "registration"),
        os.path.join(
            REPOSITORY_ROOT,
            "src",
            "integrations",
            "arnold",
            "DArnoldExtentionPlugins.cpp",
        ),
    )
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for directory, _, filenames in os.walk(root):
            for filename in sorted(filenames):
                yield os.path.join(directory, filename)


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def evaluate_node(node, definitions, cache, stack):
    if isinstance(node, ast.Expression):
        return evaluate_node(node.body, definitions, cache, stack)
    if isinstance(node, ast.Num):
        return int(node.n)
    if hasattr(ast, "Constant") and isinstance(node, ast.Constant):
        if isinstance(node.value, int):
            return int(node.value)
        raise ValueError("non-integer constant")
    if isinstance(node, ast.Name):
        return evaluate_name(node.id, definitions, cache, stack)
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
        value = evaluate_node(node.operand, definitions, cache, stack)
        return value if isinstance(node.op, ast.UAdd) else -value
    if isinstance(node, ast.BinOp) and isinstance(
        node.op, (ast.Add, ast.Sub, ast.BitOr, ast.BitAnd)
    ):
        left = evaluate_node(node.left, definitions, cache, stack)
        right = evaluate_node(node.right, definitions, cache, stack)
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
        if isinstance(node.op, ast.BitOr):
            return left | right
        return left & right
    raise ValueError("unsupported ID expression")


def evaluate_name(name, definitions, cache, stack):
    if name in cache:
        return cache[name]
    if name in stack:
        raise ValueError("recursive definition: {}".format(name))
    if name not in definitions:
        raise ValueError("undefined ID symbol: {}".format(name))

    expression = re.sub(r"(?<=\w)[uUlL]+\b", "", definitions[name])
    stack.add(name)
    try:
        tree = ast.parse(expression, mode="eval")
        value = evaluate_node(tree, definitions, cache, stack)
    finally:
        stack.remove(name)
    cache[name] = value
    return value


def main():
    definitions = {}
    errors = []
    for path in project_files((".h", ".hpp", ".inl", ".cpp")):
        with open(path, "r", errors="replace") as stream:
            source = strip_comments(stream.read())
        for line in source.splitlines():
            match = DEFINE_PATTERN.match(line)
            if not match:
                continue
            name, expression = match.groups()
            previous = definitions.get(name)
            if previous is not None and previous != expression:
                errors.append("{} has conflicting definitions".format(name))
            definitions[name] = expression

    cache = {}
    records = []
    owners = set()
    values = {}
    for path in project_files((".cpp", ".cc", ".cxx")):
        with open(path, "r", errors="replace") as stream:
            source = strip_comments(stream.read())
        relative_path = os.path.relpath(path, REPOSITORY_ROOT)
        for match in TYPE_ID_PATTERN.finditer(source):
            owner, token = match.groups()
            try:
                if token.lower().startswith("0x"):
                    value = int(re.sub(r"[uUlL]+$", "", token), 16)
                else:
                    value = evaluate_name(token, definitions, cache, set())
            except (SyntaxError, ValueError) as error:
                errors.append("{}: {}: {}".format(relative_path, owner, error))
                continue

            if owner in owners:
                errors.append("MTypeId owner is defined more than once: {}".format(owner))
                continue
            owners.add(owner)
            values.setdefault(value, []).append(owner)
            records.append((owner, value))

    for value, value_owners in sorted(values.items()):
        if len(value_owners) > 1:
            errors.append(
                "MTypeId 0x{:08X} is shared by {}".format(
                    value, ", ".join(sorted(value_owners))
                )
            )

    registered_classes = set()
    for path in registration_files():
        with open(path, "r", errors="replace") as stream:
            source = strip_comments(stream.read())
        registered_classes.update(REGISTRATION_PATTERN.findall(source))

    defined_classes = set(owner.rsplit("::", 1)[0] for owner, _ in records)
    unreachable_classes = (
        defined_classes - registered_classes - RESERVED_TYPE_ID_CLASSES
    )
    missing_reservations = RESERVED_TYPE_ID_CLASSES - defined_classes
    registered_reservations = RESERVED_TYPE_ID_CLASSES & registered_classes
    if unreachable_classes:
        errors.append(
            "MTypeId owners are neither registered nor reserved: {}".format(
                ", ".join(sorted(unreachable_classes))
            )
        )
    if missing_reservations:
        errors.append(
            "reserved MTypeId owners are missing definitions: {}".format(
                ", ".join(sorted(missing_reservations))
            )
        )
    if registered_reservations:
        errors.append(
            "reserved MTypeId owners entered a registration surface: {}".format(
                ", ".join(sorted(registered_reservations))
            )
        )

    normalized = "\n".join(
        "{}=0x{:08X}".format(owner, value)
        for owner, value in sorted(records)
    )
    digest = hashlib.sha256(normalized.encode("ascii")).hexdigest()
    if digest != EXPECTED_CONTRACT_SHA256:
        errors.append(
            "MTypeId contract digest changed: {} ({} definitions)".format(
                digest, len(records)
            )
        )

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(
        "Maya MTypeId uniqueness, reachability, and serialized contract: "
        "OK ({} definitions; {} reserved)".format(
            len(records), len(RESERVED_TYPE_ID_CLASSES)
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
