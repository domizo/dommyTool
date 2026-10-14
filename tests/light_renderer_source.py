"""Exact Light translator matrix/link stages with failure-injectable seams."""
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
LIGHT = 'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp'
TRANSFORM = 'src/modules/point/DPoint/render/light/DPLightTransform.h'


def transform_methods(overrides=None, sdk=False):
    overrides = overrides or {}
    source = overrides.get(TRANSFORM, (ROOT/TRANSFORM).read_text())
    if sdk:
        return '#include "'+TRANSFORM[len('src/'):]+'"'
    validation = (ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
    return '\n'.join((extract(source, 'enum DP_MAT_APPLY_TYPE')+';',
        'namespace dommy { namespace maya {',
        extract(validation, 'inline bool isFiniteMatrix('),
        extract(validation, 'inline bool isFiniteQuaternion('),
        '}}', 'namespace dommy { namespace light {',
        extract(source, 'inline MStatus buildTransform('), '}}'))


def methods(overrides=None):
    source = (overrides or {}).get(LIGHT, (ROOT/LIGHT).read_text())
    start = source.index('float finMatVal[4][4];')
    end = source.index('//for arnold5', start)
    code = ['void matrixStage(int applyAnimMatType,int lightTypeVal,double globalX,float pointScale,const MMatrix& animation) {',
        'const MVector globalScale(globalX,3,4),lookAtVec(10,20,30);',
        'const std::vector<float> scaleArray{pointScale};const std::size_t i=0U;',
        'const std::vector<MMatrix> animMatArray{animation};',
        'const std::vector<MVector> lightDirVecArray{MVector(0,0,-1)};',
        'const DPointList points{DPoint(1,2,3)};auto pIt=points.begin();',
        source[start:end],
        'for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)submittedMatrix[r][c]=finMatVal[r][c];',
        'accepted=true;}']
    for parameter in ('slidemap', 'color'):
        target_name = 'goboNode' if parameter == 'slidemap' else 'light'
        statement = 'AiNodeLink(texNode, "'+parameter+'", '+target_name+')'
        guard = re.search(r'if\s*\([^\n]*'+re.escape(statement)+r'[^\n]*\)\s*\{', source)
        stage = extract(source, guard.group(0)) if guard else statement+';'
        assert source.index(statement) < source.index('if (!destroyGeneratedNodes(previousNodes))')
        code += ['void '+parameter+'Stage() {',
            'dommy::arnold::NodeOwner texture(AiNode("image")),target(AiNode("light"));',
            'AtNode* texNode=texture.get();AtNode* '+target_name+'=target.get();',
            stage, 'accepted=true;++retireCalls;}']
    return '\n'.join(code)


def run(overrides=None, capture=False, sdk=None, phase='all'):
    with tempfile.TemporaryDirectory(prefix='dommy-light-renderer-') as directory:
        temp = Path(directory)
        (temp/'light_renderer_methods.inc').write_text(methods(overrides))
        (temp/'light_transform_methods.inc').write_text(transform_methods(overrides, bool(sdk)))
        for name in ('ai.h','ai_array.h','ai_nodes.h','ai_universe.h','ai_msg.h'):
            (temp/name).write_text('#include "arnold_export_api.h"\n')
        compiler = shlex.split(os.environ.get('CXX','clang++'))
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unknown-pragmas',
            '-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core')]
        unit = ROOT/'tests/cpp/light_renderer_tests.cpp'
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            flags += ['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Light shared matrix/translator/link stages: Maya SDK syntax/analysis passed; Arnold/MtoA not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(unit),'-o',str(binary)],check=True,capture_output=capture,text=True)
        options = {'preexec_fn':disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary),phase],cwd=temp,capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)


def mutations():
    variants = (
        ('translator transform status',LIGHT,'if (!transformStatus) {','if (false) {','status'),
        ('scale API status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(base.addScale(scale, MSpace::kWorld));','(void)base.addScale(scale, MSpace::kWorld);','status'),
        ('rotation API status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(base.addRotationQuaternion(','CHECK_MSTATUS(base.addRotationQuaternion(','status'),
        ('translation API status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(transformed.addTranslation(position, MSpace::kWorld));','(void)transformed.addTranslation(position, MSpace::kWorld);','status'),
        ('oriented translation API status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(base.addTranslation(currentPosition, MSpace::kWorld));','(void)base.addTranslation(currentPosition, MSpace::kWorld);','status'),
        ('translation query status',TRANSFORM,'DOMMY_RETURN_IF_FAILURE(status);','(void)status;','status'),
        ('Ignore All selected before scale',TRANSFORM,'if (mode == DP_MAT_FULL_MAT_IGNORE_ALL) {','if (mode == DP_MAT_FULL_MAT_IGNORE_ALL && false) {','ignore'),
        ('gobo link status',LIGHT,'if (!AiNodeLink(texNode, "slidemap", goboNode)) {','if (!AiNodeLink(texNode, "slidemap", goboNode) && false) {','link'),
        ('color link status',LIGHT,'if (!AiNodeLink(texNode, "color", light)) {','if (!AiNodeLink(texNode, "color", light) && false) {','link'),
    )
    for label,path,old,new,phase in variants:
        source=(ROOT/path).read_text(); assert old in source,label
        result=run({path:source.replace(old,new,1)},capture=True,phase=phase)
        assert result.returncode != 0 and ('Assertion' in result.stderr or 'runtime error:' in result.stderr),label
        print('Detected Light runtime mutation:',label,flush=True)
