#!/usr/bin/env python3
"""Execute actual callback bodies/grid naming fragments under allocation failure."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
CALLBACK = ROOT / 'src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp'
ATTRIBUTE = ROOT / 'src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp'
FRACTURE = ROOT / 'src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp'
LIGHT_TRANSLATOR = ROOT / 'src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp'
LIGHT_EXPORT = ROOT / 'src/modules/point/DPoint/render/light/DPLightGen.cpp'


def methods(changes):
    source = changes.get(CALLBACK, CALLBACK.read_text())
    result = [extract(source, signature) for signature in (
        'bool prepareExportCallback(', 'void processBestEffortCallback(')]
    for path, end, signature in (
        (ATTRIBUTE, 'mgrid->setName(name.str());',
         'void nameAttributeGrid(NameGrid* mgrid, const std::string& gridName, int attributeTupleSize, int component)'),
        (FRACTURE, 'grid->setName(name.str());',
         'void nameFractureGrid(NameGrid* grid, std::size_t index)'),
    ):
        source = changes.get(path, path.read_text())
        start = 'std::ostringstream name;'
        if source.count(start) != 1 or source.count(end) != 1:
            raise AssertionError('Ambiguous naming fragment: ' + path.name)
        beginning = source.index(start)
        ending = source.index(end, beginning) + len(end)
        result.append(signature + ' {\n' + source[beginning:ending] + '\n}')
    for path, end, signature, prefix, output in (
        (LIGHT_TRANSLATOR, 'lightNodeNames.push_back(stream.str());',
         'std::string translatorLightName(const std::string& nodePrefix, std::size_t i)',
         'std::vector<std::string> lightNodeNames;', 'lightNodeNames.at(0U)'),
        (LIGHT_EXPORT, 'std::string lightNameStr = stream.str();',
         'std::string exportLightName(std::size_t i)', '', 'lightNameStr'),
    ):
        source = changes.get(path, path.read_text())
        # End statements change when the success guard is implemented.
        end = end.replace('stream.str()', 'dommy::text::checkedStreamText(stream, "Arnold light name formatting failed")') \
            if 'dommy::text::checkedStreamText(stream, "Arnold light name formatting failed")' in source else end
        if source.count(end) != 1:
            raise AssertionError('Ambiguous light naming fragment: ' + path.name)
        ending = source.index(end) + len(end)
        beginning = source.rindex('std::ostringstream stream;', 0, ending)
        result.append(signature + ' {\n' + prefix + '\n' + source[beginning:ending] +
                      '\nreturn ' + output + ';\n}')
    return '\n\n'.join(result)


def run(changes=None, mutation=False, section='all', analyze=False):
    with tempfile.TemporaryDirectory(prefix='dommy-text-formatting-') as directory:
        temporary = Path(directory)
        (temporary / 'text_formatting_methods.inc').write_text(methods(changes or {}))
        binary = temporary / 'text_formatting_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I' + str(temporary), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'tests/cpp'), '-I' + str(ROOT / 'tests/stubs')]
        if analyze:
            command += ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]
        else:
            command += ['-fno-omit-frame-pointer', '-fsanitize=address,undefined,float-cast-overflow',
                        '-fno-sanitize-recover=all', '-o', str(binary)]
        command += [str(ROOT / 'tests/cpp/text_formatting_tests.cpp')]
        try:
            subprocess.run(command, check=True, capture_output=True)
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stderr.decode(errors='replace'))
            raise
        if analyze:
            print('Text formatting fixture Clang analysis: OK')
            return
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
    section = ('grids' if '--grids-only' in sys.argv else
               'callbacks' if '--callbacks-only' in sys.argv else
               'lights' if '--lights-only' in sys.argv else 'all')
    run(section=section, analyze='--analyze' in sys.argv)
    if '--baseline-only' in sys.argv or '--analyze' in sys.argv:
        return
    cases = (
        ('frame stream failure ignored', CALLBACK, 'if (!value) return false;', ''),
        ('scalar bounds stream failure ignored', CALLBACK, 'if (!scalar || !array) return false;',
         'if (!array) return false;'),
        ('array bounds stream failure ignored', CALLBACK, 'if (!scalar || !array) return false;',
         'if (!scalar) return false;'),
        ('attribute name stream failure ignored', ATTRIBUTE,
         'if (!name) throw std::runtime_error("VDB attribute name formatting failed");', ''),
        ('fracture name stream failure ignored', FRACTURE,
         'if (!name) throw std::runtime_error("VDB fracture name formatting failed");', ''),
        ('fracture name inherits global locale', FRACTURE,
         'name.imbue(std::locale::classic());', ''),
        ('light translator accepts partial name', LIGHT_TRANSLATOR,
         'dommy::text::checkedStreamText(stream, "Arnold light name formatting failed")', 'stream.str()'),
        ('light export accepts partial name', LIGHT_EXPORT,
         'dommy::text::checkedStreamText(stream, "Arnold light name formatting failed")', 'stream.str()'),
        ('light translator inherits grouped locale', LIGHT_TRANSLATOR,
         'stream.imbue(std::locale::classic());', ''),
        ('light export inherits grouped locale', LIGHT_EXPORT,
         'stream.imbue(std::locale::classic());', ''),
    )
    for label, path, before, after in cases:
        source = path.read_text()
        if source.count(before) != 1:
            raise AssertionError('Ambiguous mutation boundary: ' + label)
        run({path: source.replace(before, after, 1)}, mutation=True)
        print('Detected mutation: ' + label, flush=True)
    translator = extract(LIGHT_TRANSLATOR.read_text(), 'void DPLightTranslatorAi::Update(')
    assert translator.index('lightNodeNames.push_back(') < translator.rindex('if (!destroyGeneratedNodes(previousNodes))')


if __name__ == '__main__':
    main()
