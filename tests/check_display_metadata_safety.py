#!/usr/bin/env python3
"""Execute production display metadata callbacks with failure-injectable hosts."""
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
PATHS = {
    'DPointDisplay':'src/modules/point/DPoint/Display/DPointDisplay.cpp',
    'DPMarker':'src/modules/point/DPoint/Display/DPMarker.cpp',
    'DCMarker':'src/modules/curve/DCurve/Display/DCMarker.cpp',
    'DMMarker':'src/modules/mesh/DMesh/Display/DMMarker.cpp',
    'DMeshDisplay':'src/modules/mesh/DMesh/Display/DMeshDisplay.cpp',
    'DCurveDisplay':'src/modules/curve/DCurve/Display/DCurveDisplay.cpp',
    'DOpenVDBDisplayVP2':'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp',
    'DPInstancer':'src/modules/point/DPoint/Duplicator/DPInstancer.cpp',
    'DPLightGen':'src/modules/point/DPoint/render/light/DPLightGen.cpp',
}
GUARD = 'src/core/common/utility/DViewportCallbackGuard.h'


def bodies(overrides=None, sdk=False):
    code = [extract((ROOT / 'src/core/common/string/DStringOp.cpp').read_text(),
                    'std::string mergeStringList('),
            extract((ROOT / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                    'MStatus getValidInputDP(')]
    if not sdk:
        guard = (ROOT/'src/core/common/utility/DComputeGuard.h').read_text()
        code += [guard[guard.index('#define DOMMY_COMPUTE_CATCH'):guard.index('#endif')]]
        vdb_guard = (ROOT/'src/modules/volume/VDB/Common/DVDBComputeGuard.h').read_text()
        code += [vdb_guard[vdb_guard.index('#define DOMMY_VDB_COMPUTE_CATCH'):]]
        viewport = (overrides or {}).get(GUARD,(ROOT/GUARD).read_text())
        code += [viewport[viewport.index('#define DOMMY_VIEWPORT_VOID_CATCH'):]]
        validation = (ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
        code += ['namespace dommy {namespace maya {',
                 extract(validation,'inline bool isFinitePoint('),
                 extract(validation,'inline bool isFiniteBoundingBox('), '}}']
    attribute_id = 100
    for name,path in PATHS.items():
        source = (overrides or {}).get(path,(ROOT/path).read_text())
        callback = extract(source,'MStatus '+name+'::compute(')
        compute_guard = 'DOMMY_VDB_COMPUTE_CATCH' if name == 'DOpenVDBDisplayVP2' else 'DOMMY_COMPUTE_CATCH'
        assert re.match(r'\s*'+compute_guard+r'\(',source[source.index(callback)+len(callback):])
        attributes = re.findall(r'MObject\s+'+name+r'::(\w+);',source)
        code += ['struct '+name+' {'+
                 ''.join('static MObject '+a+';' for a in attributes)+
                 'MStatus compute(const MPlug&,MDataBlock&); MObject thisMObject() const {return MObject();}'+
                 'MBoundingBox boundingBox() const; MString name() const {return "'+name+'";} };']
        if not sdk:
            for attribute in attributes:
                code += ['MObject '+name+'::'+attribute+'('+str(attribute_id)+');']
                attribute_id += 1
        code += [callback,compute_guard+'("'+name+'")']
        if name not in ('DOpenVDBDisplayVP2', 'DPLightGen'):
            bounds = extract(source,'MBoundingBox '+name+'::boundingBox()')
            suffix = source[source.index(bounds)+len(bounds):]
            macro = re.match(r'\s*(DOMMY_VIEWPORT_BOUNDING_BOX_CATCH[^\n]*)',suffix)
            assert macro,'Bounds exception guard missing: '+name
            code += [bounds,macro.group(1)]
    return '\n'.join(code)


def run(overrides=None, capture=False, sdk=None, section=None):
    with tempfile.TemporaryDirectory(prefix='dommy-display-metadata-') as directory:
        temp = Path(directory)
        (temp/'display_metadata_methods.inc').write_text(bodies(overrides,bool(sdk)))
        compiler = shlex.split(os.environ.get('CXX','clang++'))
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-DDOMMY_WITH_DG_FILE_WRITERS=0',
                 '-Wno-unused-parameter','-Wno-unused-variable','-Wno-unknown-pragmas',
                 '-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'src')]
        if sdk:
            (temp/'ImathVec.h').write_text((ROOT/'tests/stubs/ImathVec.h').read_text())
            flags += ['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include'),
                      '-idirafter',str(ROOT/'tests/stubs')]
            unit = temp/'sdk.cpp'
            unit.write_text('#include "display_metadata_host.h"\n#include "display_metadata_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Nine display/light compute and seven bounds callbacks: Maya SDK syntax/analysis passed; procedural data/handle/OpenVDB boundaries declared, DG file writers disabled, not linked')
            return
        flags += ['-DDOMMY_DISPLAY_ALLOCATION_TESTS=1','-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/display_metadata_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary)]+([section] if section else []),env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')),
            capture_output=capture,text=True,cwd=str(temp),**options)


def formatting_mutations():
    for name in ('DPointDisplay','DPMarker','DCurveDisplay','DCMarker','DMeshDisplay','DMMarker','DPLightGen'):
        path = PATHS[name]; source = (ROOT / path).read_text()
        stream = 'info' if name == 'DPointDisplay' else 'infoStr'
        before = 'dommy::text::checkedStreamText(' + stream + ', "' + name + ' info formatting failed")'
        assert source.count(before) == 1, name
        result = run({path: source.replace(before, stream + '.str()')}, capture=True, section='format')
        assert result.returncode != 0 and 'Assertion' in result.stderr, 'Runtime mutation not detected: ' + name
        print('Detected formatting mutation: ' + name, flush=True)


def light_input_mutations():
    path = PATHS['DPLightGen']; source = (ROOT / path).read_text()
    callback = extract(source, 'MStatus DPLightGen::compute(')
    for label, old, new in (
            ('connection status ignored', 'inputPlug.isConnected(&stat);', 'inputPlug.isConnected();'),
            ('connected input failure hidden',
             'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, dp));',
             '(void)getValidInputDP(inDP, data, dp);'),
            ('disconnected input still acquired', 'if (connected) {', 'if (true) {')):
        assert callback.count(old) == 1, label
        result = run({path: source.replace(callback, callback.replace(old, new, 1), 1)}, capture=True)
        assert result.returncode != 0 and 'Assertion' in result.stderr, 'Runtime mutation not detected: ' + label
        print('Detected light input mutation: ' + label, flush=True)


def main():
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--format-only' in sys.argv:
        result = run(section='format')
        assert result.returncode == 0
    elif '--format-mutations' in sys.argv:
        formatting_mutations()
    elif '--input-mutations' in sys.argv:
        light_input_mutations()
    elif '--connection-mutations' in sys.argv:
        for name in ('DPointDisplay','DPMarker','DCurveDisplay','DCMarker','DMeshDisplay','DMMarker'):
            path=PATHS[name]; source=(ROOT/path).read_text()
            for signature in ('MStatus '+name+'::compute(', 'MBoundingBox '+name+'::boundingBox()'):
                callback=extract(source,signature)
                query=re.search(r'isConnected\(&\w+\)',callback)
                assert query,'Checked connection query missing: '+signature
                modified=callback[:query.start()]+'isConnected()'+callback[query.end():]
                result=run({path:source.replace(callback,modified,1)},capture=True)
                assert result.returncode!=0,'Undetected connection mutation: '+signature
                print('Detected connection mutation: '+signature,flush=True)
    elif '--mutations' in sys.argv:
        for label,name,old,new in (
            ('wrong disconnected marker output','DPMarker',
             'plug == aAllAttributeNames ? aAllAttributeNames : aAllGroupNames,', 'aNodeInfo,'),
            ('marker output status','DCMarker',
             '&outputStatus);\n            if (!outputStatus) return outputStatus;',
             '&outputStatus);\n            if (false) return outputStatus;'),
            ('mesh info output status','DMeshDisplay',
             'MDataHandle strHandle = data.outputValue(aNodeInfo, &stat);\n\t\tif (!stat) return stat;',
             'MDataHandle strHandle = data.outputValue(aNodeInfo, &stat);\n\t\tif (false) return stat;'),
            ('point info duplicate publication','DPointDisplay','    output.set(text);', '    output.set(text); output.set(text);'),
            ('point unknown plug status','DPointDisplay','return MS::kUnknownParameter;', 'return MS::kSuccess;'),
            ('curve connected input status','DCurveDisplay',
             'stat = getValidInputDC(inDC, data, dCurves);\n\t\t\tif (!stat) return stat;',
             'stat = getValidInputDC(inDC, data, dCurves);\n\t\t\tif (false) return stat;'),
            ('VDB connected input status','DOpenVDBDisplayVP2',
             'status = getValidInputVDB(aVdbInput, data, inputVdb);\n        if (status != MS::kSuccess) return status;',
             'status = getValidInputVDB(aVdbInput, data, inputVdb);\n        if (false) return status;'),
            ('instancer unknown plug status','DPInstancer','return MS::kUnknownParameter;', 'return MS::kSuccess;'),
            ('instancer cleans unsupported plug','DPInstancer',
             'return MS::kUnknownParameter;', 'data.setClean(plug); return MS::kUnknownParameter;'),
            ('point display fallback extent','DPointDisplay',
             'DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT("DPointDisplay", 1.0)',
             'DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT("DPointDisplay", 0.5)')):
            path = PATHS[name]; source = (ROOT/path).read_text()
            start = source.index('MStatus '+name+'::compute(')
            assert old in source[start:], 'Mutation target missing: '+label
            modified = source[:start]+source[start:].replace(old,new,1)
            if name == 'DPInstancer' and 'setClean' in new:
                modified = modified.replace('const MPlug& /*plug*/, MDataBlock& /*data*/',
                                            'const MPlug& plug, MDataBlock& data',1)
            result = run({path:modified},capture=True)
            assert result.returncode != 0,'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
        viewport = (ROOT/GUARD).read_text()
        old = '        return MBoundingBox( \\\n'
        assert old in viewport
        result = run({GUARD:viewport.replace(old,'        throw; \\\n        return MBoundingBox( \\\n',1)},capture=True)
        assert result.returncode != 0,'Undetected mutation: bounds exception containment'
        print('Detected mutation: bounds exception containment',flush=True)
    else:
        result = run()
        assert result.returncode == 0
        print('Display metadata outputs, checked connections and typed input status, disconnected clearing, unsupported instancer plugs, failure/retry and seven bounds callbacks: OK')
        formatting_mutations()
        light_input_mutations()


if __name__ == '__main__':
    main()
