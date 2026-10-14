#!/usr/bin/env python3
"""Execute production VDB viewport builders and complete DG/bounds callbacks."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_vdb_viewport_clip_safety import bodies as clip_bodies

ROOT = Path(__file__).resolve().parent.parent
DISPLAY = 'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp'
UTILITY = 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp'
HEADER = 'src/modules/volume/VDB/Common/DOpenVDBUtil.h'


def bodies(overrides=None, sdk=False):
    overrides = overrides or {}
    source = overrides.get(DISPLAY, (ROOT / DISPLAY).read_text())
    render=extract(source,'void DOpenVDBDisplayVP2Override::updateRenderItems(')
    assert 'MPlug(' not in render, 'DG evaluation returned to the VDB render callback'
    common = overrides.get(UTILITY, (ROOT / UTILITY).read_text())
    header = overrides.get(HEADER, (ROOT / HEADER).read_text())
    data = (ROOT / 'src/modules/volume/VDB/Data/DOpenVDBData.cpp').read_text()
    values = (ROOT / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    plugin = (ROOT / 'src/core/common/utility/DMayaPluginData.h').read_text()
    code = ['#include "vdb_viewport_doubles.h"']
    if not sdk:
        code += ['namespace dommy {namespace maya {',
                 extract(values, 'inline bool isFinitePoint('),
                 extract(values, 'inline bool isFloatRepresentablePoint('),
                 extract(values, 'inline bool isFiniteColor('),
                 extract(plugin, 'template <typename DataType>\nMStatus getPluginData(const MFnPluginData&'),
                 extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromObject('),
                 extract(plugin, 'template <typename DataType>\nMStatus getPluginDataFromPlug('), '}}']
    code += [extract(data, 'template <typename GridVector>'),
             extract(data, 'bool DOpenVDBData::hasValidData('),
             extract(common, 'void getGrids('), clip_bodies(overrides)]
    code += [extract(header, 'class WireBoxBuilder')+';',
             extract(common, 'WireBoxBuilder::WireBoxBuilder('),
             extract(common, 'void WireBoxBuilder::add('),
             extract(header, 'class BoundingBoxGeoVP2')+';',
             extract(header, 'class SurfaceGeoVP2')+';',
             'namespace util {',
             header[header.index('template<class TreeType>\n\tclass MinMaxVoxel'):header.index('} // namespace util')],
             '}',
             extract(header, 'template<typename TreeType>\nclass PointGenerator')+';',
             extract(header, 'template<typename LeafManagerType>\ndommy::viewport::PointSamplePlan buildLeafPointSamplePlan('),
             extract(header, 'inline size_t checkedViewportComponentCount('),
             extract(header, 'template<typename GridType>\nclass PointAttributeGenerator')+';',
             header[header.index('template< typename valueType>'):header.index('template<typename GridType>\nclass VectorAttributeGenerator')],
             extract(header, 'template<typename GridType>\nclass VectorAttributeGenerator')+';',
             extract(header, 'class ActiveVoxelGeo\n')+';',
             extract(header, 'class ActiveVoxelGeoVP2')+';',
             extract(header, 'class ActiveVoxelVecVP2')+';',
             extract(source, 'bool getValueBufferLayout('),
             extract(source, 'struct GeometryBufferLayout')+';',
             extract(source, 'bool getLineBufferLayout('),
             extract(source, 'bool getSurfaceBufferLayout(')]
    if sdk:
        # Production scalar/vector dispatch instantiates more types than the
        # visible Float/Vec3f/Vec3d branches. Check their template call shapes too.
        code += ['void instantiateViewportTemplates(){valueBufferObjectVP2 buffer;BufferObject legacy;',
                 'ActiveVoxelGeoVP2 scalar(buffer,false,0,1);ActiveVoxelGeo oldScalar(legacy);',
                 'ActiveVoxelVecVP2 vector(buffer,false,1);']
        for value_type in ('float','double','std::int32_t','std::int64_t'):
            grid_type='openvdb::TypedGrid<'+value_type+',0>'
            code += ['scalar.operator()<'+grid_type+'>(nullptr);oldScalar.operator()<'+grid_type+'>(nullptr);']
        for value_type in ('openvdb::Vec3s','openvdb::Vec3d','openvdb::Vec3i'):
            code += ['vector.operator()<openvdb::TypedGrid<'+value_type+',1>>(nullptr);']
        code += ['}']
    attributes = re.findall(r'MObject DOpenVDBDisplayVP2::(\w+);', source)
    code += [('struct DOpenVDBDisplayVP2 : MPxLocatorNode {' if sdk else 'struct DOpenVDBDisplayVP2 {'),
             'MBoundingBox boundingBox() const;',
             '' if sdk else 'MObject thisMObject()const{return MObject(1000);}',
             ''.join('static MObject '+name+';' for name in attributes), '};']
    for index, name in enumerate(attributes, 1):
        code += ['MObject DOpenVDBDisplayVP2::'+name+(';' if sdk else '{'+str(index)+'};')]
    if not sdk:
        code += ['void initializeAttributes(){'+''.join(
            'names[DOpenVDBDisplayVP2::'+name+'.key]="'+name+'";' for name in attributes)+'}']
    code += ['struct DOpenVDBDisplayVP2Override {MObject mLocatorNode; bool mMultiplierChanged=false;',
             'bool visBBox=false,visNode=false,visLeaf=false,visTile=false,visSurf=false,visPoint=false,visVector=false;',
             'std::vector<lineBufferObjectVP2> mBBoxBuffersVP2,mNodeBuffersVP2,mLeafBuffersVP2,mTileBuffersVP2;',
             'std::vector<valueBufferObjectVP2> mPointBuffersVP2,mVectorBuffersVP2;',
             'std::vector<BufferObjectVP2> mSurfaceBuffersVP2;',
             '\n'.join(re.findall(r'^    (?:float|bool|MColor) m(?:PointSize|VectorScale|DrawHeads|VectorColor) = [^;]+;',
                 (ROOT/'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.h').read_text(), re.M)),
             'void updateDG(); void clearGeometryBuffers(); void clearViewportState();};',
             extract(source, 'struct VdbViewportSettings')+';',
             extract(source, 'MStatus readVdbViewportSettings('),
             extract(source, 'MBoundingBox defaultVdbBoundingBox('),
             extract(source, 'MBoundingBox DOpenVDBDisplayVP2::boundingBox('),
             extract(source, 'void DOpenVDBDisplayVP2Override::clearGeometryBuffers('),
             extract(source, 'void DOpenVDBDisplayVP2Override::clearViewportState('),
             extract(source, 'void DOpenVDBDisplayVP2Override::updateDG(')]
    return '\n'.join(code)


def run(overrides=None, sdk=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-viewport-') as directory:
        temp = Path(directory)
        (temp / 'vdb_viewport.inc').write_text(bodies(overrides, bool(sdk)))
        flags = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++14' if sdk else '-std=c++11', '-Wall','-Wextra','-Wpedantic','-Werror',
            '-Wno-unknown-pragmas','-Wno-unused-function','-Wno-unused-variable',
            '-I'+str(temp),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core'),
            '-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/modules/volume/VDB/Common')]
        if sdk:
            includes = ('MPxLocatorNode','MFnPluginData','MPlug','MGlobal','MVector')
            source = '\n'.join('#include <maya/'+name+'.h>' for name in includes)+\
                '\n#define DOMMY_VDB_VIEWPORT_SDK\n#include "common/utility/DMayaPluginData.h"\n'+\
                '#include "common/utility/DMayaValueValidation.h"\n#include "common/utility/DStatusOperation.h"\n'+\
                '#include "common/utility/DViewportCallbackGuard.h"\n#include "vdb_viewport.inc"\n'
            path = temp/'sdk.cpp'; path.write_text(source)
            flags += ['-isystem',str(Path(sdk)/'include'),'-Wno-deprecated-declarations']
            subprocess.run(flags+['-fsyntax-only',str(path)],check=True,capture_output=capture,text=True)
            subprocess.run(flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(path)],check=True,capture_output=capture,text=True)
            print('VDB full callbacks/settings and wire/surface/point/vector builders: Maya SDK syntax/analysis passed; native OpenVDB/TBB/GPU boundaries are doubles')
            return
        flags += ['-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        executable=temp/'test'
        subprocess.run(flags+[str(ROOT/'tests/cpp/vdb_viewport_tests.cpp'),'-o',str(executable)],
                       check=True,capture_output=capture,text=True)
        return subprocess.run([str(executable)],check=True,capture_output=capture,text=True,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))


def mutations(settings_only=False):
    originals={path:(ROOT/path).read_text() for path in (DISPLAY,UTILITY,HEADER)}
    cases=[
        ('cached point size publication',DISPLAY,'    mPointSize = settings.pointSize;', ''),
        ('cached vector scale publication',DISPLAY,'    mVectorScale = settings.vectorScale;', ''),
        ('cached vector heads publication',DISPLAY,'    mDrawHeads = settings.drawHeads;', ''),
        ('cached vector color publication',DISPLAY,'    mVectorColor = settings.vectorColor;', ''),
        ('cached failed visibility reset',DISPLAY,
         '    visBBox = visNode = visLeaf = visTile = visSurf = visPoint = visVector = false;', ''),
        ('cached point size admission',DISPLAY,'if (!std::isfinite(settings.pointSize))', 'if (false)'),
        ('cached vector scale admission',DISPLAY,'!std::isfinite(settings.vectorScale))', 'false)'),
        ('cached vector color admission',DISPLAY,'!dommy::maya::isFiniteColor(settings.vectorColor) ||', ''),
        ('wire finite admission',UTILITY,'!dommy::numeric::checkedFloatCast(point[axis], points[component])',
         '(points[component]=static_cast<float>(point[axis]), false)'),
        ('wire output-range admission',UTILITY,'componentEnd > mPoints->size()', 'false'),
        ('wire late failure writes',UTILITY,'point = mXForm->indexToWorld(point);',
         '(*mPoints)[componentBase] = 99.0f; point = mXForm->indexToWorld(point);'),
        ('empty bounds draw',HEADER,'if (bounds.empty()) return;', ''),
        ('surface iso admission',HEADER,'!std::isfinite(mIso) ||', ''),
        ('surface world admission',HEADER,'!dommy::numeric::canRepresentAsFloat(worldBounds.max()[axis])','false'),
        ('surface point admission',HEADER,'if (!dommy::maya::isFloatRepresentablePoint(points[n])) return;', ''),
        ('surface early publication',HEADER,'openvdb::tools::PolygonPoolList& polygonPools =',
         'mBuffer->points = points; openvdb::tools::PolygonPoolList& polygonPools ='),
        ('surface GPU admission',DISPLAY,'!dommy::maya::isFloatRepresentablePoint(it->points[vertex])','false'),
        ('wire GPU admission',DISPLAY,'if (!std::isfinite(it->points[component])) return false;', ''),
        ('settings failure bypass',DISPLAY,'if (readVdbViewportSettings(mLocatorNode, settings) != MS::kSuccess)',
         'if (readVdbViewportSettings(mLocatorNode, settings) != MS::kSuccess && false)'),
        ('DG payload validation',DISPLAY,'!inputVdb || !inputVdb->hasValidData()) return;', '!inputVdb) return;'),
        ('bounds payload validation',DISPLAY,'!inputVdb || !inputVdb->hasValidData()) return defaultVdbBoundingBox();',
         '!inputVdb) return defaultVdbBoundingBox();'),
        ('DG clears stale data',DISPLAY,'mMultiplierChanged = true;\n    clearViewportState();', 'mMultiplierChanged = true;'),
        ('settings enum admission',DISPLAY,'if (settings.colorType < 0 || settings.colorType > 1)', 'if (false)'),
        ('sample point float admission',HEADER,'!dommy::numeric::checkedFloatCast(position[axis], point[axis])',
         '(point[axis]=static_cast<float>(position[axis]), false)'),
        ('sample coordinate identity',HEADER,'(*mCoordinates)[index] = coordinate;',
         '(*mCoordinates)[index] = openvdb::Coord(99);'),
        ('sample coordinate storage',HEADER,'mCoordinates->size() != mIndices->size() ||', ''),
        ('sample ranges overlap',HEADER,'(*mIndexMap)[leaf] != nextOffset ||', ''),
        ('sample leaf count postcondition',HEADER,
         'if (visitedCount != activeVoxels || index - (*mIndexMap)[n] != sampleCount)', 'if (false)'),
        ('nonfinite unsampled scalar',HEADER,'!std::isfinite(static_cast<double>(value))', 'false'),
        ('scalar gradient coordinate headroom',HEADER,'!dommy::vdb::filterCoordinate(ijk[axis], 1)', 'false'),
        ('vector float admission',HEADER,'!dommy::numeric::checkedFloatCast(pColor[axis], components[axis])',
         '(components[axis]=static_cast<float>(pColor[axis]), false)'),
        ('point CPU early publication',HEADER,
         'PointAttributeGenerator<GridType> attributeGen(\n\t\t\tcoordinates, generatedBuffer.colors',
         'mPointBuffer->points = generatedBuffer.points; PointAttributeGenerator<GridType> attributeGen(\n\t\t\tcoordinates, generatedBuffer.colors'),
        ('point GPU finite admission',DISPLAY,'!std::isfinite(it->points[component]) ||', 'false ||'),
        ('vector GPU finite admission',DISPLAY,'!std::isfinite(it->normals[component]) ||', 'false ||'),
        ('point color GPU finite admission',DISPLAY,'!std::isfinite(it->colors[component]))', 'false)'),
    ]
    if settings_only:
        cases=[case for case in cases if case[0].startswith('cached ')]
    for label,path,old,new in cases:
        assert old in originals[path],label
        try:
            run({path:originals[path].replace(old,new)},capture=True)
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('runtime error:','ERROR: AddressSanitizer')),'Compile failure is not detection: '+label
            print('Detected mutation: '+label,flush=True)
        else:
            raise AssertionError('Undetected mutation: '+label)


if __name__ == '__main__':
    if '--settings-mutations' in sys.argv: mutations(settings_only=True)
    elif '--mutations' in sys.argv: mutations()
    elif '--maya-sdk' in sys.argv: run(sdk=sys.argv[sys.argv.index('--maya-sdk')+1])
    else: run()
