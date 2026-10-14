#!/usr/bin/env python3
"""Exercise actual clip/bounds helpers with explicit OpenVDB API-shaped doubles."""
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
DISPLAY = 'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp'
UTILITY = 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp'
MATH = 'src/modules/volume/VDB/Common/DVdbViewportMath.h'


def bodies(overrides=None):
    overrides = overrides or {}
    source = overrides.get(DISPLAY, (ROOT / DISPLAY).read_text())
    update = extract(source, 'void DOpenVDBDisplayVP2Override::updateDG(')
    assert 'buildVdbViewportClipBox(grids, axis, position, width, bbox, hasActiveBounds)' in update
    assert 'isClip = hasActiveBounds;' in update
    assert 'invalid viewport clip bounds or settings' in update
    assert 'static_cast<float>(worldMax' not in update
    common = overrides.get(UTILITY, (ROOT / UTILITY).read_text())
    first = extract(common, 'bool getActiveVoxelWorldBounds(')
    second = extract(common[common.index(first) + len(first):], 'bool getActiveVoxelWorldBounds(')
    spatial = (ROOT / 'src/modules/volume/VDB/Edit/DOpenVDBSpatialPolicy.h').read_text()
    filter_source = (ROOT / 'src/modules/volume/VDB/Edit/DOpenVDBFilterPolicy.h').read_text()
    return '\n'.join([
        'namespace dommy { namespace vdb { const int kGridCoordinatePadding=4096;',
        extract(filter_source, 'inline bool filterCoordinate('),
        extract(spatial, 'inline bool validGridBox('),
        extract(spatial, 'inline bool indexGridBox('), '}}', second, first,
        extract(source, 'bool buildVdbViewportClipBox(')])


def run(overrides=None, math=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-viewport-clip-') as directory:
        temp = Path(directory)
        (temp / 'vdb_viewport_clip.inc').write_text(bodies(overrides))
        (temp / 'DVdbViewportMath.h').write_text(math or (ROOT / MATH).read_text())
        flags = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I' + str(temp), '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'src/core'),
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer']
        binary = temp / 'test'
        subprocess.run(flags + [str(ROOT / 'tests/cpp/vdb_viewport_clip_tests.cpp'), '-o', str(binary)],
                       check=True, capture_output=capture, text=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary)], check=True, env=environment, capture_output=capture, text=True)


def mutations():
    display = (ROOT / DISPLAY).read_text()
    utility = (ROOT / UTILITY).read_text()
    math = (ROOT / MATH).read_text()
    cases = [
        ('voxel center policy lost', DISPLAY, 'world, false)', 'world, true)'),
        ('index coordinate admission removed', DISPLAY,
         'if (!dommy::vdb::indexGridBox(openvdb::BBoxd(indexMinimum, indexMaximum), region))', 'if (false)'),
        ('grid budget removed', DISPLAY, 'grids.size() > dommy::vdb::kMaxVdbGridCount', 'false'),
        ('fixed padding changed', MATH, 'extent[component] += 0.8f;', 'extent[component] += 0.0f;'),
        ('invalid slider accepted', MATH, 'position < 0.0f || position > 1.0f ||', ''),
        ('axis admitted outside enum', MATH, 'axis < 0 || axis > 2 ||', ''),
        ('output published before index admission', DISPLAY,
         "// Match Clip's world-to-index conversion", "output = candidate;\n    // Match Clip's world-to-index conversion"),
        ('float extent admission removed', MATH,
         '!numeric::checkedFloatCast(maximum[component] - minimum[component],\n                                       extent[component])',
         '(extent[component] = static_cast<float>(maximum[component] - minimum[component]), false)'),
        ('two-corner bounds returned', UTILITY, 'worldBounds = grid.transform().indexToWorld(indexBounds);',
         'worldBounds = openvdb::BBoxd(grid.transform().indexToWorld(indexBounds.min()), grid.transform().indexToWorld(indexBounds.max()));'),
    ]
    for label, path, old, new in cases:
        original = {DISPLAY: display, UTILITY: utility, MATH: math}[path]
        assert old in original, label
        changed = original.replace(old, new)
        try:
            run(math=changed if path == MATH else None,
                overrides={path: changed} if path != MATH else None, capture=True)
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('runtime error:', 'ERROR: AddressSanitizer')), 'Compile failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    if '--mutations' in sys.argv:
        mutations()
    else:
        run()
