#!/usr/bin/env python3
"""Production point collection/export with Maya seams and real temporary files.

Argument parsing and count/byte policy limits are explicit fixture seams. The
serializer, checked array/handle helpers and atomic file implementation are real.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/core/DDataUtility/IO/DPointDataExportCmd.cpp'


def generate():
    source = (ROOT / SOURCE).read_text()
    start = source.index('const unsigned int kPointRecordsPerLine')
    bodies = [source[start:source.index(';', start) + 1]]
    bodies += [extract(source, signature) for signature in (
        'bool isWithinLegacyTextBudget(', 'bool isFinite(',
        'MStatus collectPoints(', 'MStatus DPointDataExportCmd::doIt(')]
    # Exercise the identical limit logic with small budgets, without allocating
    # ten million native points or a 512 MiB text stream in the test host.
    return '\n\n'.join(bodies).replace(
        'dommy::file::kMaxLegacyTextRecords', 'export_host::recordBudget').replace(
        'dommy::file::kMaxLegacyTextBytes', 'export_host::byteBudget')


def check(body, mutation=False):
    with tempfile.TemporaryDirectory(prefix='dommy-point-export-') as directory:
        temp = Path(directory)
        (temp / 'maya').mkdir()
        for name in ('MGlobal', 'MString'):
            (temp / ('maya/' + name + '.h')).write_text('#pragma once\n')
        (temp / 'point_export_methods.inc').write_text(body)
        (temp / 'destination-directory').mkdir()
        binary = temp / 'point_export_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp),
            '-I' + str(ROOT / 'tests/stubs'), '-I' + str(ROOT / 'src/core'),
            str(ROOT / 'tests/cpp/point_export_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True, capture_output=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        result = subprocess.run([str(binary), str(temp)], env=env,
                                text=True, capture_output=True, timeout=20)
        if mutation:
            assert result.returncode != 0, 'Mutation unexpectedly passed'
            assert 'Assertion' in result.stderr or 'runtime error:' in result.stderr, result.stderr
        else:
            if result.returncode:
                sys.stderr.write(result.stderr)
                raise AssertionError('Point export fixture failed')
            assert not list(temp.glob('*.dommytool-*.tmp')), 'Temporary output leaked'
            print(result.stdout.strip())


def mutations(body):
    cases = (
        ('particle conversion failure ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(',
         '(void)(dommy::maya::buildCheckedArray('),
        ('original unchecked particle allocation/write restored',
         '''DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            positions.length(),
            [&](unsigned int index, MPoint& point) -> MStatus {
                point = MPoint(positions[index]);
                return isFinite(point) ? MStatus(MS::kSuccess)
                                       : MStatus(MS::kInvalidParameter);
            }, points));''',
         '''points.setLength(positions.length());
        for (unsigned int index = 0; index < positions.length(); ++index) {
            points[index] = MPoint(positions[index]);
        }'''),
        ('remaining export quota not passed to collector',
         'export_host::recordBudget - totalPointCount,', 'export_host::recordBudget,'),
        ('mesh payload fetched before count admission',
         'const int rawCount = function.numVertices(&status);',
         'DOMMY_RETURN_IF_FAILURE(function.getPoints(points, MSpace::kObject));\n'
         '        const int rawCount = function.numVertices(&status);'),
        ('surface product bound ignored',
         '!dommy::generation::checkedProduct(uCount, vCount, maximumPointCount,',
         '!dommy::generation::checkedProduct(uCount, vCount, std::numeric_limits<std::size_t>::max(),'),
        ('particle returned count mismatch ignored',
         'readCount != expectedPointCount || positions.length() != expectedPointCount',
         '(static_cast<void>(readCount), false) || positions.length() != expectedPointCount'),
        ('actual geometry count mismatch ignored',
         'if (points.length() != expectedPointCount) {', 'if (false) {'),
        ('point-data copy failure ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(source, points));',
         '(void)dommy::maya::copyCheckedArray(source, points);'),
        ('non-finite point accepted',
         'if (!isFinite(points[index])) {', 'if (false) {'),
        ('text-size rejection omitted',
         'static_cast<unsigned long long>(offset) <=\n               export_host::byteBudget',
         '(offset >= 0)'),
        ('failed commit reported as success',
         'if (!pending.commit()) {', 'if (false && !pending.commit()) {'),
        ('numeric output inherits the host locale',
         'serialized.imbue(std::locale::classic());', 'serialized.imbue(std::locale());'),
    )
    for label, old, new in cases:
        assert old in body, 'Mutation not applied: ' + label
        check(body.replace(old, new, 1), mutation=True)
        print('Detected mutation:', label)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--sdk', type=Path)
    args = parser.parse_args()
    body = generate()
    try:
        check(body)
        if args.mutations:
            mutations(body)
    except subprocess.CalledProcessError as error:
        sys.stderr.write(error.stderr.decode() if isinstance(error.stderr, bytes) else error.stderr or '')
        raise
    if args.sdk:
        from check_motion_duplicator_safety import sdk_check
        sdk_check(ROOT, args.sdk, source_path=SOURCE, label='DPointDataExportCmd')


if __name__ == '__main__':
    main()
