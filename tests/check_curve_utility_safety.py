#!/usr/bin/env python3
"""Execute complete curve utility compute bodies with failure-injectable hosts.

Maya rebuild, ramps, custom-data storage and geometry are boundaries, not a Maya
emulator. Shared checked array, interpolation and polyline math are production.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run as run_typed
from check_path_reservation_exception_safety import disable_core_dumps

FOLDER = 'src/modules/curve/DCurve/Utility/'
NAMES = ('DCReform', 'DCResample', 'DCCharactor')


def bodies(root, overrides=None):
    overrides = overrides or {}
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                      'class DCurveOutputBatch final') + ';',
              extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC(')]
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'void postConstructor();'
                      'MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}'
                      'MString name() const {return "' + name + '";}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DCCharactor':
            result.append(extract(source, 'void DCCharactor::postConstructor('))
        # Attribute-declaration doubles and runtime ramp doubles are distinct.
        compute = extract(source, 'MStatus ' + name + '::compute(')
        compute = compute.replace('MRampAttribute widthRampAttr', 'TestValueRamp widthRampAttr')
        result.append(compute + '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def typed_character(root, source=None):
    source = source or (root / (FOLDER + 'DCCharactor.cpp')).read_text()
    common = (root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text()
    from check_point_utility_safety import declarations
    result = [extract(common, 'void AddAttributeList('),
              extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC(')]
    declared = declarations('DCCharactor', source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};} MStatus compute(')
    result += declared
    result.append(extract(source, 'MStatus DCCharactor::compute(').replace(
        'MRampAttribute widthRampAttr', 'TestValueRamp widthRampAttr') +
        '\nDOMMY_COMPUTE_CATCH("DCCharactor")')
    return '\n\n'.join(result)


def run(root, source, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-curve-utility-') as directory:
        temp = Path(directory)
        (temp / 'curve_utility.inc').write_text(source)
        binary = temp / 'curve_utility_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp),
            '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/curve_utility_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary), str(root / 'tests/fixtures/curve_utility_initialization_contract.txt')],
                              check=True, env=environment, capture_output=capture, text=True,
                              preexec_fn=disable_core_dumps if os.name == 'posix' else None)


def attribute_mutations(root):
    source = (root / (FOLDER + 'DCCharactor.cpp')).read_text()
    commit = ('if (!dommy::attribute::commitStagedAttribute(\n'
              '                            **cIt, widthAttrName, candidate, (*cIt)->numberOfPoints())) return MS::kFailure;')
    cases = (
        ('Point writer deletes Primitive field',
         'const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(widthArray);',
         'checkDCurveAttribute(widthAttrName, *outDCrvs, true);\n'
         'const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(widthArray);'),
        ('Point type conflict ignored', commit, '(*cIt)->addAttrWithValues(widthAttrName, widthArray);'),
        ('same-scope name collision ignored', 'widthAttrName == paramAttrName', 'false'),
    )
    for label, old, new in cases:
        assert old in source, label
        try:
            run_typed(root, typed_character(root, source.replace(old, new, 1)),
                      suite='curve_character_attribute', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_character_attribute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected Character attribute mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected Character mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root))
    run_typed(root, typed_character(root), suite='curve_character_attribute', arguments=[])
    attribute_mutations(root)
