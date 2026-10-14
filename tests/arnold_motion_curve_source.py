"""Actual motion and Curve/Mesh field stages; not native MtoA event delivery."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps
from arnold_name_list_source import name_helper

ROOT=Path(__file__).resolve().parent.parent
PATHS={name:'src/modules/'+folder+'/Translator/D'+name+'TranslatorAi.cpp'
       for name,folder in (('Point','point/DPoint'),('Curve','curve/DCurve'),('Mesh','mesh/DMesh'))}
MOTION='src/integrations/arnold/DArnoldMotionKey.h'

def bodies(overrides=None):
    overrides=overrides or {}
    resources=overrides.get('src/integrations/arnold/DArnoldResources.h',
                            (ROOT/'src/integrations/arnold/DArnoldResources.h').read_text())
    guard=(ROOT/'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    code=['namespace dommy {namespace arnold {',extract(resources,'inline void requireArrayWrite('),extract(resources,'class ArrayOwner')+';',
          extract(resources,'class ArrayMapGuard')+';',extract(guard,'inline void hideIncompleteUpdate('),
          extract(guard,'class UpdateVisibilityGuard')+';',extract(guard,'inline void reportTranslatorException('),'}}',
          guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):],name_helper()]
    if (ROOT/MOTION).exists():
        helper=overrides.get(MOTION,(ROOT/MOTION).read_text())
        code += ['namespace dommy {namespace arnold {',extract(helper,'inline bool setCheckedVectorMotionKey('),'}}']
    for name,path in PATHS.items():
        source=overrides.get(path,(ROOT/path).read_text())
        body=extract(source,'void D'+name+'TranslatorAi::ExportMotion(')
        if 'setCheckedVectorMotionKey' in body:
            first=body.index('AtArray* const destinationPoints')
            end=body.index('#pragma mark',first) if name!='Mesh' else body.index('//export main',first)
            stage=body[first:end].rstrip()
            # Mesh's closing geometry brace is outside this isolated handoff.
            if name=='Mesh':stage=stage.rsplit('}',1)[0].rstrip()
        else:
            first=body.index('dommy::arnold::ArrayMapGuard mappedPoints')
            end=body.index('mappedPoints.data());',first)+len('mappedPoints.data());')
            stage=body[first:end]
        code += ['void motion'+name+'(AtNode* node,unsigned motionstep,unsigned numMotionStep) try {',
                 'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',
                 'struct Borrowed {AtArray* get()const{return &motionSource;}} pointarray;',
                 'const AtString vlistParam("vlist");' if name=='Mesh' else '',stage,
                 'accepted=true;updateVisibility.complete();}',
                 'DOMMY_ARNOLD_TRANSLATOR_CATCH("motion test",node)']
    source=overrides.get(PATHS['Curve'],(ROOT/PATHS['Curve']).read_text())
    code.append(extract(source,'namespace {'))
    body=extract(source,'void DCurveTranslatorAi::Update(')
    first=body.index('const bool isCubicMode =') if 'makeRenderableCurvePlan' in body else body.index('size_t totalCurveCount =')
    end=body.index('MPlug CurveShaderPlug =',first)
    code += ['void curveStage(AtNode* node,const DCurveData* dCurves,short basisModeVal,bool isAddEdgeCVVal,short curveWidthType,short modeType,short orientType,const MString& exportCurveAttributeNames,const MString& exportPointAttributeNames) try {',
             'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',
             'const unsigned numMotionStep=2U;const char* visParam="visibility";',
             'const double constantWidth=0.25;const float minPixelWidth=0.0f;',
             'const std::string widthAttributeName="width",orientAttrName="orientation";',
             body[first:end],'accepted=true;updateVisibility.complete();}',
             'DOMMY_ARNOLD_TRANSLATOR_CATCH("curve test",node)']
    motion=extract(source,'void DCurveTranslatorAi::ExportMotion(')
    first=motion.index('const bool isCubicMode =')
    end=motion.index('#pragma mark set curve width',first)
    code += ['void curveMotionStage(AtNode* node,const DCurveData* dCurves,short basisModeVal,bool isAddEdgeCVVal,unsigned motionstep,unsigned numMotionStep) try {',
             'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',
             'const char* visParam="visibility";',motion[first:end],
             'accepted=true;updateVisibility.complete();}',
             'DOMMY_ARNOLD_TRANSLATOR_CATCH("curve motion test",node)']
    mesh=overrides.get(PATHS['Mesh'],(ROOT/PATHS['Mesh']).read_text())
    first=mesh.index('for (uint i=0; i<pointAttrNameArray.length(); i++)')
    end=mesh.index('#pragma mark Group Matte',first)
    code += ['void meshAttributeStage(AtNode* node,const MeshCollection* dMeshes,const MStringArray& pointAttrNameArray) try {',
             'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',
             'dommy::arnold::ArrayLayout pointAttributeLayout;',
             'assert(dommy::arnold::checkedArrayLayout(dMeshes->numberOfPoints(),1U,pointAttributeLayout));',
             mesh[first:end],'accepted=true;updateVisibility.complete();}',
             'DOMMY_ARNOLD_TRANSLATOR_CATCH("mesh attribute test",node)']
    notification=extract(mesh,'void DMeshTranslatorAi::NodeChanged(')
    # The notification name comparison/long-name request remains production
    # code; plug retrieval and MtoA event delivery are explicit test seams.
    code.append(notification.replace('DMeshTranslatorAi::','MeshNotifier::')
        .replace('MObject&','NotificationObject&').replace('MPlug&','NotificationPlug&')
        .replace('MFnDependencyNode','NotificationNodeFn'))
    code.append('DOMMY_ARNOLD_TRANSLATOR_CATCH("MeshNotifier::NodeChanged",nullptr)')
    return '\n'.join(code)

def run(overrides=None,capture=False,sdk=None,phase='all'):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-motion-curve-') as directory:
        temp=Path(directory);(temp/'motion_curve_methods.inc').write_text(bodies(overrides))
        fields=(ROOT/'src/core/Data/DPrimBaseData.h').read_text()
        getter=extract(fields,'template <typename T>\ninline const std::vector<T>& DPrimBaseData::attrListValue(')
        lookup=extract(fields,'template <typename T>\ninline typename T::ConstPtr DPrimBaseData::getAttribute(')
        typed=extract(fields,'template <typename T>\ninline typename TypedDPAttributeData<T>::ConstPtr\nDPrimBaseData::getValidTypedAttributedata(')
        (temp/'motion_curve_fields.inc').write_text((lookup+'\n'+getter+'\n'+typed)
            .replace('DPrimBaseData::','Fields::').replace('mAttributeList','attributes'))
        export_header='src/core/common/utility/DAttributeExport.h'
        if export_header in (overrides or {}):
            target=temp/export_header[len('src/'):];target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(overrides[export_header])
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-variable',
               '-Wno-unused-parameter','-Wno-unused-function','-Wno-unknown-pragmas',
               '-I'+str(temp),'-I'+str(temp/'core'),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core'),
               '-I'+str(ROOT/'src/core/common/utility'),
               '-I'+str(ROOT/'src/modules/curve/DCurve/IO')]
        compiler=shlex.split(os.environ.get('CXX','clang++'));unit=ROOT/'tests/cpp/arnold_motion_curve_tests.cpp'
        if sdk:
            for header in ('ImathVec.h','DType.h'):(temp/header).write_text((ROOT/'tests/stubs'/header).read_text())
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Motion, complete Curve payload, merged Mesh fields and notification body: Maya SDK syntax/analysis passed; renderer/data/event ownership remain seams')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        binary=temp/'test';subprocess.run(compiler+flags+[str(unit),'-o',str(binary)],check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),phase],capture_output=capture,text=True,cwd=temp,
            timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),
            preexec_fn=disable_core_dumps)


def mutations():
    variants=[]
    helper=(ROOT/MOTION).read_text()
    for check in ('!destination || ', '!source || ', 'key >= expectedKeys ||',
                  'AiArrayGetType(source) != AI_TYPE_VECTOR ||',
                  'AiArrayGetType(destination) != AI_TYPE_VECTOR ||',
                  'AiArrayGetNumKeys(source) != 1U ||',
                  'AiArrayGetNumKeys(destination) != layout.keyCount ||'):
        assert helper.count(check)==1
        variants.append(('motion '+check.strip(),{MOTION:helper.replace(check,'',1)},'motion'))
    old='AiArrayGetNumElements(destination) != layout.elementCount'
    assert helper.count(old)==1
    variants.append(('motion element correspondence',{MOTION:helper.replace(old,'false',1)},'motion'))
    old='return mapped && AiArraySetKey(destination, static_cast<std::uint8_t>(key), mapped.data());'
    assert old in helper
    variants.append(('motion false key write ignored',{MOTION:helper.replace(old,
        'if (!mapped) return false; AiArraySetKey(destination, static_cast<std::uint8_t>(key), mapped.data()); return true;',1)},'motion'))
    for name,path in PATHS.items():
        source=(ROOT/path).read_text()
        call=re.search(r'if \(!dommy::arnold::setCheckedVectorMotionKey\([\s\S]+?numMotionStep\)\)',source)
        assert call,name
        ignored=call.group().replace('if (!','if ((',1).replace('numMotionStep))','numMotionStep), false))',1)
        variants.append((name+' motion result ignored',{path:source[:call.start()]+ignored+source[call.end():]},'motion'))
    path=PATHS['Curve'];source=(ROOT/path).read_text()
    old='if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))'
    positions=list(re.finditer(re.escape(old),source));assert len(positions)==9
    for index,match in enumerate(positions):
        variants.append(('uniform source remap '+str(index),{path:source[:match.start()]+'if (false)'+source[match.end():]},'curve'))
    old='const std::size_t c = layout.varyingSourceIndex(varyingIndex);'
    positions=list(re.finditer(re.escape(old),source));assert len(positions)==4
    for index,match in enumerate(positions):
        variants.append(('varying source index '+str(index),{path:source[:match.start()]+
            'const std::size_t c = varyingIndex;'+source[match.end():]},'curve'))
    for value in ('wList','oriList'):
        old=value+'[renderableCurveIndices[renderCurveIndex]]';assert old in source
        variants.append((value+' retained source index',{path:source.replace(old,value+'[renderCurveIndex]')},'curve'))
    for label,overrides,phase in variants:
        result=run(overrides,capture=True,phase=phase)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr), \
            'No runtime assertion/sanitizer failure for mutation: '+label+'\n'+result.stderr
        print('Detected motion/Curve mutation:',label,flush=True)
    print('Motion/Curve runtime mutations detected:',len(variants),flush=True)

def mesh_write_mutations():
    variants=[];path=PATHS['Mesh'];source=(ROOT/path).read_text()
    old='if (attribute) break;';assert source.count(old)==1
    variants.append(('first present Mesh schema',{path:source.replace(old,'break;',1)},'mesh_missing'))
    old='if (!valid || tmp.size() != pointAttributeLayout.elementCount)'
    positions=list(re.finditer(re.escape(old),source));assert len(positions)==3
    for index,match in enumerate(positions):
        variants.append(('merged Mesh family '+str(index)+' admission',
            {path:source[:match.start()]+'if (false)'+source[match.end():]},'mesh'))
    first=source.index('if (!dommy::attribute::buildExportAttributeValues<int32_t, int32_t>(')
    end=source.index('{',first)
    variants.append(('merged Mesh integer admission',{path:source[:first]+'if (false) '+source[end:]},'mesh'))
    header='src/core/common/utility/DAttributeExport.h';helper=(ROOT/header).read_text()
    for label,old,new in (
        ('missing-owner default','std::vector<Destination> candidate(total, defaultValue);','std::vector<Destination> candidate(total, Destination());'),
        ('missing-owner offset','offset += count;','if (attribute) offset += count;')):
        assert helper.count(old)==1
        variants.append((label,{header:helper.replace(old,new,1)},'mesh'))
    header='src/integrations/arnold/DArnoldResources.h';helper=(ROOT/header).read_text()
    old='if (!success) throw std::runtime_error("Unable to write an Arnold array element.");'
    assert old in helper
    variants.append(('false element result ignored',{header:helper.replace(old,'(void)success;',1)},'mesh_write'))
    old='pName == "ExportMeshAttribute"';assert source.count(old)==1
    variants.append(('canonical Mesh notification',{path:source.replace(old,'pName == "aExportMeshAttribute"',1)},'notification'))
    for label,overrides,phase in variants:
        result=run(overrides,capture=True,phase=phase)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label+'\n'+result.stderr
        print('Detected Mesh/element-write mutation:',label,flush=True)
    print('Mesh/element-write runtime mutations detected:',len(variants),flush=True)
