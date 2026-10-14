#!/usr/bin/env python3
"""Actual index parser, point ASCII writer and ASS bounds reader contracts."""

import argparse
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
PATTERN = ROOT / 'src/core/common/utility/DIndexPattern.h'
POINT = ROOT / 'src/core/DDataUtility/Data/DPData.cpp'
ASS = ROOT / 'src/modules/point/DPoint/Duplicator/DPInstancer.cpp'
LEGACY = ROOT / 'src/core/common/file/DLegacyTextData.h'
NUMBER = ROOT / 'src/core/common/file/DNumericTextRead.h'
UNSIGNED = ROOT / 'src/core/common/utility/DUnsignedDecimal.h'


def methods(changes):
    point = changes.get(POINT, POINT.read_text())
    ass = changes.get(ASS, ASS.read_text())
    macros = re.findall(r'^#define k\w+\s+[^\n]+', point, re.MULTILINE)
    limits = re.findall(r'const (?:std::size_t|unsigned long long) kMaxAssToc\w+\s*=[^;]+;', ass)
    if len(macros) != 4 or len(limits) != 3:
        raise AssertionError('Point codec/ASS source declaration inventory changed')
    result = macros + limits
    result += [extract(point, signature) for signature in (
        'MStatus DPData::writeASCII(', 'MStatus DPData::writePointArrayASCII(')]
    result.append(extract(ass, 'bool readAssBoundingBoxFromStream('))
    return '\n\n'.join(result)


def run(changes=None, mutation=False, section='all', analyze=False, sdk=None):
    changes = changes or {}
    with tempfile.TemporaryDirectory(prefix='dommy-parser-codec-') as directory:
        temporary = Path(directory)
        (temporary / 'parser_codec_methods.inc').write_text(methods(changes))
        for header in (PATTERN, LEGACY, NUMBER, UNSIGNED):
            if header not in changes:
                continue
            path = temporary / header.relative_to(ROOT / 'src/core')
            path.parent.mkdir(parents=True)
            path.write_text(changes[header])
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I' + str(temporary), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/common/file'),
            '-I' + str(ROOT / 'src/core/common/utility'), '-I' + str(ROOT / 'tests/cpp')]
        if sdk:
            command += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(sdk / 'include'),
                        '-idirafter', str(ROOT / 'tests/stubs')]
        else:
            command += ['-I' + str(ROOT / 'tests/stubs')]
        source = str(ROOT / 'tests/cpp/parser_codec_tests.cpp')
        if sdk:
            subprocess.run(command + ['-fsyntax-only', source], check=True)
        if analyze or sdk:
            subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                     '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, source], check=True)
            print('Parser/codec bodies: Clang analysis' + (' and Maya SDK syntax' if sdk else '') + ': OK')
            return
        binary = temporary / 'parser_codec_tests'
        command += ['-fno-omit-frame-pointer', '-fsanitize=address,undefined,float-cast-overflow',
                    '-fno-sanitize-recover=all', source, '-o', str(binary)]
        try:
            subprocess.run(command, check=True, capture_output=True)
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stderr.decode(errors='replace'))
            raise
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary), section], env=environment, cwd=str(temporary),
                                text=True, capture_output=True, timeout=20, **options)
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('Mutation must be detected by a runtime assertion:\n' + result.stderr)
        else:
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            print(result.stdout.strip(), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-only', action='store_true')
    parser.add_argument('--section', choices=('all', 'patterns', 'points', 'ass',
                        'legacy-numbers', 'legacy-counts', 'legacy-allocation', 'ass-tokens', 'tokens'), default='all')
    parser.add_argument('--analyze', action='store_true')
    parser.add_argument('--sdk', type=Path)
    args = parser.parse_args()
    run(section=args.section, analyze=args.analyze, sdk=args.sdk)
    if args.baseline_only or args.analyze or args.sdk:
        return
    guard = 'if (!stream.eof() || stream.bad()) return false;'
    cases = (
        ('failed pattern extraction accepted', PATTERN, guard, ''),
        ('pattern result published before stream validation', PATTERN,
         guard + '\n\n    reverse = candidateReverse;\n    indices.swap(candidate);',
         'reverse = candidateReverse;\n    indices.swap(candidate);\n    ' + guard),
        ('point ASCII inherits the host locale', POINT, 'encoded.imbue(std::locale::classic());', ''),
        ('ASS bounds inherit the host locale', ASS, 'values.imbue(std::locale::classic());', ''),
        ('legacy extraction badbit ignored', LEGACY, 'values.exceptions(std::ios::badbit);', ''),
        ('invalid numeric extraction confused with normal EOF', NUMBER,
         'if (!(input >> parsed) || !std::isfinite(parsed)) return NumericTokenRead::Invalid;',
         'if (!(input >> parsed) || !std::isfinite(parsed)) return input.eof() ? NumericTokenRead::End : NumericTokenRead::Invalid;'),
        ('numeric prefix accepted without a boundary', NUMBER,
         'if (!delimiter) return NumericTokenRead::Invalid;', '(void)delimiter;'),
        ('legacy unchecked numeric loop restored', LEGACY,
         'for (;;) {\n        const NumericTokenRead status = readFiniteDoubleToken(values, value);\n'
         '        if (status == NumericTokenRead::End) break;\n'
         '        if (status != NumericTokenRead::Value) return false;\n'
         '        components.push_back(value);\n    }',
         'while (values >> value) { components.push_back(value); }'),
        ('ASS unchecked numeric chain restored', ASS,
         'double* coordinates[] = {&parsedMin.x, &parsedMin.y, &parsedMin.z,\n'
         '\t\t                         &parsedMax.x, &parsedMax.y, &parsedMax.z};\n'
         '\t\tfor (double* coordinate : coordinates) {\n'
         '\t\t\tif (dommy::file::readFiniteDoubleToken(values, *coordinate, true) !=\n'
         '\t\t\t    dommy::file::NumericTokenRead::Value) return false;\n\t\t}',
         'if (!(values >> parsedMin.x >> parsedMin.y >> parsedMin.z >> '
         'parsedMax.x >> parsedMax.y >> parsedMax.z)) return false;'),
        ('count reader restores unsigned wrapping extraction', NUMBER,
         'std::string token;\n    if (!(input >> token)) return false;\n'
         '    return numeric::parseUnsignedDecimal(token, result, true);',
         'unsigned long long candidate = 0;\n    if (!(input >> candidate)) return false;\n'
         '    result = static_cast<std::size_t>(candidate);\n    return true;'),
        ('decimal accumulation overflow unchecked', UNSIGNED,
         'if (candidate > (std::numeric_limits<std::size_t>::max() - digit) / 10U)\n'
         '            return false;', ''),
        ('index parser accepts leading plus', PATTERN,
         'return numeric::parseUnsignedDecimal(token, value);',
         'return numeric::parseUnsignedDecimal(token, value, true);'),
    )
    for label, path, before, after in cases:
        source = path.read_text()
        expected = 2 if path == LEGACY and before == 'values.exceptions(std::ios::badbit);' else 1
        if source.count(before) != expected:
            raise AssertionError('Ambiguous mutation boundary: ' + label)
        run({path: source.replace(before, after)}, mutation=True)
        print('Detected mutation: ' + label, flush=True)


if __name__ == '__main__':
    main()
