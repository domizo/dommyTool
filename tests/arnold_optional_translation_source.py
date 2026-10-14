"""Optional mesh/light/attribute stages with explicit renderer/payload seams."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_path_reservation_exception_safety import disable_core_dumps
from arnold_parameter_source import prepare as prepare_parameters

ROOT = Path(__file__).resolve().parent.parent
INSTANCER = 'src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp'
LIGHT = 'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp'
VDB = 'src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp'


def methods(overrides):
    instancer = overrides.get(INSTANCER, (ROOT/INSTANCER).read_text())
    light = overrides.get(LIGHT, (ROOT/LIGHT).read_text())
    resources = overrides.get('src/integrations/arnold/DArnoldResources.h',
                              (ROOT/'src/integrations/arnold/DArnoldResources.h').read_text())
    code = ['namespace dommy {namespace arnold {',
            extract(resources, 'inline void requireArrayWrite('),
            extract(resources, 'class NodeOwner')+';',
            extract(resources, 'class ArrayOwner')+';', '}}',
            'namespace dommy {namespace maya {',
            extract((ROOT/'src/core/common/utility/DMayaMeshImport.h').read_text(), 'inline MStatus preflightMeshImport('), '}}',
            extract(instancer, 'AtNode* DPInstancerTranslatorAi::generateAiMeshFromMPlug('),
            extract(instancer, 'void DPInstancerTranslatorAi::instanceAssignDPAttribute('),
            extract(instancer, 'bool DPInstancerTranslatorAi::RequiresMotionData('),
            extract(instancer, 'void DPInstancerTranslatorAi::ExportMotion(')]
    update = extract(instancer, 'void DPInstancerTranslatorAi::Update(')
    if 'attributeWorkItems' in update:
        start = update.index('std::size_t attributeWorkItems')
        end = update.index('INST_DEBUG("exAttrNameArray:', start)
        code += ['void DPInstancerTranslatorAi::attributeAdmission(const DPData* dPoints,const MStringArray& exAttrNameArray,std::size_t instCount) {',
                 update[start:end], 'attributeAccepted=true;}']
        assert update.index('instanceAssignDPAttribute(dPoints, exAttrNameArray, nullptr, index)') < update.index('if (!deleteInstancedNodes(')
    else:
        code += ['void DPInstancerTranslatorAi::attributeAdmission(const DPData*,const MStringArray&,std::size_t) {attributeAccepted=true;}']
    if 'preparedMeshOwner' in update:
        assert update.index('dommy::arnold::NodeOwner preparedMeshOwner(') < update.index('if (!deleteInstancedNodes(')
        assert 'std::move(preparedMeshOwner)' in update
    if 'Static instancing only' in instancer:
        assert 'return false;' in extract(instancer,'bool DPInstancerTranslatorAi::RequiresMotionData(')
        motion = strip_comments_and_literals(extract(instancer,'void DPInstancerTranslatorAi::ExportMotion('))
        assert not motion[motion.index('{')+1:motion.rindex('}')].strip()
    if 'checkedFloatMatrix(matrix, validatedMatrix)' in update:
        assert update.index('checkedFloatMatrix(matrix, validatedMatrix)') < update.index('if (!deleteInstancedNodes(')
    setter = lambda token: next(line.split('//')[0].strip() for line in light.splitlines() if token in line)
    code += ['float repeatStage(double value) {AtNode node;AtNode* texNode=&node;const std::array<double,2> repeatTexVal{{value,1.0}};',
             next(line.strip() for line in light.splitlines() if 'AiNodeSetFlt(texNode, "sscale",' in line and 'repeatTexVal[0]' in line), 'return submitted;}']
    code += ['float exposureStage(double exposure) {AtNode node;AtNode* light=&node;',
             setter('AiNodeSetFlt(light, "exposure",'), 'return submitted;}']
    code += ['float radiusStage(double value,float scale) {AtNode node;AtNode* light=&node;MVector globalScale(value,1,1);std::vector<float> scaleArray{scale};std::size_t i=0U;',
             setter('AiNodeSetFlt(light, "radius", globalScale[0]') if 'AiNodeSetFlt(light, "radius", globalScale[0]' in light else setter('AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(globalScale[0]'),
             'return submitted;}']
    first = light.index('DScalarDataList intensityArray(')
    end = light.index('DVec3DataList colorArray(', first)
    code += ['float intensityStage(double intensity,bool useAttribute,DP_ATTR_DATA_TYPE type=DP_ATTR_DOUBLE) {DPData points;points.exists=useAttribute;points.attribute.type=type;points.attribute.count=1U;points.scalarValues={2.5};const DPData* dPoints=&points;',
             'std::vector<int> pointStorage(1U);const std::vector<int>* ptsPtr=&pointStorage;std::string intensityAttributeName="value";',
             light[first:end].replace('return;', 'throw std::invalid_argument("rejected light field");'), 'return intensityArray.at(0);}']
    statement = next(line.strip() for line in light.splitlines()
                     if ('lightMatrix.get(finMatVal)' in line or 'checkedFloatMatrix(lightMatrix, finMatVal)' in line))
    code += ['float matrixStage(const MMatrix& value) {const MMatrix& lightMatrix=value;float finMatVal[4][4];',
             statement, 'return finMatVal[0][0];}']
    return '\n'.join(code)


def vdb_methods(overrides):
    source = overrides.get(VDB, (ROOT/VDB).read_text())
    callback = extract(source, 'void DVDBTranslatorAi::Update(')
    prefix = callback[:callback.index('if (vdb)')]
    clean = strip_comments_and_literals(prefix)
    guard = (ROOT/'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    helper = (ROOT/'src/integrations/arnold/DArnoldMayaPlug.h').read_text()
    completion = ''
    if 'UpdateVisibilityGuard' in callback:
        active = strip_comments_and_literals(callback)
        assert active.count('UpdateVisibilityGuard updateVisibility(node);') == 1
        assert active.count('updateVisibility.complete();') == 1
        assert active.index('UpdateVisibilityGuard') < active.index('ScopedPlugDataHandle')
        assert active.index('updateVisibility.complete();') > active.index('ExportLightLinking(node)')
        tail = active.split('updateVisibility.complete();',1)[1]
        assert not tail.replace('}', '').strip()
        completion = 'updateVisibility.complete();'
    return '\n'.join(('namespace dommy{namespace arnold{',extract(helper,'class PlugReader')+';',
        extract(guard,'inline void hideIncompleteUpdate('),extract(guard,'class UpdateVisibilityGuard')+';',
        extract(guard,'inline void reportTranslatorException('),'}}',
        guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):],prefix,
        'if(vdb){if(api("cache/render.stage")!=MS::kSuccess){AiMsgError("stage failure");return;}admitted=true;}',
        completion,'}'*(clean.count('{')-clean.count('}')),
        'DOMMY_ARNOLD_TRANSLATOR_CATCH("VDB fixture",node)'))


def run(overrides=None, capture=False, sdk=None, phase='all'):
    overrides = overrides or {}
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-optional-') as directory:
        temp = Path(directory)
        prepare_parameters(temp,overrides,api='arnold_parameter_api.h')
        (temp/'optional_translation.inc').write_text(methods(overrides))
        (temp/'optional_vdb.inc').write_text(vdb_methods(overrides))
        compiler = shlex.split(os.environ.get('CXX','clang++'))
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-variable',
                 '-Wno-unused-parameter','-Wno-unknown-pragmas','-I'+str(temp),
                 '-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core')]
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            flags += ['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit = ROOT/'tests/cpp/arnold_optional_translation_tests.cpp'
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Optional renderer mesh/attribute/numeric stages: Maya SDK syntax/analysis passed; Arnold/payload seams are not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        results = []
        for suite in ('arnold_optional_translation_tests.cpp','arnold_optional_vdb_tests.cpp'):
            if phase == 'vdb' and 'vdb' not in suite: continue
            if phase not in ('all','vdb') and 'vdb' in suite: continue
            binary = temp/suite[:-4]
            subprocess.run(compiler+flags+[str(ROOT/'tests/cpp'/suite),'-o',str(binary)],
                           check=True,capture_output=capture,text=True)
            options = {'preexec_fn':disable_core_dumps} if os.name == 'posix' else {}
            result = subprocess.run([str(binary),phase],cwd=temp,capture_output=capture,text=True,timeout=30,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)
            results.append(result)
            if result.returncode != 0:return result
        return results[-1]


def mutations():
    variants = (
        ('mesh binding status',INSTANCER,'if (!status) {','if (false) {','mesh'),
        ('polygon indices',INSTANCER,'!dommy::mesh::validPolygonArrays(pts.length(), faceCountArray, connectArray)', 'false','mesh'),
        ('UV topology',INSTANCER,'if (!validUVs) {','if (!validUVs && false) {','mesh'),
        ('world-point Float range',INSTANCER,'if (!dommy::numeric::canRepresentAsFloat(pts[point][axis])) {','if (false) {','mesh'),
        ('Int64 range',INSTANCER,'if (!dommy::numeric::checkedScalarCast(value, narrowed))','if (false && !dommy::numeric::checkedScalarCast(value, narrowed))','numeric'),
        ('attribute preflight',INSTANCER,'instanceAssignDPAttribute(dPoints, exAttrNameArray, nullptr, index);','if (false) instanceAssignDPAttribute(dPoints, exAttrNameArray, nullptr, index);','numeric'),
        ('light Float exposure',LIGHT,'dommy::arnold::checkedFloat(exposure)','static_cast<float>(exposure)','numeric'),
        ('texture reciprocal',LIGHT,'dommy::arnold::checkedTextureReciprocal(repeatTexVal[0])','(repeatTexVal[0]==0.0?0.0:1.0/repeatTexVal[0])','numeric'),
        ('light matrix range',LIGHT,'dommy::arnold::checkedFloatMatrix(lightMatrix, finMatVal);','lightMatrix.get(finMatVal);','numeric'),
        ('VDB returned acquisition status',VDB,'if (vdbHndOwner.read(vdbPlug) != MS::kSuccess)','if ((vdbHndOwner.read(vdbPlug), false))','vdb'),
    )
    for label,path,old,new,phase in variants:
        source=(ROOT/path).read_text();assert old in source,label
        if label=='mesh binding status':
            # Other callbacks now also check MStatus. Mutate the actual mesh
            # generator, not the first same-spelled guard in the source file.
            body=extract(source,'AtNode* DPInstancerTranslatorAi::generateAiMeshFromMPlug(')
            assert old in body
            changed=source.replace(body,body.replace(old,new,1),1)
        else:
            changed=source.replace(old,new,1)
        result=run({path:changed},capture=True,phase=phase)
        assert result.returncode!=0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected optional translator mutation:',label,flush=True)
    source=(ROOT/INSTANCER).read_text()
    changed=source.replace('unsigned int /*numMotionStep*/','unsigned int numMotionStep',1).replace('vertexNum, 1U, pointLayout','vertexNum, numMotionStep, pointLayout',1)
    assert changed!=source
    result=run({INSTANCER:changed},capture=True,phase='motion')
    assert result.returncode!=0 and 'Assertion' in result.stderr
    print('Detected optional translator mutation: unassigned motion keys',flush=True)
