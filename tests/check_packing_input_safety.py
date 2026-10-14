#!/usr/bin/env python3
"""Production packing admission stages, not VDB/texture/DG emulation."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
FOLDER = 'src/core/DDataUtility/Generate/'
NAMES = ('DPointPlaneEmit', 'DPScatter', 'DPointVolumeEmit')
COMMON = 'src/modules/point/DPoint/Common/DPointCommon.cpp'
RAMP = 'src/core/common/utility/DMayaRampSampling.h'


def bodies(overrides=None):
    overrides = overrides or {}
    common = overrides.get(COMMON, (ROOT / COMMON).read_text())
    result = []
    if 'MStatus getValidInputDP(' in common:
        result.append(extract(common, 'MStatus getValidInputDP('))
    result.append(extract(common, 'const DPData* getInputDP('))
    types = (ROOT / (FOLDER + 'DPointPackingCommon.h')).read_text()
    result.append(re.search(r'enum\s*\{[^}]+\};\s*typedef short D_PACKING_TYPE;', types).group(0))
    types = (ROOT / (FOLDER + 'DPointVolumeEmit.h')).read_text()
    result.append(re.search(r'enum\s*\{[^}]+\};typedef short D_PACKIN_3D_TYPE;', types).group(0))
    ramp = overrides.get(RAMP, (ROOT / RAMP).read_text())
    if 'inline MStatus bindValueRamp(' in ramp:
        result.append('namespace dommy {namespace maya {' + extract(ramp, 'inline MStatus bindValueRamp(') + '}}')
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (ROOT / path).read_text())
        start = source.index('MPlug prePackDPPlug(thisMObject(), aPrePackingDP);')
        marker = ('const std::size_t prePointCount =' if name == 'DPointVolumeEmit'
                  else 'if (preDPData->numberOfPoints() >')
        end = source.index(marker, start)
        prefix = source[start:end]
        stripped = strip_comments_and_literals(prefix)
        closing = '}' * (stripped.count('{') - stripped.count('}'))
        result.append('MStatus prePacking' + name + '(MDataBlock& data, std::size_t& count, '
                      'bool& admitted) try {MStatus stat; std::size_t candidateCount=0U;'
                      'const MObject aPrePackingDP{"pre"}, aPreDPGroupName{"group"};' + prefix +
                      'candidateCount=preDPData->numberOfPoints();' + closing +
                      'count=candidateCount; admitted=true; return MS::kSuccess;}'
                      'DOMMY_COMPUTE_CATCH("pre-packing input")')
        if 'MPlug rampPlug(thisMObject(), aPackingGradientBiasRamp);' in source:
            start = source.index('MPlug rampPlug(thisMObject(), aPackingGradientBiasRamp);')
        else:
            start = source.index('m_packGradientRamp = MRampAttribute(')
        marker = ('MDataHandle radiusRangeHandle =' if name == 'DPointVolumeEmit'
                  else 'const MDataHandle aRadiusRangeInputHandle =')
        end = source.index(marker, start)
        result.append('MStatus ramps' + name + '(MDataBlock& data, short mode, '
                      'MRampAttribute& gradient, MRampAttribute& radius, bool& admitted) try {'
                      'MStatus stat; const short m_pack_type=mode; bool m_use_rad_bias=false;'
                      'MRampAttribute& m_packGradientRamp=gradient; MRampAttribute& m_radiusBiasRamp=radius;'
                      'const MObject aPackingGradientBiasRamp{"gradient"}, aUseRadiusBias{"biasEnabled"}, '
                      'aRadiusBiasRamp{"radius"};' + source[start:end] +
                      '(void)m_use_rad_bias; (void)m_pack_type; admitted=true; return MS::kSuccess;}'
                      'DOMMY_COMPUTE_CATCH("packing ramps")')
        mode = 'D_PACK_3D_MAP' if name == 'DPointVolumeEmit' else 'D_PACK_MAP'
        compute = extract(source, 'MStatus ' + name + '::compute(')
        branch = extract(compute, 'if (m_pack_type == ' + mode + ')')
        result.append('MStatus map' + name + '(MDataBlock& data, short m_pack_type, '
                      'bool m_is3d_calc, bool& admitted) try {MStatus stat; '
                      'MFnNurbsSurface srfFn; MFnMesh meshFn; MDataHandle m_obj_hnd;'
                      'const MObject aMap{"map"}, inObject{"geometry"}, inPackingUVObject{"geometry"};' +
                      branch + '(void)data; (void)m_is3d_calc; admitted=true; return MS::kSuccess;}'
                      'DOMMY_COMPUTE_CATCH("packing map")')
    result.append('using PrePackingProbe=MStatus(*)(MDataBlock&,std::size_t&,bool&);'
                  'const PrePackingProbe prePackingProbes[]={' +
                  ','.join('prePacking' + name for name in NAMES) + '};')
    result.append('using RampProbe=MStatus(*)(MDataBlock&,short,MRampAttribute&,MRampAttribute&,bool&);'
                  'const RampProbe rampProbes[]={' + ','.join('ramps' + name for name in NAMES) + '};')
    result.append('using MapProbe=MStatus(*)(MDataBlock&,short,bool,bool&);'
                  'const MapProbe mapProbes[]={' + ','.join('map' + name for name in NAMES) + '};')
    scatter = (ROOT / (FOLDER + 'DPScatter.cpp')).read_text()
    start = scatter.index('MPlug meshPlg(thisMObject(), inObject);', scatter.index('MStatus DPScatter::compute('))
    end = scatter.index('#pragma mark アトリビュート値の取得', start)
    result.append('MStatus scatterConnection(bool& admitted) try {MStatus stat; '
                  'const MObject inObject{"geometry"};' + scatter[start:end] +
                  'admitted=true; return MS::kSuccess;} DOMMY_COMPUTE_CATCH("scatter connection")')
    return '\n\n'.join(result)


def sdk_check(sdk):
    headers = ('MDataBlock', 'MDataHandle', 'MPlug', 'MStatus', 'MObject', 'MFnMesh',
               'MFnNurbsSurface', 'MRampAttribute', 'MGlobal', 'MTypeId', 'MString')
    code = '\n'.join('#include <maya/' + header + '.h>' for header in headers)
    code += '''
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include <cstddef>
#include <vector>
using DPIdList=std::vector<bool>;
struct DPData {static const MTypeId id; bool hasValidPointTopology()const; std::size_t numberOfPoints()const;};
namespace dommy {namespace maya {
MStatus getPluginDataFromHandle(const MDataHandle&,const MTypeId&,const DPData*&);
}}
MObject thisMObject();
MString name();
void EchoWarning(const char*,const char*);
'''
    code += re.sub(r'\{"[^\"]+"\}', '{}', bodies())
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-unused-variable', '-Wno-unused-function', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(ROOT / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Packing acquisition/ramp/map stages: native SDK syntax/analyzer passed; opaque DPData boundary, not native runtime')


def mutations():
    generated = bodies()
    ramp = extract(generated, 'inline MStatus bindValueRamp(')
    cases = [
        ('topology failure ignored', 'if (!candidate || !candidate->hasValidPointTopology())', 'if (!candidate)'),
        ('failed input leaves a borrowed pointer', 'inputData = nullptr;', '(void)inputData;'),
        ('ramp binding status ignored', ramp, ramp.replace('if (!status) return status;', '(void)status;', 1)),
        ('inactive gradient acquired', 'if (m_pack_type == D_PACK_GRADATION)', 'if (true)'),
        ('disabled radius ramp acquired', 'm_pack_type == D_PACK_RANDOM && m_use_rad_bias', 'm_pack_type == D_PACK_RANDOM'),
        ('scatter connection status ignored',
         'const bool meshConnected = meshPlg.isConnected(&stat);\n        DOMMY_RETURN_IF_FAILURE(stat);',
         'const bool meshConnected = meshPlg.isConnected(&stat);'),
    ]
    for name in NAMES:
        for label, signature, old, new in (
            ('pre-packing connection', 'MStatus prePacking',
             'const bool prePackingConnected = prePackDPPlug.isConnected(&stat);\n            DOMMY_RETURN_IF_FAILURE(stat);',
             'const bool prePackingConnected = prePackDPPlug.isConnected(&stat);'),
            ('pre-packing acquisition', 'MStatus prePacking',
             'DOMMY_RETURN_IF_FAILURE(getValidInputDP(aPrePackingDP, data, preDPData));',
             '(void)getValidInputDP(aPrePackingDP, data, preDPData);'),
            ('map connection', 'MStatus map',
             'const bool mapConnected = mapPlug.isConnected(&stat);\n                DOMMY_RETURN_IF_FAILURE(stat);',
             'const bool mapConnected = mapPlug.isConnected(&stat);'),
        ):
            body = extract(generated, signature + name + '(')
            assert old in body, (name, label)
            cases.append((name + ' ' + label, body, body.replace(old, new, 1)))
    for label, old, new in cases:
        assert old in generated, label
        try:
            run(ROOT, generated.replace(old, new, 1), suite='packing_input', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'packing_input_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if '--sdk' in sys.argv:
        sdk_check(sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations()
    else:
        run(ROOT, bodies(), suite='packing_input', arguments=[])
