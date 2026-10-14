"""Real typed accessors and all renderer control read branches, not MtoA."""
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
ACCESS = 'src/modules/point/DPoint/Common/DPAttributeAccess.h'
PATHS = (
    'src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp',
    'src/core/DDataUtility/IO/DPWriteToAi.cpp',
    'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp',
    'src/modules/point/DPoint/render/light/DPLightGen.cpp',
)


def stages(overrides=None):
    result = []
    for path in PATHS:
        source = (overrides or {}).get(path, (ROOT/path).read_text())
        for match in re.finditer(r'if\s*\([^\{;]*isExistAttr\([^\)]*\)\)\s*\{', source):
            branch = extract(source[match.start():], 'if')
            call = re.search(r'(getChecked\w+ArrayFromAttribute|dommy::attribute::convertVector3Attribute)\(\s*([^;]+)\)', branch)
            if not call:
                continue
            args = call.group(2).split(',')
            name = re.search(r'isExistAttr\((\w+)\)', branch).group(1)
            output = re.match(r'\s*(\w+)', args[-1]).group(1)
            family = ('scalar' if 'DScalar' in call.group(1) else
                      'real' if 'MReal' in call.group(1) else
                      'matrix' if 'MMatrix' in call.group(1) else 'vector')
            result.append((path, branch, name, output, family))
    assert len(result) == 20, len(result)
    return result


def bodies(overrides=None):
    access = (overrides or {}).get(ACCESS, (ROOT/ACCESS).read_text())
    detail = extract(access, 'namespace dommy {')
    detail = detail.replace(extract(detail, 'inline bool copyCheckedStringAttribute('), '')
    code = [detail]
    code += [extract(access, 'inline bool getChecked'+kind+'ArrayFromAttribute(')
             for kind in ('DScalar','MVector','MDouble','MReal','MMatrix')]
    cases = []
    for index, (_, branch, name, output, family) in enumerate(stages(overrides)):
        label = 'control'+str(index)
        array = {'scalar':'DScalarDataList','real':'MDoubleArray',
                 'vector':'MVectorArray','matrix':'MMatrixArray'}[family]
        if 'convertVector3Attribute' in branch:array='DVec3DataList'
        declarations=[]
        for owner in ('dPoints','inputDPData','dp'):
            if re.search(r'\b'+owner+r'\b',branch):declarations.append('const DPData* '+owner+'=&points;')
        if re.search(r'\bpts(?:Ptr)?\b',branch):declarations.append('std::vector<int> pts(1U);')
        if 'ptsPtr' in branch:declarations.append('const auto* ptsPtr=&pts;')
        if 'totalCount' in branch:declarations.append('const std::size_t totalCount=1U;')
        for control in ('dirTypeVal','curveWidthType'):
            if control in branch:declarations.append('const int '+control+'=mode;')
        if 'data->' in branch:declarations.append('DrawData storage;DrawData* data=&storage;')
        code += ['bool '+label+'(DPAttributeData::ConstPtr field,int mode=1) {',
                 'DPData points;points.attributes["value"]=field;',
                 ''.join(declarations),
                 'const std::string '+name+'="value";'+array+' '+output+';',
                 branch.replace('return MS::kFailure;', 'return false;').replace('return NULL;', 'return false;').replace('return;', 'return false;'),
                 'return true;}']
        cases += ['check('+label+',"'+family+'",'+str('dirTypeVal' in branch or 'curveWidthType' in branch).lower()+');']
    return '\n'.join(code), '\n'.join(cases)


def run(overrides=None, capture=False, sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-renderer-controls-') as directory:
        temp=Path(directory)
        methods,cases=bodies(overrides)
        (temp/'renderer_control_methods.inc').write_text(methods)
        (temp/'renderer_control_cases.inc').write_text(cases)
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
               '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unknown-pragmas',
               '-I'+str(temp),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core')]
        unit=ROOT/'tests/cpp/arnold_control_attribute_tests.cpp'
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            (temp/'DType.h').write_text((ROOT/'tests/stubs/DType.h').read_text())
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            compiler=shlex.split(os.environ.get('CXX','clang++'))
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Twenty control branches and real typed accessors: Maya SDK syntax/analysis passed; Arnold logging/payload ownership remain seams')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all']
        binary=temp/'test'
        subprocess.run(shlex.split(os.environ.get('CXX','clang++'))+flags+
                       [str(unit),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        options={'preexec_fn':disable_core_dumps} if os.name=='posix' else {}
        return subprocess.run([str(binary)],capture_output=capture,text=True,cwd=temp,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')),**options)


def mutations():
    for index,(path,branch,_,_,_) in enumerate(stages()):
        source=(ROOT/path).read_text()
        changed=re.sub(r'if \(!(getChecked\w+ArrayFromAttribute|dommy::attribute::convertVector3Attribute)\(',
                       r'if (false && !\1(',branch,count=1)
        assert changed!=branch
        result=run({path:source.replace(branch,changed,1)},capture=True)
        assert result.returncode!=0 and 'Assertion' in result.stderr, index
        print('Detected renderer control admission mutation:',index,flush=True)
