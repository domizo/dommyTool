#!/usr/bin/env python3
"""Run deletion/reader/surface computes and attribute/polyBlast admission.

Maya and OpenVDB are explicit failure-injectable seams. The production data
publisher, name-list budget, sequence parser and selected source bodies execute.
Blend arithmetic and SeExpr execution are not emulated by the admission tests.
Surface intersections are a host seam. polyBlast stops before mesh copying and
surfaceDuplicater before generation admission; their later geometry is not run.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
DELETE = 'src/core/DDataUtility/Attribute/DPAttributeDelete.cpp'
BLEND = 'src/core/DDataUtility/Attribute/DPAttributeBlend.cpp'
EXPR = 'src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp'
READER = 'src/modules/volume/VDB/IO/DReadOpenVDB.cpp'
GROUP_DELETE = 'src/core/DDataUtility/Group/DPGroupDelete.cpp'
SURFACE = 'src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp'
BLAST = 'src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp'
DUPLICATER = 'src/modules/generators/legacy/surfaceDuplicater.cpp'


def bodies(overrides=None, sdk=False):
    overrides = overrides or {}
    def read(path):
        return overrides.get(path, (ROOT / path).read_text())
    result = []
    if not sdk:
        result += ['namespace dommy { namespace maya {']
        publisher = read('src/core/common/utility/DMayaPluginDataOutput.h')
        for signature in ('MStatus createPluginDataObject(', 'MStatus publishPluginDataObject(',
                          'MStatus publishTypedPluginDataObject('):
            result.append('template <typename DataType>\n' + extract(publisher, signature))
        # Select the callback overload, not the mutable-pointer overload.
        start = publisher.index('template <typename DataType, typename Populate>')
        result.append('template <typename DataType, typename Populate>\n' +
                      extract(publisher[start:], 'MStatus createPluginDataObject('))
        result.append('template <typename DataType, typename Populate>\n' +
                      extract(publisher, 'MStatus setPluginDataOutput('))
        result.append('}}')
        result.append(extract(read('src/core/common/utility/DMayaNameList.h'), 'namespace dommy {'))
        result.append(extract(read('src/core/common/file/DFileOperation.cpp'), 'bool getSequenceFileName('))
        validation = read('src/core/common/utility/DMayaValueValidation.h')
        result.append('namespace dommy { namespace maya {\n' + '\n'.join(
            extract(validation, 'inline bool ' + name + '(') for name in (
                'isFinitePoint', 'isFiniteVector', 'isFloatRepresentableVector', 'normalizedVector')) + '\n}}')
        result.append(extract(read('src/modules/point/DPoint/Common/DPointCommon.cpp'),
                              'MStatus getValidInputDP('))
    types = read('src/modules/point/DPoint/DType.h')
    result.append(re.search(r'enum\s*\{[^}]+\};\s*typedef short DP_ATTR_DIM_TYPE;', types).group(0))
    result.append(extract(read(EXPR), 'struct SeExprParameters') + ';')
    for name, path in (('DPAttributeDelete', DELETE), ('DPAttributeBlend', BLEND),
                       ('DPAttributeSeExpr', EXPR), ('DReadOpenVDB', READER),
                       ('DPGroupDelete', GROUP_DELETE), ('DPointToSurfaceTransformNode', SURFACE),
                       ('polyBlastNode', BLAST), ('surfaceDuplicater', DUPLICATER)):
        source = read(path)
        if name == 'polyBlastNode':
            # Inherited attributes are defined in polyModifierNode, not this TU.
            source += '\n' + '\n'.join('MObject polyBlastNode::' + attr + ';'
                                       for attr in ('state', 'inMesh', 'outMesh'))
        node_declarations = declarations(name, source, False)
        result += node_declarations
        body = extract(source, 'MStatus ' + name + '::compute(')
        if name == 'DPAttributeBlend':
            body = body[:body.index('    if (!std::isfinite(defaultWeight)')] + \
                '    (void)defaultWeight; (void)deleteOriginal;\n    return MS::kSuccess;\n}'
        elif name == 'DPAttributeSeExpr':
            body = body[:body.index('    if (!hasFiniteParameters(parameters))')] + \
                '    (void)dimension;\n    return MS::kSuccess;\n}'
        elif name == 'polyBlastNode':
            body = body[:body.index('    MFnMeshData meshDataFunction;')] + \
                '    (void)percent;\n    return MS::kSuccess;\n}'
        elif name == 'surfaceDuplicater':
            prefix = body[:body.index('std::size_t checkedNumU')]
            values = re.findall(r'\b(?:int|double|bool|short)\s+(\w+)\s*=\s*\w+Handle\.as\w+\(\)', prefix)
            assert len(values) == 32
            body = prefix + '\n'.join('(void)' + value + ';' for value in values) + \
                '\nreturn MS::kSuccess;\n}\nreturn MS::kUnknownParameter;\n}' + \
                'catch (...) {return MS::kFailure;}\n}'
        result.append(body)
        if name != 'surfaceDuplicater':
            result.append('DOMMY_COMPUTE_CATCH("' + name + '")')
    code = '\n\n'.join(result)
    return re.sub(r'\{"[^\"]+"\}', '{}', code) if sdk else code


def check(overrides=None, capture=False):
    return run(ROOT, bodies(overrides), suite='attribute_vdb_read_status',
               arguments=[str(ROOT / 'tests/fixtures/curve_path_initialization_contract.txt')], capture=capture)


def sdk_boundary(sdk):
    code = '#include "attribute_vdb_read_status_sdk.h"\n' + bodies(sdk=True)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-variable', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src/core'),
        '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests/cpp'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Native Maya header syntax/analysis: complete attribute/group Delete, VDB reader, surface computes and Blend/SeExpr/polyBlast/surfaceDuplicater admission passed; payload/OpenVDB declarations only')


def mutations():
    check()  # Never count an unrelated failure in the baseline as detection.
    cases = (
        ('deletion ignores name acquisition', DELETE,
         'DOMMY_RETURN_IF_FAILURE(status);\n    const MString names',
         'const MString names'),
        ('deletion ignores split failure', DELETE,
         'DOMMY_RETURN_IF_FAILURE(\n            dommy::maya::splitCheckedNameList(names, 1U, budget, attributes));',
         'dommy::maya::splitCheckedNameList(names, 1U, budget, attributes);'),
        ('blend reads failed source handle', BLEND,
         'CHECK_MSTATUS_AND_RETURN_IT(status);\n    const std::string sourceName = sourceHandle.asString().asChar();',
         'const std::string sourceName = sourceHandle.asString().asChar();\n    CHECK_MSTATUS_AND_RETURN_IT(status);'),
        ('expression reads failed vector handle', EXPR,
         'if (!status)\n            return status;\n        parameters.userVectors[i] = vectorHandle.asVector();',
         'parameters.userVectors[i] = vectorHandle.asVector();\n        if (!status) return status;'),
        ('reader reads failed time handle', READER,
         'if (stat != MS::kSuccess)\n\t\treturn stat;\n\tconst MTime evaluationTime = timeHandle.asTime();',
         'const MTime evaluationTime = timeHandle.asTime();\n\tif (stat != MS::kSuccess) return stat;'),
        ('reader ignores filename acquisition', READER,
         'DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName(filename.c_str()));',
         'fileObject.setRawFullName(filename.c_str());'),
        ('reader publishes before input admission', READER,
         '\tMStatus stat;\n\tconst MDataHandle timeHandle',
         '\tMStatus stat;\n\tDOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DOpenVDBData>(data, aVdbOutput, DOpenVDBData::id, [](DOpenVDBData&) { return MStatus(MS::kSuccess); }));\n\tconst MDataHandle timeHandle'),
        ('group reads failed name handle', GROUP_DELETE,
         'DOMMY_RETURN_IF_FAILURE(status);\n    const MString names',
         'const MString names'),
        ('group reads failed enable handle', GROUP_DELETE,
         'DOMMY_RETURN_IF_FAILURE(status);\n    const bool enabled = enableHandle.asBool();',
         'const bool enabled = enableHandle.asBool();\n    DOMMY_RETURN_IF_FAILURE(status);'),
        ('group ignores split failure', GROUP_DELETE,
         'DOMMY_RETURN_IF_FAILURE(\n            dommy::maya::splitCheckedNameList(names, 1U, budget, groups));',
         'dommy::maya::splitCheckedNameList(names, 1U, budget, groups);'),
        ('surface reads failed offset handle', SURFACE,
         'if (!status)\n        return status;\n    const float offsetValue = offsetHandle.asFloat();',
         'const float offsetValue = offsetHandle.asFloat();\n    if (!status) return status;'),
        ('surface position written before normal acquisition', SURFACE,
         '    MDataHandle normalOutput = block.outputValue(outputNormal, &status);\n    if (!status)\n        return status;\n\n    // Neither output is changed if acquiring either handle fails.\n    positionOutput.set(outputPoint.x, outputPoint.y, outputPoint.z);',
         '    positionOutput.set(outputPoint.x, outputPoint.y, outputPoint.z);\n    MDataHandle normalOutput = block.outputValue(outputNormal, &status);\n    if (!status)\n        return status;'),
        ('polyBlast reads failed percent handle', BLAST,
         'if (!status) return status;\n    const double percent = percentHandle.asDouble();',
         'const double percent = percentHandle.asDouble();\n    if (!status) return status;'),
        ('surfaceDuplicater reads failed scalar handle', DUPLICATER,
         'CHECK_MSTATUS_AND_RETURN_IT(stat);\n\t\tdouble randamUVal = randamUHandle.asDouble();',
         'double randamUVal = randamUHandle.asDouble();\n\t\tCHECK_MSTATUS_AND_RETURN_IT(stat);'),
    )
    for label, path, old, new in cases:
        source = (ROOT / path).read_text()
        assert old in source, label
        try:
            check({path: source.replace(old, new, 1)}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'attribute_vdb_read_status_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        sdk_boundary(sys.argv[2])
        from attribute_blend_compute_source import sdk_boundary as blend_sdk
        blend_sdk(ROOT, sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
        from attribute_blend_compute_source import execute, mutations as blend_mutations
        execute(ROOT)
        blend_mutations(ROOT)
    else:
        assert not sys.argv[1:]
        check()
        from attribute_blend_compute_source import execute, mutations as blend_mutations
        execute(ROOT)
        blend_mutations(ROOT)
