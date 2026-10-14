#!/usr/bin/env python3
"""Audit Maya-owned plug-in data and registered extension declarations."""

from __future__ import print_function

import os
import re
import sys


REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
REGISTRATION_ROOT = os.path.join(
    REPOSITORY_ROOT, "src", "plugin", "registration", "modules"
)
REGISTRATION_FILES = (
    "CoreRegistrations.inl",
    "PointRegistrations.inl",
    "CurveRegistrations.inl",
    "MeshRegistrations.inl",
    "RenderRegistrations.inl",
)
DEBT_MANIFEST = os.path.join(
    REPOSITORY_ROOT, "tests", "maya_data_pointer_output_debt.txt"
)

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(REPOSITORY_ROOT, "tests"))

from check_active_cpp_safety import strip_comments_and_literals  # noqa: E402


NODE_ID_PATTERN = re.compile(r"([A-Za-z_]\w*)::(?:typeId|id)\b")
COMMAND_CREATOR_PATTERN = re.compile(r"([A-Za-z_]\w*)::creator\b")
DATA_ID_PATTERN = re.compile(r"([A-Za-z_]\w*)::(?:typeId|id)\b")
NODE_DEFINITION_PATTERN = re.compile(
    r"(?:MTypeId|const\s+MTypeId)\s+"
    r"([A-Za-z_]\w*)::(?:typeId|id)\b"
)
CLASS_DECLARATION_PATTERN = (
    r"class\s+(?:[A-Za-z_]\w*_EXPORT\s+)?"
    r"(?P<class>{})\s+(?P<final>final\s+)?:"
)
COMPUTE_DECLARATION_PATTERN = re.compile(
    r"\b(?P<virtual>virtual\s+)?MStatus\s+compute\s*"
    r"\([^;]*\)\s*(?P<qualifiers>[^;]*);",
    re.DOTALL,
)
PLUGIN_DATA_POINTER_PATTERN = re.compile(
    r"(?:const\s+)?"
    r"(?:DPData|DMeshData|DCurveData|DOpenVDBData)"
    r"\s*\*\s*(?:const\s+)?(?P<name>[A-Za-z_]\w*)"
)
UNCHECKED_OBJECT_OUTPUT_PATTERN = re.compile(
    r"MDataHandle\s+(?P<handle>[A-Za-z_]\w*)\s*=\s*"
    r"[A-Za-z_]\w*\.outputValue\(\s*[^,()\n]+\s*\)\s*;"
    r"(?P<tail>.{0,800})",
    re.DOTALL,
)
C_STDIO_INCLUDE_PATTERN = re.compile(
    r"#\s*include\s*<(?:cstdio|stdio\.h)>"
)
C_STDIO_USE_PATTERN = re.compile(
    r"\b(?:FILE|fclose|fopen|fprintf|perror|printf|puts|snprintf|sprintf)\b"
)


def default_registration_text(path):
    """Return registration source with every default-off feature removed."""
    active = [True]
    output = []
    with open(path, "r") as source_file:
        for line in source_file:
            directive = line.strip()
            feature = re.match(r"#if\s+DOMMY_WITH_([A-Za-z0-9_]+)", directive)
            if feature:
                active.append(False)
                continue
            if directive.startswith("#else"):
                if len(active) == 1:
                    raise ValueError("unmatched #else in {}".format(path))
                active[-1] = active[-2] and not active[-1]
                continue
            if directive.startswith("#endif"):
                if len(active) == 1:
                    raise ValueError("unmatched #endif in {}".format(path))
                active.pop()
                continue
            if active[-1]:
                output.append(line)
    if len(active) != 1:
        raise ValueError("unterminated feature block in {}".format(path))
    return strip_comments_and_literals("".join(output))


def registered_node_classes():
    classes = set()
    for file_name in REGISTRATION_FILES:
        source = default_registration_text(
            os.path.join(REGISTRATION_ROOT, file_name)
        )
        for call in re.findall(
            r"registration\.registerNode\((.*?)\)\s*\)", source, re.DOTALL
        ):
            identifiers = NODE_ID_PATTERN.findall(call)
            if identifiers:
                classes.add(identifiers[0])
    return classes


def all_registration_text():
    """Return every registration branch, with comments and literals removed."""
    sources = []
    for file_name in sorted(os.listdir(REGISTRATION_ROOT)):
        if not file_name.endswith(".inl"):
            continue
        path = os.path.join(REGISTRATION_ROOT, file_name)
        with open(path, "r") as source_file:
            sources.append(source_file.read())
    return strip_comments_and_literals("\n".join(sources))


def registered_classes(source, registration_method, identifier_pattern):
    """Return concrete classes referenced by one registration API."""
    classes = set()
    call_pattern = (
        r"registration\.{}\s*\((.*?)\)\s*\)".format(registration_method)
    )
    for call in re.findall(call_pattern, source, re.DOTALL):
        identifiers = identifier_pattern.findall(call)
        if identifiers:
            classes.add(identifiers[0])
    return classes


def source_files_for_classes(classes):
    sources = {}
    source_root = os.path.join(REPOSITORY_ROOT, "src")
    for directory, _, file_names in os.walk(source_root):
        for file_name in file_names:
            if not file_name.endswith((".cpp", ".cc")):
                continue
            path = os.path.join(directory, file_name)
            with open(path, "r", errors="replace") as source_file:
                source = strip_comments_and_literals(source_file.read())
            for class_name in set(NODE_DEFINITION_PATTERN.findall(source)) & classes:
                relative_path = os.path.relpath(path, REPOSITORY_ROOT)
                previous = sources.get(class_name)
                if previous and previous != relative_path:
                    raise ValueError(
                        "multiple node-id definitions for {}: {}, {}".format(
                            class_name, previous, relative_path
                        )
                    )
                sources[class_name] = relative_path
    return sources


def project_cpp_sources():
    sources = []
    source_root = os.path.join(REPOSITORY_ROOT, "src")
    for directory, _, file_names in os.walk(source_root):
        for file_name in file_names:
            if file_name.endswith((".cpp", ".cc")):
                sources.append(
                    os.path.relpath(
                        os.path.join(directory, file_name),
                        REPOSITORY_ROOT,
                    )
                )
    return sources


def class_declaration_body(source, declaration, class_name, relative_path):
    """Return one complete class declaration, including nested type bodies."""
    opening_brace = source.find("{", declaration.end())
    if opening_brace < 0:
        raise ValueError(
            "class body is missing for {} in {}".format(
                class_name, relative_path
            )
        )
    depth = 0
    for index in range(opening_brace, len(source)):
        character = source[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[declaration.start():index + 1]
    raise ValueError(
        "unterminated class declaration for {} in {}".format(
            class_name, relative_path
        )
    )


def registered_extension_header_style(classes):
    """Require concrete registered extensions to declare modern override intent."""
    headers = {}
    source_root = os.path.join(REPOSITORY_ROOT, "src")
    for directory, _, file_names in os.walk(source_root):
        for file_name in file_names:
            if not file_name.endswith((".h", ".hpp")):
                continue
            path = os.path.join(directory, file_name)
            with open(path, "r", errors="replace") as source_file:
                raw_source = source_file.read()
            source = strip_comments_and_literals(raw_source)
            has_unused_c_stdio = (
                C_STDIO_INCLUDE_PATTERN.search(raw_source) is not None
                and C_STDIO_USE_PATTERN.search(source) is None
            )
            for class_name in classes:
                declaration = re.search(
                    CLASS_DECLARATION_PATTERN.format(re.escape(class_name)),
                    source,
                )
                if declaration:
                    relative_path = os.path.relpath(path, REPOSITORY_ROOT)
                    previous = headers.get(class_name)
                    if previous and previous[0] != relative_path:
                        raise ValueError(
                            "multiple class declarations for {}: {}, {}".format(
                                class_name, previous[0], relative_path
                            )
                        )
                    class_body = class_declaration_body(
                        source, declaration, class_name, relative_path
                    )
                    headers[class_name] = (
                        relative_path,
                        bool(declaration.group("final")),
                        tuple(COMPUTE_DECLARATION_PATTERN.finditer(class_body)),
                        class_body,
                        has_unused_c_stdio,
                    )

    missing = sorted(classes - set(headers))
    not_final = sorted(
        class_name
        for class_name, (_, is_final, _, _, _) in headers.items()
        if not is_final
    )
    invalid_compute = []
    redundant_virtual = []
    unused_c_stdio = []
    for class_name, (
        _,
        _,
        compute_declarations,
        class_body,
        has_unused_c_stdio,
    ) in headers.items():
        for declaration in compute_declarations:
            qualifiers = declaration.group("qualifiers")
            if (
                "override" not in qualifiers
                or declaration.group("virtual") is not None
            ):
                invalid_compute.append(class_name)
                break
        if re.search(r"\bvirtual\b", class_body):
            redundant_virtual.append(class_name)
        if has_unused_c_stdio:
            unused_c_stdio.append(class_name)
    return (
        missing,
        not_final,
        sorted(invalid_compute),
        sorted(redundant_virtual),
        sorted(unused_c_stdio),
    )


def raw_publication_counts(source_paths):
    counts = {}
    for relative_path in set(source_paths):
        path = os.path.join(REPOSITORY_ROOT, relative_path)
        with open(path, "r", errors="replace") as source_file:
            source = strip_comments_and_literals(source_file.read())
        pointer_names = {
            match.group("name")
            for match in PLUGIN_DATA_POINTER_PATTERN.finditer(source)
        }
        count = 0
        for pointer_name in pointer_names:
            count += len(
                re.findall(
                    r"\.set\(\s*{}\s*\)".format(re.escape(pointer_name)),
                    source,
                )
            )
        if count:
            counts[relative_path] = count
    return counts


def unchecked_object_output_handles(source_paths):
    failures = []
    for relative_path in set(source_paths):
        path = os.path.join(REPOSITORY_ROOT, relative_path)
        with open(path, "r", errors="replace") as source_file:
            source = strip_comments_and_literals(source_file.read())
        for match in UNCHECKED_OBJECT_OUTPUT_PATTERN.finditer(source):
            handle = re.escape(match.group("handle"))
            if re.search(
                r"\b{}\s*\.set\(\s*"
                r"[A-Za-z_]\w*\.object\(\)\s*\)".format(handle),
                match.group("tail"),
            ):
                failures.append(relative_path)
                break
    return sorted(failures)


def load_debt_manifest():
    entries = {}
    with open(DEBT_MANIFEST, "r") as manifest_file:
        for line_number, line in enumerate(manifest_file, 1):
            entry = line.strip()
            if not entry or entry.startswith("#"):
                continue
            fields = entry.rsplit(None, 1)
            if len(fields) != 2 or not fields[1].isdigit():
                raise ValueError(
                    "{}:{} invalid debt entry".format(
                        os.path.relpath(DEBT_MANIFEST, REPOSITORY_ROOT),
                        line_number,
                    )
                )
            relative_path, count_text = fields
            if relative_path in entries:
                raise ValueError("duplicate debt entry: {}".format(relative_path))
            entries[relative_path] = int(count_text)
    return entries


def main():
    try:
        classes = registered_node_classes()
        registration_source = all_registration_text()
        all_node_classes = registered_classes(
            registration_source, "registerNode", NODE_ID_PATTERN
        )
        command_classes = registered_classes(
            registration_source,
            "registerCommand",
            COMMAND_CREATOR_PATTERN,
        )
        data_classes = registered_classes(
            registration_source, "registerData", DATA_ID_PATTERN
        )
        registered_extensions = (
            all_node_classes | command_classes | data_classes
        )
        sources = source_files_for_classes(classes)
        missing_classes = sorted(classes - set(sources))
        if missing_classes:
            raise ValueError(
                "default registered nodes without source definitions: {}".format(
                    ", ".join(missing_classes)
                )
            )

        actual = raw_publication_counts(sources.values())
        expected = load_debt_manifest()
        all_cpp_sources = project_cpp_sources()
        project_raw_publications = raw_publication_counts(all_cpp_sources)
        unchecked_outputs = unchecked_object_output_handles(all_cpp_sources)
        (
            missing_headers,
            non_final_classes,
            invalid_compute_classes,
            redundant_virtual_classes,
            unused_c_stdio_classes,
        ) = registered_extension_header_style(registered_extensions)
    except (IOError, OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1

    if actual != expected:
        for relative_path in sorted(set(actual) | set(expected)):
            actual_count = actual.get(relative_path, 0)
            expected_count = expected.get(relative_path, 0)
            if actual_count != expected_count:
                print(
                    "{} raw plug-in data publications: expected {}, found {}".format(
                        relative_path, expected_count, actual_count
                    ),
                    file=sys.stderr,
                )
        print(
            "Update code to publish MObject values, or deliberately refresh "
            "tests/maya_data_pointer_output_debt.txt after review.",
            file=sys.stderr,
        )
        return 1

    if project_raw_publications:
        for relative_path, count in sorted(
            project_raw_publications.items()
        ):
            print(
                "{} project-wide raw plug-in data publications: {}".format(
                    relative_path, count
                ),
                file=sys.stderr,
            )
        return 1

    if unchecked_outputs:
        print(
            "MObject publications acquire output handles without MStatus: "
            "{}".format(", ".join(unchecked_outputs)),
            file=sys.stderr,
        )
        return 1

    if missing_headers:
        print(
            "Registered extensions without class headers: {}".format(
                ", ".join(missing_headers)
            ),
            file=sys.stderr,
        )
        return 1

    if non_final_classes:
        print(
            "Registered extension classes must be final: {}".format(
                ", ".join(non_final_classes)
            ),
            file=sys.stderr,
        )
        return 1

    if invalid_compute_classes:
        print(
            "Registered compute declarations must use non-redundant override: "
            "{}".format(", ".join(invalid_compute_classes)),
            file=sys.stderr,
        )
        return 1

    if redundant_virtual_classes:
        print(
            "Final registered classes must use override instead of redundant "
            "virtual declarations: {}".format(
                ", ".join(redundant_virtual_classes)
            ),
            file=sys.stderr,
        )
        return 1

    if unused_c_stdio_classes:
        print(
            "Registered extension headers include unused C stdio: {}".format(
                ", ".join(unused_c_stdio_classes)
            ),
            file=sys.stderr,
        )
        return 1

    print(
        "Default Maya data ownership inventory: OK "
        "({} default nodes; {} total nodes, {} commands, {} data types; "
        "{} tracked legacy publications; all C++ sources publish "
        "Maya-owned objects)".format(
            len(classes),
            len(all_node_classes),
            len(command_classes),
            len(data_classes),
            sum(actual.values()),
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
