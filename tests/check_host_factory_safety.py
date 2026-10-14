#!/usr/bin/env python3
"""Run every production pointer-factory body with allocation/constructor faults.

Fake concrete types preserve private constructor access and host ownership, but
do not exercise Maya/Arnold constructors or the hosts' null-return behavior.
"""
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
from check_active_cpp_safety import strip_comments_and_literals


def generate(root, overrides=None):
    overrides = overrides or {}
    methods, cases = [], []
    for path in sorted((root / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        source = overrides.get(path, path.read_text())
        code = strip_comments_and_literals(source)
        for match in re.finditer(r'\b(?:creator|Creator)\s*\(([^;{}]*)\)\s*\{', code):
            # creator() signatures can repeat within one translation unit;
            # start at this match, not at the file's first same-spelled body.
            full = extract(source[match.start():], source[match.start():match.end()])
            body = full[full.index('{'):]
            allocation = re.search(r'\bnew\s+(\w+)\b', strip_comments_and_literals(body))
            if not allocation:
                continue
            if not re.search(r'::dommy::(?:maya|arnold)::createObject\(', body):
                raise AssertionError('Unguarded host factory: ' + str(path))
            name = allocation.group(1)
            owner = re.search(r'(\w+)::\s*$', code[:match.start()])
            if owner and owner.group(1) != name:
                raise AssertionError('Factory creates a different type: ' +
                                     str(path) + ': ' + owner.group(1) + ' -> ' + name)
            parameters = match.group(1).strip()
            if parameters not in ('', 'const MObject& obj'):
                raise AssertionError('Review new factory arguments: ' + parameters)
            index = len(cases)
            namespace = 'factory_' + str(index)
            methods.append('namespace ' + namespace + ' { class ' + name + ''' {
                int argument_;
                explicit ''' + name + '''(const MObject& object = MObject())
                    : argument_(object.value) { construct(); }
              public:
                ~''' + name + '''() { --liveObjects; }
                static ''' + name + '* create(' + parameters + ') ' + body + '''
                static void* invoke() { return create(''' + ('MObject(7)' if parameters else '') + '''); }
                static void destroy(void* value) {
                    ''' + name + '* object = static_cast<' + name + '''*>(value);
                    assert(object->argument_ == ''' + ('7' if parameters else '0') + ''');
                    delete object;
                }
            }; }''')
            cases.append('{"' + str(path.relative_to(root)) + ':' + name + '", ' +
                         namespace + '::' + name + '::invoke, ' + namespace + '::' + name + '::destroy},')
    if len(cases) < 362:
        raise AssertionError('Factory inventory unexpectedly shrank: ' + str(len(cases)))
    arnold = (root / 'src/integrations/arnold/DArnoldEntryPointGuard.h').read_text()
    return (extract(arnold, 'namespace dommy {') + '\n' + '\n'.join(methods) +
            '\nconst Factory factories[] = {\n' + '\n'.join(cases) + '\n};\n'), len(cases)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    generated, count = generate(root)
    with tempfile.TemporaryDirectory(prefix='dommy-host-factory-') as directory:
        temp = Path(directory)
        include = temp / 'host_factories.inc'
        include.write_text(generated)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/host_factory_fixture.cpp'), '-o', str(binary)]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, env=env)
        print(str(count) + ' production factory bodies passed allocation/constructor/recovery tests.')
        if args.mutations:
            path = root / 'src/core/DDataUtility/Utility/DPTransform.cpp'
            source = path.read_text()
            changed_source = source.replace('return new DPTransform();',
                                            'return new DPTransformManip();', 1)
            assert changed_source != source
            try:
                generate(root, {path: changed_source})
            except AssertionError as error:
                assert 'Factory creates a different type:' in str(error)
            else:
                raise AssertionError('Factory type mismatch was not rejected')
            print('Rejected mutation: factory type mismatch.')
            changed = generated.replace('::dommy::maya::createObject', 'unguardedFactory', 1)
            if changed == generated:
                raise AssertionError('Mutation not applied')
            include.write_text(changed)
            subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            result = subprocess.run([str(binary)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode == 0:
                raise AssertionError('Factory exception escape mutation was not detected')
            print('Rejected mutation: unguarded factory.')


if __name__ == '__main__':
    main()
