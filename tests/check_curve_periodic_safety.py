#!/usr/bin/env python3
"""Execute the actual Link curve/surface parameter expressions with API doubles."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
NODE = 'src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp'


def body(source=None):
    source = source if source is not None else (ROOT / NODE).read_text()
    start = source.index('double param;')
    curve = source[start:source.index('MPoint pt;', start)]
    start = source.index('double paramU;')
    surface = source[start:source.index('MPoint pt;', start)]
    return '''
MStatus sampleCurve(double base, float offsetU, float jitter, double start, double end,
                    int wrapMode, double& result) {
    MStatus stat; MFnNurbsCurve crvFn; const int mode=0; const std::size_t c=0;
    const double stepSizeU=base, paramRange=end-start;
    const std::vector<double> stepArray{base};const std::vector<float> randArray{jitter};
''' + curve + '''
    result=param;return MS::kSuccess;
}
MStatus sampleSurface(double baseU,double baseV,float offsetU,float offsetV,float jitter,
                      double startU,double endU,double startV,double endV,int wrapMode,
                      double& resultU,double& resultV) {
    const int mode=0;const std::size_t r=0,c=0,curveIndex=0;
    const double uParam=startU,vParam=startV,stepSizeU=1,stepSizeV=1;
    const double uTotalLength=endU-startU,vTotalLength=endV-startV;
    const std::vector<double> stepUArray{baseU-startU},stepVArray{baseV-startV};
    const std::vector<float> randUArray{jitter},randVArray{jitter};
''' + surface + '''
    resultU=paramU;resultV=paramV;return MS::kSuccess;
}
'''


def mutations():
    source=(ROOT/NODE).read_text()
    for label,old,new in (
        ('curve offset pre-added','std::array<double, 3>{{param, offsetU, randArray[c]}}',
         'std::array<double, 3>{{param + offsetU, 0.0, randArray[c]}}'),
        ('surface U offset pre-added','std::array<double, 3>{{paramU, offsetU, randUArray[curveIndex]}}',
         'std::array<double, 3>{{paramU + offsetU, 0.0, randUArray[curveIndex]}}'),
        ('surface V jitter omitted','std::array<double, 3>{{paramV, offsetV, randVArray[curveIndex]}}',
         'std::array<double, 3>{{paramV, offsetV, 0.0}}'),
    ):
        assert old in source,label
        try: run(ROOT,body(source.replace(old,new)),suite='curve_periodic',arguments=[],capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name=='curve_periodic_tests','Compile failure: '+label
            assert any(marker in error.stderr for marker in ('Assertion failed','Assertion ','runtime error:'))
            print('Detected mutation: '+label,flush=True)
        else: raise AssertionError('Undetected mutation: '+label)


if __name__ == '__main__':
    if '--mutations' in sys.argv:
        mutations()
    else:
        for contraction in ('off', 'on'):
            run(ROOT, body(), suite='curve_periodic', arguments=[],
                extra_flags=('-ffp-contract=' + contraction,))
