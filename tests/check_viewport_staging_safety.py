#!/usr/bin/env python3
"""Test complete populate callbacks and the production traversal visitors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from viewport_settings_fixture import settings_bodies

ROOT = Path(__file__).resolve().parent.parent
PATHS = ('src/modules/mesh/DMesh/Display/DMeshDisplay',
         'src/modules/mesh/DMesh/Display/DMMarker',
         'src/modules/point/DPoint/Duplicator/DPInstancer',
         'src/modules/point/DPoint/Display/DPointDisplay',
         'src/modules/point/DPoint/Display/DPMarker',
         'src/modules/curve/DCurve/Display/DCurveDisplay',
         'src/modules/curve/DCurve/Display/DCMarker',
         'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2')

def bodies(overrides=None, sdk=False):
    overrides = overrides or {}
    sources = [overrides.get(path+'.cpp', (ROOT/(path+'.cpp')).read_text()) for path in PATHS]
    assert 'normalTransform = (*it)->getTransformMatrix()' in sources[0]
    assert '.inverse().transpose()' not in sources[0], 'Display normal must use the shared original-matrix boundary'
    for source in sources:
        assert not re.search(r'for\s*\([^;]*(?:face|edge|eIt)\.reset\(', source), 'Unchecked viewport iterator returned'
        assert not any(phrase in source for phrase in
                       ('numberOfVertexRequirments','requirmentNumber',
                        'do nothing for stuff we don\'t understand')), 'Legacy sample traversal/prose returned'
    for path,source in zip(PATHS,sources):
        callback=extract(source,'void '+Path(path).name+'Override::populateGeometry(')
        assert callback.count('dommy::viewport::forEachVertexRequirement(')==1
        assert '.getDescriptor(' not in callback and 'if (!streamsReady) return;' in callback
    for source in sources[:2]:
        assert 'stat = dommy::maya::visitMeshElements(' in source
        assert 'stat = dommy::maya::visitMeshPolygons(' in source
    ass_stage=extract(sources[2], 'void DPInstancerOverride::updateDGImpl(')
    assert 'readAssBoundingBoxCache(filename, dData.assFile, dData.minPt, dData.maxPt)' in ass_stage
    assert not re.search(r'dData\.assFile\s*=', ass_stage), 'Ass cache key published before validated read'
    values = (ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
    code = ['namespace dommy { namespace maya {']
    code += [extract(values, 'inline bool '+name+'(') for name in
             ('isFiniteColor','isFiniteMatrix','isFinitePoint','isFiniteVector','isFloatRepresentablePoint',
              'isFloatRepresentableVector','normalizedVector')]
    code += ['}}', extract(sources[0], 'bool transformDisplayNormal(')]
    if not sdk:
        report=(ROOT/'src/core/common/utility/DMayaEntryPointGuard.h').read_text()
        code += ['namespace dommy {namespace maya {',
                 extract(report,'inline MStatus reportEntryPointException(').replace('reportEntryPointException(', 'productionReportEntryPointException(',1), '}}']
    render_owner = overrides.get('src/core/common/utility/DViewportRenderItem.h',
                                 (ROOT/'src/core/common/utility/DViewportRenderItem.h').read_text())
    code += ['namespace dommy {namespace maya {void reportViewportCallbackException(const char*,const char*,const std::exception_ptr&) noexcept;}}',
             'namespace dommy {namespace viewport {',
             extract(render_owner, 'struct RenderItemDeleter')+';',
             re.search(r'using PendingRenderItem\s*=[^;]+;', render_owner).group(0),
             extract(render_owner, 'class RenderItemUpdateScope')+';',
             extract(render_owner, 'class StockShaderAcquisition')+';',
             extract(render_owner, 'class StockShaderRelease')+';',
             extract(render_owner, 'class RenderItemShaderUpdate')+';', '}}']
    ass_limits=re.findall(r'const (?:std::size_t|unsigned long long) kMaxAssToc\w+\s*=[^;]+;', sources[2])
    assert len(ass_limits) == 3
    code += ass_limits
    code += [extract(sources[2], signature) for signature in
             ('bool readAssBoundingBoxFromStream(', 'bool loadAssBoundingBoxFromFile(',
              'bool readAssBoundingBoxCache(')]
    code += [extract(sources[2], signature) for signature in
             ('bool validConnectionIndices(', 'bool resizeInstanceBuffers(', 'bool buildInstanceBuffers(')]
    code += [extract(sources[2], 'void reportInstancerShaderParameters(')]
    bbox_source = overrides.get('src/core/common/object/DBBoxOp.cpp',
                               (ROOT/'src/core/common/object/DBBoxOp.cpp').read_text())
    mean_body=extract(bbox_source, 'void CalBBoxFromPointArray(').replace('DPointList', 'BoundsPointList')
    if not sdk:
        code += ['struct ObservedMeanAccumulator : dommy::numeric::detail::PointMeanAccumulator {'
                 'static std::size_t additions; bool add(double value) {++additions;'
                 'return dommy::numeric::detail::PointMeanAccumulator::add(value);}};'
                 'std::size_t ObservedMeanAccumulator::additions=0U;']
        mean_body=mean_body.replace('dommy::numeric::detail::PointMeanAccumulator', 'ObservedMeanAccumulator')
    code += ['using DPoint=MFloatVector; using BoundsPointList=std::vector<DPoint>;',
             extract(bbox_source, 'MBoundingBox pointBoundsBox('), mean_body,
             extract(bbox_source, 'void CalBBoxFromPointArray(const DPointList& pts, MBoundingBox& bbox)').replace('DPointList', 'BoundsPointList'),
             extract(bbox_source, 'MStatus DGenerateDisplayBBoxDataFromMinMaxPoint(')]
    point_data=overrides.get('src/core/DDataUtility/Data/DPData.cpp',
                            (ROOT/'src/core/DDataUtility/Data/DPData.cpp').read_text())
    code += [extract(point_data, 'MBoundingBox DPData::calculateBoundingBox(').replace(
        'DPData::calculateBoundingBox() const', 'calculatePointBounds(const BoundsPointList& pointArray)').replace(
        'numberOfPoints()', 'pointArray.size()')]
    names = {}
    for source in sources:
        names.update(re.findall(r'(?:static\s+)?const MString (\w+)\s*=\s*"([^"]+)";', source))
    code += ['static const MString '+name+' = "'+value+'";' for name,value in names.items()]
    transform=(ROOT/'src/core/common/object/DTransformOp.h').read_text()
    code += [transform[transform.index('enum {'):transform.index('struct DTransformParams')],
             extract(transform,'struct DTransformParams')+';']
    # GPU packing needs no OpenVDB algorithms; use the real buffer/layout bodies
    # with API-shaped containers. SDK mode uses actual Maya MPoint/MGeometry.
    code += ['using uint=unsigned int;',
             'struct BufferObjectVP2 {std::vector<MPoint> points,normals; std::vector<unsigned int> indexArray;};',
             'struct lineBufferObjectVP2 {std::vector<float> points,colors; std::vector<unsigned int> indexArray;};',
             'struct valueBufferObjectVP2 {std::vector<float> points,normals,colors,valueArray; std::vector<unsigned int> indexArray;};']
    vdb=sources[7]
    code += [extract(vdb, 'struct GeometryBufferLayout')+';']
    code += [extract(vdb, signature) for signature in
             ('bool getValueBufferLayout(', 'bool getLineBufferLayout(',
              'bool getSurfaceBufferLayout(', 'void copyLineComponents(', 'void copyLineIndices(')]
    for path,source in zip(PATHS,sources):
        name = Path(path).name+'Override'
        header = overrides.get(path+'.h', (ROOT/(path+'.h')).read_text())
        if name != 'DOpenVDBDisplayVP2Override':
            draw=strip_comments_and_literals(extract(header,'struct DrawData'))
            assert not re.search(r'\b(?:bool|float|short|size_t|unsigned int|DM_DISPLAY_TYPE|DP_DISPLAY_VALUE_TYPE|DComponentType|DP_INST_COPY_TYPE|MColor|MPoint)\s+\w+\s*;',draw), name+' has an uninitialized drawing field'
        fields = (extract(header,'struct DrawData')+'; '+re.search(r'DrawData\s+dData\s*[^;]*;', header).group(0)
                  if name != 'DOpenVDBDisplayVP2Override' else
                  'std::vector<lineBufferObjectVP2> mBBoxBuffersVP2,mNodeBuffersVP2,mLeafBuffersVP2,mTileBuffersVP2;'
                  'std::vector<valueBufferObjectVP2> mPointBuffersVP2,mVectorBuffersVP2;'
                  'std::vector<BufferObjectVP2> mSurfaceBuffersVP2;')
        dirty = 'mMultiplierChanged' if name in ('DPointDisplayOverride','DPMarkerOverride','DOpenVDBDisplayVP2Override') else 'mDataChanged'
        shaders = re.findall(r'MHWRender::MShaderInstance\s*\*\s*(\w+)\s*;',
                             strip_comments_and_literals(header))
        constructor = extract(source, name+'::'+name+'(')
        stock_types = dict((field,kind) for kind,field in re.findall(
            r'shaderAcquisition.acquire\(MHWRender::MShaderManager::(\w+), (\w+)\)', constructor))
        assert set(stock_types) == set(shaders), name+' shader inventory mismatch'
        shader_fields = ''.join('MHWRender::MShaderInstance* '+shader+'=nullptr;' for shader in shaders)
        shader_fields += 'std::vector<MHWRender::MShaderInstance*> shaderPointers() const {return {'+','.join(shaders)+'};}'
        shader_fields += 'std::vector<int> shaderTypes() const {return {'+','.join(
            'int(MHWRender::MShaderManager::'+stock_types[shader]+')' for shader in shaders)+'};}'
        shader_fields += 'void setAllShaders(MHWRender::MShaderInstance* shader) {'+''.join(
            field+'=shader;' for field in shaders)+'}'
        if name == 'DOpenVDBDisplayVP2Override':
            shader_fields += 'MObject mLocatorNode;'+''.join('bool '+field+'=false;' for field in
                ('visBBox','visNode','visLeaf','visTile','visSurf','visPoint','visVector'))
            shader_fields += '\n'.join(re.findall(r'^    (?:float|bool|MColor) m(?:PointSize|VectorScale|DrawHeads|VectorColor) = [^;]+;', header, re.M))
        stream_dirty = extract(header, 'bool isStreamDirty(')
        dirty_expression = re.search(r'return\s+([^;]+);', stream_dirty).group(1)
        code += ['struct '+name+' {', fields, 'bool '+dirty+'=true; bool mShaderUpdateFailed=false;',
                 shader_fields,
                 'bool isDirty() const {return '+dirty_expression+';} void setDirty(bool value) {'+dirty+'=value;}',
                 'void populateGeometry(const MHWRender::MGeometryRequirements&, const MHWRender::MRenderItemList&, MHWRender::MGeometry&);',
                 'void updateRenderItems(const MDagPath&,MHWRender::MRenderItemList&);',
                 ('MObject mInstancerNode; MStatus evalParms(DTransformParams&); void clearDrawData() noexcept; void updateDGImpl(); void updateDG(); void connectionAdmission();'
                  if name == 'DPInstancerOverride' else ''),
                 ('MObject mDGNode; void updateDG();'
                  if name == 'DPointDisplayOverride' else ''),
                 '};', extract(source,'void '+name+'::populateGeometry('),
                 'DOMMY_VIEWPORT_VOID_CATCH("'+name+'", "populateGeometry")']
        # Compile the complete real constructors/destructors separately from the
        # copyable callback fixtures. Only the native base is API-shaped on host;
        # SDK mode parses these bodies against the real MPxGeometryOverride base.
        lifecycle='Lifecycle'+name
        initializer=constructor[:constructor.index('{')]
        members=re.findall(r',\s*(\w+)\s*\(',initializer)
        assert set(shaders).issubset(members), name+' missing shader initialization'
        lifecycle_fields=[]
        for member in members:
            kind=('MHWRender::MShaderInstance*' if member in shaders else
                  'bool' if member == dirty or member.startswith('vis') else 'MObject')
            lifecycle_fields.append(kind+' '+member+';')
        destructor=extract(source,name+'::~'+name+'(')
        assert set(re.findall(r'shaderRelease.release\((\w+)\)',destructor)) == set(shaders)
        assert not re.search(r'->(?:getStockShader|releaseShader)\(',constructor+destructor)
        release_order=re.findall(r'shaderRelease.release\((\w+)\)',destructor)
        lifecycle_methods=shader_fields[shader_fields.index('std::vector<MHWRender::MShaderInstance*> shaderPointers()'):]
        # Do not duplicate VDB's native members in the accessors suffix.
        lifecycle_methods=lifecycle_methods.split('MObject mLocatorNode;')[0]
        code += ['struct '+lifecycle+' : MHWRender::MPxGeometryOverride {',
                 fields, '\n'.join(lifecycle_fields), 'bool mShaderUpdateFailed=false;',
                 lifecycle_methods,
                 'std::vector<MHWRender::MShaderInstance*> releaseOrder() const {return {'+','.join(release_order)+'};}',
                 lifecycle+'(const MObject&); ~'+lifecycle+'();',
                 ('void clearGeometryBuffers();' if name == 'DOpenVDBDisplayVP2Override' else ''),
                 lifecycle+'(const '+lifecycle+'&)=delete; '+lifecycle+'& operator=(const '+lifecycle+'&)=delete;',
                 '};', constructor.replace(name+'::'+name+'(',lifecycle+'::'+lifecycle+'(',1),
                 destructor.replace(name+'::~'+name+'(',lifecycle+'::~'+lifecycle+'(',1)]
        if name == 'DOpenVDBDisplayVP2Override':
            code += [extract(source,'void '+name+'::clearGeometryBuffers(').replace(
                name+'::',lifecycle+'::',1)]
    visitors = [extract(sources[0], '[&](MItMeshEdge& edge) -> MStatus'),
                extract(sources[0], '[&](MItMeshPolygon& face) -> MStatus'),
                extract(sources[1], '[&](MItMeshPolygon& face) -> MStatus'),
                extract(sources[1], '[&](MItMeshEdge& edge) -> MStatus')]
    for index,visitor in enumerate(visitors):
        marker = index >= 2
        kind = 'MItMeshEdge' if index in (0,3) else 'MItMeshPolygon'
        draw = 'DMMarkerOverride' if marker else 'DMeshDisplayOverride'
        code += ['MStatus traversal'+str(index)+'('+draw+'::DrawData& dData, std::size_t count, bool selected) {',
                 'MStatus stat; MObject object; '+kind+' iterator(object, &stat); if (!stat) return stat;',
                 'std::size_t vOffset=0U, nOffset=0U, totalPoint=0U;',
                 'std::size_t nextWireIndexCount=count*2U;' if index == 0 else '',
                 'std::vector<bool> idsList(count,selected); MMatrix transform, normalTransform; bool applyTransform=false; MFnMesh meshFn;']
        variable = 'eIt' if index == 0 else ('face' if index in (1,2) else 'edge')
        code += [kind+'& '+variable+'=iterator;',
                 'return dommy::maya::'+('visitMeshPolygons' if index in (1,2) else 'visitMeshElements')+'(iterator,count,'+visitor+');', '}']
    if sdk:
        update=extract(sources[2], 'void DPInstancerOverride::updateDGImpl(')
        attributes=sorted(set(re.findall(r'DPInstancer::(\w+)',update+extract(sources[2],'MStatus DPInstancerOverride::evalParms('))))
        code += ['struct DPInstancer {'+''.join('static MObject '+a+';' for a in attributes)+'};',
                 'template <typename T> MStatus readPlugValue(const MObject&,const MObject&,T&);',
                 'template <typename T> MStatus readPlugValue(const MPlug&,T&);',
                 'struct DPData {static const MTypeId id; bool hasValidPointTopology() const; std::size_t numberOfPoints() const;'
                 'std::vector<MVector>::const_iterator beginPt() const; std::vector<MVector>::const_iterator endPt() const;'
                 'const std::vector<MVector>* getPositionPtrConst() const;};',
                 'namespace dommy {namespace maya {template <typename T> MStatus getPluginDataFromPlug(const MPlug&, const MTypeId&, MObject&, const T*&);}}',
                 'void EchoWarning(const char*,const char*);',
                 'MStatus DGenerateTransformMatrixFromDP(const DPData*, const DTransformParams&, std::vector<MMatrix>&);',
                 extract(sources[2], 'MStatus readDataHandle('),
                 extract(sources[2], 'void DPInstancerOverride::clearDrawData('), update]
        code += ['using DPointList=std::vector<MVector>;',
                 'struct DPointDisplay {static MObject inDP,aSolidColor,aPointSize;};',
                 'struct DOpenVDBDisplayVP2 {static MObject aVectorColor,aIsDrawHeads,aVectorScale,aPointSize;};',
                 'template <typename T> MStatus readPlugValue(const MObject&,const MObject&,T&);',
                 'std::size_t DGetGlobalProcessNum(std::size_t);',
                 'namespace tbb {template <typename T> struct blocked_range {'
                 'blocked_range(T,T,std::size_t); T begin() const; T end() const;};'
                 'template <typename R,typename F> void parallel_for(const R&,F) {}}',
                 'namespace dommy {namespace maya {void reportViewportCallbackException(const char*,const char*,const std::exception_ptr&) noexcept;}}']
    code += [extract(sources[3], 'void DPointDisplayOverride::updateDG(')]
    code += [settings_bodies(ROOT, PATHS, sources, sdk)]
    arnold_path='src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp'
    arnold=overrides.get(arnold_path,(ROOT/arnold_path).read_text())
    assert 'new std::vector<MMatrix>(instCount' not in arnold
    code += ['struct DPInstancerTranslatorAi {MPlug FindMayaPlug(const char*) const;'
             'MObject GetMayaObject() const; MStatus evalAnimMatrixParms(DTransformParams&);};',
             extract(arnold,'MStatus DPInstancerTranslatorAi::evalAnimMatrixParms(')]
    curve_color = extract(sources[5][sources[5].index('void DCurveDisplayOverride::updateDG('):],
                          'if (readPlugValue(mCurveDisplayNode, DCurveDisplay::aSolidColor, dData.solidColor)')
    code += ['void curveSolidColor(MColor& color) {MObject mCurveDisplayNode;'
             'struct {MColor solidColor;} dData{};'+curve_color+' color=dData.solidColor;}']
    for path, source in zip(PATHS, sources):
        name=Path(path).name+'Override'
        callback=extract(source, 'void '+name+'::updateRenderItems(')
        acquisition = dict((field,kind) for kind,field in re.findall(
            r'shaderAcquisition.acquire\(MHWRender::MShaderManager::(\w+), (\w+)\)', callback))
        constructor = extract(source, name+'::'+name+'(')
        assert acquisition == dict((field,kind) for kind,field in re.findall(
            r'shaderAcquisition.acquire\(MHWRender::MShaderManager::(\w+), (\w+)\)', constructor)), name+' changes a stock shader identity'
        assert 'RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);' in callback
        assert callback.count('RenderItemShaderUpdate shaderUpdate(') == (5,9,3,1,7,2,9,7)[PATHS.index(path)]
        assert not re.search(r'->(?:setParameter|setShader|enable)\(', callback)
        assert re.search(r'\)\s*try\s*\{', callback), name+' lacks an exception boundary'
        suffix=source[source.index(callback)+len(callback):]
        assert re.match(r'\s*DOMMY_VIEWPORT_VOID_CATCH\(\s*"'+name+
                        r'"\s*,\s*"updateRenderItems"\s*\)', suffix), name+' catch guard is missing'
        code += [callback,
                 'DOMMY_VIEWPORT_VOID_CATCH("'+name+'", "updateRenderItems")']
    return '\n'.join(code)

PREFIX = '''#include <vector>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <array>
#include <limits>
#include <sstream>
#include "common/utility/DAttributeBlendMath.h"
#include "common/file/DFileSize.h"
#include "common/file/DBoundedTextRead.h"
#include "common/file/DNumericTextRead.h"
#include "common/utility/DPointMeanAccumulator.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DMayaMeshTraversal.h"
using DM_DISPLAY_TYPE=int; using DP_DISPLAY_VALUE_TYPE=int;
using DComponentType=int; using DP_INST_COPY_TYPE=short;
enum {DM_DPY_HARD=1,DM_DPY_SMOOTH=2,DM_DPY_FLAT=0,DM_DISPLAY_COLOR_SOLID=0,DM_DISPLAY_COLOR_RANDOM=1,
      DM_DISPLAY_TYPE_COUNT=3,DM_DISPLAY_COLOR_TYPE_COUNT=2,
      DP_VAL_INT2=3,DP_VAL_FLOAT2=4,DP_VAL_INT=5,DP_VAL_FLOAT=6,DP_VAL_INT3=7,DP_VAL_FLOAT3=8};
enum {D_COMP_PRIM=0,D_COMP_POINT=1,D_COMP_EDGE=2,D_COMP_FACE=3};
enum {DP_INST_POINT=1,DP_INST_BBOX=2,DP_INST_WIRE=3,DP_INST_SHADE=4};
enum {DP_INST_COPY_MESH=0,DP_INST_COPY_ASS=1,DP_INST_COPY_ASS_SEQUENCE=2};
#define DOMMY_VIEWPORT_VOID_CATCH(...) catch (...) {}
#define DP_INSTANCER_DEBUG 0
#define DOMMY_RETURN_IF_FAILURE(expression) do {const MStatus dommyStatus=(expression);if(!dommyStatus)return dommyStatus;}while(false)
#define DOMMY_ENABLE_DEBUG_LOGS 1
void recordDebugMessage(const std::string&);
#define DOMMY_MAYA_DEBUG(owner, expression) do {std::ostringstream message; message << expression; recordDebugMessage(message.str());} while(false)
'''

def run(source, capture=False, sdk=None, buffer_source=None, debug_enabled=True,
        text_reader_source=None):
    with tempfile.TemporaryDirectory(prefix='dommy-viewport-safety-') as directory:
        temp = Path(directory)
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-Wno-unknown-pragmas','-Wno-unused-function','-Wno-unused-variable',
                 '-Wno-unused-parameter','-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        prefix=PREFIX if buffer_source is None else PREFIX.replace(
            '#include "common/utility/DViewportBuffer.h"', buffer_source)
        if text_reader_source is not None:
            prefix=prefix.replace('#include "common/file/DBoundedTextRead.h"', text_reader_source)
        if not debug_enabled:
            prefix=prefix.replace('#define DOMMY_ENABLE_DEBUG_LOGS 1',
                                  '#define DOMMY_ENABLE_DEBUG_LOGS 0')
        if sdk:
            flags[0] = '-std=c++14'  # Maya 2025's MSharedPtr header requires C++14.
            headers = ('MStatus','MMatrix','MString','MGlobal','MPoint','MVector','MFloatVector','MBoundingBox',
                       'MFloatVectorArray','MIntArray','MPointArray','MItMeshEdge',
                       'MItMeshPolygon','MFnMesh','MColor','MPxGeometryOverride',
                       'MFnDependencyNode','MFnDagNode','MPlugArray','MFileObject','MShaderManager',
                       'MHWGeometryUtilities','MFnNumericData','MViewport2Renderer')
            code='\n'.join('#include <maya/'+h+'.h>' for h in headers)+'\n'+prefix+\
                '#include "common/utility/DMayaPlugDataHandle.h"\n'+source
            path=temp/'sdk.cpp'; path.write_text(code)
            flags += ['-isystem',str(Path(sdk)/'include'),'-Wno-deprecated-declarations']
            subprocess.run(compiler+flags+['-fsyntax-only',str(path)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(path)],check=True)
            print('All 8 constructors/destructors and populateGeometry/updateRenderItems callbacks, Ass parser/cache, DPointDisplay updateDG, five settings/connection admission stages, DPInstancer DG/evalParms and Arnold transform parameter reader: SDK syntax/analysis passed (project data/transform/TBB/OpenVDB/Alembic/Arnold boundaries declared)')
            return
        (temp/'maya').mkdir()
        for header in ('MHWGeometry','MFnMesh','MItMeshPolygon'):
            (temp/'maya'/(header+'.h')).write_text('#include "'+str(ROOT/'tests/cpp/viewport_safety_host.h')+'"\n')
        (temp/'viewport_staging.inc').write_text(prefix+source)
        flags += ['-I'+str(temp),'-I'+str(ROOT/'tests/stubs'),
                  '-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/viewport_staging_tests.cpp'),'-o',str(binary)],check=True,
                       capture_output=capture,text=True)
        environment=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        return subprocess.run([str(binary),str(temp)],check=True,env=environment,capture_output=capture,text=True)

def detect_mutation(label, overrides=None, buffer_source=None, text_reader_source=None):
    try:
        run(bodies(overrides),capture=True,buffer_source=buffer_source,
            text_reader_source=text_reader_source)
    except subprocess.CalledProcessError as error:
        assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
            ('runtime error:', 'ERROR: AddressSanitizer')), 'Compile failure is not detection: '+label
        print('Detected mutation: '+label,flush=True)
    else:
        raise AssertionError('Undetected mutation: '+label)

def stream_requirement_mutations():
    header=(ROOT/'src/core/common/utility/DViewportBuffer.h').read_text()
    for label,old,new in (
        ('empty stream list rejected','if (count < 0) return false;','if (count <= 0) return false;'),
        ('invalid stream count accepted','if (count < 0) return false;',''),
        ('descriptor failure ignored','!streams.getDescriptor(index, stream)',
         '(streams.getDescriptor(index, stream), false)'),
        ('stream visitor rejection ignored','!visitor(stream)','(visitor(stream), false)'),
        ('stream order reversed','getDescriptor(index, stream)','getDescriptor(count-1-index, stream)'),
        ('last stream skipped','index < count','index + 1 < count'),
    ):
        assert old in header,label
        detect_mutation(label,buffer_source=header.replace(old,new,1))


def arnold_input_mutations():
    path='src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp'
    source=(ROOT/path).read_text()
    for label,old,new in (
        ('Arnold typed data used as attribute', 'matrixPlug.attribute(&status)', 'matrixPlug.asMObject(&status)'),
        ('Arnold parameter transaction removed', 'DTransformParams parms = output;', 'DTransformParams& parms = output;'),
        ('Arnold borrowed-data flag substituted', 'parms.mAnimationInputIsData = false;', 'parms.mAnimationInputIsData = true;'),
    ):
        assert old in source,label
        detect_mutation(label,{path:source.replace(old,new,1)})

def normal_mutations():
    path=PATHS[0]+'.cpp';source=(ROOT/path).read_text()
    helper=extract(source,'bool transformDisplayNormal(')
    for label,old,new in (
        ('display zero-normal rejection removed',
         'if (source.x == 0.0 && source.y == 0.0 && source.z == 0.0) return false;', ''),
        ('display inverse-transpose replaced by direction transform',
         'checkedTransformNormal(source, transform, normal)',
         'checkedTransformDirection(source, transform, normal)'),
    ):
        assert old in helper,label
        detect_mutation(label,{path:source.replace(helper,helper.replace(old,new,1),1)})

def shader_mutations():
    path=PATHS[2]+'.cpp'
    source=(ROOT/path).read_text()
    diagnostic=extract(source,'void reportInstancerShaderParameters(')
    for label,old,new in (
        ('diagnostic null shader guard removed','if (!shader) return;',''),
        ('diagnostic enum upper bound removed',
         'static_cast<unsigned int>(type) < sizeof(typeNames) / sizeof(typeNames[0])','true'),
        ('diagnostic failure report removed',
         'dommy::maya::reportViewportCallbackException(\n            "DPInstancerOverride", "shader diagnostics", std::current_exception());',''),
    ):
        assert old in diagnostic,label
        detect_mutation(label,{path:source.replace(diagnostic,diagnostic.replace(old,new,1),1)})
    path='src/core/common/utility/DViewportRenderItem.h'
    source=(ROOT/path).read_text()
    acquisition=extract(source,'class StockShaderAcquisition')
    for label,old,new in (
        ('cached shader replaced','if (shader) return;',''),
        ('missing shader retry flag lost','if (!shader) retry_ = true;',''),
        ('shader manager queried repeatedly','if (!queriedManager_) {','if (true) {'),
        ('shader acquisition exception report removed',
         'maya::reportViewportCallbackException(\n                ownerName_, "acquireStockShader", std::current_exception());',''),
    ):
        assert old in acquisition,label
        detect_mutation(label,{path:source.replace(acquisition,acquisition.replace(old,new,1),1)})

def lifecycle_mutations():
    path=PATHS[2]+'.cpp';source=(ROOT/path).read_text()
    parser=extract(source,'bool readAssBoundingBoxFromStream(')
    for label,old,new in (
        ('Ass midpoint overflow restored',
         'if (!dommy::attribute::checkedLinearBlend(parsedMin[axis], parsedMax[axis], 0.5, center))\n            return false;',
         'center = parsedMin[axis] + (parsedMax[axis] - parsedMin[axis]) * 0.5;'),
        ('Ass float extent validation removed',
         'if (!dommy::numeric::canRepresentAsFloat(parsedMin[axis]) ||\n            !dommy::numeric::canRepresentAsFloat(parsedMax[axis])) return false;', ''),
        ('Ass partial bounds published',
         'parsedMax[axis] -= center;', 'parsedMax[axis] -= center; minPt=parsedMin; maxPt=parsedMax;'),
        ('Ass malformed read admitted','if (!foundBounds || in.bad()) {','if ((!foundBounds && false) || in.bad()) {'),
    ):
        assert old in parser,label
        detect_mutation(label,{path:source.replace(parser,parser.replace(old,new,1),1)})
    cache=extract(source,'bool readAssBoundingBoxCache(')
    for label,old,new in (
        ('Ass failed cache key retained','cachedFilename.clear();',''),
        ('Ass filename cached before parsing',
         'if (filename == cachedFilename) return true;',
         'cachedFilename=filename; if (filename == cachedFilename) return true;'),
        ('Ass cache bypasses missing file admission',
         'const std::string tocFilename = filename + "toc";',
         'if (filename==cachedFilename) return true; const std::string tocFilename = filename + "toc";'),
        ('Ass invalid candidate cache admitted',
         'if (!loadAssBoundingBoxFromFile(tocFilename, candidateMin, candidateMax)) {',
         'if ((loadAssBoundingBoxFromFile(tocFilename, candidateMin, candidateMax), false)) {'),
    ):
        assert old in cache,label
        detect_mutation(label,{path:source.replace(cache,cache.replace(old,new),1)})
    path='src/core/common/utility/DViewportRenderItem.h';source=(ROOT/path).read_text()
    release=extract(source,'class StockShaderRelease')
    for label,old,new in (
        ('released shader pointer retained','shader = nullptr;',''),
        ('release manager queried repeatedly','if (!queriedManager_) {','if (true) {'),
        ('shader cleanup report omitted',
         'maya::reportViewportCallbackException(\n                ownerName_, "releaseStockShader", std::current_exception());',''),
        ('shader cleanup exception escapes',
         'maya::reportViewportCallbackException(\n                ownerName_, "releaseStockShader", std::current_exception());',
         'maya::reportViewportCallbackException(ownerName_, "releaseStockShader", std::current_exception()); std::terminate();'),
    ):
        assert old in release,label
        detect_mutation(label,{path:source.replace(release,release.replace(old,new,1),1)})

def bounds_mean_mutations():
    path='src/core/common/object/DBBoxOp.cpp';source=(ROOT/path).read_text()
    body=extract(source,'void CalBBoxFromPointArray(')
    checked_mean='if (!sums[axis].mean(static_cast<std::uint32_t>(bounds.count()), mean[axis]))'
    assert checked_mean in body
    midpoint=body.replace(checked_mean,
        'if ((mean[axis]=(bounds.minimum(axis)+bounds.maximum(axis))/2.0, false))')
    detect_mutation('vertex mean replaced by box midpoint',{path:source.replace(body,midpoint,1)})
    naive=body.replace('std::array<dommy::numeric::detail::PointMeanAccumulator, 3> sums;',
                       'std::array<double, 3> sums = {};')
    naive=naive.replace('if (!sums[axis].add(static_cast<double>((*point)[axis])))',
                        'if ((sums[axis] += static_cast<double>((*point)[axis]), false))')
    naive=naive.replace(checked_mean,
                        'if ((mean[axis]=sums[axis]/static_cast<double>(bounds.count()), false))')
    detect_mutation('vertex mean loses cancellation residual',{path:source.replace(body,naive,1)})

def bounds_only_mutations():
    path='src/core/DDataUtility/Data/DPData.cpp';source=(ROOT/path).read_text()
    body=extract(source,'MBoundingBox DPData::calculateBoundingBox(')
    old='CalBBoxFromPointArray(pointArray, bounds);'
    assert old in body
    detect_mutation('point-data bounds unnecessarily calculate mean',
        {path:source.replace(body,body.replace(old,'DPoint ignored; CalBBoxFromPointArray(pointArray, bounds, ignored);'),1)})
    path='src/core/common/object/DBBoxOp.cpp';source=(ROOT/path).read_text()
    signature='void CalBBoxFromPointArray(const DPointList& pts, MBoundingBox& bbox)'
    body=extract(source,signature)
    replacement=signature+' { DPoint ignored; CalBBoxFromPointArray(pts, bbox, ignored); }'
    detect_mutation('bounds-only API delegates to mean API', {path:source.replace(body,replacement,1)})

def text_read_mutations():
    path=PATHS[2]+'.cpp';source=(ROOT/path).read_text()
    parser=extract(source,'bool readAssBoundingBoxFromStream(')
    for label,old,new in (
        ('Ass bounded read replaced by getline',
         'dommy::file::readBoundedLine(\n\t\t\tin, line, totalBytes, static_cast<std::size_t>(kMaxAssTocFileBytes),\n\t\t\tkMaxAssTocLineBytes, dommy::file::UnterminatedLinePolicy::PhysicalBytes)',
         'std::getline(in, line)'),
        ('Ass consumed-byte cap removed',
         'static_cast<std::size_t>(kMaxAssTocFileBytes)', 'std::numeric_limits<std::size_t>::max()'),
        ('Ass virtual EOF newline charged',
         'UnterminatedLinePolicy::PhysicalBytes', 'UnterminatedLinePolicy::CountTerminator'),
    ):
        assert old in parser,label
        detect_mutation(label,{path:source.replace(parser,parser.replace(old,new,1),1)})
    reader=(ROOT/'src/core/common/file/DBoundedTextRead.h').read_text()
    for label,old,new in (
        ('text line cap removed','if (line.size() >= maxLineBytes)',
         'if (line.size() >= maxLineBytes && false)'),
        ('text byte cap removed','if (totalBytes >= maxTotalBytes)',
         'if (totalBytes >= maxTotalBytes && false)'),
        ('text partial line admitted','input.setstate(std::ios::failbit);\n            return false;',
         'input.setstate(std::ios::failbit);\n            return true;'),
        ('text exact line boundary rejected','line.size() >= maxLineBytes',
         'line.size() + 1U >= maxLineBytes'),
    ):
        assert old in reader,label
        detect_mutation(label,text_reader_source=reader.replace(old,new))

def settings_mutations():
    for index,old,new in (
        (0,'readPlugValue(MPlug(mMeshDisplayNode, DMeshDisplay::aDisplayMode), displayType) != MS::kSuccess',
           '(readPlugValue(MPlug(mMeshDisplayNode, DMeshDisplay::aDisplayMode), displayType), false)'),
        (1,'readPlugValue(MPlug(mDGNode, DMMarker::aIndexType), dData.indexType) != MS::kSuccess',
           '(readPlugValue(MPlug(mDGNode, DMMarker::aIndexType), dData.indexType), false)'),
        (4,'readPlugValue(mDGNode, DPMarker::aShowIndex, dData.showIndex) != MS::kSuccess',
           '(readPlugValue(mDGNode, DPMarker::aShowIndex, dData.showIndex), false)'),
        (6,'readPlugValue(MPlug(mDGNode, DCMarker::aIndexType), dData.indexType) != MS::kSuccess',
           '(readPlugValue(MPlug(mDGNode, DCMarker::aIndexType), dData.indexType), false)'),
    ):
        path=PATHS[index]+'.cpp';source=(ROOT/path).read_text();assert old in source
        detect_mutation('settings status '+Path(PATHS[index]).name,{path:source.replace(old,new,1)})
    path=PATHS[2]+'.cpp';source=(ROOT/path).read_text()
    for label,old,new in (
        ('instancer animation read ignored',
         'DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(mInstancerNode, DPInstancer::aAnimationType), parms.mAnimMatrixType));',
         '(void)readPlugValue(MPlug(mInstancerNode, DPInstancer::aAnimationType), parms.mAnimMatrixType);'),
        ('instancer seed read ignored',
         'DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aSeed, seed));',
         '(void)readPlugValue(mInstancerNode, DPInstancer::aSeed, seed);'),
        ('instancer DG report omitted',
         'dommy::maya::reportViewportCallbackException(\n            "DPInstancerOverride", "updateDG", std::current_exception());',''),
        ('instancer failed mode retained','    dData.displayType = 0;',''),
        ('instancer empty geometry enabled','!dData.pointList.empty() && dData.displayType==DP_INST_SHADE','dData.displayType==DP_INST_SHADE'),
    ):
        assert old in source,label;detect_mutation(label,{path:source.replace(old,new,1)})
    for index in range(7):
        path=PATHS[index]+'.h';source=(ROOT/path).read_text()
        old,new=('short displayType = 0;','short displayType = 1;') if index==2 else (
            ('float pointSize = 5.0f;','float pointSize = 99.0f;') if index==6 else
            ('float pointSize = 3.0f;','float pointSize = 99.0f;'))
        assert old in source;detect_mutation('drawing initial value '+Path(PATHS[index]).name,{path:source.replace(old,new,1)})

def connection_mutations():
    for index in (0,1,3,4,5,6):
        path=PATHS[index]+'.cpp'; source=(ROOT/path).read_text()
        name=Path(PATHS[index]).name+'Override'
        callback=extract(source,'void '+name+'::updateDG(')
        query=re.search(r'isConnected\(&\w+\)',callback)
        assert query,'Checked connection query missing: '+name
        modified=callback[:query.start()]+'isConnected()'+callback[query.end():]
        detect_mutation('viewport connection status '+name,
                        {path:source.replace(callback,modified,1)})

if __name__ == '__main__':
    if '--stream-mutations' in sys.argv:
        stream_requirement_mutations()
    elif '--connection-mutations' in sys.argv:
        connection_mutations()
    elif '--normal-mutations' in sys.argv:
        normal_mutations()
    elif '--bounds-only-mutations' in sys.argv:
        bounds_only_mutations()
    elif '--bounds-mean-mutations' in sys.argv:
        bounds_mean_mutations()
    elif '--text-read-mutations' in sys.argv:
        text_read_mutations()
    elif '--lifecycle-mutations' in sys.argv:
        lifecycle_mutations()
    elif '--settings-mutations' in sys.argv:
        settings_mutations()
    elif '--arnold-input-mutations' in sys.argv:
        arnold_input_mutations()
    elif '--shader-mutations' in sys.argv:
        shader_mutations()
    elif '--mutations' in sys.argv:
        stream_requirement_mutations()
        normal_mutations()
        bounds_only_mutations()
        bounds_mean_mutations()
        text_read_mutations()
        lifecycle_mutations()
        settings_mutations()
        arnold_input_mutations()
        shader_mutations()
        for label, path, old, new in (
            ('GPU creation failure ignored', PATHS[0], 'if (!verticesBuffer) return false;', ''),
            ('GPU association failure ignored', PATHS[1],
             'if (!item->associateWithIndexBuffer(outIndexBuffer)) return;',
             'item->associateWithIndexBuffer(outIndexBuffer);'),
            ('GPU retry flag not set', PATHS[2],
             '    mDataChanged = true;\n\t', '\t'),
            ('edge endpoint failure ignored', PATHS[0],
             'if (!endpointStatus) return endpointStatus;', ''),
            ('group index failure ignored', PATHS[1],
             'if (!indexStatus) return indexStatus;', ''),
            ('normal inverse transpose omitted', PATHS[2],
             'checkedTransformNormal(source, matrix, transformed)',
             'checkedTransformDirection(source, matrix, transformed)'),
        ):
            source=(ROOT/(path+'.cpp')).read_text()
            if label == 'GPU retry flag not set':
                # Target only this callback, not updateDG's independent dirty flag.
                start=source.index('void DPInstancerOverride::populateGeometry(')
                before, callback=source[:start], source[start:]
                assert old in callback, label
                changed=before+callback.replace(old,new,1)
            else:
                assert old in source, label
                changed=source.replace(old,new)
            detect_mutation(label,{path+'.cpp':changed})
        for path in PATHS[3:]:
            source=(ROOT/(path+'.cpp')).read_text()
            name=Path(path).name+'Override'
            start=source.index('void '+name+'::populateGeometry(')
            before,callback=source[:start],source[start:]
            flag='mDataChanged' if 'curve/' in path else 'mMultiplierChanged'
            old='    '+flag+' = true;'
            assert old in callback
            detect_mutation(name+' retry flag omitted',
                {path+'.cpp':before+callback.replace(old,'',1)})
        for label,path,old,new in (
            ('point viewport admission omitted',PATHS[3],
             'count > dommy::viewport::kMaxViewportVertexCount ||',''),
            ('point coordinate validation omitted',PATHS[3],
             '!dommy::numeric::canRepresentAsFloat((*points)[i][axis])','false'),
            ('point count published during staging',PATHS[3],
             'positions.resize(scalarCount);',
             'dData.pointCount=static_cast<unsigned int>(count); positions.resize(scalarCount);'),
            ('point GPU creation failure ignored',PATHS[3],'if (!verticesBuffer) return false;',''),
            ('curve association failure ignored',PATHS[5],
             'if (!item->associateWithIndexBuffer(outIndexBuffer)) return;',
             'item->associateWithIndexBuffer(outIndexBuffer);'),
            ('VDB GPU creation failure ignored',PATHS[7],'if (!verticesBuffer) return false;',''),
            ('point render item creation failure ignored',PATHS[3],'if (!pendingItem) return;',''),
            ('point render item append failure ignored',PATHS[3],
             'if (!list.append(pendingItem.get())) return;', 'list.append(pendingItem.get());'),
        ):
            source=(ROOT/(path+'.cpp')).read_text()
            assert old in source,label
            detect_mutation(label,{path+'.cpp':source.replace(old,new)})
        for path in (PATHS[0],PATHS[1],PATHS[2],PATHS[4],PATHS[5],PATHS[6],PATHS[7]):
            source=(ROOT/(path+'.cpp')).read_text()
            name=Path(path).name+'Override'
            for label,old,new in (
                ('render item creation failure ignored','if (!pendingItem) return;',''),
                ('render item append failure ignored','if (!list.append(pendingItem.get())) return;',
                 'list.append(pendingItem.get());'),
            ):
                assert old in source
                detect_mutation(name+' '+label,{path+'.cpp':source.replace(old,new)})
        header=(ROOT/'src/core/common/utility/DViewportBuffer.h').read_text()
        for label,old,new in (
            ('float buffer type validation omitted',
             'descriptor.dataType() != MHWRender::MGeometry::kFloat ||',''),
            ('float buffer element size validation omitted',
             'descriptor.dataTypeSize() != sizeof(float) ||',''),
            ('float buffer tuple dimension validation omitted',
             'descriptorDimension != expectedDimension','descriptorDimension <= 0'),
        ):
            assert old in header,label
            detect_mutation(label,buffer_source=header.replace(old,new))
        render_path='src/core/common/utility/DViewportRenderItem.h'
        render_header=(ROOT/render_path).read_text()
        for label,old,new in (
            ('shader parameter status ignored',
             '!shader || !shader->setParameter(name, value)',
             '!shader || (shader->setParameter(name, value), false)'),
            ('shader assignment status ignored',
             'assigned_ = parametersValid_ && shader && item_.setShader(shader, name);',
             'assigned_ = parametersValid_ && shader; if (assigned_) item_.setShader(shader, name);'),
            ('failed parameters still assigned',
             'parametersValid_ && shader && item_.setShader(shader, name)',
             'shader && item_.setShader(shader, name)'),
            ('shader visibility gate removed', 'item_.enable(visible && assigned_);', 'item_.enable(visible);'),
            ('stale item not disabled before setup', 'item_.enable(false);', ''),
            ('exception retry guard removed', 'if (!completed_) retry_ = true;', ''),
            ('shader failure retry lost', 'retry_ = true;', 'retry_ = false;'),
        ):
            assert old in render_header,label
            detect_mutation(label,{render_path:render_header.replace(old,new)})
        for path in PATHS:
            header_path=path+'.h';header=(ROOT/header_path).read_text()
            dirty='mMultiplierChanged' if path in (PATHS[3],PATHS[4],PATHS[7]) else 'mDataChanged'
            old='return '+dirty+' || mShaderUpdateFailed;'
            assert old in header
            detect_mutation(Path(path).name+' shader retry not connected to stream dirtiness',
                            {header_path:header.replace(old,'return '+dirty+';')})
        source=(ROOT/(PATHS[5]+'.cpp')).read_text()
        old='MS::kSuccess || !dommy::maya::isFiniteColor(dData.solidColor)'
        assert old in source
        detect_mutation('curve nonfinite solid color accepted',
                        {PATHS[5]+'.cpp':source.replace(old,'MS::kSuccess')})
    else:
        sdk=sys.argv[sys.argv.index('--sdk')+1] if '--sdk' in sys.argv else None
        source=bodies(sdk=sdk is not None)
        run(source,sdk=sdk)
        run(source,sdk=sdk,debug_enabled=False)
