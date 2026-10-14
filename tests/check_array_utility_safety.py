#!/usr/bin/env python3
"""Execute array readers, legacy timing and utility nodes with host doubles.

Maya data ownership, DG evaluation, color conversion and custom topology
validation are not implemented by these doubles.
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

SOURCES = {
    'DPStringArrayGen': 'src/modules/point/DPoint/generate/data/DPStringArrayGen.cpp',
    'DPArraySize': 'src/modules/point/DPoint/Utility/DPArraySize.cpp',
    'DPVecCompare': 'src/modules/point/DPoint/Utility/DPVecCompare.cpp',
    'DPPointToFloat': 'src/modules/point/DPoint/Utility/DPPointToFloat.cpp',
}
COMMON = 'src/modules/point/DPoint/Common/DPointCommon.cpp'
TIMING = 'src/core/common/object/DNodeOperation.cpp'


def generated_methods(root, overrides=None):
    overrides = overrides or {}
    def source(path):
        return overrides[path] if path in overrides else (root / path).read_text()
    common, timing = source(COMMON), source(TIMING)
    declarations = re.findall(r'^MStatus get(?:Vector|Point|Matrix|Double|Float)ArrayFromData[^;]+;',
                              source('src/modules/point/DPoint/Common/DPointCommon.h'), re.M)
    validation = source('src/core/common/utility/DMayaValueValidation.h')
    bodies = ['namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(') for name in
        ('isFiniteMatrix', 'isFinitePoint', 'isFiniteVector', 'isFloatRepresentableVector')) + '\n} }']
    bodies.append('namespace dommy { namespace attribute {\n' + extract(
        source('src/core/common/utility/DAttributeArrayConversion.h'),
        'inline bool validArrayConversionBudget(') + '\n} }')
    if 'MStatus readArrayData(' in common:
        start = common.index('namespace {') + len('namespace {')
        end = common.index('} // namespace', start)
        bodies.append(common[start:end])
    bodies.extend(declarations)
    for match in re.finditer(r'MStatus get(?:Vector|Point|Matrix|Double|Float)ArrayFromData\(', common):
        bodies.append(extract(common[match.start():], match.group()))
    bodies.append(extract(timing, 'MStatus getTimeOffsetAndStretch('))
    expected = {
        'DPStringArrayGen': ['value', 'valueSize', 'outDataArray'],
        'DPArraySize': ['inputPointArray', 'inDD', 'inObject', 'aCountType', 'resultCount'],
        'DPVecCompare': ['inputArray1', 'inputArray2', 'compareType', 'value', 'valueX', 'valueY', 'valueZ', 'resultArray'],
        'DPPointToFloat': ['inputPointArray', 'convertType', 'outDataArray'],
    }
    for name, path in SOURCES.items():
        text = source(path)
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', text, re.M)
        if attrs != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attr + ';' for attr in attrs)
        bodies.append(extract(text, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(text, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    # Equivalent status constants allow archived methods to use the existing stub.
    return '\n\n'.join(bodies).replace('MStatus::kSuccess', 'MS::kSuccess').replace(
        'MStatus::kUnknownParameter', 'MS::kUnknownParameter')


def run(root, methods, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-array-utilities-') as directory:
        temp = Path(directory)
        (temp / 'array_utility_methods.inc').write_text(methods)
        binary = temp / 'array_utility_safety_tests'
        subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unknown-pragmas',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), '-I' + str(root / 'src'),
            str(root / 'tests/cpp/array_utility_safety_tests.cpp'), '-o', str(binary)], check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        subprocess.run([str(binary), str(root / 'tests/fixtures/array_utility_initialization_contract.txt')],
                       check=True, env=environment, capture_output=capture, text=True)


def main():
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1],
                  (COMMON, TIMING, 'src/core/common/object/DTransformOp.cpp',
                   *SOURCES.values(),
                   'src/modules/point/DPoint/Modify/DPEditDouble.cpp',
                   'src/modules/point/DPoint/Deform/DPConvertWeights.cpp'),
                  'Block array inputs')
        return
    # Optional VDB compilation needs the legacy dependency stack. Its three
    # reader call sites and the weights consumer must at least propagate status.
    for path, readers in {
        'src/modules/volume/VDB/Generate/DOpenVDBFromParticle.cpp': ('Point', 'Double', 'Vector'),
        'src/modules/point/DPoint/Deform/DPConvertWeights.cpp': ('Float',),
        'src/modules/point/DPoint/Modify/DPEditDouble.cpp': ('Float',),
    }.items():
        text = (root / path).read_text()
        for reader in readers:
            if not re.search(r'DOMMY_RETURN_IF_FAILURE\(\s*get' + reader +
                             r'ArrayFromData\(block,', text):
                raise ValueError(path + ' must read the block and propagate ' + reader + ' reader failure')
    methods = generated_methods(root)
    run(root, methods)
    if '--mutations' in sys.argv:
        for label, old, new in (
            ('size connection error ignored', 'DOMMY_RETURN_IF_FAILURE(status);\n    if (customConnected)',
             'if (customConnected)'),
            ('compare connection error ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n        if (secondConnected)',
             'if (secondConnected)'),
            ('block reader rereads plug', 'return read(input.data(), output);',
             '(void)input; return readPlugArrayInput(MObject("node"), attribute, output, read);'),
            ('size object rereads plug', 'const MObject object = input.data();',
             'MObject object; const MStatus readStatus = objectPlug.getValue(object); if(!readStatus) return readStatus;'),
        ):
            assert old in methods, label
            try:
                run(root, methods.replace(old, new, 1), True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'array_utility_safety_tests', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()
