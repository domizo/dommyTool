#!/usr/bin/env python3
"""DVDBNoise numerical policy and host-boundary regression checks."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/volume/VDB/Edit/DOpenVDBNoise.cpp'
MATH = 'src/modules/volume/VDB/Edit/DOpenVDBNoiseMath.h'


def compute_bodies(root):
    source = (root / SOURCE).read_text()
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
    return '\n'.join([
        extract((root / MATH).read_text(), 'namespace dommy {'),
        'namespace dommy { namespace maya {',
        extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}',
        extract(util, 'bool matchesGridName('), extract(util, 'bool containsGrid('),
        extract(util, 'bool\ngetSelectedGrids('), extract(util, 'bool getSelectedGrids('),
        extract(source, 'namespace {'),
        *declarations('DOpenVDBNoise', source, False),
        extract(source, 'MStatus DOpenVDBNoise::compute('),
        'DOMMY_VDB_COMPUTE_CATCH("DOpenVDBNoise")'])


def noise_flags(root):
    folder = root / 'third_party/libnoise/source'
    return ['-isystem', str(folder), '-Wno-unused-parameter',
            *[str(folder / name) for name in ('perlin.cpp', 'modulebase.cpp', 'noisegen.cpp')]]


def initializer(root):
    source = (root / SOURCE).read_text()
    return '\n'.join(declarations('DOpenVDBNoise', source, True) +
                     [extract(source, 'MStatus DOpenVDBNoise::initialize(')])


def sdk_check(root, sdk):
    source = (root / SOURCE).read_text()
    includes = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnEnumAttribute',
        'MFnUnitAttribute', 'MFnStringData', 'MTypeId', 'MFloatVector', 'MPxData')]
    includes += ['#include <memory>', '#include <vector>',
        '#include "common/utility/DComputeGuard.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaPluginData.h"', '#include "' + MATH[4:] + '"']
    stubs = '''
struct Grid {};
using GridCPtrVec = std::vector<std::shared_ptr<const Grid>>;
struct DOpenVDBData : MPxData {static const MTypeId id; bool hasValidData() const;};
const MTypeId DOpenVDBData::id(123);
bool getSelectedGrids(GridCPtrVec&, const std::string&, const DOpenVDBData&);
using dommy::vdb::NoiseSettings;
'''
    declared = declarations('DOpenVDBNoise', source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode').replace(
        'static MStatus initialize();', 'static MStatus initialize(); MStatus readInputs(MDataBlock&);')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    compute = extract(source, 'MStatus DOpenVDBNoise::compute(')
    readers = compute[compute.index('MStatus stat;'):compute.index('MFnPluginData pluginData;')]
    assert readers.count('data.inputValue(') == 16
    code = '\n'.join(includes + [stubs] + declared + [
        extract(source, 'MStatus DOpenVDBNoise::initialize('),
        'MStatus DOpenVDBNoise::readInputs(MDataBlock& data) try {' + readers +
        '\nreturn MS::kSuccess;} DOMMY_COMPUTE_CATCH("DVDBNoise input readers")'])
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include',
        str(root / SOURCE.replace('.cpp', '.h')), '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-noise-sdk-') as directory:
        path = Path(directory) / 'noise.cpp'
        path.write_text(code)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('DVDBNoise complete initializer and all 16 input acquisitions: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and the complete compute translation unit are NOT covered by this SDK check')


def guards(root):
    source = (root / SOURCE).read_text()
    compute = extract(source, 'MStatus DOpenVDBNoise::compute(')
    assert not re.search(r'inputValue\([^;]+\)\.as', compute)
    assert 'publishOpenVDBOutput(' in compute
    assert compute.count('hasValidData()') == 2
    assert compute.index('addNoiseWork(') < compute.index('deepCopyGrid(')
    assert 'CHECK_MSTATUS(' not in source
    assert 'FractalBoltzmanGenerator' not in (root / SOURCE.replace('.cpp', '.h')).read_text()


def mutations(root):
    init, body = initializer(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('input status ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('payload validation omitted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('optional input failures ignored', False, body.replace(
            'MDataHandle maskHandle = data.inputValue(maskVdbInput, &stat);\n    DOMMY_RETURN_IF_FAILURE(stat);',
            'MDataHandle maskHandle = data.inputValue(maskVdbInput, &stat);')),
        ('typed adapter cast guard omitted', False, re.sub(
            r'if \(!grid.isType<GridType>\(\) \|\| \(mask && !mask->isType<GridType>\(\)\)\)\s*return false;', '', body)),
        ('batch type preflight omitted', False, body.replace('if (maskGrid && ((isFloat', 'if (false && maskGrid && ((isFloat')),
        ('aggregate work reset each grid', False, body.replace('dommy::vdb::addNoiseWork(source->activeVoxelCount()', 'dommy::vdb::addNoiseWork((work = 0, source->activeVoxelCount())')),
        ('work boundary rejects equality', False, body.replace('activeVoxels > generation::kMaxWorkItemCount', 'activeVoxels >= generation::kMaxWorkItemCount').replace(
            'generation::kMaxWorkItemCount, samples)', 'generation::kMaxWorkItemCount - 1U, samples)')),
        ('octaves limit omitted', False, body.replace('s.mOctaves <= kMaxNoiseOctaves', 'true').replace('octaves > kMaxNoiseOctaves', 'false')),
        ('inclusive octave changed', False, body.replace('octave <= s.mOctaves', 'octave < s.mOctaves')),
        ('noise frequency configuration lost', False, body.replace('generator.SetFrequency(settings.mFreq);', 'generator.SetFrequency(1.0);')),
        ('mode2 negative weight restored', False, body.replace('s.mMaskMode == 2 && alignment <= 0.9 ? 1.0 : 0.0', 's.mMaskMode == 2 && alignment <= 0.9 ? distance / s.mFallOff : 0.0')),
        ('zero falloff guard lost', False, body.replace('s.mFallOff == 0.0f || distance >= s.mFallOff', 'distance > s.mFallOff')),
        ('source stencil guard lost', False, body.replace('!dommy::vdb::noiseStencilCoordinate(coordinate[axis])', 'false')),
        ('mask coordinate guard lost', False, re.sub(r'if \(!dommy::vdb::noiseMaskCoordinate\(voxel\[axis\], settings.mMaskMode == 2,\s*index\[axis\]\)\)\s*return false;',
            'index[axis] = static_cast<int>(voxel[axis]);', body)),
        ('mask frequency narrowed to float', False, body.replace('frequencyMultiplier = double(sample);', 'frequencyMultiplier = float(sample);')),
        ('inactive sample ignored', False, body.replace('openvdb::tools::BoxSampler::sample<Tree>(maskGrid->tree(), voxel, sample);',
            'if (!openvdb::tools::BoxSampler::sample<Tree>(maskGrid->tree(), voxel, sample)) continue;')),
        ('invalid numeric output published', False, body.replace('!dommy::vdb::noiseOutputValue(*it, noise, weight, settings.mOffset, value)',
            '(value = static_cast<typename GridType::ValueType>(*it + weight * (noise - settings.mOffset)), false)')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_noise_init' if initialization else 'vdb_noise_compute',
                arguments=None if initialization else [], extra_flags=[] if initialization else noise_flags(root))
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        guards(root)
        mutations(root)
    elif '--emit' in sys.argv:
        run(root, initializer(root), suite='vdb_noise_init', arguments=['--emit'])
    else:
        guards(root)
        run(root, initializer(root), suite='vdb_noise_init')
        run(root, compute_bodies(root), suite='vdb_noise_compute', arguments=[],
            extra_flags=noise_flags(root))
