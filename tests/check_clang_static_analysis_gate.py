#!/usr/bin/env python3
"""Exercise the real shell gate with controlled, analysis-only Clang inputs.

The proxy retains the production analyzer arguments and substitutes only the
translation unit. The intentionally faulty probe is never built or executed.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / 'tests/run_clang_static_analysis.sh'
SUCCESS = 'Clang static analysis: OK'
FINDING = 'Use of memory after it is freed'
ERROR_FLAG = '    -Xanalyzer -analyzer-werror \\\n'
PROXY = '''import json
import os
import subprocess
import sys

arguments = sys.argv[1:]
with open(os.environ['DOMMY_GATE_LOG'], 'a') as log:
    log.write(json.dumps(arguments) + '\\n')
result = subprocess.run([os.environ['DOMMY_GATE_CLANG']] + arguments[:-1] +
                        [os.environ['DOMMY_GATE_PROBE']])
sys.exit(result.returncode)
'''


def run_gate(script, probe, temporary, compiler):
    log = temporary / 'invocations.jsonl'
    log.write_text('')
    environment = dict(os.environ, CLANGXX=str(temporary / 'clang proxy'),
                       DOMMY_GATE_CLANG=compiler, DOMMY_GATE_PROBE=str(probe),
                       DOMMY_GATE_LOG=str(log))
    result = subprocess.run(['bash', str(script)], env=environment,
                            text=True, capture_output=True, timeout=60)
    invocations = [json.loads(line) for line in log.read_text().splitlines()]
    return result, invocations


def require_failed_finding(result):
    if result.returncode == 0 or FINDING not in result.stderr or SUCCESS in result.stdout:
        raise AssertionError('Analyzer finding must fail the shell gate before its success message:\n' +
                             result.stdout + result.stderr)


def main():
    compiler = shutil.which(os.environ.get('CLANGXX', 'clang++'))
    if not compiler:
        raise RuntimeError('Clang is required for the static-analysis gate regression')
    source = SCRIPT.read_text()
    if source.count(ERROR_FLAG) != 1:
        raise AssertionError('The analyzer-error flag must be present exactly once')

    with tempfile.TemporaryDirectory(prefix='dommy analyzer gate ') as directory:
        temporary = Path(directory)
        proxy = temporary / 'clang proxy'
        proxy.write_text('#!' + sys.executable + '\n' + PROXY)
        proxy.chmod(0o700)
        clean = temporary / 'clean.cpp'
        clean.write_text('int main() { return 0; }\n')
        faulty = temporary / 'analysis-only.cpp'
        faulty.write_text('int main() { int* value = new int(1); delete value; return *value; }\n')

        result, invocations = run_gate(SCRIPT, clean, temporary, compiler)
        if result.returncode or SUCCESS not in result.stdout or len(invocations) != 6:
            raise AssertionError('All six clean analyzer invocations must succeed:\n' +
                                 result.stdout + result.stderr)
        for arguments in invocations:
            if '--analyze' not in arguments or '-std=c++11' not in arguments:
                raise AssertionError('The proxy must retain the real analyzer mode and language baseline')
            if not Path(arguments[-1]).is_file():
                raise AssertionError('The production gate must select an existing translation unit')

        result, invocations = run_gate(SCRIPT, faulty, temporary, compiler)
        require_failed_finding(result)
        if len(invocations) != 1:
            raise AssertionError('The shell gate must stop at the first analyzer finding')

        # Restore the historical missing flag in a disposable script. Clang
        # emits the same finding with exit zero; the regression must reject it.
        mutated = temporary / 'run_clang_static_analysis.sh'
        mutated.write_text(source.replace(ERROR_FLAG, '', 1))
        result, invocations = run_gate(mutated, faulty, temporary, compiler)
        if result.returncode or len(invocations) != 6 or FINDING not in result.stderr or SUCCESS not in result.stdout:
            raise AssertionError('The negative control must reproduce the historical warning-only false success')
        try:
            require_failed_finding(result)
        except AssertionError:
            pass
        else:
            raise AssertionError('The regression did not detect removal of -analyzer-werror')

    print('Clang analyzer gate: clean success, finding failure, fail-fast and missing-flag mutation detected')


if __name__ == '__main__':
    main()
