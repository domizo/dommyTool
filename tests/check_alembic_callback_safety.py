#!/usr/bin/env python3
"""Run actual callback preparation/execution bodies with explicit command transport."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp'
REPLACE = 'src/core/common/file/DSequencePath.h'

def run(changes=None, capture=False, sdk=None):
    changes = changes or {}
    source = changes.get(SOURCE, (ROOT / SOURCE).read_text())
    methods = '\n'.join(extract(source, signature) for signature in (
        'bool prepareExportCallback(', 'void processBestEffortCallback('))
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-callback-') as directory:
        temp = Path(directory)
        (temp / 'callback_methods.inc').write_text(methods)
        if REPLACE in changes:
            path = temp / 'common/file/DSequencePath.h'
            path.parent.mkdir(parents=True)
            path.write_text(changes[REPLACE])
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(temp),
                 '-I' + str(ROOT / 'tests/cpp'), '-I' + str(ROOT / 'src/core')]
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        if sdk:
            flags += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(Path(sdk) / 'include')]
            unit = temp / 'sdk.cpp'
            unit.write_text('#include "alembic_callback_host.h"\n#include "callback_methods.inc"\n')
            subprocess.run(compiler + flags + ['-fsyntax-only', str(unit)], check=True)
            subprocess.run(compiler + flags + ['--analyze', '-Xanalyzer', '-analyzer-werror',
                           '-Xanalyzer', '-analyzer-output=text', '-o', os.devnull, str(unit)], check=True)
            print('Complete callbacks: Maya SDK syntax/analysis passed; command execution not linked')
            return
        flags += ['-I' + str(ROOT / 'tests/stubs'), '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        binary = temp / 'test'
        subprocess.run(compiler + flags + [str(ROOT / 'tests/cpp/alembic_callback_tests.cpp'), '-o', str(binary)],
                       check=True, capture_output=capture, text=True)
        return subprocess.run([str(binary), str(temp / 'existing.abc')], capture_output=capture, text=True,
                              timeout=30, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                                                   ('0' if sys.platform == 'darwin' else '1')))

if __name__ == '__main__':
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        for label, path, old, new in (
            ('frame precision', SOURCE, 'value.precision(std::numeric_limits<double>::max_digits10);', 'value.precision(6);'),
            ('bounds precision', SOURCE, 'array.precision(std::numeric_limits<double>::max_digits10);', 'array.precision(6);'),
            ('numeric locale', SOURCE, '.imbue(std::locale::classic())', '.imbue(std::locale())'),
            ('NUL admission', SOURCE, "callback.find('\\0') != std::string::npos", 'false'),
            ('replacement maximum', REPLACE, 'size > maximumBytes', 'false'),
            ('all replacements', REPLACE, 'occurrence < count', 'occurrence < 1U'),
            ('staged preparation', SOURCE, 'std::string candidate(callback);', 'std::string& candidate = callback;'),
            ('bounds numeric admission', SOURCE, 'if (!std::isfinite(values[index]))', 'if (false)'),
            ('returned callback status', SOURCE, 'if (!status)', 'if ((void)status, false)'),
        ):
            source = (ROOT / path).read_text()
            assert old in source, label
            result = run({path: source.replace(old, new)}, capture=True)
            assert result.returncode != 0, 'Undetected mutation: ' + label
            print('Detected mutation: ' + label, flush=True)
    else:
        assert run().returncode == 0
