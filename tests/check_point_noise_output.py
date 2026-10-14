#!/usr/bin/env python3
"""Complete Fast/DF/Pyro node dispatch with explicit sampling/Maya boundaries.

Typed payload admission, coordinate math, staging and seed sequencing are
production code. Noise algorithms and native queries have separate suites.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

PATHS = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPFastNoise', 'DPDFNoise', 'DPPyroNoise')}


def bodies(root):
    header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    common = (root / 'src/core/common/object/DNoiseOp.cpp').read_text()
    result = []
    for alias in ('NoiseBaseType', 'NoiseDeformType', 'FastNoiseType', 'FNInterpType',
                  'FNFractalType', 'FNCellularDistanceType', 'FNCellularReturnType'):
        result.append(re.search(r'enum\s*\{[^}]*\};typedef short ' + alias + ';', header).group())
    for name in ('NoiseBaseParams', 'FastNoiseParams'):
        result.append(extract(header, 'struct ' + name) + ';')
    for name in ('validateNoiseBaseParams', 'validateFastNoiseParams', 'validatePyroNoiseParams',
                 'validateDFNoiseParams'):
        result.append(extract(common, 'MStatus ' + name + '('))
    result.append('''
MStatus createFastNoiseFromPosition(const FastNoiseParams& params, const DPointList& points,
                                    DScalarDataList& values) {
    const auto status=operation("noise.fast");if(!status)return status;
    sampledSeeds.push_back(params.mSeed);
    for(std::size_t i=0;i<points.size();++i)values.at(i)=float(i+1U)*.25f;
    return MS::kSuccess;
}
MStatus createDFNoiseFromPosition(const NoiseBaseParams& params, const DPointList& points,
                                  DVec3DataList& values) {
    const auto status=operation("noise.df");if(!status)return status;
    sampledSeeds.push_back(params.mSeed);
    for(std::size_t i=0;i<points.size();++i)values.at(i)=DVec3Data(.25f,.5f,.75f)*float(i+1U);
    return MS::kSuccess;
}
MStatus createPyroNoiseFromPosition(const NoiseBaseParams& params,const DPoint& center,float,
                                    float,const DPointList& points,DScalarDataList& values) {
    const auto status=operation("noise.pyro");if(!status)return status;
    sampledSeeds.push_back(params.mSeed);
    for(std::size_t i=0;i<points.size();++i)values.at(i)=float(params.mSeed)*.1f+center[0];
    return MS::kSuccess;
}
''')
    for name, path in PATHS.items():
        source = (root / path).read_text()
        result.append(extract(source, 'namespace {'))
        declared = declarations(name, source, False)
        method = 'evalNoiseParms' if name == 'DPFastNoise' else 'evalNoiseBaseParms'
        params = 'FastNoiseParams' if name == 'DPFastNoise' else 'NoiseBaseParams'
        declared[0] = declared[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};} '
            'MStatus ' + method + '(MDataBlock&, ' + params + '&); MStatus compute(')
        result.extend(declared)
        result.append(extract(source, 'MStatus ' + name + '::' + method + '('))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    flags = ['-Wno-unused-parameter', '-I' + str(root / 'src/core/common/utility'),
             '-isystem', str(root / 'third_party/libnoise/source')]
    flags.extend(str(root / 'third_party/libnoise/source' / (name + '.cpp'))
                 for name in ('noisegen', 'modulebase', 'perlin'))
    run(root, source, suite='point_noise_output', arguments=[], extra_flags=flags, capture=capture)
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    run(root, source, suite='point_noise_output', arguments=['budget'], extra_flags=flags,
        capture=capture, header_overrides={path: header})


def mutations(root):
    source = bodies(root)
    execute(root, source)
    for label, old, new in (
        ('fast point admission omitted', 'dommy::point::preflightPointPayload(*input,\n',
         'if(false) dommy::point::preflightPointPayload(*input,\n'),
        ('DF point admission omitted', 'dommy::point::preflightPointPayload(*input, pointBytes, workspace);', ''),
        ('pyro point admission omitted', 'dommy::point::preflightPointPayload(*input, sizeof(DPoint) + 2U * sizeof(float), workspace);', ''),
        ('collection admission omitted', 'dommy::point::preflightPrimitivePointPayload(',
         'if(false) dommy::point::preflightPrimitivePointPayload('),
        ('generated attribute admission omitted', 'dommy::point::preflightGeneratedPointAttribute(',
         'if(false) dommy::point::preflightGeneratedPointAttribute('),
        ('fast placement omitted', 'point = points[index]; return MS::kSuccess;',
         '(void)point; (void)index; return MS::kSuccess;'),
        ('DF stored amplitude scaled', 'return DPAttributeData::Ptr(new DPVec3fAttr(divergence));',
         '{auto changed=divergence;for(auto& value:changed)value*=2.0f;return DPAttributeData::Ptr(new DPVec3fAttr(changed));}'),
        ('cumulative seed reset', 'params.mSeed += static_cast<int>(offset);', 'params.mSeed = static_cast<int>(offset);'),
    ):
        assert old in source, label
        try:
            execute(root, source.replace(old, new, 1), True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_noise_output_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk = sys.argv[sys.argv.index('--sdk') + 1]
        sdk_check(root, sdk, [PATHS['DPDFNoise']], 'DF noise')
        with tempfile.TemporaryDirectory(prefix='dommy-fast-noise-sdk-') as directory:
            # Compile-only external API seam; no generator/build/link claim.
            (Path(directory) / 'FastNoise.h').write_text(
                '#pragma once\nusing FN_DECIMAL=float;'
                'struct FastNoise {float GetNoise(float,float,float)const;};\n')
            sdk_check(root, sdk, [PATHS['DPFastNoise']], 'Fast noise (external API declaration)',
                      ['-DDOMMY_WITH_FASTNOISE=1', '-I' + directory])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
