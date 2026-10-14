#!/usr/bin/env python3
"""Execute production export admission with explicit Maya/renderer boundaries."""
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

ROOT = Path(__file__).resolve().parent.parent
LIGHT = 'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp'
UTILITY = 'src/modules/curve/DCurve/Cmd/DDataExportUtil.cpp'
JOB = 'src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp'
COMMAND = 'src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp'


def light_bodies(overrides=None):
    source = (overrides or {}).get(LIGHT, (ROOT/LIGHT).read_text())
    callback = extract(source, 'void DPLightTranslatorAi::Update(')
    # Compile the real admission statements through the valid-empty branch.
    # Geometry/Arnold node generation after this boundary is deliberately absent.
    prefix = callback[:callback.index('const DPointList *ptsPtr')]
    code = strip_comments_and_literals(prefix)
    prefix += 'reachedPayload = true;\n' + '}'*(code.count('{')-code.count('}'))
    guard = (ROOT/'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    return '\n'.join((extract(source, 'std::string generatedNodePrefix('),
                      guard[guard.index('#define DOMMY_ARNOLD_TRANSLATOR_CATCH'):],
                      prefix, 'DOMMY_ARNOLD_TRANSLATOR_CATCH("DPLightTranslatorAi::Update", node)'))


def alembic_bodies(overrides=None):
    overrides=overrides or {}
    utility=overrides.get(UTILITY,(ROOT/UTILITY).read_text())
    job=overrides.get(JOB,(ROOT/JOB).read_text())
    code=[]
    if 'void requireExportQuery(' in utility:
        code += ['namespace ddata_export_util {',extract(utility,'void requireExportQuery('),'}']
    code += [extract(utility,'int ddata_export_util::getSampledType('),
             extract(utility,'int ddata_export_util::getSampledType(const MPlug& iPlug)'),
             extract(utility,'int ddata_export_util::getVisibilityType(')]
    for name,plug,node,attribute in (
            ('Point','dpPlug','DPointDisplay','inDP'),
            ('Curve','dcPlug','DCurveDisplay','inDC'),
            ('Mesh','dmPlug','DMeshDisplay','inDM')):
        start=job.index('MPlug '+plug+'(ob, '+node+'::'+attribute+');')
        prefix=job[start:job.index('MObject dataObject;',start)]
        clean=strip_comments_and_literals(prefix)
        code += ['void shapeAdmission'+name+'(const MObject& ob) {',prefix,
                 '++admittedShapes;', '}'*(clean.count('{')-clean.count('}')+1)]
    command=overrides.get(COMMAND,(ROOT/COMMAND).read_text())
    code += [extract(command,'class PendingAbcWriteJob')+';',
             'using PendingAbcWriteJobPtr=std::shared_ptr<PendingAbcWriteJob>;']
    start=command.index('\n\t\tif ',command.index('bool lastFrame = (*j)->eval(*it);'))+1
    end=command.index('\n\t}\n\tcatch (Alembic::Util::Exception',start)
    code += ['MStatus finalizeAbcJobs(const std::list<PendingAbcWriteJobPtr>& jobList, '
             'std::vector<PendingAbcWriteJobPtr>& allJobs, '
             'dommy::maya::ScopedCurrentTimeRestore& timeRestore, '
             'dommy::maya::ScopedComputation& computation) {MStatus status;',
             command[start:end],'}']
    return '\n'.join(code)


def run(overrides=None, capture=False, sdk=None, alembic=False):
    with tempfile.TemporaryDirectory(prefix='dommy-export-input-') as directory:
        temp = Path(directory)
        methods='alembic_input_methods.inc' if alembic else 'export_input_methods.inc'
        host='alembic_input_doubles.h' if alembic else 'export_input_doubles.h'
        suite='alembic_input_tests.cpp' if alembic else 'export_input_tests.cpp'
        (temp/methods).write_text(alembic_bodies(overrides) if alembic else light_bodies(overrides))
        if alembic and not sdk:
            (temp/'maya').mkdir()
            (temp/'maya/MGlobal.h').write_text('''#pragma once
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
MStatus fixtureViewFrame(const MTime&);
class MGlobal {
public:
    static void displayError(const MString&) {}
    static MStatus viewFrame(const MTime& time) { return fixtureViewFrame(time); }
};
''')
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-Wno-unused-parameter', '-Wno-unused-variable',
                 '-Wno-unknown-pragmas',
                 '-I'+str(temp), '-I'+str(ROOT/'tests/cpp'), '-I'+str(ROOT/'src/core')]
        compiler = shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(Path(sdk)/'include')]
            unit = temp/'sdk.cpp'
            unit.write_text('#include "'+host+'"\n#include "'+methods+'"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print(('Alembic classification/admission' if alembic else 'Arnold light admission')+
                  ': Maya SDK syntax/analysis passed; writer/renderer/payload boundaries declared, not linked')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'), '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp'/suite),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),str(temp/'fixture.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))


def mutations(alembic=False):
    cases=(
        ('light node bind status',LIGHT,'GetMayaObject(), &inputStatus','GetMayaObject()'),
        ('light UUID query status',LIGHT,'nodeFn.uuid(&inputStatus)','nodeFn.uuid()'),
        ('light UUID validity',LIGHT,'!inputStatus || !nodeId.valid()','!inputStatus'),
        ('light connection query status',LIGHT,'dpPlug.isConnected(&inputStatus)','dpPlug.isConnected()'),
    ) if not alembic else (
        ('sampling status requirement',UTILITY,'if (!status) throw','if (false) throw'),
        ('sampling connection query',UTILITY,'connections, true, false, &status','connections, true, false'),
        ('sampling element query',UTILITY,'connectionByPhysicalIndex(index, &status)','connectionByPhysicalIndex(index)'),
        ('sampling child query',UTILITY,'iPlug.child(index, &status)','iPlug.child(index)'),
        ('sampling node query',UTILITY,'connections[index].node(&status)','connections[index].node()'),
        ('sampling incoming query',UTILITY,'incoming.isConnected(&status)','incoming.isConnected()'),
        ('sampling mute query',UTILITY,'mute.asBool(&status)','mute.asBool()'),
        ('visibility query',UTILITY,'iPlug.asBool(&status)','iPlug.asBool()'),
        ('sampling depth',UTILITY,'stack.size() > maximumDepth','stack.size() > maximumDepth && false'),
        ('sampling work',UTILITY,'!work.consume(1U)','false'),
        ('sampling cardinality',UTILITY,'frame.count > dommy::generation::kMaxConnectedGeometryCount','false'),
        ('point shape connection',JOB,'dpPlug.isConnected(&connectionStatus)','dpPlug.isConnected()'),
        ('curve shape connection',JOB,'dcPlug.isConnected(&connectionStatus)','dcPlug.isConnected()'),
        ('mesh shape connection',JOB,'dmPlug.isConnected(&connectionStatus)','dmPlug.isConnected()'),
    )
    if alembic:
        source=(ROOT/COMMAND).read_text()
        begin=source.index('        status = timeRestore.restore();')
        end=source.index('\n\t\tcomputation.finish();',begin)
        restoration=source[begin:end]
        cases += (
            ('restore status gate',COMMAND,'if (status != MS::kSuccess) {\n            MGlobal::displayError("Unable to restore Maya time after Alembic export.");',
             'if (false) {\n            MGlobal::displayError("Unable to restore Maya time after Alembic export.");'),
            ('restore invocation',COMMAND,'status = timeRestore.restore();','status = MS::kSuccess;'),
            ('completed archive release',COMMAND,'job_.reset();','(void)job_;'),
            ('remaining job gate',COMMAND,'if (!jobList.empty()) {','if (false) {'),
        )
        modified=source.replace(restoration,'',1).replace(
            '#pragma mark End Write\n\t\treturn MS::kSuccess;',
            '#pragma mark End Write\n\t\treturn timeRestore.restore();',1)
        result=run({COMMAND:modified},capture=True,alembic=True)
        assert result.returncode!=0,'Undetected export finalization ordering mutation'
        print('Detected export admission mutation: commit before time restoration',flush=True)
    for label,path,old,new in cases:
        source=(ROOT/path).read_text();assert old in source,label
        result=run({path:source.replace(old,new,1)},capture=True,alembic=alembic)
        assert result.returncode!=0,'Undetected export admission mutation: '+label
        print('Detected export admission mutation: '+label,flush=True)


def optional_display_mutations():
    import check_display_metadata_safety as metadata
    import check_arnold_export_safety as light
    import check_light_draw_preparation_safety as prepare
    import check_vdb_viewport_safety as vdb
    import check_viewport_staging_safety as viewport
    cases=(
        (metadata.PATHS['DPInstancer'],'MBoundingBox DPInstancer::boundingBox()',metadata.run),
        (vdb.DISPLAY,'MStatus DOpenVDBDisplayVP2::compute(',metadata.run),
        (vdb.DISPLAY,'MBoundingBox DOpenVDBDisplayVP2::boundingBox()',vdb.run),
        (light.LIGHT,'MBoundingBox DPLightGen::boundingBox()',light.run),
        (light.LIGHT,'MBoundingBox DPLightGenOverride::boundingBox(',light.run),
        (prepare.LIGHT,'MUserData* DPLightGenOverride::prepareForDraw(',prepare.run),
        (viewport.PATHS[2]+'.cpp','void DPInstancerOverride::updateDGImpl(',
         lambda changes,capture:viewport.run(viewport.bodies(changes),capture=capture)),
    )
    for path,signature,runner in cases:
        source=(ROOT/path).read_text();callback=extract(source,signature)
        queries=list(re.finditer(r'isConnected\(&\w+\)',callback));assert queries,signature
        for query in queries:
            modified=callback[:query.start()]+'isConnected()'+callback[query.end():]
            try:
                result=runner({path:source.replace(callback,modified,1)},capture=True)
                assert result.returncode!=0,'Undetected optional display mutation: '+signature
            except subprocess.CalledProcessError as error:
                assert error.returncode!=1 or any(marker in (error.stderr or '') for marker in
                    ('runtime error:','ERROR: AddressSanitizer')),'Compile failure is not detection: '+signature
            print('Detected optional display mutation: '+signature,flush=True)


if __name__ == '__main__':
    alembic='--alembic' in sys.argv
    if '--optional-display-mutations' in sys.argv:
        optional_display_mutations()
    elif '--mutations' in sys.argv:
        mutations(alembic)
    elif '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1],alembic=alembic)
    else:
        result=run(alembic=alembic)
        assert result.returncode==0
        print('Alembic classification/admission failures preserve the destination and remove temporary output: OK' if alembic else
              'Arnold light admission preserves existing lights on query/data failure; disconnection, empty input and retry: OK')
