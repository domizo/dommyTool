#!/usr/bin/env python3
"""Actual point-advection bodies; native sampling/DG/TBB are explicit boundaries."""
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

FOLDER = 'src/modules/volume/VDB/Edit/'
NAME = 'DOpenVDBAdvectPoints'

from vdb_dispatch_source import grid_dispatch_source

def enums(source):
    return source[source.index('enum PropagationType'):source.index('struct AdvectionPointParms')]

def initializers(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    return '\n'.join([enums(source)] + declarations(NAME, source, True) +
                     [extract(source, 'MStatus ' + NAME + '::initialize(')])

def compute_bodies(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.h').read_text()
    value = (root / (FOLDER + 'DOpenVDBValuePolicy.h')).read_text()
    plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
    code = ['namespace dommy { namespace vdb {',
            value[value.index('template <typename T> struct ValueComponents'):value.index('template <typename T, bool Integral')],
            extract(value, 'struct ValuePreflight') + ';', '}}',
            extract((root / (FOLDER + 'DOpenVDBFilterPolicy.h')).read_text(), 'namespace dommy {'),
            extract((root / (FOLDER + 'DOpenVDBTransportPolicy.h')).read_text(), 'namespace dommy {'),
            'namespace dommy { namespace maya {',
            extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}',
            extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(), 'class DCurveOutputBatch final') + ';']
    code += [extract(util, signature) for signature in (
        'bool matchesGridName(', 'bool containsGrid(', 'bool\ngetSelectedGrids(', 'bool getSelectedGrids(')]
    code += [grid_dispatch_source(root, point_index_in_points=True)]
    code += declarations(NAME, source, False)
    code += [extract(source, 'namespace {'), extract(source, 'MStatus ' + NAME + '::compute('),
             'DOMMY_VDB_COMPUTE_CATCH("' + NAME + '")']
    return '\n'.join(code)

def guards(root):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    assert 'CHECK_MSTATUS(' not in source
    assert not re.search(r'inputValue\([^;]+\)\.as', source)
    assert 'namespace {' not in (root / (FOLDER + NAME + '.h')).read_text()
    assert 'PointAdvect.h' not in (root / (FOLDER + NAME + '.h')).read_text()

def sdk_check(root, sdk):
    source = (root / (FOLDER + NAME + '.cpp')).read_text()
    code = ['#include <maya/MPxNode.h>', '#include <maya/MPxData.h>',
            '#include <maya/MFnNumericAttribute.h>', '#include <maya/MFnTypedAttribute.h>',
            '#include <maya/MFnEnumAttribute.h>', '#include <maya/MFnStringData.h>',
            '#include <maya/MPointArray.h>', '#include <maya/MDoubleArray.h>',
            '#include <maya/MDataBlock.h>', '#include <maya/MGlobal.h>',
            '#include <memory>', '#include <vector>', '#include <string>', '#include <cmath>',
            '#include "common/utility/DComputeGuard.h"',
            '#include "common/utility/DStatusOperation.h"',
            '#include "common/utility/DMayaPluginData.h"',
            'namespace openvdb {enum GridClass {GRID_UNKNOWN, GRID_STAGGERED};}',
            'struct Grid {openvdb::GridClass getGridClass() const;};',
            'using GridCPtr=std::shared_ptr<const Grid>; using GridCPtrVec=std::vector<GridCPtr>;',
            'enum class DVdbGridType {kVec3f,kVec3d,kOther}; DVdbGridType getVdbGridType(const Grid&);',
            'struct DOpenVDBData:MPxData {static const MTypeId id; bool hasValidData() const;};',
            'bool getSelectedGrids(GridCPtrVec&,const std::string&,const DOpenVDBData&);',
            'struct DPData {static const MTypeId id;};',
            'struct DCurveData {static const MTypeId id;};',
            'struct DCurveInternalData {DCurveInternalData(const MPointArray&,const MDoubleArray&,int);};',
            'using DCItem=std::shared_ptr<DCurveInternalData>;',
            'struct DCurveOutputBatch {bool append(const DCItem&);bool publishTo(DCurveData&)const;};',
            'void GenerateDCurveKnots(std::size_t,int,MDoubleArray&);',
            'struct DPoint {float operator[](int)const;}; using DPointList=std::vector<DPoint>;',
            enums(source), extract(source, 'struct AdvectionPointParms') + ';']
    declared = declarations(NAME, source, True)
    declared[0] = declared[0].replace(':AttributeNode', ':MPxNode')
    declared[1:] = [re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    code += declared + [extract(source, 'MStatus ' + NAME + '::initialize(')]
    code += [extract(source, 'MStatus ' + name + '(') for name in
             ('readPointGrid', 'readPointAdvectionSettings', 'appendLineNodes', 'buildStreamlines')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-deprecated-declarations',
        '-Wno-unused-variable', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core')]
    subprocess.run(command + ['-x', 'c++', '-include', str(root / (FOLDER + NAME + '.h')),
                             '-fsyntax-only', os.devnull], check=True)
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-point-advection-sdk-') as directory:
        path = Path(directory) / 'point_advection.cpp'
        path.write_text('\n'.join(code))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('Point-advection header, initializer, input readers and curve assembly: SDK syntax/analysis passed')
    print('Native OpenVDB kernels/full compute translation unit are NOT covered by this SDK check')

def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('initializer status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('initializer exception escapes', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('failed input handle consumed', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('invalid VDB payload accepted', False, body.replace('!payload || !payload->hasValidData()', '!payload')),
        ('RK order undercounted', False, body.replace('static_cast<std::size_t>(parms.mIntegrationType) + 1U', '1U')),
        ('curve workspace omitted', False, body.replace('workspace.add(samples, sizeof(MPoint) + sizeof(double) + sizeof(DPoint));', '')),
        ('unsafe sample domain admitted', False, body.replace('!dommy::vdb::filterCoordinate(index[axis], 4)', 'false')),
        ('RK envelope omitted', False, body.replace('parms.motionRadius = radius;', 'parms.motionRadius = 0;')),
        ('late CPT sample unchecked', False, body.replace('!pointSampleDomain(*cpt, world, 0)', 'false')),
        ('failed array append ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(curves[i].append(MPoint(p[0], p[1], p[2])));', '(void)curves[i].append(MPoint(p[0], p[1], p[2]));')),
        ('two CV streamline discarded', False, body.replace('if (points.length() < 2U)', 'if (points.length() <= 2U)')),
        ('failed curve batch ignored', False, body.replace('return batch.publishTo(output) ? MS::kSuccess : MS::kFailure;', '(void)batch.publishTo(output); return MS::kSuccess;')),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_point_advection_init' if initialization else 'vdb_point_advection_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    guards(root)
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='vdb_point_advection_init')
        run(root, compute_bodies(root), suite='vdb_point_advection_compute', arguments=[])
