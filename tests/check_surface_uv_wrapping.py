#!/usr/bin/env python3
"""Execute the legacy duplicater's UV admission and sample math, not geometry."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/generators/legacy/surfaceDuplicater.cpp'


def bodies(source=None):
    source = source or (ROOT / SOURCE).read_text()
    start = source.index('double uLength = maxUVal - minUVal;')
    setup = source[start:source.index('int i,j;', start)]
    start = source.index('double preUVal = minUVal+uMidPos')
    sample = source[start:source.index('MPoint uvPos;', start)]
    assert 'while (' not in sample, 'Unbounded UV wrap restored'
    admission = extract(source, 'if (normalSetVal < 0')
    return ('MStatus admitRanges(double minUVal, double maxUVal, double minVVal, double maxVVal) {\n'
            'const int normalSetVal=0, scaleColorXVal=0, scaleColorYVal=0, scaleColorZVal=0, UVTypeVal=0;'
            'const double UVsizeVal=1;\n' + admission + '\nreturn MS::kSuccess; }\n'
            'MStatus sampleUV(double minUVal,double maxUVal,double minVVal,double maxVVal,'
            'int numUVal,int numVVal,int i,int j,double offsetUVal,double offsetVVal,'
            'double uStep,double vStep,double randamUVal,double randamVVal,'
            'double& outputU,double& outputV) {\n'
            'dommy::math::RandomGenerator randomGenerator(42);\n' + setup + sample +
            '\noutputU=UVal; outputV=VVal; return MS::kSuccess; }')


def check(source=None, capture=False):
    return run(ROOT, bodies(source), suite='surface_uv_wrapping', arguments=[], capture=capture)


def mutations():
    check()
    source = (ROOT / SOURCE).read_text()
    for label, old, new in (
        ('U origin ignored', 'randU, minUVal, maxUVal, UVal)', 'randU, 0.0, maxUVal - minUVal, UVal)'),
        ('V origin ignored', 'randV, minVVal, maxVVal, VVal)', 'randV, 0.0, maxVVal - minVVal, VVal)'),
        ('U range overflow admitted', '!std::isfinite(maxUVal - minUVal)', 'false'),
        ('V range overflow admitted', '!std::isfinite(maxVVal - minVVal)', 'false'),
    ):
        assert old in source, label
        try:
            check(source.replace(old, new), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'surface_uv_wrapping_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_boundary(sdk):
    code = ('#include <maya/MStatus.h>\n#include <maya/MGlobal.h>\n'
            '#include "common/utility/DUvParameter.h"\n#include "math/DRandomGenerator.h"\n' + bodies())
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(ROOT / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Production surface UV admission/math: Maya SDK syntax/analysis passed; no geometry')


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_boundary(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()
