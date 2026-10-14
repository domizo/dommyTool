#!/usr/bin/env python3
"""Exercise owned Wood math/draw callbacks and the shared Field I/O boundary.

Recording GL/Maya doubles verify control flow, not native rendering or DG behavior.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps
from check_vdb_field_safety import bodies as vdb_bodies
from check_curve_path_safety import run as run_field

ROOT = Path(__file__).resolve().parent.parent
WOOD = 'noise/DWoodNoise.h'
FIELD = 'common/utility/DMayaFieldData.h'


def run_wood(override=None, mutation=False):
    guard = (ROOT / 'src/core/common/utility/DLegacyDrawGuard.h').read_text()
    draw = (ROOT / 'src/core/common/utility/DMayaFieldDraw.h').read_text()
    code = ['namespace dommy { namespace maya {',
            extract(guard, 'class ScopedLegacyGL final') + ';',
            extract(guard, 'class ScopedLegacyGLPrimitive final') + ';',
            extract(draw, 'inline void drawFieldIcon('), '}}']
    for name, path in (
        ('DCurlNoiseField', 'src/modules/particles/particle/field/DCurlNoiseField.cpp'),
        ('DOpenVDBField', 'src/modules/volume/VDB/Dynamics/DOpenVDBField.cpp')):
        code += [extract((ROOT / path).read_text(), 'void ' + name + '::draw('),
                 'DOMMY_DRAW_CATCH("' + name + '")']
    with tempfile.TemporaryDirectory(prefix='dommy-owned-noise-fields-') as directory:
        temp = Path(directory)
        (temp / 'owned_field_draw.inc').write_text('\n'.join(code))
        if override is not None:
            path = temp / WOOD
            path.parent.mkdir(parents=True)
            path.write_text(override)
        binary = temp / 'test'
        vendor = ROOT / 'third_party/libnoise/source'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-parameter',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(ROOT / 'src'),
            '-I' + str(ROOT / 'src/core'), '-isystem', str(vendor),
            str(ROOT / 'tests/cpp/owned_noise_field_tests.cpp'),
            str(ROOT / 'src/core/noise/PerlinNoise.cpp'), str(vendor / 'modulebase.cpp'),
            '-o', str(binary)]
        try:
            subprocess.run(command, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as error:
            raise RuntimeError(error.stdout + error.stderr) from error
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary)], cwd=str(temp), capture_output=True, text=True,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')),
            **options)
        if mutation:
            assert result.returncode != 0 and 'Assertion' in result.stderr, result.stderr
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            print(result.stdout.strip())


def main():
    run_wood()
    wood = (ROOT / 'src/core' / WOOD).read_text()
    for old, new in (
        ('settings_.ringFrequency);', '1.0);'),
        ('settings_.grainAmount, grain)', '0.0, grain)'),
        ('+ settings_.bias;', ';'),
        ('settings_.warpAmount != 0.0', 'false'),
    ):
        assert wood.count(old) == 1, old
        run_wood(wood.replace(old, new), mutation=True)

    field = (ROOT / 'src/core' / FIELD).read_text()
    run_field(ROOT, vdb_bodies(), suite='vdb_field', arguments=[])
    for old, new in (
        ('DOMMY_RETURN_IF_FAILURE(elements.jumpToElement(logicalIndex));',
         'elements.jumpToElement(logicalIndex);'),
        ('return result.copy(values);', 'result.copy(values); return MS::kSuccess;'),
        ('DOMMY_RETURN_IF_FAILURE(element.set(payload));', 'element.set(payload);'),
        ('DOMMY_RETURN_IF_FAILURE(array.set(builder));', 'array.set(builder);'),
        ('return data.setClean(requested);', 'data.setClean(requested); return MS::kSuccess;'),
        ('builder.addElement(logicalIndex, &status)', 'builder.addElement(logicalIndex - logicalIndex, &status)'),
    ):
        assert field.count(old) == 1, old
        changed = vdb_bodies().replace(extract(field, 'namespace dommy {'),
                                      extract(field.replace(old, new), 'namespace dommy {'))
        try:
            run_field(ROOT, changed, suite='vdb_field', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'vdb_field_tests', 'Compile failure is not mutation detection'
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, error.stderr
        else:
            raise AssertionError('Undetected Field mutation: ' + old)
    print('Owned Wood/Field negative controls: 10 runtime mutations rejected')


if __name__ == '__main__':
    main()
