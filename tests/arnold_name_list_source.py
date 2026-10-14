"""Renderer name admission and reused VDB state with explicit API boundaries."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
VDB = 'src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp'
LISTS = (
    ('src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp','otherAttrName','attrNameArray','totalCount'),
    ('src/core/DDataUtility/IO/DPWriteToAi.cpp','otherAttrName','attrNameArray','pts.size()'),
    ('src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp','exportCurveAttributeNames','curveAttrNameArray','totalCurveCount'),
    ('src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp','exportPointAttributeNames','pointAttrNameArray','totalCVCount'),
    ('src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp','exportCurveAttributeNames','curveAttrNameArray','totalCurveCount'),
    ('src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp','exportPointAttributeNames','pointAttrNameArray','totalCVCount'),
    ('src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp','exportMeshAttributeNames','meshAttrNameArray','totalFaceCount'),
    ('src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp','exportPointAttributeNames','pointAttrNameArray','totalVertexCount'),
    ('src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp','exportAttributeNames','exAttrNameArray','instCount'),
    (VDB,'additionalGridsString','additional_grids','1U'),
    (VDB,'velocityGridsString','velocity_grids','1U'),
)

def name_helper(source=None, nonempty=False):
    source=source or (ROOT/'src/core/common/utility/DMayaNameList.h').read_text()
    return '\n'.join(('namespace dommy {namespace maya {',
        extract(source,'struct NameListBudget')+';',
        extract(source,'struct NameListScan')+';',
        extract(source,'inline MStatus admitNameList('),
        extract(source,'inline MStatus '+('splitCheckedNonemptyNameList(' if nonempty else 'splitCheckedNameList(')), '}}'))


def name_stages(overrides=None):
    code=[];cases=[];stages=[]
    for index,(path,value,output,count) in enumerate(LISTS):
        source=(overrides or {}).get(path,(ROOT/path).read_text())
        token='dommy::maya::splitCheckedNameList(\n'
        matches=list(re.finditer(re.escape(token)+r'\s*'+value+r'\s*,',source))
        assert len(matches)==1,(path,value)
        call=matches[0].start()
        start=source.rfind('\n',0,call)+1
        if source[start:call].strip().startswith('if ('):
            original=extract(source[start:],'if')
            stage=original.replace('return;', 'return MS::kFailure;')
        else:
            end=source.index(';',call)+1;original=stage=source[start:end]
        assert re.search(re.escape(value)+r'\s*,\s*'+re.escape(count)+r'\s*,\s*nameListBudget\s*,\s*'+output,stage)
        stages.append((path,original))
        count_declaration=('CountingPoints pts{repetitions};' if count=='pts.size()' else
                           '' if count=='1U' else 'const std::size_t '+count+'=repetitions;')
        code += ['MStatus listStage'+str(index)+'(const MString& value,std::size_t repetitions,dommy::maya::NameListBudget& nameListBudget,MStringArray& output) {',
                 'const MString& '+value+'=value;MStringArray& '+output+'=output;',count_declaration,stage,'return MS::kSuccess;}']
        cases += ['checkList(listStage'+str(index)+','+str(count!='1U').lower()+');']
    return '\n'.join(code),'\n'.join(cases),stages


def bodies(overrides=None, names=True):
    source=(overrides or {}).get(VDB,(ROOT/VDB).read_text())
    first=source.index('const unsigned int velocity_grids_count =')
    end=source.index('#pragma mark option',first)
    resources=(overrides or {}).get('src/integrations/arnold/DArnoldResources.h',
                                   (ROOT/'src/integrations/arnold/DArnoldResources.h').read_text())
    guard=(ROOT/'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    names_path='src/core/common/utility/DMayaNameList.h'
    code=[name_helper((overrides or {}).get(names_path)),
          'namespace dommy {namespace arnold {',extract(resources,'inline void requireArrayWrite('),extract(resources,'class ArrayOwner')+';',
          extract(guard,'inline void hideIncompleteUpdate('),extract(guard,'class UpdateVisibilityGuard')+';',
          extract(guard,'inline void reportTranslatorException('),'}}',
          guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):],
          'void velocityStage(AtNode* node,const MStringArray& velocity_grids) try {',
          'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',
          'const float velocityScale=2,velocityFps=24,velocityShutterStart=0,velocityShutterEnd=1,velocityThreshold=0.001f;',
          source[first:end],'accepted=true;updateVisibility.complete();}',
          'DOMMY_ARNOLD_TRANSLATOR_CATCH("velocity",node)']
    cases=''
    if names:
        methods,cases,_=name_stages(overrides);code.append(methods)
        first=source.index('dommy::maya::NameListBudget nameListBudget;')
        end=source.index('MFnDependencyNode vdbNode',first)
        code += ['void velocityUpdate(AtNode* node,const MString& additionalGridsString,const MString& velocityGridsString) try {',
                 'dommy::arnold::UpdateVisibilityGuard updateVisibility(node);',source[first:end],
                 '++cacheCommits;velocityStage(node,velocity_grids);if(accepted)updateVisibility.complete();}',
                 'DOMMY_ARNOLD_TRANSLATOR_CATCH("velocity update",node)']
        # All admission must precede the first payload mutation/allocation,
        # node retirement, or VDB cache write; matrix/visibility setup is not
        # claimed to be rolled back by this source-placement check.
        sinks={LISTS[0][0]:'//make point',LISTS[1][0]:'dommy::arnold::Session arnoldSession',
               LISTS[2][0]:'#pragma mark set curve mode',LISTS[4][0]:'#pragma mark export start',
               LISTS[6][0]:'static const AtString smoothingParam',LISTS[8][0]:'if (!deleteInstancedNodes(',
               VDB:'writeVdbCache('}
        for path,_,_,_ in LISTS:
            text=(overrides or {}).get(path,(ROOT/path).read_text())
            assert text.count('dommy::maya::NameListBudget nameListBudget;')==1,path
            positions=[m.start() for m in re.finditer(r'dommy::maya::splitCheckedNameList\(',text)]
            start=text.index('void DVDBTranslatorAi::Update(') if path==VDB else 0
            sink=text.index(sinks[path],start);assert all(p<sink for p in positions),path
    return '\n'.join(code),cases


def run(overrides=None,capture=False,sdk=None,names=True):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-names-') as directory:
        temp=Path(directory);methods,cases=bodies(overrides,names)
        (temp/'renderer_list_methods.inc').write_text(methods)
        (temp/'renderer_list_cases.inc').write_text(cases)
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
               '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unknown-pragmas',
               '-I'+str(temp),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        unit=ROOT/'tests/cpp/arnold_name_list_tests.cpp'
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Eleven name admissions and VDB velocity commit: Maya SDK syntax/analysis passed; renderer/cache boundaries are seams')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        binary=temp/'test';subprocess.run(compiler+flags+[str(unit),'-o',str(binary)],check=True,capture_output=capture,text=True)
        options={'preexec_fn':disable_core_dumps} if os.name=='posix' else {}
        return subprocess.run([str(binary)],capture_output=capture,text=True,cwd=temp,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)


def mutations():
    preallocation_mutations()
    for index,(path,stage) in enumerate(name_stages()[2]):
        source=(ROOT/path).read_text()
        if 'DOMMY_RETURN_IF_FAILURE' in stage:
            changed=stage.replace('DOMMY_RETURN_IF_FAILURE(', '(void)(',1)
        else:
            changed=stage.replace(') != MS::kSuccess)', ') != MS::kSuccess && false)',1)
        assert changed!=stage,index
        result=run({path:source.replace(stage,changed,1)},capture=True)
        assert result.returncode!=0 and 'Assertion' in result.stderr,index
        print('Detected name admission mutation:',index,flush=True)
    source=(ROOT/VDB).read_text()
    for parameter in ('velocity_grids','velocity_scale','velocity_fps','velocity_shutter_start','velocity_shutter_end','velocity_outlier_threshold'):
        old='AiNodeResetParameter(node, "'+parameter+'");';assert source.count(old)==1
        result=run({VDB:source.replace(old,'(void)node;',1)},capture=True)
        assert result.returncode!=0 and 'Assertion' in result.stderr,parameter
        print('Detected VDB velocity reset mutation:',parameter,flush=True)


def preallocation_mutations():
    path='src/core/common/utility/DMayaNameList.h'
    source=(ROOT/path).read_text()
    for label,old,new in (
        ('native pre-admission ignored',
         'DOMMY_RETURN_IF_FAILURE(admitNameList(value, repetitions, budget, true, scan));',
         '(void)admitNameList(value, repetitions, budget, true, scan);'),
        ('native possible empty fields not counted',
         'admitNameList(value, repetitions, budget, true, scan)',
         'admitNameList(value, repetitions, budget, false, scan)'),
        ('native parse work undercounted', 'checkedProduct(bytes, 3U,', 'checkedProduct(bytes, 2U,'),
        ('native split result bound ignored','if (actualCount > scan.names)', 'if (false)'),
        ('native budget committed before split','DOMMY_RETURN_IF_FAILURE(value.split(\' \', names));',
         'budget = scan.admitted; DOMMY_RETURN_IF_FAILURE(value.split(\' \', names));'),
        ('native unused fields not refunded','candidate.names -= unused;', '(void)unused;'),
        ('native encoded-byte consistency ignored','if (bytes != encodedBytes)', 'if (false)')):
        assert old in source,label
        result=run({path:source.replace(old,new,1)},capture=True)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected native name pre-admission mutation:',label,flush=True)
