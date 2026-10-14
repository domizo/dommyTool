#!/usr/bin/env python3
"""Attribute smoothing and voxel sampling, without claiming native VDB coverage."""
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

NODES = {
    'DPAttrSmooth': 'src/core/DDataUtility/Attribute/Edit/DPAttrSmooth.cpp',
    'DOpenVDBSamplingDP': 'src/modules/volume/VDB/Generate/DOpenVDBSamplingDP.cpp',
}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, True)
        declared[0] = declared[0].replace(' {', ' {static MObject message;', 1)
        result.extend(declared)
        result.append('MObject ' + name + '::message{"message"};')
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def compute_bodies(root):
    result = ['#include "common/utility/DPointMeanAccumulator.h"',
              extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'MStatus getValidInputDP(')]
    for path in ('src/core/common/utility/DAttributeSmoothing.h',
                 'src/core/common/utility/DPointPayloadBudget.h',
                 'src/modules/volume/VDB/Common/DVdbParticleRasterPlan.h'):
        result.append(extract((root / path).read_text(), 'namespace dommy {'))
    force = (root / 'src/modules/point/DPoint/Common/DPointForceOutput.h').read_text()
    result.append('namespace dommy {namespace point {' +
                  extract(force, 'struct PointForceInput') + ';}}')
    for name, path in NODES.items():
        source = (root / path).read_text()
        if name == 'DPAttrSmooth':
            result.append(extract(source, 'namespace {'))
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_VDB_COMPUTE_CATCH("' + name + '")')
    return '\n'.join(result)


def guards(root):
    smooth, sampling = ((root / path).read_text() for path in NODES.values())
    assert 'removeAttr(' not in smooth
    assert 'getPositionConst()' not in sampling
    assert 'checkedSamplingCoordinate(' in sampling
    assert smooth.index('preflightSmoothingPartition(*input.points') < smooth.index('setPluginDataOutput<DPData>')
    assert sampling.index('budget.add(count') < sampling.index('coordinates(count)')
    for source in (smooth, sampling):
        assert not re.search(r'inputValue\([^;]+\)\.as', source)
        assert 'reportEntryPointException' in source


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('int64 precision lost', compute.replace('static_cast<std::uint64_t>(value)',
                                                'static_cast<std::uint64_t>(static_cast<double>(value))')),
        ('integer remainder borrow omitted', compute.replace('--magnitude;', '(void)magnitude;')),
        ('vector default not initialized', compute.replace('return Imath::Vec3<T>(T(0), T(0), T(0));',
                                                          'return Imath::Vec3<T>();')),
        ('rounded coordinate unchecked', compute.replace('numeric::checkedNumericCast(rounded, coordinate)',
                                                         '(coordinate = static_cast<int>(rounded), true)')),
        ('input status ignored', compute.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('iteration budget omitted', compute.replace('checkedProduct(passes, iterations,',
                                                     'checkedProduct(passes, ((void)iterations, 0U),')),
        ('aggregate work reset', compute.replace('checkedSum(total, next,', 'checkedSum(0U, next,')),
        ('payload budget removed', compute.replace('!dommy::attribute::validArrayConversionBudget(1U, total)', 'false')),
        ('metadata payload omitted', compute.replace('dommy::point::preflightMetadataPayload(input, count, budget);', '')),
        ('mesh corner payload omitted', re.sub(r'dommy::point::preflightAttributePayload\(\s*mesh\.beginPVtxAttr\(\)[\s\S]*?;', '', compute)),
        ('bad neighbor accepted', compute.replace('static_cast<std::size_t>(ids[i]) >= values.size()', 'false')),
    ]
    for label, body in cases + [
            ('initializer status ignored', init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
            ('initializer exception escaped', init.replace('return dommy::maya::reportEntryPointException(',
                                                         'throw; return dommy::maya::reportEntryPointException(', 1))]:
        initialization = label.startswith('initializer')
        assert body != (init if initialization else compute), label
        try:
            run(root, body, capture=True,
                suite='vdb_smoothing_init' if initialization else 'vdb_smoothing_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    # Real Maya and geometry/attribute declarations. VDB/TBB work is explicitly
    # excluded; only a vector alias and serial runner declaration are supplied.
    headers = ('MPxNode', 'MFnNumericAttribute', 'MFnGenericAttribute', 'MFnTypedAttribute',
               'MFnStringData', 'MDataBlock', 'MDataHandle')
    source = ['#include <maya/' + h + '.h>' for h in headers]
    source += ['#include "DPData.h"', '#include "DCurveData.h"', '#include "DMeshData.h"',
               '#include "common/utility/DAttributeSmoothing.h"',
               '#include "common/utility/DPointPayloadBudget.h"',
               '#include "common/utility/DComputeGuard.h"',
               '#include "common/utility/DStatusOperation.h"',
               '#include "common/utility/DMayaPluginData.h"',
               '#include "modules/volume/VDB/Common/DVDBComputeGuard.h"',
               '#include "modules/volume/VDB/Common/DVdbParticleRasterPlan.h"',
               'struct DOpenVDBData {static const MTypeId id;}; const MTypeId DOpenVDBData::id(4);',
               'namespace openvdb {using Vec3R = Imath::V3d;}',
               'using IDs = std::vector<uint32_t>; using IDsListArray = std::vector<IDs>;',
               'using ParallelSmoothingPass = dommy::attribute::SerialSmoothingPass;']
    source.append(extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                          'MStatus getValidInputDP('))
    for name, path in NODES.items():
        body = (root / path).read_text()
        declared = declarations(name, body, True)
        declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
        if name == 'DOpenVDBSamplingDP':
            declared[0] = declared[0].replace('static MStatus initialize();',
                'static MStatus initialize(); MStatus compute(const MPlug&,MDataBlock&);')
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', line) for line in declared[1:]]
        source.extend(declared)
        source.append(extract(body, 'MStatus ' + name + '::initialize('))
    smooth = (root / NODES['DPAttrSmooth']).read_text()
    for signature in ('std::size_t smoothingComponents(', 'struct SmoothingRequest',
                      'void preflightTopologyStorage(const DPData&',
                      'void preflightTopologyStorage(const DCurveInternalData&',
                      'void preflightTopologyStorage(const DMeshInternalData&',
                      'template <typename Owner>\nvoid preflightSmoothingPartition',
                      'template <typename Container>\nvoid preflightSmoothingContainer',
                      'template <typename T, typename Owner>\nvoid smoothTypedAttribute'):
        source.append(extract(smooth, signature) + (';' if signature.startswith('struct') else ''))
    source.append('void instantiate(DPData& p, DCurveData& c, DMeshData& m, '
                  'DMeshInternalData& primitive, const SmoothingRequest& r, const IDsListArray& ids) {'
                  'dommy::point::PointPayloadBudget b; std::size_t work=0;'
                  'preflightSmoothingPartition(p,r,b,work);'
                  'preflightSmoothingContainer(c,c.numberOfCurves(),r,b,work);'
                  'preflightSmoothingContainer(m,m.numberOfMesh(),r,b,work);'
                  'smoothTypedAttribute<int64_t>(p,r,ids); smoothTypedAttribute<Imath::V3d>(primitive,r,ids);}')
    sample = (root / NODES['DOpenVDBSamplingDP']).read_text()
    prefix = extract(sample, 'MStatus DOpenVDBSamplingDP::compute(').split('    MStatus status;')[0]
    source.append(prefix + 'return MS::kSuccess; } DOMMY_VDB_COMPUTE_CATCH("Sampling input boundary")')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-variable',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-idirafter', str(root / 'tests/stubs')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-smoothing-sdk-') as directory:
        path = Path(directory) / 'smoothing.cpp'
        path.write_text('\n'.join(source))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB smoothing/sampling: initializers, typed smoothing/geometry helpers and checked sampling input boundary passed SDK syntax/analysis')
    print('Limited Imath declarations; native VDB/TBB, full compute units and Maya runtime are not covered')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='vdb_smoothing_init', arguments=['--emit'])
    elif '--mutations' in sys.argv:
        guards(root)
        mutations(root)
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    else:
        guards(root)
        run(root, initializers(root), suite='vdb_smoothing_init')
        run(root, compute_bodies(root), suite='vdb_smoothing_compute', arguments=[])
        body = compute_bodies(root)
        old = 'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));'
        assert old in body
        try:
            run(root, body.replace(old, 'if (!getValidInputDP(inDP, data, input)) return MS::kFailure;', 1),
                capture=True, suite='vdb_smoothing_compute', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'vdb_smoothing_compute_tests', 'Compile failure'
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr
            print('Detected sampling required-input status mutation', flush=True)
        else:
            raise AssertionError('Sampling status mutation was not detected')
