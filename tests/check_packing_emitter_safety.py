#!/usr/bin/env python3
"""Exercise real sampling/distance bodies and five full attribute initializers.

Optional VDB/texture hosts are not emulated. Their changed I/O boundaries are
checked separately; this suite does not claim full compute coverage for them.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

FOLDER = 'src/core/DDataUtility/Generate/'
NAMES = ('DPointPlaneEmit', 'DPScatter', 'DPointVolumeEmit', 'DPAnimationEmit', 'DPAddPoint')
READ = re.compile(r'(?m)^(?P<indent>[ \t]*)(?P<left>(?:const\s+)?[\w:]+(?:[ \t]*[&*][ \t]*|[ \t]+)\w+|\w+)\s*=\s*'
                  r'(?P<block>data|block)\.inputValue\((?P<attr>\w+),\s*&(?P<status>stat|status)\)\s*'
                  r'\.(?P<access>as\w+\(\)(?:\.asChar\(\))?);')


def input_boundaries(source):
    # Preserve the actual acquisition/check/access sequence, only discard the
    # consumed value so scalar/vector/string members need not emulate a node.
    result = []
    for match in READ.finditer(source):
        end = source.index(';', match.end()) + 1
        tail = source[match.end():end]
        assert 'RETURN' in tail or 'return' in tail
        result.append((match['attr'], re.sub(r'^.*?=\s*', '(void)', match.group(), count=1,
                                              flags=re.S) + tail))
    pattern = re.compile(r'const MDataHandle \w+InputHandle\s*=\s*(?:data|block)\.inputValue\((?:\w+::)?(\w+),\s*&(?:stat|status)\);'
                         r'\s*DOMMY_RETURN_IF_FAILURE\((?:stat|status)\);\s*[^;]+;')
    for match in pattern.finditer(source):
        code = match.group()
        code = re.sub(r'\b\w+::(?=' + re.escape(match[1]) + r'\b)', '', code)
        last = code.rfind(';', 0, len(code)-1) + 1
        code = code[:last] + re.sub(r'^.*?=\s*', '(void)', code[last:], count=1, flags=re.S)
        result.append((match[1], code))
    return result


def bodies(root, overrides=None):
    overrides = overrides or {}
    result, reads, writers = [], [], []
    for name in ('DPointPlaneEmit', 'DPointVolumeEmit', 'DPAnimationEmit'):
        header = (root / (FOLDER + name + '.h')).read_text()
        result.extend(re.findall(r'enum\s*\{[^}]+\};\s*typedef\s+\w+\s+\w+;', header))
    common = (root / (FOLDER + 'DPointPackingCommon.h')).read_text()
    result.append(common[common.index('enum {'):])
    for name in NAMES:
        path = FOLDER + name + '.cpp'
        source = overrides.get(path, (root / path).read_text())
        if name in NAMES[:3]:
            compute = extract(source, 'MStatus ' + name + '::compute(')
            starts = list(re.finditer(r'MObject outputObject;|MDataHandle outputData = data.outputValue', compute))
            # New output handles are acquired after construction, not separate transactions.
            if 'MObject outputObject;' in compute:
                starts = [m for m in starts if m.group() == 'MObject outputObject;']
            for start in starts:
                set_pos = compute.index('dpData->setPosition(newPtArray);', start.end())
                prefix = compute[start.start():set_pos]
                bounds = compute.index('dpData->updateBBox();', set_pos)
                end = re.search(r'return data.setClean\(\s*plug\s*\);', compute[bounds:])
                assert end
                suffix = compute[bounds:bounds+end.end()]
                function = 'writeOutput' + str(len(writers)); writers.append(function)
                result.append('MStatus '+function+'() try {EmitterBlock data; MStatus stat;'
                              'const MObject outDP{"outDP"}; MPlug plug({}, outDP);'
                              'DPointList newPtArray{{1,2,3},{4,5,6}};'+prefix+
                              'dpData->setPosition(newPtArray);'+suffix+'} DOMMY_COMPUTE_CATCH("output boundary")')
        for attr, code in input_boundaries(source):
            function = 'readInput' + str(len(reads))
            reads.append(function)
            result.append('MStatus '+function+'() try {MStatus stat, status; PackingBlock data, block;'
                          'const MObject '+attr+'{"input"};'+code.replace('MDataHandle', 'PackingHandle')+
                          'return MS::kSuccess;} DOMMY_COMPUTE_CATCH("input boundary")')
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        extra = ''
        if name in NAMES[:2]:
            extra = ('std::vector<PackedCircle> placedPoints;'
                     'float getDistanceToNearestSample(const PackedCircle&) const;')
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();' + extra +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name in NAMES[:2]:
            result.append(extract(source, 'float ' + name + '::getDistanceToNearestSample('))
        if name == 'DPointPlaneEmit':
            for function, token in (('poisson', '} else if (rndType == D_DIST_POISSON)'),
                                    ('blueNoise', 'case DT_PLANE_EMIT_BLUENOISE:')):
                start = source.index(token) + len(token)
                branch = extract(source[start:], '{')
                # Count construction without changing either RNG implementation.
                branch = branch.replace('PoissonGenerator::DefaultPRNG', 'PoissonGenerator::CountingPRNG').replace(
                    'dommy::math::RandomGenerator', 'dommy::math::CountingRandomGenerator')
                result.append('MStatus ' + function + '(size_t countVal, DPointList& newPtArray) try {'
                              'EmitterBlock data; MStatus stat; const MObject aPoissonMinDistance{"distance"};'
                              'const unsigned seedVal=1; const float sizeXVal=10, sizeYVal=10;'
                              'const float offsetX=5, offsetY=5; switch(0){case 0:' + branch +
                              'break;} return MS::kSuccess;} DOMMY_COMPUTE_CATCH("sampling")')
    result.append('const std::vector<std::pair<const char*, MStatus(*)()>> initializers = {' +
                  ','.join('{"'+n+'", &'+n+'::initialize}' for n in NAMES) + '};')
    result.append('const std::vector<MStatus(*)()> readers = {' + ','.join(reads) + '};')
    result.append('const std::vector<MStatus(*)()> writers = {' + ','.join(writers) + '};')
    assert len(reads) == 73 and len(writers) == 5, (len(reads), len(writers))
    return '\n\n'.join(result).replace('MFnNumericAttribute', 'PackingNumericAttribute').replace(
        'MFnPluginData', 'MeshPluginData')


def check_sdk(root, sdk):
    # Exact initializers and input-read sequences with real Maya declarations.
    # Optional VDB/SeExpr compute bodies are deliberately not represented here.
    headers = ('MPxNode', 'MFnNumericAttribute', 'MFnEnumAttribute', 'MFnGenericAttribute',
               'MFnTypedAttribute', 'MFnStringData', 'MFnMatrixAttribute', 'MFnPointArrayData',
               'MDataBlock', 'MDataHandle', 'MPointArray', 'MRampAttribute')
    result = ['#include <maya/'+h+'.h>' for h in headers]
    result += ['#include "common/utility/DComputeGuard.h"',
               '#include "common/utility/DStatusOperation.h"',
               '#include "DDataUtility/Generate/DPointPackingCommon.h"',
               'struct DPData {static const MTypeId id;};',
               'enum {D_X_AXIS=0, D_Y_AXIS, D_Z_AXIS};']
    for name in ('DPointPlaneEmit', 'DPointVolumeEmit', 'DPAnimationEmit'):
        header = (root / (FOLDER + name + '.h')).read_text()
        result.extend(re.findall(r'enum\s*\{[^}]+\};\s*typedef\s+\w+\s+\w+;', header))
    count = 0
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct '+name+':MPxNode {static MStatus initialize();'+
                      ''.join('static MObject '+a+';' for a in attrs)+'};')
        result.extend('MObject '+name+'::'+a+';' for a in attrs)
        result.append(extract(source, 'MStatus '+name+'::initialize('))
        for attr, code in input_boundaries(source):
            result.append('MStatus read'+str(count)+'(MDataBlock& data) try {'
                          'MDataBlock& block=data; MStatus stat, status; MObject '+attr+';'+code+
                          'return MS::kSuccess;} DOMMY_COMPUTE_CATCH("input")')
            count += 1
    assert count == 73
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-variable',
        '-Wno-unknown-pragmas', '-isystem', str(Path(sdk)/'include'),
        '-I'+str(root/'src/core'), '-x', 'c++', '-']
    source = '\n'.join(result)
    subprocess.run(command+['-fsyntax-only'], input=source, text=True, check=True)
    subprocess.run(command+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                           '-Xanalyzer', '-analyzer-werror', '-o', os.devnull],
                   input=source, text=True, check=True)
    print('Packing emitters: five initializers and 73 input boundaries passed real Maya SDK syntax/analyzer checks')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        check_sdk(root, sys.argv[sys.argv.index('--sdk')+1])
    else:
        run(root, bodies(root), suite='packing_emitter', arguments=sys.argv[1:] or [
            str(root / 'tests/fixtures/packing_emitter_initialization_contract.txt')])
