#!/usr/bin/env python3
"""Test real frame algebra and source-extracted transform status transactions.

The transform doubles verify control flow, not Maya decomposition or TBB races.
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


def generated_transform(root, source=None):
    header = (root / 'src/core/common/object/DTransformOp.h').read_text()
    source = source or (root / 'src/core/common/object/DTransformOp.cpp').read_text()
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    return '\n'.join([
        'namespace dommy { namespace attribute {',
        extract((root / 'src/core/common/utility/DAttributeArrayConversion.h').read_text(),
                'inline bool validArrayConversionBudget('),
        '} namespace point {',
        extract((root / 'src/core/common/utility/DPointPayloadBudget.h').read_text(),
                'class PointPayloadBudget') + ';', '} }',
        'namespace dommy { namespace math {',
        re.search(r'const double kDegreesToRadians = [^;]+;', math).group(),
        extract(math, 'double degreesToRadians('), '} }',
        header[header.index('enum {'):header.index('inline void DMatrixDivide')],
        source[source.index('namespace {'):source.index('MStatus DGenerateTransformMatrixFromDP(')],
        extract(source, 'MStatus DGenerateTransformMatrixFromDP(')])


def run(root, transform, arguments, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-frame-transform-') as directory:
        temp = Path(directory)
        (temp / 'transform_methods.inc').write_text(transform)
        (temp / 'frame_adapter.inc').write_text(extract(
            (root / 'src/core/common/object/DNodeOperation.cpp').read_text(),
            'MStatus getAlignSpaceMatrix('))
        results = []
        for name in ('frame_alignment', 'transform_status'):
            binary = temp / name
            subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
                '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-Wno-unused-function', '-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
                '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
                str(root / ('tests/cpp/' + name + '_tests.cpp')), '-o', str(binary)], check=True)
            environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                               ('0' if sys.platform == 'darwin' else '1'))
            results.append(subprocess.run([str(binary)] + (arguments if name == 'transform_status' else []),
                                          check=True, env=environment, capture_output=capture, text=True))
        return results


def main():
    root = Path(__file__).resolve().parent.parent
    methods = generated_transform(root)
    run(root, methods,
        [str(root / 'tests/fixtures/transform_result_contract.txt')])
    reduced = methods.replace('maximum = 256U * 1024U * 1024U', 'maximum = 16384U')
    run(root, reduced, ['--budget'])
    if '--mutations' in sys.argv:
        for label, old, new in (
            ('transform aggregate admission omitted', 'workspace.add(count,',
             'if(false) workspace.add(count,'),
            ('previous transform result omitted',
             'if (count != 0U) workspace.add(matrixArray.capacity(), sizeof(MMatrix));', ''),
        ):
            assert old in reduced, label
            try:
                run(root, reduced.replace(old, new, 1), ['--budget'], True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'transform_status', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()
