#!/usr/bin/env python3
"""Execute complete attribute-create and constraint computes, plus curve reads.

Payload owners and Maya are failure-injectable seams. Real typed attributes,
numeric conversion/publication helpers and the complete production bodies run.
This is not native DG, message evaluation, curve geometry or Bullet simulation.
"""
from pathlib import Path
import re
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
ATTRIBUTE = 'src/core/DDataUtility/Attribute/DPAttributeCreate.cpp'
CURVE = 'src/core/common/object/DNodeOperation.cpp'
CONSTRAINTS = ('DBLinkConstraint', 'DBPointConstraint', 'BTFixConstraint',
               'BTP2PConstraint', 'BTConeTwistConstraint', 'BT6DofConstraint')


def bodies(overrides=None, sdk=False):
    overrides = overrides or {}
    def read(path):
        return overrides.get(path, (ROOT / path).read_text())
    result = []
    types = read('src/modules/point/DPoint/DType.h')
    for alias in ('DA_CREATE_TYPE', 'DP_ATTRIBUTE_CLASS_TYPE'):
        result.append(re.search(r'enum\s*\{[^}]+\};\s*typedef short ' + alias + ';', types).group(0))
    result.append(re.search(r'enum\s*\{[^}]+\};\s*typedef short DP_ATTR_DIM_TYPE;', types).group(0))
    helper = ROOT / 'src/core/common/utility/DAttributeCreateNumeric.h'
    if helper.exists() and not sdk:
        result.append(extract(read('src/core/common/utility/DAttributeCreateNumeric.h'), 'namespace dommy {'))
    numeric = read('src/core/common/utility/DMayaNumericInput.h')
    if not sdk:
        result.append('namespace dommy {namespace maya {' +
                      extract(numeric, 'inline MStatus readNumericScalar(') + '}}')
    source = read(ATTRIBUTE)
    result.append(extract(read('src/core/common/utility/DMayaAttributeCreatePlan.h'), 'namespace dommy {'))
    for path, name in (
        ('src/modules/point/DPoint/Common/DPointCommon.cpp', 'getValidInputDP'),
        ('src/modules/curve/DCurve/DCurveUtil.cpp', 'getValidInputDC'),
        ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 'getValidInputDM')):
        result.append(extract(read(path), 'MStatus ' + name + '('))
    attrs = declarations('DPAttributeCreate', source, False)
    attrs[0] = attrs[0].replace('MStatus compute(const MPlug&, MDataBlock&);',
                              'MStatus compute(const MPlug&, MDataBlock&);'
                              + ('MObject thisMObject()const; MString name()const;' if sdk else
                                 'MObject thisMObject()const{return {"node"};}'
                                 'MString name()const{return "DPAttributeCreate";}'))
    result.extend(attrs)
    result.append(extract(source, 'MStatus DPAttributeCreate::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DPAttributeCreate")')
    result.append(extract(read(CURVE), 'MStatus getMayaNurbsCurve('))
    for name in CONSTRAINTS:
        path = 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/' + name + '.cpp'
        source = read(path)
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    array_path = 'src/core/DDataUtility/Attribute/Convert/DPAttributeToDataArray.cpp'
    source = read(array_path)
    result.extend(declarations('DPAttributeToDataArray', source, False))
    result.append(extract(source, 'namespace {'))
    output = read('src/core/common/object/DNodeOperation.cpp')
    result.append('template <typename FunctionSet, typename Array>\n' +
                  extract(output, 'MStatus outputMayaArray('))
    for kind in ('Int', 'Float', 'Double', 'Vector'):
        result.append(extract(output, 'MStatus DOutputDataMaya' + kind + 'Array('))
    result.append(extract(source, 'MStatus DPAttributeToDataArray::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DPAttributeToDataArray")')
    expression = read('src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp')
    result.extend(declarations('DPAttributeSeExpr', expression, False))
    result.append(extract(expression, 'struct SeExprParameters') + ';')
    result.append(extract(expression, 'bool hasFiniteParameters('))
    # Expression arithmetic remains an explicit seam: this suite checks
    # the complete typed-input routing, not SeExpr language execution.
    if not sdk:
        result.append('template <typename Owner> MStatus evaluateAttribute(Owner&, '
                      'const std::string&, int, const MTime&, const SeExprParameters&, '
                      'DPData* = nullptr) { return operation("expression.evaluate"); }')
    result.append(extract(expression, 'MStatus DPAttributeSeExpr::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DPAttributeSeExpr")')
    if not sdk:
        result.append('struct DGroupParams {};')
        for name, path in (
            ('DPGroup', 'src/core/DDataUtility/Group/DPGroup.cpp'),
            ('DCGroup', 'src/modules/curve/DCurve/Group/DCGroup.cpp'),
            ('DMGroup', 'src/modules/mesh/DMesh/Data/DMGroup.cpp')):
            source = read(path)
            decl = declarations(name, source, False)
            decl[0] = decl[0].replace('MStatus compute(const MPlug&, MDataBlock&);',
                'MStatus compute(const MPlug&, MDataBlock&);'
                'MStatus evalParms(MDataBlock&, DGroupParams&) { return operation("group.parameters"); }')
            result.extend(decl)
            body = extract(source, 'MStatus ' + name + '::compute(')
            stop = '    MTime currentTime;' if name == 'DPGroup' else '    MStatus status ='
            assert stop in body
            result.append(body[:body.index(stop)] + '    return MS::kSuccess;\n}\n' +
                          'DOMMY_COMPUTE_CATCH("' + name + '")')
    # Execute the exact mandatory read boundary. Geometry/expression branches
    # of these historical generators are not emulated by this suite.
    for function, path, suffix, attr in (
        ('readPointCurveInput', 'src/modules/curve/DCurve/Generate/DCurveFromDP.cpp', 'DP', 'inDP'),
        ('readWireCurveInput', 'src/modules/mesh/DMesh/Generate/DMWireGen.cpp', 'DC', 'inDC'),
        ('readDuplicatorPoints', 'src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp', 'DP', 'inDP'),
        ('readDuplicatorMesh', 'src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp', 'DM', 'inDM'),
        ('readVoronoiPoints', 'src/modules/point/DPoint/mesh/DPVoronoi.cpp', 'DP', 'inDP')):
        source = read(path)
        pattern = (r'const (?:DPData|DCurveData|DMeshData)\* \w+ = nullptr;\s*'
                   r'DOMMY_RETURN_IF_FAILURE\(getValidInput' + suffix +
                   r'\(' + attr + r', data, \w+\)\);')
        matches = re.findall(pattern, source)
        if len(matches) != 1:
            raise RuntimeError('Incomplete required-input boundary: ' + function)
        block = matches[0]
        if function == 'readDuplicatorMesh':
            query = extract(source, 'MStatus DPMeshDuplicator::compute(')
            start = query.index('        MPlug indmPlug(')
            stop = query.index('        if (hasMeshInput) {', start)
            block = query[start:stop].replace('thisMObject()', 'MObject{}') + 'if (hasMeshInput) {' + block + '}'
        result.append('MStatus ' + function + '(MDataBlock& data) try {'
                      'MStatus stat; const MObject ' + attr +
                      ('{};' if sdk else '{"' + attr + '"};') + block +
                      'return MS::kSuccess;} DOMMY_COMPUTE_CATCH("' + function + '")')
    code = '\n\n'.join(result)
    return re.sub(r'\{"[^\"]+"\}', '{}', code) if sdk else code


def check(overrides=None, arguments=(), capture=False, small_budget=False):
    headers = None
    flags = ()
    if small_budget:
        path = 'common/utility/DAttributeArrayConversion.h'
        source = (ROOT / 'src/core' / path).read_text()
        assert '256U * 1024U * 1024U' in source
        headers = {path: source.replace('256U * 1024U * 1024U', '4096U')}
        flags = ('-DDOMMY_ATTRIBUTE_CREATE_SMALL_BUDGET',
                 '-I' + str(ROOT / 'src/core/common/utility'))
    return run(ROOT, bodies(overrides), suite='attribute_curve_constraint',
               arguments=list(arguments), capture=capture, header_overrides=headers, extra_flags=flags)


def sdk_boundary(sdk):
    code = '#include "attribute_curve_constraint_sdk.h"\n' + bodies(sdk=True)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-variable', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src/core'),
        '-I' + str(ROOT / 'tests/cpp'), '-idirafter', str(ROOT / 'tests/stubs'),
        '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Entry/read bodies, five generator-input boundaries, array publisher and creation preflight: native Maya syntax/analysis passed')


def admission_mutations():
    planner = 'src/core/common/utility/DMayaAttributeCreatePlan.h'
    state = 'common/utility/DAttributeCreatePlan.h'
    cases = (
        ('creation planning omitted before copy', ATTRIBUTE,
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightAttributeCreateRows(\n'
         '            data, thisMObject(), controls, false, creationPlan));',
         '(void)controls;'),
        ('retained point payload omitted', ATTRIBUTE,
         'dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), retained);', ''),
        ('repeated string bytes omitted', planner,
         'generation::checkedProduct(points, repeatedStrings,',
         'generation::checkedProduct(0U, repeatedStrings,'),
        ('required point status erased', ATTRIBUTE,
         'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));',
         'inputDPData = getInputDP(inDP, data);'),
        ('expression typed binding failure ignored', 'src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input));',
         '(void)dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input);'),
    )
    for label, path, old, new in cases:
        source = (ROOT / path).read_text()
        assert old in source, label
        arguments = ['required'] if path.endswith('DPAttributeSeExpr.cpp') else ['budget']
        try:
            check({path: source.replace(old, new, 1)}, arguments=arguments,
                  small_budget=path != planner and arguments == ['budget'], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'attribute_curve_constraint_tests', 'Compile failure: ' + label
            assert any(marker in error.stderr for marker in ('Assertion failed', 'Assertion ', 'runtime error:')), label
            print('Detected creation/admission mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    # Shadow just this helper; all actual callbacks still run from production.
    source = (ROOT / 'src/core' / state).read_text()
    old = 'target.fields.emplace(name, bytes);'
    assert old in source
    headers = {state: source.replace(old, '(void)bytes;', 1),
               'common/utility/DAttributeArrayConversion.h':
               (ROOT / 'src/core/common/utility/DAttributeArrayConversion.h').read_text().replace(
                   '256U * 1024U * 1024U', '4096U')}
    try:
        run(ROOT, bodies(), suite='attribute_curve_constraint', arguments=['budget'], capture=True,
            header_overrides=headers, extra_flags=('-DDOMMY_ATTRIBUTE_CREATE_SMALL_BUDGET',
                                                  '-I' + str(ROOT / 'src/core/common/utility')))
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'attribute_curve_constraint_tests', 'Compile failure: planned name state'
        assert 'Assertion' in error.stderr
        print('Detected creation/admission mutation: planned Not Add names forgotten', flush=True)
    else:
        raise AssertionError('Undetected planned Not Add name mutation')


def mutations():
    helper = 'src/core/common/utility/DAttributeCreateNumeric.h'
    constraints = 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTFixConstraint.cpp'
    cases = (
        ('Not Add owner policy bypassed', helper,
         'return replaceExisting_ || !owner.isExistAttr(name);',
         '(void)owner; (void)name; (void)replaceExisting_; return true;'),
        ('point creation deletes primitive field', ATTRIBUTE,
         'if (attrClassType == DP_ATTR_CLASS_POINT) {\n                if (!creation.hasPointTargets(outDCrvs',
         'if (attrClassType == DP_ATTR_CLASS_POINT) { outDCrvs->removeAttr(attrName);\n'
         '                if (!creation.hasPointTargets(outDCrvs'),
        ('Not Add shifts flattened array offsets', ATTRIBUTE,
         'for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {',
         'if (!creation.shouldCreate(**cIt, attrName)) continue;\n'
         'for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {'),
        ('scalar conversion bypassed', helper,
         '!convertCreatedNumericValue(source, value)', '!((void)source, value = Value(), true)'),
        ('default conversion bypassed', helper,
         '!convertCreatedNumericValue(defaultSource, defaultValue)', '!((void)defaultSource, defaultValue = Value(), true)'),
        ('array count status ignored', ATTRIBUTE,
         'const unsigned int count = arrayHandle.elementCount(&stat);\n        DOMMY_RETURN_IF_FAILURE(stat);',
         'const unsigned int count = arrayHandle.elementCount(&stat);'),
        ('connection status ignored', ATTRIBUTE,
         'const bool inputConnected = inputDataPlug.isConnected(&stat);\n\t\t\tDOMMY_RETURN_IF_FAILURE(stat);',
         'const bool inputConnected = inputDataPlug.isConnected(&stat);'),
        ('vector conversion failure ignored', ATTRIBUTE,
         'if (!dommy::numeric::checkedVectorArrayCopy(', 'if (false && !dommy::numeric::checkedVectorArrayCopy('),
        ('curve acquisition status ignored', CURVE,
         'MDataHandle crvHnd = data.inputValue(plugObj, &stat);\n    DOMMY_RETURN_IF_FAILURE(stat);',
         'MDataHandle crvHnd = data.inputValue(plugObj, &stat);'),
        ('curve commits null output', CURVE,
         'const MObject candidate = crvHnd.data();', 'const MObject candidate = crvHnd.data();\n    crvData = candidate;'),
        ('message output writes numeric data', constraints,
         'return data.setClean(plug);', 'MDataHandle output = data.outputValue(outConst);\n        output.set(true);\n        return data.setClean(plug);'),
        ('message clean failure ignored', constraints,
         'return data.setClean(plug);', 'data.setClean(plug); return MS::kSuccess;'),
    )
    for label, path, old, new in cases:
        source = (ROOT / path).read_text()
        assert old in source, label
        try:
            check({path: source.replace(old, new)}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'attribute_curve_constraint_tests', 'Compile failure: ' + label
            assert any(marker in error.stderr for marker in ('Assertion failed', 'Assertion ', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        sdk_boundary(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
        admission_mutations()
    else:
        check(arguments=sys.argv[1:])
        check(arguments=['budget'], small_budget=True)
        admission_mutations()
