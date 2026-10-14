#!/usr/bin/env python3
"""Run complete production command transaction bodies against a bounded fake host.

The fixture verifies compensation/control flow, not Maya's mutation atomicity.
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


def generate(root):
    particle = (root / 'src/commands/setParticleRGBCmd.cpp').read_text()
    bodies = [extract(particle, 'MStatus commandFailure('),
              extract(particle, 'MStatus extendToShape(')]
    for name, relative, methods in (
        ('shapeFillCmd', 'src/commands/shapeFillCmd.cpp', ('doIt', 'undoIt', 'redoIt')),
        ('spaceDuplicaterCmd', 'src/commands/spaceDuplicaterCmd.cpp', ('doIt', 'undoIt', 'redoIt')),
        ('surfaceDuplicaterCmd', 'src/commands/surfaceDuplicaterCmd.cpp', ('doIt', 'undoIt', 'redoIt')),
        ('icosahedronCmd', 'src/modules/generators/icosahedron/icosahedronCmd.cpp', ('doIt', 'undoIt', 'redoIt')),
        ('woolCurveCmd', 'src/commands/woolCurveCmd.cpp', ('doIt', 'undoIt', 'redoIt')),
        ('DClusterWeightCmd', 'src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp', ('doIt', 'buildWeightPlan', 'applyWeights', 'undoIt', 'redoIt')),
        ('setParticleRGBCmd', 'src/commands/setParticleRGBCmd.cpp', ('doIt', 'applyColors', 'undoIt', 'redoIt')),
    ):
        source = (root / relative).read_text()
        bodies.extend(extract(source, 'MStatus ' + name + '::' + method) for method in methods)
    return '\n\n'.join(bodies)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--sdk', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    generated = generate(root)
    guard = (root / 'src/core/common/utility/DCommandGuard.h').read_text()
    with tempfile.TemporaryDirectory(prefix='dommy-command-transactions-') as directory:
        temp = Path(directory)
        (temp / 'maya').mkdir()
        # The fixture supplies its domain-specific MGlobal after defining host
        # types; do not alter the shared lightweight test double.
        (temp / 'maya/MGlobal.h').write_text('#pragma once\n')
        (temp / 'common/utility').mkdir(parents=True)
        guard_path = temp / 'common/utility/DCommandGuard.h'
        guard_path.write_text(guard)
        methods = temp / 'command_transaction_methods.inc'
        methods.write_text(generated)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), '-I' + str(root / 'src/core/common/utility'),
            str(root / 'tests/cpp/command_transaction_fixture.cpp'), '-o', str(binary)]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))

        def check(method_source, guard_source=guard, mutation=False):
            methods.write_text(method_source)
            guard_path.write_text(guard_source)
            subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            result = subprocess.run([str(binary)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
            if mutation:
                if result.returncode == 0:
                    raise AssertionError('Mutation unexpectedly passed')
            else:
                if result.returncode != 0:
                    sys.stderr.buffer.write(result.stderr)
                    raise AssertionError('Command transaction fixture failed')
                print(result.stdout.decode().strip())

        try:
            check(generated)
            if args.mutations:
                # Match independently of formatting while preserving the full
                # production function: indentation is not a safety contract.
                import re
                create_pattern = re.compile(r'const unsigned int shapeIndex = createdParticleShapes.length\(\);\s*status = createdParticleShapes.append\(MObject::kNullObj\);\s*if \(!status\) return status;\s*MFnParticleSystem particleFn;\s*createdParticleShapes\[shapeIndex\] = particleFn.create\(MObject::kNullObj, &status\);')
                late_tracking = create_pattern.sub('''const unsigned int shapeIndex = createdParticleShapes.length();
            MFnParticleSystem particleFn;
            const MObject pending = particleFn.create(MObject::kNullObj, &status);
            status = createdParticleShapes.append(pending);
            if (!status) return status;''', generated, count=1)
                mutations = (
                    ('tracking allocated after creation', late_tracking, guard),
                    ('transaction compensation removed', generated, guard.replace('if (status == MS::kSuccess) return status;', 'return status;', 1)),
                    ('iterator advance status ignored', generated.replace('status = geometry.next();\n        if (!status) return status;', 'geometry.next();', 1), guard),
                    ('iterator count bound removed', re.sub(r'        if \(componentIds.length\(\) >= checkedCount\) \{.*?\n        \}', '', generated, count=1, flags=re.S), guard),
                    ('result-name append failure ignored', generated.replace('if (!status) return status;\n\n                status = iterator.next();', '\n                status = iterator.next();', 1), guard),
                    ('independent attribute cleanup skipped', generated.replace('// Attribute cleanup is independent:', 'if (!firstFailure) return firstFailure;\n            // Attribute cleanup is independent:', 1), guard),
                    ('rollback status discarded', generated, guard.replace('return rollbackStatus;', 'return status;', 1)),
                    ('RGB original reset failure ignored', generated.replace(
                        'MStatus status = originalColors.clear();\n    if (!status) return status;',
                        'MStatus status = originalColors.clear();', 1), guard),
                    ('RGB target reset failure ignored', generated.replace(
                        'status = targetColors.clear();\n    if (!status) return status;',
                        'status = targetColors.clear();', 1), guard),
                    ('RGB sampling reset failure ignored', generated.replace(
                        'status = targetColors.clear();\n    if (!status) return status;\n    for',
                        'status = targetColors.clear();\n    for', 1), guard),
                    ('weight original reset failure ignored', generated.replace(
                        'MStatus status = originalWeights.clear();\n        if (!status) return status;',
                        'MStatus status = originalWeights.clear();', 1), guard),
                    ('weight target reset failure ignored', generated.replace(
                        'status = targetWeights.clear();\n        if (!status) return status;',
                        'status = targetWeights.clear();', 1), guard),
                    ('cluster selection reset failure ignored', generated.replace(
                        'status = selection.clear();\n        if (!status) return status;',
                        'status = selection.clear();', 1), guard),
                )
                for label, source, changed_guard in mutations:
                    if source == generated and changed_guard == guard:
                        raise AssertionError('Mutation was not applied: ' + label)
                    check(source, changed_guard, mutation=True)
                    print('Rejected mutation:', label)
        except subprocess.CalledProcessError as error:
            sys.stderr.buffer.write(error.stderr or b'')
            raise
    if args.sdk:
        from check_motion_duplicator_safety import sdk_check
        for path in ('src/commands/setParticleRGBCmd.cpp',
                     'src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp'):
            sdk_check(root,args.sdk,source_path=path,label=Path(path).stem)


if __name__ == '__main__':
    main()
