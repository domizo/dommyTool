#!/usr/bin/env python3
"""Complete numeric compute body; context/API doubles, not a Maya DG emulator."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
NODE = 'src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp'


def bodies(source=None):
    source = source or (ROOT / NODE).read_text()
    helper = (ROOT / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    return '\n\n'.join([
        extract(helper, 'namespace dommy {'),
        source[source.index('namespace {'):source.index('const MTypeId')],
        'struct DNumericToFloatArray {static MObject inputData,outDataArray;'
        'MObject thisMObject()const{return MObject("node");}'
        'MStatus compute(const MPlug&,MDataBlock&);};'
        'MObject DNumericToFloatArray::inputData("inputData");'
        'MObject DNumericToFloatArray::outDataArray("outDataArray");',
        extract(source, 'MStatus DNumericToFloatArray::compute('),
        'DOMMY_COMPUTE_CATCH("DNumericToFloatArray")',
    ])


def run(source, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-numeric-to-float-') as directory:
        temp = Path(directory)
        (temp / 'numeric_to_float.inc').write_text(source)
        binary = temp / 'numeric_to_float_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-function', '-Wno-unused-parameter',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(ROOT / 'tests/stubs'),
            '-I' + str(ROOT / 'src/core'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/cpp/numeric_to_float_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True, capture_output=capture, text=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary)], check=True, env=env, capture_output=capture, text=True)


def main():
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(ROOT, sys.argv[sys.argv.index('--sdk') + 1], (NODE,), 'Numeric conversion')
        return
    generated = bodies()
    run(generated)
    if '--mutations' in sys.argv:
        for label, old, new in (
            ('connection failure ignored', 'DOMMY_RETURN_IF_FAILURE(status);\n    if (!connected)', 'if (!connected)'),
            ('scalar context replaced by plug value',
             'return appendFloat(static_cast<double>(value), output);',
             'return appendFloat(activeData->inputs["inputData"].number, output);'),
            ('generic compounds rejected', 'if (generic && genericNumeric)', 'if (generic)'),
            ('non-finite accepted', '!dommy::numeric::checkedFloatCast(value, converted)',
             '(converted=static_cast<float>(value), false)'),
            ('array length status ignored',
             'const unsigned int count = data.length(&status);\n    if (!status) return status;',
             'const unsigned int count = data.length(&status);'),
            ('array budget bypassed',
             'count, components, generation::kMaxGeneratedPointCount, valueCount))',
             'count, components, std::numeric_limits<std::size_t>::max(), valueCount))'),
            ('array cardinality ignored',
             'if (array.length() != count) return MS::kFailure;', ''),
            ('matrix array components undercounted',
             'inputHandle, 16U, output, appendMatrix', 'inputHandle, 1U, output, appendMatrix'),
        ):
            assert old in generated, label
            try:
                run(generated.replace(old, new, 1), True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'numeric_to_float_tests', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()
