#!/usr/bin/env python3
"""Keep every standalone C++ regression command covered exactly once by CTest."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
CI_OPTIONS = ('-DCMAKE_BUILD_TYPE=Debug', '-DDOMMY_BUILD_TESTS=ON',
              '-DDOMMY_ENABLE_SANITIZERS=ON', '-DDOMMY_BUILD_PLUGIN=OFF')
CI_PYTHON_VERSION = '3.12'


def canonical_command(arguments):
    if not arguments:
        raise ValueError('Missing test command')
    return (Path(arguments[0]).name,) + tuple(
        value.replace('$repo_root', str(ROOT)) for value in arguments[1:])


def standalone_commands(source):
    begin = 'if [[ "$run_runtime_tests" == true ]]; then\n'
    end = '\nfi\n\nforbid_regex() {'
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError('The standalone runtime/source boundary is ambiguous')
    block = source.split(begin, 1)[1].split(end, 1)[0]
    commands = []
    for line in block.splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        arguments = shlex.split(line)
        if len(arguments) < 2 or arguments[0] != 'python3' or not arguments[1].startswith('$repo_root/tests/'):
            raise ValueError('Unrecognized standalone regression command: ' + line)
        commands.append(canonical_command(arguments))
    if not commands or len(commands) != len(set(commands)):
        raise ValueError('Empty or duplicated standalone regression inventory')
    return commands


def verify_coverage(required, tests):
    enabled = [test for test in tests if not any(
        item['name'] == 'DISABLED' and item['value'] for item in test.get('properties', []))]
    registered = Counter()
    for test in enabled:
        if not test.get('command'):
            raise ValueError('CTest cannot resolve the enabled test command for ' +
                             test.get('name', '<unnamed>') + '; build the test binaries first')
        registered[canonical_command(test['command'])] += 1
    for command in required:
        if registered[command] != 1:
            raise ValueError('CTest must register exactly one copy of: ' + ' '.join(command))
    aggregate = [test for test in enabled if test['name'] == 'cpp_regressions']
    expected = ('bash', str(ROOT / 'tests/check_cpp_regressions.sh'), '--source-only')
    if len(aggregate) != 1 or canonical_command(aggregate[0]['command']) != expected:
        raise ValueError('CTest aggregate must retain source guards without rerunning runtime suites')


def workflow_job(source, name):
    match = re.search(r'^  ' + re.escape(name) + r':\n(.*?)(?=^  [A-Za-z][\w-]*:\n|\Z)',
                      source, re.M | re.S)
    if not match:
        raise ValueError('Missing CI job: ' + name)
    return match.group(1)


def verify_python_setup(job):
    setups = list(re.finditer(
        r'^      - uses: actions/setup-python@[0-9a-f]{40}[^\n]*\n(.*?)(?=^      - |\Z)',
        job, re.M | re.S))
    if len(setups) != 1:
        raise ValueError('CI validation job requires one revision-pinned Python setup')
    version = re.search(r'^\s+python-version:\s*([\'"])([^\'"\n]+)\1\s*$',
                        setups[0].group(1), re.M)
    first_run = re.search(r'^        run:', job, re.M)
    if not version or version.group(2) != CI_PYTHON_VERSION:
        raise ValueError('CI validation Python must be ' + CI_PYTHON_VERSION)
    if not first_run or setups[0].start() > first_run.start():
        raise ValueError('CI must select Python before running validation commands')


def verify_ci(source):
    for name in ('python-publication', 'sanitizers'):
        verify_python_setup(workflow_job(source, name))
    job = workflow_job(source, 'sanitizers')
    timeout = re.search(r'^    timeout-minutes:\s*(\d+)\s*$', job, re.M)
    parallel = re.search(r'^\s+run:\s*ctest[^\n]* --parallel (\d+)\s*$', job, re.M)
    if not timeout or int(timeout.group(1)) < 30:
        raise ValueError('Sanitizer CI requires at least 30 minutes for build/tests/analysis')
    if not parallel or int(parallel.group(1)) < 2:
        raise ValueError('Sanitizer CTest invocation must enable bounded parallel execution')
    step = re.search(r'^      - name: Run safety tests\n(.*?)(?=^      - |\Z)', job, re.M | re.S)
    if not step or not re.search(r'^        working-directory:\s*build\s*$', step.group(1), re.M):
        raise ValueError('CTest must run inside the build directory for CMake 3.15 compatibility')
    invocation = re.search(r'^        run:\s*(ctest[^\n]+)$', step.group(1), re.M)
    if not invocation:
        raise ValueError('Missing CTest invocation in the validation step')
    arguments = shlex.split(invocation.group(1))
    if any(argument.split('=', 1)[0] == '--test-dir' for argument in arguments):
        raise ValueError('CTest --test-dir requires CMake 3.20 and must not be used by this 3.15 baseline')
    filters = {'-E', '-R', '-L', '-LE', '-I', '--exclude-regex', '--tests-regex',
               '--label-regex', '--label-exclude', '--exclude-label-regex',
               '--tests-information', '--rerun-failed'}
    if any(argument.split('=', 1)[0] in filters for argument in arguments):
        raise ValueError('Sanitizer CI must execute the complete configured test inventory')
    for option in CI_OPTIONS:
        if option not in job:
            raise ValueError('Missing archival sanitizer CI option: ' + option)


def require_rejection(callback, label):
    try:
        callback()
    except ValueError:
        return
    raise ValueError('Inventory self-test failed to detect: ' + label)


def load_ctest_tests(build_dir, run=subprocess.run):
    # JSON inventory is available from CMake 3.14. Use cwd rather than the
    # --test-dir option introduced in 3.20, preserving the declared 3.15 baseline.
    result = run(['ctest', '--show-only=json-v1'], cwd=str(build_dir.resolve()),
                 check=True, stdout=subprocess.PIPE, text=True)
    return json.loads(result.stdout)['tests']


def self_test(required, ci_source):
    tests = [{'name': 'runtime_' + str(index), 'command': list(command)}
             for index, command in enumerate(required)]
    tests.append({'name': 'cpp_regressions', 'command':
                  ['bash', str(ROOT / 'tests/check_cpp_regressions.sh'), '--source-only']})
    verify_coverage(required, tests)
    require_rejection(lambda: verify_coverage(required, tests[1:]), 'lost runtime coverage')
    require_rejection(lambda: verify_coverage(required, tests + [tests[0]]), 'duplicated runtime execution')
    disabled = dict(tests[0], properties=[{'name': 'DISABLED', 'value': True}])
    require_rejection(lambda: verify_coverage(required, [disabled] + tests[1:]), 'disabled runtime coverage')
    missing_command = dict(tests[0])
    del missing_command['command']
    require_rejection(lambda: verify_coverage(required, [missing_command] + tests[1:]), 'unresolved enabled command')
    altered = tests[:-1] + [{'name': 'cpp_regressions', 'command': tests[-1]['command'][:-1]}]
    require_rejection(lambda: verify_coverage(required, altered), 'aggregate runtime duplication')
    job = workflow_job(ci_source, 'sanitizers')
    short_timeout = re.sub(r'(?m)^(    timeout-minutes:)\s*\d+', r'\1 10', job, count=1)
    sequential = re.sub(r'(?m)^(\s+run:\s*ctest[^\n]*) --parallel \d+', r'\1', job, count=1)
    require_rejection(lambda: verify_ci(ci_source.replace(job, short_timeout, 1)),
                      'short job timeout')
    require_rejection(lambda: verify_ci(ci_source.replace(job, sequential, 1)), 'sequential CI tests')
    for option in CI_OPTIONS:
        require_rejection(lambda: verify_ci(ci_source.replace(option, '', 1)), 'lost CI option ' + option)
    filtered = job.replace('run: ctest ', 'run: ctest -E core_safety ', 1)
    require_rejection(lambda: verify_ci(ci_source.replace(job, filtered, 1)), 'filtered CI coverage')
    for name in ('python-publication', 'sanitizers'):
        current = workflow_job(ci_source, name)
        absent = current.replace('actions/setup-python@', 'missing/setup-python@', 1)
        require_rejection(lambda: verify_ci(ci_source.replace(current, absent, 1)), name + ' missing Python setup')
        unpinned = re.sub(r'actions/setup-python@[0-9a-f]{40}', 'actions/setup-python@v5', current, count=1)
        require_rejection(lambda: verify_ci(ci_source.replace(current, unpinned, 1)), name + ' unpinned Python setup')
        for version in ('3.10', '3.13'):
            altered = re.sub(r'(?m)^(\s+python-version:)[^\n]+', r"\1 '" + version + "'", current, count=1)
            require_rejection(lambda: verify_ci(ci_source.replace(current, altered, 1)),
                              name + ' incompatible Python ' + version)
        setup_start = current.index('      - uses: actions/setup-python@')
        setup_end = current.index('\n      - ', setup_start + 1)
        late_setup = current[:setup_start] + current[setup_end:] + '\n' + current[setup_start:setup_end]
        require_rejection(lambda: verify_ci(ci_source.replace(current, late_setup, 1)), name + ' late Python setup')
    for directory in ('', '        working-directory: .\n'):
        altered = job.replace('        working-directory: build\n', directory, 1)
        require_rejection(lambda: verify_ci(ci_source.replace(job, altered, 1)), 'wrong CTest working directory')
    newer_option = job.replace('run: ctest ', 'run: ctest --test-dir build ', 1)
    require_rejection(lambda: verify_ci(ci_source.replace(job, newer_option, 1)), 'newer CTest option')

    fixture_dir = Path('inventory fixture with spaces')

    def fixture_run(command, **options):
        if command != ['ctest', '--show-only=json-v1'] or options.get('cwd') != str(fixture_dir.resolve()):
            raise ValueError('CTest loader must pass its build directory via cwd, not command arguments')
        if options.get('check') is not True or options.get('text') is not True:
            raise ValueError('CTest inventory transport must propagate failure and decode text')
        return SimpleNamespace(stdout=json.dumps({'tests': tests}))

    if load_ctest_tests(fixture_dir, run=fixture_run) != tests:
        raise ValueError('CTest JSON inventory changed while loading')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', required=True, type=Path)
    arguments = parser.parse_args()
    required = standalone_commands((ROOT / 'tests/check_cpp_regressions.sh').read_text())
    ci_source = (ROOT / '.github/workflows/cpp-safety-tests.yml').read_text()
    self_test(required, ci_source)
    verify_coverage(required, load_ctest_tests(arguments.build_dir))
    verify_ci(ci_source)
    print('CTest covers all {} standalone regression commands exactly once; source guards and CI policy: OK'.format(
        len(required)))


if __name__ == '__main__':
    main()
