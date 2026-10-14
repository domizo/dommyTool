#!/usr/bin/env python3
"""Execute complete DCFitCopy and DCCopy control flow.

Geometry and Maya boundaries are host doubles; this is not a Maya runtime test.
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

FIT = 'src/modules/curve/DCurve/Utility/DCFitCopy.cpp'
COPY = 'src/modules/curve/DCurve/Utility/DCCopy.cpp'


def bodies(root, overrides=None):
    overrides = overrides or {}
    fit = overrides.get(FIT, (root / FIT).read_text())
    copy = overrides.get(COPY, (root / COPY).read_text())
    attrs = re.findall(r'^MObject\s+DCFitCopy::(\w+);', fit, re.M)
    result = ['struct DCFitCopy:AttributeNode { static MStatus initialize(); '
              'MStatus compute(const MPlug&, MDataBlock&); '
              'MObject thisMObject() const {return {"node"};} '
              'MString name() const {return "DCFitCopy";} ' +
              ''.join('static MObject ' + a + ';' for a in attrs) + '};']
    result.extend('MObject DCFitCopy::' + a + ';' for a in attrs)
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    result.append(extract(utility, 'class DCurveOutputBatch final') + ';')
    node_operation = (root / 'src/core/common/object/DNodeOperation.cpp').read_text()
    result.append(extract(node_operation, 'MStatus DGetMayaCurveFromPlug('))
    result.append(extract(node_operation, 'MStatus getAlignSpaceMatrix('))
    curve_utility = (root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result.append(extract(curve_utility, 'MStatus getValidInputDC('))
    result.append(extract(fit, 'void DGenerateRandomValue('))
    result.append(extract(fit, 'MStatus DCFitCopy::initialize('))
    result.append(extract(fit, 'MStatus DCFitCopy::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DCFitCopy")')
    # Exercise the legacy Fit connection loop separately with identity rotations
    # and per-copy markers. DCCopy's shared emitter is covered by full compute.
    for name, source in (('Fit', fit),):
        loop = extract(source.rsplit('//connect mode', 1)[1],
                       'for(uint i=0;i<matrixArray->size();i++)')
        matrix_type = 'std::vector<MMatrix>'
        setup = ('(*matrixArray)[i][u].resize(4);'
                 'for(auto& m:(*matrixArray)[i][u])m[3][1]=100*i+10*u;')
        result.append('MStatus connect' + name + '''(unsigned rows,unsigned cols,
                DCurveOutputBatch& generatedCurves) try {
            auto matrixArray=std::make_shared<std::vector<std::vector<''' + matrix_type + '''>>>(rows);
            for(unsigned i=0;i<rows;++i) {(*matrixArray)[i].resize(cols);
                for(unsigned u=0;u<cols;++u) {''' + setup + '''}}
            MPointArray copyCvs={{1,0,0},{1,1,0},{1,2,0},{1,3,0}};
            double scale[3]={1,1,1},uniScale=1;
            std::vector<double> rndScaleXValues(rows*cols),rndScaleYValues(rows*cols,0),rndScaleZValues(rows*cols,0);
            std::vector<double> rndRotXValues(rows*cols,0),rndRotYValues(rows*cols,0),rndRotZValues(rows*cols,0);
            for(unsigned i=0;i<rows*cols;++i)rndScaleXValues[i]=i;
            const int copyDegree=3;size_t indexOffset=0;
            ''' + loop + '\nreturn MS::kSuccess;} DOMMY_COMPUTE_CATCH("copy loop")')
    attrs = re.findall(r'^MObject\s+DCCopy::(\w+);', copy, re.M)
    result.append('struct DCCopy:AttributeNode { static MStatus initialize(); '
                  'MStatus compute(const MPlug&, MDataBlock&); '
                  'MObject thisMObject() const {return {"node"};} ' +
                  ''.join('static MObject ' + a + ';' for a in attrs) + '};')
    result.extend('MObject DCCopy::' + a + ';' for a in attrs)
    result.append(copy[copy.index('namespace {'):copy.index('MStatus DCCopy::compute(')])
    result.append(extract(copy, 'MStatus DCCopy::initialize('))
    result.append(extract(copy, 'MStatus DCCopy::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DCCopy")')
    # Cover all connected append/copy sites.
    for source in (fit, copy):
        assert not re.search(r'MPointArray\s+cvs\(copyCvs\)', source)
        for line in source.splitlines():
            if 'totalCvs.append(finPt)' in line:
                assert 'DOMMY_RETURN_IF_FAILURE(totalCvs.append(finPt))' in line
    return '\n\n'.join(result)


def run(root, source, arguments, capture=False, header_overrides=None):
    with tempfile.TemporaryDirectory(prefix='dommy-curve-copy-') as directory:
        temp = Path(directory)
        (temp / 'curve_copy.inc').write_text(source)
        for path, content in (header_overrides or {}).items():
            header = temp / path
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text(content)
        binary = temp / 'curve_copy_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp),
            '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/curve_copy_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary)] + arguments, check=True, env=environment,
                              capture_output=capture, text=True)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    run(root, bodies(root), [str(root / 'tests/fixtures/curve_copy_initialization_contract.txt'),
                            str(root / 'tests/fixtures/dccopy_initialization_contract.txt')])
