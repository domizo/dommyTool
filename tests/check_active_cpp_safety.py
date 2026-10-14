#!/usr/bin/env python3
"""Check active legacy sources without being confused by commented-out code."""

from __future__ import print_function

import os
import re
import sys


REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(REPOSITORY_ROOT, "tools"))

from list_active_cpp_sources import load_manifest  # noqa: E402


PLAYBACK_STATE_ALLOWED = set()
# Match the node method, not helpers such as ::initializeCurveConversionPlan.
INITIALIZE_METHOD_PATTERN = re.compile(r"\bMStatus\s+\w+::initialize\s*\(")
SERIAL_MAYA_API_SOURCES = {
    "src/core/Data/DPrimBaseData.cpp",
    "src/core/DDataUtility/Particle/DPEmitter.cpp",
    "src/core/DDataUtility/Particle/field/DPGoalField.cpp",
    "src/core/DDataUtility/Particle/field/DPFluidNoiseField.cpp",
    "src/core/DDataUtility/Particle/field/DPFluidVectorField.cpp",
    "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp",
    "src/modules/particles/particle/emitter/DPolygonEmitter.cpp",
    "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp",
    "src/modules/particles/particle/field/DKillField.cpp",
    "src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp",
    "src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp",
    "src/modules/curve/DCurve/Utility/DCClumping.cpp",
    "src/modules/curve/DCurve/Utility/DCWeightMap.cpp",
    "src/modules/point/DPoint/math/other/DPRemap.cpp",
}
VIEWPORT_BUFFER_SOURCES = {
    "src/modules/point/DPoint/Display/DPointDisplay.cpp",
    "src/modules/point/DPoint/Display/DPMarker.cpp",
    "src/modules/point/DPoint/Duplicator/DPInstancer.cpp",
    "src/modules/curve/DCurve/Display/DCurveDisplay.cpp",
    "src/modules/curve/DCurve/Display/DCMarker.cpp",
    "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp",
    "src/modules/mesh/DMesh/Display/DMMarker.cpp",
    "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp",
}
DIRECT_FUNCTION_SET_OUTPUT_ALLOWED = {
    # MFnNObjectData is Maya's nDynamics container, not one of the Dommy
    # MPxData types handled by DMayaPluginDataOutput.
    "src/modules/mesh/DMesh/Dynamics/nObject/DMnRigidGen.cpp",
}

# Only inventoried mandatory inputs are covered here. Genuine optional inputs
# (for example DCurveFromDP's target points and dynamics fallbacks) keep their
# separate nullable/optional contracts; this is not a global nullable ban.
REQUIRED_TYPED_INPUTS = {
    "src/core/DDataUtility/Attribute/DPAttributeBlend.cpp": ("DP", "inDP"),
    "src/core/DDataUtility/Utility/DPFillSpace.cpp": ("DP", "inDP"),
    "src/modules/point/DPoint/Deform/DPVectorDeform.cpp": ("DP", "inDP"),
    "src/modules/point/DPoint/Deform/DPPointDeform.cpp": ("DP", "inDP"),
    "src/modules/point/DPoint/Utility/DPAdjustSpace.cpp": ("DP", "inDP"),
    "src/modules/curve/DCurve/Utility/DCBend.cpp": ("DC", "inDC"),
    "src/modules/mesh/DMesh/Utility/DMCharactor.cpp": ("DM", "inDM"),
    "src/modules/mesh/DMesh/Utility/DMRemesh.cpp": ("DM", "inDM"),
    "src/modules/mesh/DMesh/Utility/DMDebrisGen.cpp": ("DM", "inDM"),
    "src/modules/mesh/DMesh/Utility/DMCopyUV.cpp": ("DM", "inDM"),
    "src/core/DDataUtility/Attribute/DPAttributeCreate.cpp":
        (("DP", "inDP"), ("DC", "inDC"), ("DM", "inDM")),
    "src/core/DDataUtility/Attribute/Convert/DPAttributeToDataArray.cpp": ("DP", "inDP"),
    "src/core/DDataUtility/Group/DPGroup.cpp": ("DP", "inDP"),
    "src/modules/curve/DCurve/Group/DCGroup.cpp": ("DC", "inDC"),
    "src/modules/mesh/DMesh/Data/DMGroup.cpp": ("DM", "inDM"),
    "src/modules/point/DPoint/mesh/DPToMesh.cpp": ("DP", "inDP"),
    "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp": ("DM", "inDM"),
    "src/modules/curve/DCurve/Generate/DCurveFromDP.cpp": ("DP", "inDP"),
    "src/modules/mesh/DMesh/Generate/DMWireGen.cpp": ("DC", "inDC"),
    "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp": (("DM", "inDM"), ("DP", "inDP")),
    "src/modules/point/DPoint/mesh/DPVoronoi.cpp": ("DP", "inDP"),
    "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp": (("DP", "inDP"), ("DP", "nextDP")),
    "src/core/DDataUtility/Attribute/Edit/DPAttributeVDB.cpp": ("DP", "inDP"),
    "src/modules/point/DPoint/Utility/DPClosestPointVDB.cpp": ("DP", "inDP"),
    "src/modules/point/DPoint/Utility/DPTimeShift.cpp": ("DP", "inDP"),
    "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp": ("DC", "inDC"),
    "src/modules/volume/VDB/Generate/DOpenVDBSamplingDP.cpp": ("DP", "inDP"),
    # The checked read stays inside missing-cache/bypass branches. An existing
    # valid cache does not require connected source points.
    "src/modules/point/DPoint/Utility/DPCache.cpp": ("DP", "inDP"),
    "src/core/DDataUtility/IO/DPWriteToAi.cpp": ("DP", "inDP"),
    "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp": ("DC", "inDC"),
}


def required_input_reader_errors(relative_path, source):
    contract = REQUIRED_TYPED_INPUTS.get(relative_path)
    if contract is None:
        return []
    errors = []
    contracts = (contract,) if isinstance(contract[0], str) else contract
    for suffix, attribute in contracts:
        call = r"\(\s*" + re.escape(attribute) + r"\s*,"
        if re.search(r"\bgetInput" + suffix + call, source):
            errors.append(relative_path + " discards mandatory input status via nullable reader: " + attribute)
        if not re.search(r"\bgetValidInput" + suffix + call, source):
            errors.append(relative_path + " is missing its mandatory checked input reader: " + attribute)
    return errors


def expression_input_binding_errors(source):
    errors = []
    for data_type in ("DPData", "DCurveData", "DMeshData"):
        binding = (r"DOMMY_RETURN_IF_FAILURE\s*\(\s*dommy::maya::getPluginDataFromHandle\s*"
                   r"\(\s*inputHandle\s*,\s*" + data_type + r"::id\s*,\s*input\s*\)\s*\)")
        if not re.search(binding, source):
            errors.append("DPAttributeSeExpr discards typed binding status: " + data_type)
    return errors


def strip_comments_and_literals(source):
    output = []
    index = 0
    state = "code"
    while index < len(source):
        character = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""

        if state == "code":
            if character == "/" and following == "/":
                output.extend("  ")
                index += 2
                state = "line_comment"
            elif character == "/" and following == "*":
                output.extend("  ")
                index += 2
                state = "block_comment"
            elif character == '"':
                output.append(" ")
                index += 1
                state = "string"
            elif character == "'":
                output.append(" ")
                index += 1
                state = "character"
            else:
                output.append(character)
                index += 1
        elif state == "line_comment":
            output.append("\n" if character == "\n" else " ")
            index += 1
            if character == "\n":
                state = "code"
        elif state == "block_comment":
            if character == "*" and following == "/":
                output.extend("  ")
                index += 2
                state = "code"
            else:
                output.append("\n" if character == "\n" else " ")
                index += 1
        else:
            if character == "\\" and following:
                output.append("\n" if following == "\n" else " ")
                output.append(" ")
                index += 2
            elif (state == "string" and character == '"') or (
                state == "character" and character == "'"
            ):
                output.append(" ")
                index += 1
                state = "code"
            else:
                output.append("\n" if character == "\n" else " ")
                index += 1

    return "".join(output)


def line_number(source, position):
    return source.count("\n", 0, position) + 1


UNCHECKED_CONNECTION_PATTERN = re.compile(
    r"(?:\.|->)\s*isConnected\s*\(\s*(?:nullptr|NULL|0)?\s*\)"
)


def connection_query_inventory():
    # This inventory is unresolved audit work, not approval of these calls.
    # Exact counts detect per-file increases and require explicitly shrinking the
    # inventory when an existing callback/translator/simulation path is fixed.
    path = os.path.join(REPOSITORY_ROOT, "tests/fixtures/unchecked_connection_queries.txt")
    result = {}
    with open(path) as stream:
        for line in stream:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            count, source = line.split()
            if source in result or int(count) <= 0:
                raise ValueError("Invalid unchecked connection inventory entry: " + line)
            result[source] = int(count)
    return result


def unobserved_maya_plug_get_values(source):
    """Yield getValue calls whose MStatus is discarded.

    This deliberately limits the receiver to MPlug expressions and common
    plug variable names so OpenVDB accessors with the same method name are not
    mistaken for Maya API calls.
    """
    get_value_pattern = re.compile(r"\.\s*getValue\s*\(")
    plug_receiver_pattern = re.compile(
        r"(?:\bMPlug\s*\([^;{}]*\)|"
        r"\b[A-Za-z_]\w*(?:[Pp]lug|[Pp]lg))\s*$",
        re.DOTALL,
    )
    for match in get_value_pattern.finditer(source):
        receiver_start = max(
            source.rfind(";", 0, match.start()),
            source.rfind("{", 0, match.start()),
            source.rfind("}", 0, match.start()),
        ) + 1
        statement_prefix = source[receiver_start:match.start()]
        if not plug_receiver_pattern.search(statement_prefix):
            continue

        # Assignment captures MStatus variables and checked value-returning
        # expressions. Conditions, returns, and Maya's status macros observe
        # the result directly.
        if (
            "=" in statement_prefix
            or re.search(r"\b(?:if|while|return)\b", statement_prefix)
            or "CHECK_MSTATUS" in statement_prefix
            or "DOMMY_RETURN_IF_FAILURE" in statement_prefix
        ):
            continue
        yield match.start()


PLUGIN_DATA_POINTER_PATTERN = re.compile(
    r"(?P<type>[A-Za-z_]\w*(?:::\w+)*)\s*\*\s*(?:const\s+)?"
    r"(?P<name>[A-Za-z_]\w*)\s*(?:\(|=)\s*"
    r"static_cast<[^>]+\*>\s*\(\s*"
    r"[A-Za-z_]\w*\.data\s*\(&[A-Za-z_]\w*\)\s*\)\s*\)?\s*;"
)
CHECKED_PLUGIN_DATA_PATTERN = re.compile(
    r"\bgetPluginData\([^,]+,\s*(?P<name>[A-Za-z_]\w*)\s*\)"
)
LEGACY_CHECKED_PLUGIN_DATA_PATTERN = re.compile(
    r"(?:DPData|DMeshData|DCurveData|DOpenVDBData)\s*\*\s*"
    r"(?:const\s+)?(?P<name>[A-Za-z_]\w*)\s*(?:\(|=)\s*"
    r"(?:dommy\s*::\s*maya\s*::\s*)?pluginDataPointer\s*<"
)
LEGACY_PLUGIN_DATA_POINTER_PATTERN = re.compile(
    r"(?P<type>DPData|DMeshData|DCurveData|DOpenVDBData)\s*\*\s*"
    r"(?:const\s+)?(?P<name>[A-Za-z_]\w*)\s*(?:\(|=)\s*"
    r"(?:dommy\s*::\s*maya\s*::\s*)?pluginDataPointer\s*<[^>]+>"
    r"\s*\([^;]+\)\s*\)?\s*;"
)
CUSTOM_DATA_OUTPUT_ATTRIBUTE_PATTERN = re.compile(
    r"(?m)^\s*(?P<name>(?:out\w*|a\w*(?:Out|Output)\w*))\s*=\s*"
    r"(?P<function_set>[A-Za-z_]\w*)\.create\([^;]*"
    r"(?:DPData::id|DCurveData::id|DMeshData::id|DOpenVDBData::id)"
    r"[^;]*;"
)
ATTRIBUTE_CREATE_PATTERN = re.compile(
    r"(?m)^\s*[A-Za-z_]\w*\s*=\s*[A-Za-z_]\w*\.create\("
)


def first_plugin_data_use_is_checked(source, match):
    """Return true when a pointer is null-checked before its first use."""
    name = re.escape(match.group("name"))
    # A guard or dereference is expected next to the conversion. Limiting the
    # window avoids treating an unrelated later scope as validation.
    tail = source[match.end():match.end() + 4000]
    uses = []
    patterns = (
        ("dereference", r"\b" + name + r"\s*->"),
        ("dereference", r"\*\s*" + name + r"\b"),
        ("guard", r"\bif\s*\(\s*(?:!\s*)?" + name + r"\b"),
        (
            "guard",
            r"\bif\s*\([^)]*" + name +
            r"\s*(?:==|!=)\s*(?:NULL|nullptr)",
        ),
    )
    for use_kind, pattern in patterns:
        found = re.search(pattern, tail)
        if found:
            uses.append((found.start(), use_kind))
    return not uses or min(uses)[1] == "guard"


def main():
    manifest_file = os.path.join(
        REPOSITORY_ROOT,
        "config/active_cpp_sources.txt",
    )
    sources = load_manifest(manifest_file, REPOSITORY_ROOT)
    errors = []
    mandatory_path = "src/modules/point/DPoint/Deform/DPVectorDeform.cpp"
    for sample, expected in (
            ("getValidInputDP(inDP, data, input);", False),
            ("getInputDP(inDP, data);", True),
            ('// getValidInputDP(inDP, data, input);\ngetInputDP(inDP, data);', True)):
        if bool(required_input_reader_errors(mandatory_path, strip_comments_and_literals(sample))) != expected:
            errors.append("mandatory input reader source-check self-test failed")
    multiple = "src/core/DDataUtility/Attribute/DPAttributeCreate.cpp"
    checked = "getValidInputDP(inDP,data,input); getValidInputDC(inDC,data,input); getValidInputDM(inDM,data,input);"
    if required_input_reader_errors(multiple, checked) or not required_input_reader_errors(
            multiple, checked.replace("getValidInputDC", "getInputDC")):
        errors.append("multi-output mandatory input source-check self-test failed")
    checked_bindings = " ".join("DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle," +
                                kind + "::id,input));" for kind in ("DPData", "DCurveData", "DMeshData"))
    if expression_input_binding_errors(checked_bindings) or not expression_input_binding_errors(
            checked_bindings.replace("DOMMY_RETURN_IF_FAILURE", "ignored")):
        errors.append("expression binding source-check self-test failed")
    playback_users = set()
    connection_inventory = connection_query_inventory()
    connection_counts = {}
    for source, count in (
        ("if (plug.isConnected()) {}", 1),
        ("plug . isConnected ( );", 1),
        ("plug.isConnected(nullptr);", 1),
        ("plug.isConnected(&status);", 0),
        ('// plug.isConnected();\nconst char* s="plug.isConnected()";', 0),
    ):
        actual = len(UNCHECKED_CONNECTION_PATTERN.findall(strip_comments_and_literals(source)))
        if actual != count:
            errors.append("unchecked connection source-check self-test failed")

    get_value_cases = (
        ("MPlug(node, attribute).getValue(value);", 1),
        ("inputPlug.getValue(value);", 1),
        ("MStatus status = inputPlug.getValue(value);", 0),
        ("if (inputPlug.getValue(value) != MS::kSuccess) return;", 0),
        ("const int value = accessor.getValue(coord);", 0),
    )
    for source, expected_count in get_value_cases:
        observed_count = len(list(unobserved_maya_plug_get_values(source)))
        if observed_count != expected_count:
            errors.append(
                "MPlug::getValue source-check self-test failed for {!r}".format(
                    source
                )
            )

    # A status-bearing acquisition must be checked before dereferencing the
    # returned handle. Chained getters cannot leave room for that check. This
    # guard intentionally does not flag wrappers that throw on failed reads.
    chained_input_getter_pattern = re.compile(
        r"\binputValue\s*\([^;{}]*,\s*&\s*[A-Za-z_]\w*\s*\)"
        r"\s*\.\s*as[A-Za-z0-9_]*\s*\(", re.DOTALL,
    )
    input_getter_cases = (
        ("block.inputValue(attr, &status).asDouble();", 1),
        ("block.inputValue(attr,\n &status) . asVector();", 1),
        ("auto h = block.inputValue(attr, &status); if (!status) return status; h.asBool();", 0),
        ("inputValue(attr).asBool();", 0),
        ('// block.inputValue(attr, &status).asBool();\nconst char* note = "inputValue(attr, &status).asBool()";', 0),
    )
    for source, expected_count in input_getter_cases:
        if len(list(chained_input_getter_pattern.finditer(strip_comments_and_literals(source)))) != expected_count:
            errors.append("status-bearing input getter source-check self-test failed")
    # Include headers and default-disabled source files, not just active TUs.
    for directory, _, filenames in os.walk(os.path.join(REPOSITORY_ROOT, "src")):
        for filename in filenames:
            if not filename.endswith((".cpp", ".h", ".hpp")):
                continue
            absolute_path = os.path.join(directory, filename)
            with open(absolute_path, "r", errors="replace") as stream:
                stripped = strip_comments_and_literals(stream.read())
            relative_path = os.path.relpath(absolute_path, REPOSITORY_ROOT)
            connection_count = len(UNCHECKED_CONNECTION_PATTERN.findall(stripped))
            if connection_count:
                connection_counts[relative_path] = connection_count
            if connection_count > connection_inventory.get(relative_path, 0):
                errors.append("{} introduces an untracked unchecked connection query ({} calls)".format(
                    relative_path, connection_count))
            for match in chained_input_getter_pattern.finditer(stripped):
                errors.append("{}:{} reads a status-bearing input handle before checking acquisition".format(
                    os.path.relpath(absolute_path, REPOSITORY_ROOT), line_number(stripped, match.start())))
    for path, count in connection_inventory.items():
        if connection_counts.get(path, 0) < count:
            errors.append("{}: shrink the resolved unchecked connection inventory".format(path))

    playback_pattern = re.compile(r"\bMAnimControl\s*::\s*currentTime\s*\(")
    global_random_pattern = re.compile(r"\b(?:srand|rand)\s*\(")
    parallel_pattern = re.compile(
        r"\btbb\s*::\s*(?:parallel_for|task_group)\b"
    )
    raw_array_ownership_pattern = re.compile(
        r"\b(?:new\s+[A-Za-z_][A-Za-z0-9_:]*"
        r"(?:\s*<[^;\n>]+>)?\s*\[|delete\s*\[\s*\])"
    )
    compute_pattern = re.compile(
        r"\bMStatus\s+[A-Za-z_][A-Za-z0-9_:<>]*\s*::\s*compute\s*\("
    )
    compute_guard_pattern = re.compile(
        r"\bDOMMY_(?:VDB_)?COMPUTE_CATCH\s*\("
    )
    raw_legacy_gl_scope_pattern = re.compile(
        r"\b(?:beginGL|endGL|glPushAttrib|glPopAttrib)\s*\("
    )
    direct_viewport_acquire_pattern = re.compile(r"->\s*acquire\s*\(")
    raw_custom_data_cast_pattern = re.compile(
        r"\(\s*(?:DPData|DMeshData|DCurveData)\s*\*\s*\)\s*"
        r"[A-Za-z_]\w*\.(?:constData|data)\s*\("
    )
    raw_custom_data_static_cast_pattern = re.compile(
        r"static_cast\s*<\s*(?:const\s+)?"
        r"(?:DPData|DMeshData|DCurveData|DOpenVDBData)\s*\*\s*>\s*"
        r"\(\s*[A-Za-z_]\w*\.data\s*\("
    )
    direct_console_pattern = re.compile(
        r"\b(?:std::)?(?:cout|cerr|clog)\s*<<|"
        r"\b(?:std::)?(?:printf|fprintf)\s*\("
    )
    direct_function_set_output_pattern = re.compile(
        r"\.\s*set\s*\(\s*[A-Za-z_]\w*\s*\.\s*object\s*"
        r"\(\s*\)\s*\)"
    )
    mutable_position_escape_pattern = re.compile(
        r"\bgetPositionPtr\s*\("
    )
    mutable_attribute_array_pattern = re.compile(
        r"(?<!const\s)std\s*::\s*vector\s*<\s*T\s*>\s*&\s*"
        r"(?:[A-Za-z_]\w*\s*::\s*)?(?:value|attrListValue)\s*\("
    )
    disabled_arnold_resource_pattern = re.compile(
        r"\bAt(?:Array|NodeIterator|UserParamIterator)\s*\*|"
        r"\bAi(?:Node|UserParam)IteratorDestroy\s*\("
    )
    comment_pattern = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)
    always_true_string_condition_pattern = re.compile(
        r'\|\|\s*"[^"\n]+"'
    )
    if (
        not always_true_string_condition_pattern.search(
            'name == "first" || "second"'
        )
        or always_true_string_condition_pattern.search(
            'name == "first" || name == "second"'
        )
    ):
        errors.append("string-literal condition source-check self-test failed")

    stripped_sources = {}
    for relative_path in sources:
        absolute_path = os.path.join(REPOSITORY_ROOT, relative_path)
        with open(absolute_path, "r", errors="replace") as stream:
            stripped = strip_comments_and_literals(stream.read())
        stripped_sources[relative_path] = stripped

        if relative_path.startswith("src/"):
            for match in mutable_position_escape_pattern.finditer(stripped):
                errors.append(
                    "{}:{} exposes mutable position storage outside the "
                    "validated replacement boundary".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            for match in mutable_attribute_array_pattern.finditer(stripped):
                errors.append(
                    "{}:{} exposes mutable attribute-array storage".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            for match in raw_array_ownership_pattern.finditer(stripped):
                errors.append(
                    "{}:{} uses project-owned raw array allocation".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            for match in raw_legacy_gl_scope_pattern.finditer(stripped):
                errors.append(
                    "{}:{} manages legacy OpenGL scope without the RAII guard".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            for match in raw_custom_data_cast_pattern.finditer(stripped):
                errors.append(
                    "{}:{} bypasses checked custom-data access".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            for match in raw_custom_data_static_cast_pattern.finditer(stripped):
                errors.append(
                    "{}:{} bypasses checked custom-data access".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )
            if relative_path not in DIRECT_FUNCTION_SET_OUTPUT_ALLOWED:
                for match in direct_function_set_output_pattern.finditer(
                    stripped
                ):
                    errors.append(
                        "{}:{} publishes a function-set object without the "
                        "reviewed output gate".format(
                            relative_path,
                            line_number(stripped, match.start()),
                        )
                    )

        if relative_path in VIEWPORT_BUFFER_SOURCES:
            match = direct_viewport_acquire_pattern.search(stripped)
            if match:
                errors.append(
                    "{}:{} bypasses checked VP2 buffer allocation".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )

        compute_matches = list(compute_pattern.finditer(stripped))
        if len(compute_matches) > 1:
            errors.append(
                "{} contains multiple compute definitions; the entry-point "
                "guard audit requires an explicit review".format(relative_path)
            )
        elif compute_matches:
            compute_position = compute_matches[0].start()
            guard_match = compute_guard_pattern.search(stripped, compute_position)
            initialize_match = INITIALIZE_METHOD_PATTERN.search(stripped, compute_position)
            initialize_position = initialize_match.start() if initialize_match else -1
            if not guard_match or (
                initialize_position >= 0 and guard_match.start() >= initialize_position
            ):
                errors.append(
                    "{} has no compute exception boundary".format(relative_path)
                )

        for match in playback_pattern.finditer(stripped):
            playback_users.add(relative_path)
            if relative_path not in PLAYBACK_STATE_ALLOWED:
                errors.append(
                    "{}:{} reads global playback time in active code".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )

        for match in global_random_pattern.finditer(stripped):
            errors.append(
                "{}:{} uses process-global C random state".format(
                    relative_path, line_number(stripped, match.start())
                )
            )

        if relative_path in SERIAL_MAYA_API_SOURCES:
            match = parallel_pattern.search(stripped)
            if match:
                errors.append(
                    "{}:{} parallelizes Maya API access".format(
                        relative_path, line_number(stripped, match.start())
                    )
                )

    if playback_users != PLAYBACK_STATE_ALLOWED:
        errors.append(
            "global playback-time allowlist is stale: observed {}".format(
                ", ".join(sorted(playback_users)) or "none"
            )
        )

    # Include dormant/optional modules as well as the active source manifest:
    # these files are part of the published source tree and may be re-enabled.
    source_root = os.path.join(REPOSITORY_ROOT, "src")
    for directory, _, filenames in os.walk(source_root):
        for filename in filenames:
            if not filename.endswith((".cc", ".cpp", ".h", ".hpp", ".inl")):
                continue
            absolute_path = os.path.join(directory, filename)
            relative_path = os.path.relpath(absolute_path, REPOSITORY_ROOT)
            with open(absolute_path, "r", errors="replace") as stream:
                raw_source = stream.read()
            source = stripped_sources.get(relative_path)
            if source is None:
                source = strip_comments_and_literals(raw_source)
            errors.extend(required_input_reader_errors(relative_path, source))
            if relative_path == "src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp":
                errors.extend(expression_input_binding_errors(source))
            for comment in comment_pattern.finditer(raw_source):
                unsafe_resource = disabled_arnold_resource_pattern.search(
                    comment.group(0)
                )
                if unsafe_resource:
                    errors.append(
                        "{}:{} retains a disabled raw Arnold resource "
                        "recipe".format(
                            relative_path,
                            line_number(raw_source, comment.start()),
                        )
                    )
            code_with_literals = comment_pattern.sub(
                lambda match: re.sub(r"[^\n]", " ", match.group(0)),
                raw_source,
            )
            for condition in always_true_string_condition_pattern.finditer(
                code_with_literals
            ):
                errors.append(
                    "{}:{} uses a string literal as a boolean condition".format(
                        relative_path,
                        line_number(code_with_literals, condition.start()),
                    )
                )
            if (relative_path !=
                    "src/core/common/utility/DMayaPluginData.h" and
                    re.search(r"\basPluginData\s*\(", source)):
                errors.append(
                    "{} calls MDataHandle::asPluginData outside the checked "
                    "accessor".format(relative_path)
                )
            direct_console = direct_console_pattern.search(source)
            if direct_console:
                errors.append(
                    "{}:{} writes directly to a process console instead of "
                    "the Maya diagnostic channel".format(
                        relative_path,
                        line_number(source, direct_console.start()),
                    )
                )
            for position in unobserved_maya_plug_get_values(source):
                errors.append(
                    "{}:{} discards MPlug::getValue status".format(
                        relative_path, line_number(source, position)
                    )
                )
            if (
                relative_path.endswith("TranslatorAi.cpp")
                and re.search(
                    r"\bAt(?:Node|UserParam)Iterator\s*\*", source
                )
            ):
                errors.append(
                    "{} uses a raw Arnold iterator instead of the shared "
                    "RAII owner".format(relative_path)
                )
            if not filename.endswith((".cc", ".cpp")):
                continue
            for match in CUSTOM_DATA_OUTPUT_ATTRIBUTE_PATTERN.finditer(source):
                next_attribute = ATTRIBUTE_CREATE_PATTERN.search(
                    source, match.end())
                block_end = (next_attribute.start() if next_attribute
                             else len(source))
                attribute_setup = source[match.end():block_end]
                function_set = re.escape(match.group("function_set"))
                if not re.search(
                        r"\b" + function_set +
                        r"\.setStorable\s*\(\s*false\s*\)",
                        attribute_setup):
                    errors.append(
                        "{}:{} custom-data output {} must be explicitly "
                        "non-storable".format(
                            relative_path,
                            line_number(source, match.start()),
                            match.group("name"),
                        )
                    )
            for match in PLUGIN_DATA_POINTER_PATTERN.finditer(source):
                if not first_plugin_data_use_is_checked(source, match):
                    errors.append(
                        "{}:{} dereferences unchecked Maya plug-in data".format(
                            relative_path, line_number(source, match.start())
                        )
                    )
            for match in LEGACY_PLUGIN_DATA_POINTER_PATTERN.finditer(source):
                if not first_plugin_data_use_is_checked(source, match):
                    errors.append(
                        "{}:{} dereferences unchecked Maya plug-in data".format(
                            relative_path, line_number(source, match.start())
                        )
                    )
            checked_plugin_data_matches = list(
                CHECKED_PLUGIN_DATA_PATTERN.finditer(source)
            )
            checked_plugin_data_matches.extend(
                LEGACY_CHECKED_PLUGIN_DATA_PATTERN.finditer(source)
            )
            for match in checked_plugin_data_matches:
                output_name = re.escape(match.group("name"))
                direct_publication = re.search(
                    r"(?m)^[ \t]*[A-Za-z_]\w*\.set\(\s*" +
                    output_name + r"\s*\)\s*;",
                    source[match.end():match.end() + 30000],
                )
                if direct_publication:
                    errors.append(
                        "{}:{} publishes Maya plug-in data without checking "
                        "MStatus".format(
                            relative_path,
                            line_number(
                                source,
                                match.end() + direct_publication.start(),
                            ),
                        )
                    )

    # A persistence callback that writes or reads nothing must not report
    # success: Maya would accept a scene payload while silently losing data.
    unsupported_persistence_methods = (
        ("src/core/DDataUtility/Data/DPData.cpp", "DPData", "readBinary"),
        ("src/core/DDataUtility/Data/DPData.cpp", "DPData", "writeBinary"),
        ("src/core/Data/DCurveData.cpp", "DCurveData", "readASCII"),
        ("src/core/Data/DCurveData.cpp", "DCurveData", "writeASCII"),
        ("src/core/Data/DCurveData.cpp", "DCurveData", "readBinary"),
        ("src/core/Data/DCurveData.cpp", "DCurveData", "writeBinary"),
        ("src/modules/mesh/DMesh/Data/DMeshData.cpp", "DMeshData", "readASCII"),
        ("src/modules/mesh/DMesh/Data/DMeshData.cpp", "DMeshData", "writeASCII"),
        ("src/modules/mesh/DMesh/Data/DMeshData.cpp", "DMeshData", "readBinary"),
        ("src/modules/mesh/DMesh/Data/DMeshData.cpp", "DMeshData", "writeBinary"),
    )
    for relative_path, class_name, method_name in unsupported_persistence_methods:
        source = stripped_sources[relative_path]
        method_pattern = re.compile(
            r"\bMStatus\s+{}\s*::\s*{}\s*\([^)]*\)\s*"
            r"\{{(?P<body>.*?)\}}".format(class_name, method_name),
            re.DOTALL,
        )
        method_match = method_pattern.search(source)
        if not method_match or not re.search(
            r"\breturn\s+MS\s*::\s*kFailure\s*;", method_match.group("body")
        ):
            errors.append(
                "{}::{} must explicitly reject unsupported persistence".format(
                    class_name, method_name
                )
            )

    instancer_path = "src/modules/point/DPoint/Duplicator/DPInstancer.cpp"
    instancer = stripped_sources[instancer_path]
    pointer_assignment = instancer.find("const DPData* dpdata")
    first_dereference = instancer.find("dpdata->", pointer_assignment)
    guard_match = re.search(
        r"\bif\s*\([^)]*!\s*dpdata\b",
        instancer[pointer_assignment:first_dereference]
        if pointer_assignment >= 0 and first_dereference >= 0
        else "",
    )
    if not (
        pointer_assignment >= 0 and
        first_dereference > pointer_assignment and
        guard_match
    ):
        errors.append("DPInstancer must reject null DPData before its first dereference")

    direction_path = "src/modules/point/DPoint/Utility/DPDirection.cpp"
    direction = stripped_sources[direction_path]
    direction_dispatch = direction.find("class DirectionGen")
    maya_object_case = direction.find("if (m_type == 2)", direction_dispatch)
    next_case = direction.find("if (m_type == 3)", maya_object_case)
    if direction_dispatch < 0 or maya_object_case < 0 or next_case < 0:
        errors.append("DPDirection Maya-object dispatch could not be located")
    elif parallel_pattern.search(direction, maya_object_case, next_case):
        errors.append("DPDirection parallelizes Maya curve, surface, or mesh function sets")

    serial_dispatches = (
        ("src/modules/point/DPoint/Utility/DPAvoid.cpp", "class AvoidGen", "case 1:", "case 2:"),
        ("src/modules/point/DPoint/Utility/DPAvoid.cpp", "class AvoidGen", "case 3:", "default:"),
        ("src/modules/point/DPoint/Utility/DPMagnet.cpp", "class MagnetGen", "case 1:", "case 2:"),
        ("src/modules/point/DPoint/Utility/DPMagnet.cpp", "class MagnetGen", "case 3:", "default:"),
        ("src/modules/point/DPoint/Utility/DPDistance.cpp", "class DistanceGen", "case 1:", "case 2:"),
    )
    for relative_path, class_token, start_token, end_token in serial_dispatches:
        source = stripped_sources[relative_path]
        dispatch = source.find(class_token)
        start = source.find(start_token, dispatch)
        end = source.find(end_token, start)
        if dispatch < 0 or start < 0 or end < 0:
            errors.append("{} serial dispatch could not be located".format(relative_path))
        elif parallel_pattern.search(source, start, end):
            errors.append("{} parallelizes Maya API access".format(relative_path))

    wholly_serial_generators = (
        ("src/modules/point/DPoint/Utility/DPAffect.cpp", "class AffectGen", "const MTypeId DPAffect"),
        ("src/modules/point/DPoint/Utility/DPSpiralField.cpp", "class SpiralGen", "const MTypeId DPSpiralField"),
    )
    for relative_path, class_token, end_token in wholly_serial_generators:
        source = stripped_sources[relative_path]
        start = source.find(class_token)
        end = source.find(end_token, start)
        if start < 0 or end < 0:
            errors.append("{} generator boundary could not be located".format(relative_path))
        elif parallel_pattern.search(source, start, end):
            errors.append("{} parallelizes Maya Ramp or function-set access".format(relative_path))

    guarded_compute_sources = (
        "src/modules/particles/particle/field/DCurlNoiseField.cpp",
        "src/modules/particles/particle/field/DKillField.cpp",
        "src/modules/point/DPoint/Utility/DPAffect.cpp",
        "src/modules/point/DPoint/Utility/DPAvoid.cpp",
        "src/modules/point/DPoint/Utility/DPMagnet.cpp",
        "src/modules/point/DPoint/Utility/DPDistance.cpp",
        "src/modules/point/DPoint/Utility/DPSpiralField.cpp",
        "src/core/DDataUtility/Generate/DPointCircleEmit.cpp",
        "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp",
        "src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp",
        "src/modules/generators/icosahedron/icosahedronNode.cpp",
        "src/modules/generators/Generate/Human/DHEyeGen.cpp",
        "src/modules/generators/Generate/Human/DHEyelashGen.cpp",
        "src/modules/generators/Generate/LeafGen/LeafGen.cpp",
        "src/modules/generators/Generate/DBubbleCrystal.cpp",
        "src/modules/generators/Generate/DCurveDuplicator.cpp",
        "src/modules/generators/Generate/motionDuplicator.cpp",
        "src/modules/generators/legacy/surfaceDuplicater.cpp",
        "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp",
        "src/core/UtilityNode/DStretchData/DStretchData.cpp",
    )
    for relative_path in guarded_compute_sources:
        guarded_source = stripped_sources[relative_path]
        compute_position = guarded_source.find("::compute")
        exception_position = guarded_source.find(
            "DOMMY_COMPUTE_CATCH", compute_position
        )
        initialize_match = INITIALIZE_METHOD_PATTERN.search(guarded_source, max(0, compute_position))
        initialize_position = initialize_match.start() if initialize_match else -1
        if exception_position < 0:
            errors.append("{} has no compute exception boundary".format(relative_path))
        elif compute_position < 0 or (
            initialize_position >= 0 and exception_position >= initialize_position
        ):
            errors.append("{} has a misplaced compute exception boundary".format(relative_path))

    curve_duplicator = stripped_sources[
        "src/modules/generators/Generate/DCurveDuplicator.cpp"
    ]
    curve_normalization = curve_duplicator.find("normal /= normalLength")
    curve_exception_boundary = curve_duplicator.find("DOMMY_COMPUTE_CATCH")
    curve_initialize = curve_duplicator.find("DCurveDuplicator::initialize")
    if not (
        0 <= curve_normalization < curve_exception_boundary < curve_initialize
    ):
        errors.append("DCurveDuplicator compute exception boundary is misplaced")

    guarded_command_sources = (
        "src/commands/Cmd/BlindData/DBlindDataSelectCmd.cpp",
        "src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp",
        "src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp",
        "src/commands/shapeFillCmd.cpp",
        "src/commands/setParticleRGBCmd.cpp",
        "src/commands/woolCurveCmd.cpp",
        "src/commands/getCurveLengthPosCmd.cpp",
        "src/commands/surfaceDuplicaterCmd.cpp",
        "src/commands/spaceDuplicaterCmd.cpp",
        "src/modules/generators/icosahedron/icosahedronCmd.cpp",
        "src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp",
        "src/core/DDataUtility/IO/DPointDataExportCmd.cpp",
    )
    for relative_path in guarded_command_sources:
        if "DOMMY_COMMAND_CATCH" not in stripped_sources[relative_path]:
            errors.append("{} has no command exception boundary".format(relative_path))

    particle_path = "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp"
    particle_source = stripped_sources[particle_path]
    unsafe_particle_array = re.compile(
        r"&\s*\(\s*\*\s*(?:thickArray|massArray|radArray)\s*\)\s*\[\s*0\s*\]"
    )
    if unsafe_particle_array.search(particle_source):
        errors.append("DPnParticle passes an unchecked empty vector to the Maya API")

    registration_path = os.path.join(
        REPOSITORY_ROOT, "src/plugin/registration/modules/CoreRegistrations.inl"
    )
    with open(registration_path, "r", errors="replace") as stream:
        core_registrations = strip_comments_and_literals(stream.read())
    vdb_registration = core_registrations.find("DOpenVDBData::typeName")
    vdb_guard = core_registrations.rfind("#if DOMMY_WITH_OPENVDB", 0, vdb_registration)
    vdb_guard_end = core_registrations.find("#endif", vdb_registration)
    if vdb_registration < 0 or vdb_guard < 0 or vdb_guard_end < 0:
        errors.append("DOpenVDBData registration is not guarded by DOMMY_WITH_OPENVDB")

    viewport_sources = (
        "src/modules/curve/DCurve/Display/DCurveDisplay.cpp",
        "src/modules/curve/DCurve/Display/DCMarker.cpp",
        "src/modules/point/DPoint/Display/DPMarker.cpp",
        "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp",
        "src/modules/mesh/DMesh/Display/DMMarker.cpp",
    )
    flat_point_pattern = re.compile(r"&\s*\(?\s*\*?\w+\s*\)?\s*\[\s*0\s*\]\s*\[\s*0\s*\]")
    for relative_path in viewport_sources:
        source = stripped_sources[relative_path]
        if "acquireFloatBuffer" not in source:
            errors.append("{} bypasses checked VP2 vertex allocation".format(relative_path))
        if flat_point_pattern.search(source):
            errors.append("{} assumes a flat or non-empty point container".format(relative_path))

    display_path = "src/modules/point/DPoint/Display/DPointDisplay.cpp"
    display = stripped_sources[display_path]
    compute_start = display.find("DPointDisplay::compute")
    draw_start = display.find("DPointDisplay::draw", compute_start)
    if compute_start < 0 or draw_start < 0:
        errors.append("DPointDisplay compute boundary could not be located")
    elif re.search(r"\bgl[A-Z]\w*\s*\(", display[compute_start:draw_start]):
        errors.append("DPointDisplay compute performs OpenGL work")

    for relative_path, stripped in stripped_sources.items():
        is_vdb_compute = (
            relative_path.startswith("src/modules/volume/VDB/") or
            "openvdb::" in stripped
        )
        if "::compute" in stripped and is_vdb_compute:
            if "DOMMY_VDB_COMPUTE_CATCH" not in stripped:
                errors.append("{} has no OpenVDB compute exception boundary".format(relative_path))

    checked_array_output_sources = (
        "src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp",
        "src/modules/generators/Generate/motionDuplicator.cpp",
        "src/modules/generators/Generate/Human/DHEyelashGen.cpp",
        "src/modules/curve/DCurve/Convert/DCToCurves.cpp",
        "src/modules/deformers/transform/DTDelayTransformMatrix.cpp",
    )
    unchecked_add_element = re.compile(
        r"\baddElement\s*\(\s*[^,\)]+\s*\)"
    )
    for relative_path in checked_array_output_sources:
        source = stripped_sources.get(relative_path, "")
        match = unchecked_add_element.search(source)
        if match:
            errors.append(
                "{}:{} adds an output-array element without MStatus".format(
                    relative_path, line_number(source, match.start())
                )
            )

    required_motion_safety = {
        "src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp": (
            "buildMotionTrailBudget",
            "motionTrailStep",
            "sampleMatrixPlugSeries",
            "setObjectArrayOutput",
        ),
        "src/modules/generators/Generate/motionDuplicator.cpp": (
            "buildMotionDuplicationBudget",
            "sampleScalarPlugSeries",
            "sampleMatrixPlugSeries",
            "setObjectArrayOutput",
        ),
        "src/modules/generators/legacy/surfaceDuplicater.cpp": (
            "connectedSourceNode",
            "validTextureSamples",
            "isFiniteMatrix",
            "DOMMY_RETURN_IF_FAILURE(animMatPlug.getValue",
        ),
    }
    for relative_path, tokens in required_motion_safety.items():
        source = stripped_sources.get(relative_path, "")
        for token in tokens:
            if token not in source:
                errors.append(
                    "{} is missing required motion-safety boundary {}".format(
                        relative_path, token
                    )
                )

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("Active C++ evaluation, RNG, and Maya API boundary check: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
