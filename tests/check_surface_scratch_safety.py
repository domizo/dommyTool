#!/usr/bin/env python3
"""Execute production scratch declarations/appends, texture adapter and Dot UV.

Geometry queries and texture evaluation are explicit seams, not native Maya DG.
"""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/generators/legacy/surfaceDuplicater.cpp'


def bodies(source=None):
    source = source or (ROOT / SOURCE).read_text()
    header = (ROOT / SOURCE.replace('.cpp', '.h')).read_text()
    assert 'schedulingType() const override { return kUntrusted; }' in header
    header_code = strip_comments_and_literals(header)
    for name in ('colPoints', 'preUCoords', 'preVCoords'):
        assert name not in header_code, 'Scratch member restored: ' + name
    assert 'static MStatus getColorFromTex(' in header_code
    calls = source[:source.index('MStatus surfaceDuplicater::getColorFromTex(')]
    assert calls.count('getColorFromTex(') == 6
    assert calls.count('.object(), colUCoords, colVCoords,') == 6
    start = source.index('MFloatPointArray colPoints;')
    declarations = source[start:source.index('#pragma mark get attribute value', start)]
    assert 'clear(' not in declarations
    start = source.index('DOMMY_RETURN_IF_FAILURE(\n\t\t\t\t\tcolPoints.append(')
    appends = source[start:source.index('DOMMY_RETURN_IF_FAILURE(planeNormArray.append(', start)]
    assert appends.count('append(') == 5
    dot = extract(source, 'case 1://dot')
    dot = dot[dot.index('{'):]
    adapter = extract(source, 'MStatus surfaceDuplicater::getColorFromTex(')
    return ('struct surfaceDuplicater {\n'
            'static MStatus getColorFromTex(MObject, const MFloatArray&, const MFloatArray&,'
            'MVectorArray&, MDoubleArray&);\n'
            'MStatus evaluate(const std::vector<Sample>& samples, float& outputU, float& outputV);\n};\n'
            + adapter + '\n'
            'MStatus surfaceDuplicater::evaluate(const std::vector<Sample>& samples,'
            'float& outputU, float& outputV) try {\n' + declarations + '\n'
            'for (const Sample& sample : samples) {\n'
            'const MFloatPoint uvPos=sample.point;\n'
            'const double UVal=sample.u,VVal=sample.v,preUVal=sample.preU,preVVal=sample.preV;\n'
            + appends + '\n}\n'
            'MVectorArray colors; MDoubleArray transparencies;\n'
            'DOMMY_RETURN_IF_FAILURE(getColorFromTex({},colUCoords,colVCoords,colors,transparencies));\n'
            'assert(colPoints.length()==samples.size());\n'
            'for (unsigned int index=0;index<colPoints.length();++index) {'
            'assert(colPoints[index].x==samples[index].point.x);}\n'
            'if (samples.empty()) return MS::kSuccess;\n'
            'const MFloatArray newUCoords({0.0f}),newVCoords({0.0f});\n'
            'const int p=0; const unsigned int fin=static_cast<unsigned int>(samples.size()-1);\n'
            'const double UVsizeVal=1.0;\n'
            'dommy::maya::CheckedArray<MFloatArray> finUCoords,finVCoords;\n'
            + dot + '\n'
            'outputU=finUCoords[0];outputV=finVCoords[0];return MS::kSuccess;\n'
            '} catch (...) {return MS::kFailure;}\n')


def check(source=None, capture=False):
    return run(ROOT, bodies(source), suite='surface_scratch', arguments=[], capture=capture)


def mutations():
    check()
    source = (ROOT / SOURCE).read_text()
    for label, old, new in (
        ('U scratch persists between evaluations', 'MFloatArray colUCoords, colVCoords;',
         'static MFloatArray colUCoords; MFloatArray colVCoords;'),
        ('Dot scratch persists between evaluations', 'MFloatArray preUCoords, preVCoords;',
         'static MFloatArray preUCoords; MFloatArray preVCoords;'),
        ('sample append failure ignored', 'DOMMY_RETURN_IF_FAILURE(preVCoords.append(preVVal));',
         '(void)preVCoords.append(preVVal);'),
        ('texture adapter allocation status ignored',
         'MStatus status = uCoordinates.setLength(colUCoords.length());\n\tif (!status) return status;',
         'MStatus status = uCoordinates.setLength(colUCoords.length());'),
        ('texture coordinate write status ignored',
         'status = vCoordinates.set(colVCoords[index], index);\n\t\tif (!status) return status;',
         'status = vCoordinates.set(colVCoords[index], index);'),
    ):
        assert old in source, label
        try:
            check(source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'surface_scratch_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation:', label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(ROOT, sys.argv[2], source_path=SOURCE, label='surfaceDuplicater')
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()
