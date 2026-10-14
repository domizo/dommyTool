#!/usr/bin/env python3
"""Exercise complete polygon-command production code with a bounded fake host.

This verifies ownership and compensation, not Maya's actual graph evaluation.
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    base = (root / 'src/modules/mesh/polyModifier/basePolyModifierCmd.cpp').read_text()
    command = (root / 'src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp').read_text()
    # Compile the entire implementation, including local cleanup ownership and
    # constructors, against the real class declarations and bounded host types.
    source = re.sub(r'^#include .*\n', '', base + '\n' + command, flags=re.M)
    with tempfile.TemporaryDirectory(prefix='dommy-poly-modifier-') as directory:
        temp = Path(directory)
        (temp / 'maya').mkdir()
        for header in ('MIntArray', 'MFloatVector', 'MDagPath', 'MDGModifier',
                       'MDagModifier', 'MPlug', 'MObjectHandle', 'MPxCommand',
                       'MObject', 'MGlobal', 'MTypeId'):
            (temp / ('maya/' + header + '.h')).write_text('#pragma once\n')
        methods = temp / 'poly_modifier_methods.inc'
        binary = temp / 'test'
        compile_command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'),
            '-I' + str(root / 'src/modules/mesh/polyModifier'),
            str(root / 'tests/cpp/poly_modifier_transactions.cpp'), '-o', str(binary)]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))

        def check(candidate, mutation=False):
            methods.write_text(candidate)
            compiled = subprocess.run(compile_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if compiled.returncode:
                sys.stderr.buffer.write(compiled.stderr)
                raise AssertionError('Polygon transaction fixture did not compile')
            result = subprocess.run([str(binary)], env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=20)
            if mutation:
                if result.returncode == 0:
                    raise AssertionError('Mutation unexpectedly passed')
            elif result.returncode:
                sys.stderr.buffer.write(result.stderr)
                raise AssertionError('Polygon transaction fixture failed')
            else:
                print(result.stdout.decode().strip())

        check(source)
        if args.mutations:
            mutations = (
                ('temporary deletion skipped', source.replace('if (!root.isNull()) recovery.run', 'if (false && !root.isNull()) recovery.run', 1)),
                ('temporary disconnect skipped', source.replace('if (linkAttempted) recovery.run', 'if (false && linkAttempted) recovery.run', 1)),
                ('independent DAG cleanup skipped', source.replace('if (dagAttempted) {', 'if (recovery.result && dagAttempted) {', 1)),
                ('tweak read failure ignored', source.replace('require(numeric.getData(item.value.x, item.value.y, item.value.z));', '(void)numeric.getData(item.value.x, item.value.y, item.value.z);', 1)),
                ('iterator advance failure ignored', source.replace('status = selectionIterator.next();\n        if (!status) return status;', 'selectionIterator.next();', 1)),
                ('iterator bound removed', source.replace('if (visited++ >= selectionCount) return MS::kFailure;', '++visited; (void)visited;', 1)),
                ('result compensation skipped', source.replace('editApplied ? cancelModifyPoly()', 'false && editApplied ? cancelModifyPoly()', 1)),
                ('invalid rollback state accepted', source.replace('if (!recovery.result) s.phase = State::Phase::invalid;', '', 1)),
                ('independent mesh snapshot copy skipped', source.replace('const MObject copied = mesh.copy(evaluated, result, &query);', 'const MObject copied = evaluated; (void)mesh;', 1)),
                ('migrated offsets applied twice', source.replace('require(graph.newPlugValue(wiring[i].plug, data));', '(void)data;', 1)),
            )
            for label, candidate in mutations:
                if candidate == source:
                    raise AssertionError('Mutation not applied: ' + label)
                check(candidate, mutation=True)
                print('Rejected mutation:', label)


if __name__ == '__main__':
    main()
