#!/usr/bin/env python3
"""Execute camera ownership and light UI callbacks with explicit host doubles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
CAMERA = 'src/modules/rendering/Render/Camera/DCameraFrustum.cpp'
LIGHT = 'src/modules/point/DPoint/render/light/DPLightGen.cpp'


def bodies(overrides=None, sdk=False):
    overrides = overrides or {}
    camera = overrides.get(CAMERA, (ROOT/CAMERA).read_text())
    light = overrides.get(LIGHT, (ROOT/LIGHT).read_text())
    guard = (ROOT/'src/core/common/utility/DViewportCallbackGuard.h').read_text()
    validation = (ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
    code = ['#include "common/utility/DNumericValidation.h"',
            '#include "common/utility/DStatusOperation.h"',
            '#include "common/utility/DMayaCheckedArray.h"']
    if sdk:
        code += ['#include "common/utility/DViewportCallbackGuard.h"',
                 '#include "common/utility/DMayaValueValidation.h"',
                 '#include <maya/MUserData.h>', '#include <maya/MDagPath.h>',
                 '#include <maya/MFrameContext.h>', '#include <maya/MMatrixArray.h>',
                 '#include <maya/MDoubleArray.h>', '#include <maya/MVectorArray.h>',
                 '#include <maya/MPointArray.h>', '#include <memory>',
                 'template <typename Frustum> MStatus populateCameraFrustumData(const MObject&, Frustum&);',
                 'using uint = unsigned int;',
                 'namespace dommy {namespace math {double degreesToRadians(double);}}']
    else:
        code += ['namespace dommy {namespace maya {',
                 extract(guard, 'class ScopedUIDrawable final')+';',
                 extract(validation, 'inline bool isFiniteMatrix('),
                 extract(validation, 'inline bool isFinitePoint('),
                 extract(validation, 'inline bool isFloatRepresentablePoint('),
                 extract(validation, 'inline bool isFiniteVector('),
                 extract(validation, 'inline bool isFloatRepresentableVector('), '}}',
                 guard[guard.index('#define DOMMY_VIEWPORT_VOID_CATCH'):]]
    code += [extract((ROOT/'src/modules/rendering/Render/Camera/DCameraFrustum.h').read_text(),
                     'class DCameraFrustumData')+';',
             extract((ROOT/'src/modules/point/DPoint/render/light/DPLightGen.h').read_text(),
                     'class DPLightGenData final')+';',
             'struct DCameraFrustumOverride {MUserData* prepareForDraw(const MDagPath&,const MDagPath&,const MHWRender::MFrameContext&,MUserData*);void addUIDrawables(const MDagPath&,MHWRender::MUIDrawManager&,const MHWRender::MFrameContext&,const MUserData*);};',
             'struct DPLightGenOverride {void addUIDrawables(const MDagPath&,MHWRender::MUIDrawManager&,const MHWRender::MFrameContext&,const MUserData*);};',
             extract(light, 'bool validLightDrawData('),
             extract(light, 'MStatus transformLightPoints('),
             extract(light, 'bool lightIntensityAlpha(')]
    code += ['template<typename Frustum>\n'+extract(camera,'MStatus frustumCorners('),
             extract(camera,'MStatus frustumLines(')]
    for source, name, signature, macro in (
            (camera, 'DCameraFrustumOverride', 'MUserData* DCameraFrustumOverride::prepareForDraw(',
             'DOMMY_VIEWPORT_POINTER_CATCH'),
            (camera, 'DCameraFrustumOverride', 'void DCameraFrustumOverride::addUIDrawables(',
             'DOMMY_VIEWPORT_VOID_CATCH'),
            (light, 'DPLightGenOverride', 'void DPLightGenOverride::addUIDrawables(',
             'DOMMY_VIEWPORT_VOID_CATCH')):
        callback = extract(source, signature)
        suffix = source[source.index(callback)+len(callback):]
        assert re.match(r'\s*'+macro+r'\(', suffix), 'Callback exception boundary missing'
        code += [callback, macro+'("'+name+'", "'+signature.split('::')[1].split('(')[0]+'")']
    return '\n'.join(code)


def run(overrides=None, capture=False, sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-draw-callback-') as directory:
        temp = Path(directory)
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-Wno-unused-parameter', '-Wno-unused-variable',
                 '-I'+str(ROOT/'src/core')]
        if sdk:
            flags[0] = '-std=c++14'
            flags += ['-isystem', str(Path(sdk)/'include'), '-Wno-deprecated-declarations']
            source = temp/'sdk.cpp'
            source.write_text(bodies(overrides, sdk=True))
            subprocess.run(compiler+flags+['-fsyntax-only', str(source)], check=True)
            subprocess.run(compiler+flags+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(source)], check=True)
            print('Camera prepareForDraw/addUIDrawables and light addUIDrawables: Maya SDK syntax/analysis passed; camera query and Arnold boundaries not linked')
            return
        (temp/'draw_callback_methods.inc').write_text(bodies(overrides))
        binary = temp/'test'
        flags += ['-I'+str(temp), '-I'+str(ROOT/'tests/stubs'),
                  '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        # A compilation failure propagates; it is never a mutation detection.
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/draw_callback_tests.cpp'),
                        '-o', str(binary)], check=True, capture_output=capture, text=True)
        return subprocess.run([str(binary)], env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')),
            capture_output=capture, text=True)


def main():
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label, path, old, new in (
            ('empty light cache guard', LIGHT, 'lightCount == 0U ||', ''),
            ('color cardinality guard', LIGHT, '(data.colorVis && data.colorArray.length() != lightCount) ||', ''),
            ('intensity cardinality guard', LIGHT, '(data.intensityVis && data.intensityArray.length() != lightCount) ||', ''),
            ('spot cardinality guard', LIGHT, '(needsSpotAngles && data.spotAngleArray.length() != lightCount)', 'false'),
            ('color numeric admission', LIGHT, '!dommy::maya::isFloatRepresentableVector(data.colorArray[index])', 'false'),
            ('spot angle admission', LIGHT, 'needsSpotAngles && !dommy::numeric::checkedFloatCast(', 'false && !dommy::numeric::checkedFloatCast('),
            ('point size admission', LIGHT, '!std::isfinite(data.pointSize) || data.pointSize <= 0.0f ||', ''),
            ('matrix admission', LIGHT, 'if (!dommy::maya::isFiniteMatrix(data.matrixArray[index]))', 'if (false)'),
            ('point coordinate admission', LIGHT, 'if (!dommy::maya::isFloatRepresentablePoint(point)) return;', 'if (false) return;'),
            ('line coordinate admission', LIGHT, 'return dommy::maya::isFloatRepresentablePoint(point)', 'return true'),
            ('shape append status', LIGHT, 'dommy::maya::CheckedArray<MPointArray> pts;', 'MPointArray pts;'),
            ('camera early ownership release', CAMERA, '    const MStatus status =\n        populateCameraFrustumData',
             '    newFrustum.release();\n    const MStatus status =\n        populateCameraFrustumData'),
            ('camera missing ownership handoff', CAMERA, '    newFrustum.release();', ''),
            ('camera normal draw preflight', CAMERA,
             'if (!frustumCorners(*frustum, false, corners) || !frustumLines(corners, lines)) return;',
             'frustumCorners(*frustum, false, corners);frustumLines(corners, lines);'),
            ('camera extension draw preflight', CAMERA,
             'if (drawExtension && (!frustumCorners(*frustum, true, extendedCorners) ||\n                          !frustumLines(extendedCorners, extendedLines))) return;',
             'if (drawExtension) {frustumCorners(*frustum, true, extendedCorners);frustumLines(extendedCorners, extendedLines);}'),
            ('camera corner copy status', CAMERA, 'return dommy::maya::copyCheckedArray(candidate, corners);',
             'dommy::maya::copyCheckedArray(candidate, corners);return MS::kSuccess;'),
            ('camera line copy status', CAMERA, 'return dommy::maya::copyCheckedArray(candidate, lines);',
             'dommy::maya::copyCheckedArray(candidate, lines);return MS::kSuccess;'),
            ('normalization zero guard', LIGHT, 'normalization <= 0.0', 'normalization < 0.0'),
            ('intensity finite guard', LIGHT, '!std::isfinite(intensity) ||', ''),
            ('normalization finite guard', LIGHT, '!std::isfinite(normalization) ||', ''),
            ('negative ratio work guard', LIGHT, 'intensity < 0.0 && normalization <', 'false && normalization <'),
            ('invalid alpha fallback', LIGHT, 'float alphaVal = 1.0;', 'float alphaVal = std::numeric_limits<float>::quiet_NaN();'),
        ):
            source = (ROOT/path).read_text()
            assert old in source, 'Mutation target missing: '+label
            result = run({path: source.replace(old, new, 1)}, capture=True)
            assert result.returncode != 0, 'Undetected mutation: '+label
            print('Detected mutation: '+label, flush=True)
    else:
        result = run()
        assert result.returncode == 0, 'Draw callback regression failed'
        print('Camera ownership/draw preflight, empty/malformed light caches, alpha fallback and callback exception regression: OK')


if __name__ == '__main__':
    main()
