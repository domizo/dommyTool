#!/usr/bin/env python3
"""Exercise production historical readers, input/preflight blocks and initializers.

The fixtures supply explicit snapshots, not a Maya DG or deformer evaluator.
Complete translation units are checked separately against a real Maya SDK.
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
from motion_deform_compute_source import computes

PATHS = {
    'DMotionDeformer': 'src/modules/deformers/Deformer/DMotionDeformer.cpp',
    'DMMotionDeform': 'src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp',
    'DMTimeShapeGen': 'src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp',
}


def bodies(root, overrides=None):
    overrides = overrides or {}
    sources = {name: overrides.get(path, (root / path).read_text()) for name, path in PATHS.items()}
    historical = (root / 'src/core/common/utility/DMayaHistoricalSampling.h').read_text()
    result = ['namespace dommy {namespace maya {' +
              extract(historical, 'inline bool historicalSampleTime(') + '}}']
    for name, source in sources.items():
        attrs = re.findall(r'^MObject '+name+r'::(\w+);', source, re.M)
        extra = ''
        if name == 'DMotionDeformer':
            extra = ('MStatus getPlugTimeValue(std::vector<MPointArray>&,long,double,const MTime&,unsigned,unsigned);'
                     'MDataBlock forceCache() const {queriedTimes.push_back(activeTime);return MDataBlock(&snapshots.at(activeTime.value()));}')
        if name == 'DMMotionDeform':
            extra = ('MStatus getDMeshFromTime(std::vector<std::vector<DPointList>>&,const std::vector<std::size_t>&,'
                     'std::size_t,long,double,const MTime&);')
        result.append('struct '+name+':AttributeNode {static MStatus initialize();'
                      'MObject thisMObject()const{return {"node"};}' + extra +
                      ''.join('static MObject '+a+';' for a in attrs)+'};')
        result.extend('MObject '+name+'::'+a+';' for a in attrs)
        result.append(source[source.index('MStatus '+name+'::initialize()'):])
    result.append(extract(sources['DMotionDeformer'], 'MStatus DMotionDeformer::getPlugTimeValue('))
    result.append(extract(sources['DMMotionDeform'], 'MStatus DMMotionDeform::getDMeshFromTime('))
    result.append(extract(sources['DMTimeShapeGen'], 'MStatus sampleMeshPoints('))

    # Execute the actual read/check/access sequence, including string/bool inputs.
    readers = []
    pattern = (r'const MDataHandle (\w+) = data\.inputValue\((\w+), &(stat|status)\);\s*'
               r'if \(!\3\) return \3;\s*const (?:int|float|double|bool|std::string) (\w+) = '
               r'\1\.as\w+\(\)(?:\.asChar\(\))?;')
    for name in ('DMMotionDeform', 'DMTimeShapeGen'):
        for match in re.finditer(pattern, sources[name]):
            body = match[0].replace('inputValue('+match[2]+',', 'inputValue('+name+'::'+match[2]+',')
            function = 'readControl'+str(len(readers))
            result.append('MStatus '+function+'(MDataBlock& data) {MStatus '+match[3]+';'+body+
                          '(void)'+match[4]+';return MS::kSuccess;}')
            readers.append(function)
    if len(readers) != 12:
        raise RuntimeError('Incomplete historical-node control inventory')
    result.append('using ControlReader=MStatus(*)(MDataBlock&);ControlReader controlReaders[]={'+','.join(readers)+'};')
    source = sources['DMMotionDeform']
    block = source[source.index('    const std::size_t meshCount = inputMeshes->numberOfMesh();'):
                   source.index('    MFnPluginData pluginData;')]
    result.append('MStatus preflightMotion(const DMeshData* inputMeshes,int sampleCount,double timeRange,'
                  'double amplitude,double defaultWeight,bool writeMotionPower=false,'
                  'const std::string& motionPowerAttribute="",bool writeMotionVector=false,'
                  'const std::string& motionVectorAttribute=""){'+block+'return MS::kSuccess;}')
    source = sources['DMTimeShapeGen']
    block = source[source.index('            // Scalar preflight precedes'):
                   source.index('            const double stepTime =')]
    result.append('MStatus preflightShape(MFnMesh& meshFn,std::size_t sampling){MStatus stat;'+
                  block+'return MS::kSuccess;}')
    return '\n\n'.join(result)


def check_sdk(root, sdk):
    includes = ['-I' + str(directory) for directory, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(root / 'third_party/libnoise/source'),
        '-idirafter', str(root / 'tests/stubs')]
    for path in PATHS.values():
        subprocess.run(command + ['-fsyntax-only', str(root / path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null',
                                 str(root / path)], check=True)
    print('Motion history: 3 complete SDK translation units passed syntax/analysis')


def compute_mutations(root):
    source = computes(root)
    weight_read = ('if (weight && !dommy::attribute::convertScalarAttribute(\n'
                   '                    weight, (*inputMesh)->numberOfPoints(), weights)) {\n'
                   '                return MS::kInvalidParameter;\n            }')
    power_commit = ('if (!dommy::attribute::commitStagedAttribute(\n'
                    '                        **mesh, motionPowerAttribute, candidate, (*mesh)->numberOfPoints()))\n'
                    '                    return MS::kFailure;')
    cases = (
        ('output mutation before weight read',
         'auto inputMesh = inputMeshes->beginMeshes();',
         'checkDMeshPointAttribute(motionPowerAttribute, *outputMeshes, writeMotionPower);'
         'checkDMeshPointAttribute(motionVectorAttribute, *outputMeshes, writeMotionVector);'
         'auto inputMesh = outputMeshes->beginMeshes();'),
        ('unsupported weight type ignored', weight_read,
         'if (weight) getDMPAttributeFromScalar(*inputMesh, weightAttribute, weights);'),
        ('scalar/vector name collision ignored',
         'motionPowerAttribute == motionVectorAttribute', 'false'),
        ('name bound preflight omitted',
         'motionPowerAttribute.size() > dommy::generation::kMaxGeneratedStringByteCount', 'false'),
        ('attribute write failure ignored', power_commit,
         '(void)dommy::attribute::commitStagedAttribute('
         '**mesh, motionPowerAttribute, candidate, (*mesh)->numberOfPoints());'),
        ('required input status replaced by generic failure',
         'DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputMeshes));',
         'if (!getValidInputDM(inDM, data, inputMeshes)) return MS::kFailure;'),
    )
    for label, old, new in cases:
        assert source.count(old) == 1, label
        try:
            run(root, source.replace(old, new, 1), suite='motion_deform_compute',
                arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'motion_deform_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected motion-compute mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected motion-compute mutation: ' + label)
    historical = bodies(root)
    old = 'if (!bindingStatus) return bindingStatus;'
    assert historical.count(old) == 1
    try:
        run(root, historical.replace(old, 'if (!bindingStatus) return MS::kFailure;', 1),
            suite='motion_history', capture=True)
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'motion_history_tests', 'Compile failure: sampled binding status'
        assert 'Assertion' in error.stderr
        print('Detected historical mutation: sampled binding status replaced', flush=True)
    else:
        raise AssertionError('Undetected sampled binding status mutation')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        check_sdk(root, sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        compute_mutations(root)
    else:
        run(root, computes(root), suite='motion_deform_compute', arguments=[])
        run(root, bodies(root), suite='motion_history', arguments=sys.argv[1:] or
            [str(root / 'tests/fixtures/motion_history_initialization_contract.txt')])
        if not sys.argv[1:]:
            compute_mutations(root)
