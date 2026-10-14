#!/usr/bin/env python3
"""Execute the complete DCWeightMap body, ramp admission and detached output."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
NODE = 'src/modules/curve/DCurve/Utility/DCWeightMap.cpp'


def methods(overrides=None):
    source = (overrides or {}).get(NODE, (ROOT/NODE).read_text())
    attrs = re.findall(r'^MObject\s+DCWeightMap::(\w+);', source, re.M)
    result = [extract((ROOT/'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC('),
              'namespace dommy {namespace maya {', extract(
                  (ROOT/'src/core/common/utility/DMayaRampSampling.h').read_text(),
                  'inline bool sampleFiniteRamp(').replace('MRampAttribute', 'TestWeightMapRamp'), '}}',
              'struct DCWeightMap:AttributeNode {static MStatus initialize();'
              'MStatus compute(const MPlug&,MDataBlock&);'
              'MObject thisMObject()const{return {"node"};}' +
              ''.join('static MObject '+attr+';' for attr in attrs) + '};']
    result.extend('MObject DCWeightMap::'+attr+';' for attr in attrs)
    result.append(extract(source, 'MStatus DCWeightMap::initialize('))
    result.append(extract(source, 'MStatus DCWeightMap::compute(').replace(
        'MRampAttribute ramp(', 'TestWeightMapRamp ramp(') + '\nDOMMY_COMPUTE_CATCH("DCWeightMap")')
    return '\n'.join(result)


def run(overrides=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-weight-map-') as directory:
        temp=Path(directory)
        (temp/'curve_weight_map.inc').write_text(methods(overrides))
        for path, content in (overrides or {}).items():
            if path.endswith('.h'):
                destination=temp/Path(path).relative_to('src/core')
                destination.parent.mkdir(parents=True,exist_ok=True)
                destination.write_text(content)
        for contraction in ('off','on'):
            binary=temp/('test_'+contraction)
            command=shlex.split(os.environ.get('CXX','clang++'))+[
                '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                '-Wno-unknown-pragmas','-Wno-unused-function','-Wno-unused-variable',
                '-ffp-contract='+contraction,'-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all','-fno-omit-frame-pointer',
                '-I'+str(temp),'-I'+str(ROOT/'tests/stubs'),'-I'+str(ROOT/'src'),
                '-I'+str(ROOT/'src/core'),str(ROOT/'tests/cpp/curve_weight_map_tests.cpp'),
                '-o',str(binary)]
            subprocess.run(command,check=True,capture_output=capture,text=True)
            environment=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
            subprocess.run([str(binary)],check=True,env=environment,capture_output=capture,text=True)


def mutations():
    source=(ROOT/NODE).read_text()
    variants=(
        ('detached input copy omitted', source.replace(
            'DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*input));', '(void)input;')),
        ('ramp status ignored', source.replace(
            '!dommy::maya::sampleFiniteRamp(ramp, distance, value)',
            '(dommy::maya::sampleFiniteRamp(ramp, distance, value) && false)')),
        ('attribute write status ignored', source.replace(
            'DOMMY_RETURN_IF_FAILURE(DCPAddAttrScalar(\n            *curve, true, false, weightAttribute, "", weights));',
            '(void)DCPAddAttrScalar(*curve, true, false, weightAttribute, "", weights);')),
        ('publication status ignored', source.replace(
            'DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(\n        output, pluginData.object(), DCurveData::id));',
            '(void)dommy::maya::publishTypedPluginDataObject<DCurveData>(output,pluginData.object(),DCurveData::id);')),
        ('final clean status ignored', source.replace(
            'return data.setClean(plug);', '(void)data.setClean(plug);return MS::kSuccess;')),
    )
    for label, changed in variants:
        assert changed!=source, label
        try: run({NODE:changed},capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.startswith('test_'), 'Compile failure: '+label
            assert any(marker in error.stderr for marker in ('Assertion failed','Assertion ','runtime error:')), error.stderr
            print('Detected mutation: '+label,flush=True)
        else: raise AssertionError('Undetected mutation: '+label)


if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--sdk':
        from check_motion_duplicator_safety import sdk_check
        sdk_check(ROOT,sys.argv[2],NODE,'DCWeightMap')
    elif '--mutations' in sys.argv:
        mutations()
    else:
        run()
