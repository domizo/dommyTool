#!/usr/bin/env python3
"""Real generic/math computes and duplicator admission, not Maya geometry emulation."""
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
MATH = 'src/modules/point/DPoint/math/'
NODES = ('DPNegative', 'DPAdd', 'DPRound', 'DPMin', 'DPDivide', 'DPExponential',
         'DPAbsolute', 'DPMathFunc', 'DPModulo', 'DPSubtract', 'DPSqrt', 'DPCeil',
         'DPMax', 'DPFloor', 'DPMath', 'DPPow', 'DPMultiply')
PATHS = {name: MATH + name + '.cpp' for name in NODES}
PATHS.update(DPNull='src/modules/point/DPoint/Utility/DPNull.cpp',
             DPNormalize=MATH + 'vector/DPNormalize.cpp',
             DPFitRange=MATH + 'other/DPFitRange.cpp',
             DPRemap=MATH + 'other/DPRemap.cpp',
             DPUVGen='src/modules/point/DPoint/nurbs/DPUVGen.cpp',
             DPMeshDuplicator='src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp')


def body(overrides=None, sdk=False):
    overrides = overrides or {}
    result = []
    # Use the real legacy storage aliases rather than evaluation-only doubles.
    aliases = (ROOT / 'src/modules/point/DPoint/DType.h').read_text()
    for name in ('DScalarData', 'DScalarDataList', 'DVec2Data', 'DVec2DataList'):
        result.append(re.search(r'^typedef[^\n]+\b' + name + r';$', aliases, re.M).group(0))
    if not sdk:
        result.append(extract(aliases, 'inline MPoint DPToMP('))
    types = (ROOT / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text()
    result.extend(extract(types, 'bool ' + name + '(const DP_ATTR_DATA_TYPE')
                  for name in ('isSingleRealAttrType', 'isVector2DAttrType', 'isVector3DAttrType'))
    for path, name in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp', 'DP'),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'DC'),
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 'DM')):
        result.append(extract((ROOT / path).read_text(), 'MStatus getValidInput' + name + '('))
    point_source = (ROOT / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text()
    result.append(extract(point_source, 'MStatus getOptionalInputDP('))
    for path, name in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp', 'checkDPAttribute'),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'checkDCurvePointAttribute'),
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 'checkDMeshPointAttribute')):
        result.append(extract((ROOT / path).read_text(), 'void ' + name + '('))
    if not sdk:
        output = (ROOT / 'src/core/common/utility/DMayaPluginDataOutput.h').read_text()
        result.append('namespace dommy {namespace maya {')
        for signature in ('MStatus publishPluginDataObject(',
                          'MStatus publishTypedPluginDataObject(', 'MStatus setPluginDataOutput('):
            start = output.rfind('template <', 0, output.index(signature))
            result.append(output[start:output.index(signature)] + extract(output, signature))
        result.append('}}')
        unary = (ROOT / (MATH + 'DMayaUnaryMath.h')).read_text()
        start = unary.index('template <typename Owner, typename Values>')
        result.append(unary[start:unary.index('MStatus DReadUnaryMathAttribute(')] +
                      extract(unary, 'MStatus DReadUnaryMathAttribute('))
    settings_path = MATH + 'DMayaMathSettings.h'
    if not sdk and (ROOT / settings_path).exists():
        result.append('#define DOMMY_MATH_SETTINGS_AVAILABLE')
        settings = overrides.get(settings_path, (ROOT / settings_path).read_text())
        result.append(extract(settings, 'namespace dommy {'))
    math_path = MATH + 'DPMathUtility.cpp'
    math = overrides.get(math_path, (ROOT / math_path).read_text())
    # Execute both complete output handlers, not the entry-node processing seam.
    # Operand/math processing below these handlers remains an explicit boundary.
    result.append('namespace shared_math_body {')
    for signature in ('bool isValidMathOperation(', 'bool isValidSingleMathOperation(',
                      'MStatus publishMathDataObject(', 'MStatus processDDataMathOutputSingle(',
                      'MStatus processDDataMathOutput('):
        result.append(extract(math, signature))
    result.append('}')
    transform = (ROOT / 'src/core/common/object/DTransformOp.h').read_text()
    result.append(re.search(r'enum\s*\{[^}]+\};typedef short MatrixAnimationType;', transform).group(0))
    result.append(extract(transform, 'struct DTransformParams') + ';')
    for name, path in PATHS.items():
        source = overrides.get(path, (ROOT / path).read_text())
        attrs = declarations(name, source, False)
        attrs[0] = attrs[0].replace('MStatus compute(const MPlug&, MDataBlock&);',
            'MStatus compute(const MPlug&, MDataBlock&);'
            + ('MString name()const;MObject thisMObject()const;' if sdk else
               'MString name()const{return "' + name + '";}'
               'MObject thisMObject()const{return {"node"};}') +
            ('MStatus evalAnimMatrixParms(MDataBlock&,DTransformParams&);'
             if name == 'DPMeshDuplicator' else ''))
        if sdk:
            attrs[1:] = [re.sub(r'\{"[^\"]+"\}', '{}', value) for value in attrs[1:]]
        result.extend(attrs)
        if name in ('DPFitRange', 'DPRemap', 'DPUVGen'):
            result.append(extract(source, 'namespace {'))
        if name == 'DPMeshDuplicator':
            result.append(extract(source, 'MStatus DPMeshDuplicator::evalAnimMatrixParms('))
            # The complete admission prefix is executed, ending before output
            # construction/expression/geometry. The geometry loop and batch
            # commit are covered by check_mesh_duplication_vertices.py.
            compute = extract(source, 'MStatus DPMeshDuplicator::compute(')
            assert compute.count('#pragma mark outDMの作成') == 1
            result.append(compute.split('#pragma mark outDMの作成')[0] +
                          ('(void)objNumber;(void)seed;(void)aIsOutUVVal;' if sdk else '') +
                          'return MS::kSuccess;}}return MS::kUnknownParameter;}')
        else:
            result.append(extract(source, 'MStatus ' + name + '::compute('))
        result.append('DOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def check(overrides=None, arguments=(), capture=False, budget=False):
    headers = None
    if budget:
        path = 'common/utility/DAttributeArrayConversion.h'
        headers = {path: (ROOT/'src/core'/path).read_text().replace(
            'maximum = 256U * 1024U * 1024U', 'maximum = 8192U')}
    return run(ROOT, body(overrides), suite='procedural_api', arguments=list(arguments),
               capture=capture, header_overrides=headers,
               extra_flags=('-I'+str(ROOT/'src/core/common/utility'),))


def mutations():
    cases = (
        ('null output status ignored', PATHS['DPNull'],
         'DOMMY_RETURN_IF_FAILURE(stat);\n    return data.setClean(plug);',
         '(void)stat;\n    return data.setClean(plug);'),
        ('normalize output handle status ignored', PATHS['DPNormalize'],
         'if (!stat) return stat;\n\t\t\t\t\tDOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(',
         '\t\t\t\t\tDOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject('),
        ('duplicator staged reads bypassed', PATHS['DPMeshDuplicator'],
         'DOMMY_RETURN_IF_FAILURE(status);\n    candidate.mAnimMatrixType',
         'candidate.mAnimMatrixType'),
        ('duplicator commits partial parameters', PATHS['DPMeshDuplicator'],
         'DTransformParams candidate = parms;', 'DTransformParams& candidate = parms;'),
        ('duplicator array count ignored', PATHS['DPMeshDuplicator'],
         'const unsigned int count = arrayHandle.elementCount(&stat);\n            if (!stat) return stat;',
         'const unsigned int count = arrayHandle.elementCount(&stat);'),
        ('math settings status ignored', MATH + 'DMayaMathSettings.h',
         'if (!status) return status;', '(void)status;'),
        ('math settings commit partial state', MATH + 'DMayaMathSettings.h',
         'MathNodeSettings candidate;', 'MathNodeSettings& candidate = settings;'),
        ('math operation status ignored', MATH + 'DMayaMathSettings.h',
         'if (!status) return status;\n    operation = handle.asShort();',
         'operation = handle.asShort();'),
    )
    for label, path, old, new in cases:
        source = (ROOT / path).read_text()
        assert old in source, label
        changed = source.replace(old, new)
        if label == 'duplicator commits partial parameters':
            changed = changed.replace('parms = std::move(candidate);', '(void)candidate;')
        if label == 'math settings commit partial state':
            changed = changed.replace('settings = std::move(candidate);', '(void)candidate;')
        try:
            check({path: changed}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'procedural_api_tests', 'Compile failure: ' + label
            assert any(marker in error.stderr for marker in
                       ('Assertion failed', 'Assertion ', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def range_mutations():
    for name in ('DPFitRange', 'DPRemap'):
        path = PATHS[name]; source = (ROOT / path).read_text()
        changes = [('unsupported payload misclassified',
                    'return MS::kInvalidParameter;\n}\nDOMMY_COMPUTE_CATCH("' + name + '")',
                    'return MS::kUnknownParameter;\n}\nDOMMY_COMPUTE_CATCH("' + name + '")')]
        for kind in ('DP', 'DC', 'DM'):
            changes.append((kind + ' input status ignored',
                            'DOMMY_RETURN_IF_FAILURE(getValidInput' + kind + '(inDD, data, input' + kind + 'Data));',
                            '(void)getValidInput' + kind + '(inDD, data, input' + kind + 'Data);'))
        for label, old, new in changes:
            assert source.count(old) == 1, name + ': ' + label
            try:
                check({path: source.replace(old, new, 1)}, arguments=('ranges',), capture=True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'procedural_api_tests', 'Compile failure: ' + label
                assert 'Assertion' in error.stderr, name + ': ' + label
                print('Detected range mutation: ' + name + ': ' + label, flush=True)
            else:
                raise AssertionError('Undetected range mutation: ' + name + ': ' + label)


def input_body_mutations():
    uv_path = PATHS['DPUVGen']; uv = (ROOT / uv_path).read_text()
    cases = []
    for kind in ('DP', 'DC', 'DM'):
        old = 'DOMMY_RETURN_IF_FAILURE(getValidInput' + kind + '(inDD, data, input' + kind + 'Data));'
        cases.append(('UV ' + kind + ' input status ignored', uv_path, uv.replace(old, old[len('DOMMY_RETURN_IF_FAILURE('):-2] + ';', 1), 'uv'))
        assert uv.count(old) == 1
    old = 'return MS::kInvalidParameter;\n}\nDOMMY_COMPUTE_CATCH("DPUVGen")'
    assert uv.count(old) == 1
    cases.append(('UV unsupported input misclassified', uv_path,
                  uv.replace(old, 'return MS::kUnknownParameter;\n}\nDOMMY_COMPUTE_CATCH("DPUVGen")', 1), 'uv'))
    for kind in ('DCurveData', 'DMeshData'):
        match = re.search(r'"Failed to create a new ' + kind + r' object"\);\s*return stat;', uv)
        assert match, kind
        old = match.group(0)
        cases.append(('UV ' + kind + ' factory status hidden', uv_path,
                      uv.replace(old, old.replace('return stat;', 'return MS::kFailure;'), 1), 'uv'))
    helper = extract(uv, 'MStatus generateCurveUvs(')
    match = re.search(r'valueLists\[listIndex\]\[pointIndex\]\s*=\s*normalizedParameter;', helper)
    assert match
    old = match.group(0)
    assert helper.count(old) == 1
    cases.append(('UV curve normalization lost', uv_path,
                  uv.replace(helper, helper.replace(old, 'valueLists[listIndex][pointIndex] = static_cast<float>(parameter);', 1), 1), 'uv'))
    math_path = MATH + 'DPMathUtility.cpp'; math = (ROOT / math_path).read_text()
    for name in ('processDDataMathOutputSingle', 'processDDataMathOutput'):
        handler = extract(math, 'MStatus ' + name + '(')
        old = ('const DPData* inputData = nullptr;\n'
               '        status = getValidInputDP(inputDataAttribute, data, inputData);\n'
               '        if (!status) return status;\n'
               '        if (!inputData) return MS::kFailure;')
        assert handler.count(old) == 1
        for label, new in (
                ('legacy getter status loss', 'const DPData* inputData = getInputDP(inputDataAttribute, data);\n        if (!inputData) return MS::kInvalidParameter;'),
                ('checked getter status ignored', old.replace('if (!status) return status;', '(void)status;'))):
            cases.append((name + ': ' + label, math_path,
                          math.replace(handler, handler.replace(old, new, 1), 1), 'shared'))
        old = 'if (!outputData->hasValidPointTopology()) return MS::kFailure;'
        assert handler.count(old) == 1
        cases.append((name + ': final topology ignored', math_path,
                      math.replace(handler, handler.replace(old, '(void)outputData;', 1), 1), 'shared'))
    for label, path, modified, section in cases:
        try:
            check({path: modified}, arguments=(section,), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'procedural_api_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr, label
            print('Detected input-body mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected input-body mutation: ' + label)


def sdk_boundary(sdk):
    from point_input_source import optional_point_sdk_body
    boundaries = (
        ('23 entry/admission bodies, complete UV helpers, two shared math output handlers and optional DPoint reader',
         '#include "procedural_api_sdk.h"\n' + body(sdk=True), 'payload/operand boundaries'),
        ('Five optional-DPoint dynamics consumers and shared readers',
         optional_point_sdk_body(ROOT), 'custom payload storage/Imath dependency'),
    )
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src'),
        '-I' + str(ROOT / 'src/core'), '-I' + str(ROOT / 'tests/cpp'),
        '-I' + str(ROOT / 'src/modules/point/DPoint'),
        '-idirafter', str(ROOT / 'tests/stubs'), '-x', 'c++', '-']
    for label, code, seam in boundaries:
        for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
            subprocess.run(command + flags, input=code, text=True, check=True)
        print(label + ': Maya API syntax/analysis passed; ' + seam + ' declared, not linked')


def uv_mutations():
    path = PATHS['DPUVGen']; original = (ROOT/path).read_text()
    cases = []
    old = 'partition.toWorld,'
    assert old in original
    cases.append(('query transform omitted',original.replace(old,'MMatrix(),',1),False))
    old = '(*cIt)->getTransformMatrix()'
    positions = list(re.finditer(re.escape(old),original));assert len(positions)==4
    for index,match in enumerate(positions):
        cases.append(('partition transform omitted '+str(index),
                      original[:match.start()]+'MMatrix()'+original[match.end():],False))
    old = 'size_t pointCount = outputDPData->numberOfPoints();'
    assert old in original
    cases.append(('empty point UV field removed',original.replace(old,old+
                  '\nif(pointCount==0U) outputDPData->removeAttr(uvAttrName);',1),False))
    helper = extract(original,'MStatus publishUvAttribute(')
    changed = re.sub(r'return dommy::attribute::commitStagedAttribute\((.*?)\)\s*\? MStatus\(MS::kSuccess\)\s*:\s*MStatus\(MS::kFailure\);',
                     r'(void)dommy::attribute::commitStagedAttribute(\1);return MS::kSuccess;',helper,flags=re.S)
    assert changed!=helper
    cases.append(('UV publication failure ignored',original.replace(helper,changed,1),False))
    old = 'dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);'
    assert old in original
    cases.append(('point metadata preflight omitted',original.replace(old,'',1),True))
    for kind in ('DC','DM'):
        pattern = r'DOMMY_RETURN_IF_FAILURE\(\s*preflightUvPrimitives\(\s*\*input'+kind+r'Data,[\s\S]*?\)\);'
        match=re.search(pattern,original);assert match,kind
        cases.append((kind+' metadata preflight omitted',original[:match.start()]+original[match.end():],True))
    for method,function in (('FaceAttr','preflightAttributePayload'),('PVtxAttr','preflightAttributePayload'),
                            ('FaceGrp','preflightGroupPayload'),('EdgeGrp','preflightGroupPayload')):
        pattern=r'dommy::point::'+function+r'\(\s*\(\*mesh\)->begin'+method+r'\(\),[\s\S]*?;'
        match=re.search(pattern,original);assert match,method
        cases.append((method+' payload omitted',original[:match.start()]+original[match.end():],True))
    for label,changed,budget in cases:
        try:
            check({path:changed},arguments=('uv-budget' if budget else 'uv',),capture=True,budget=budget)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name=='procedural_api_tests','Compile failure: '+label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr,label
            print('Detected UV mutation:',label,flush=True)
        else:
            raise AssertionError('Undetected UV mutation: '+label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        sdk_boundary(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_motion_duplicator_safety import sdk_check
        sdk_check(ROOT, sys.argv[2], PATHS['DPNull'], 'DPNull')
    elif sys.argv[1:] == ['--mutations']:
        mutations()
        range_mutations()
        input_body_mutations()
    elif sys.argv[1:] == ['--range-mutations']:
        range_mutations()
    elif sys.argv[1:] == ['--input-body-mutations']:
        input_body_mutations()
    else:
        check(arguments=sys.argv[1:])
        if not sys.argv[1:]:
            check(arguments=('uv-budget',), budget=True)
            check(arguments=('duplicator-budget',), budget=True)
            uv_mutations()
            range_mutations()
            input_body_mutations()
