#!/usr/bin/env python3
"""Run complete experimental fracture implementations against pinned native Bullet."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bullet-root', required=True, type=Path)
    parser.add_argument('--bullet-build', required=True, type=Path)
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/mesh/DMesh/Dynamics/Bullet'
    world = (folder / 'DBFractureDynamicsWorld.cpp').read_text()
    body = (folder / 'dbtFractureBody.cpp').read_text()
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-fracture-native-') as directory:
        temp = Path(directory)
        world_path, body_path = temp / 'world.cpp', temp / 'body.cpp'
        binary = temp / 'fracture_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-variable', '-Wno-unused-but-set-variable',
            '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'), '-I' + str(folder),
            '-isystem', str(args.bullet_root / 'src'),
            str(root / 'tests/cpp/bullet_fracture_native_tests.cpp'), str(world_path), str(body_path)]
        command += [str(args.bullet_build / 'src' / name / ('lib' + name + '.a'))
                    for name in ('BulletDynamics', 'BulletCollision', 'LinearMath')]
        command += ['-o', str(binary)]

        def run(candidate_world, candidate_body, capture=False):
            world_path.write_text(candidate_world)
            body_path.write_text(candidate_body)
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True, env=environment, capture_output=capture)

        run(world, body)
        if args.mutations:
            cases = [
                ('uniform rather than weighted COM', world.replace(
                    'totalMass = childMass;',
                    'totalMass = childMass;\n'
                    '            for (int i = 0; i < massArray.size(); ++i) massArray[i] = totalMass / massArray.size();', 1), body),
                ('bounding-box rather than principal inertia', world.replace(
                    'std::unique_ptr<dbtFractureBody> newBody(new dbtFractureBody(',
                    'newCompound->calculateLocalInertia(totalMass, localInertia);\n'
                    '            std::unique_ptr<dbtFractureBody> newBody(new dbtFractureBody(', 1), body),
                ('glued world/interpolation inertia stale', world.replace(
                    'setCenterOfMassTransform(fracObj->getWorldTransform()*shift)',
                    'setWorldTransform(fracObj->getWorldTransform()*shift)', 1), body),
                ('fragment world/interpolation inertia stale', world.replace(
                    'setCenterOfMassTransform(oldTransform*shift)', 'setWorldTransform(oldTransform*shift)', 1), body),
                ('intrinsic spin momentum discarded', world.replace(
                    'oldAngularMomenta[i] + rel_pos.cross(imp)', 'rel_pos.cross(imp)', 1), body),
                ('orbital momentum discarded', world.replace(
                    'oldAngularMomenta[i] + rel_pos.cross(imp)', 'oldAngularMomenta[i]', 1), body),
                ('secondary fracture child masses lost', world.replace(
                    'otherFracture ? otherFracture->m_masses[c] :\n                            ', '', 1), body),
                ('fragments lose tangential velocity', world.replace(
                    'fracObj->getVelocityInLocalPoint(offset)', 'fracObj->getLinearVelocity()', 1), body),
                ('glued interpolation velocity stale', world.replace(
                    'newBody->setInterpolationAngularVelocity(angularVelocity);', '', 1), body),
                ('invalid child masses admitted', world, body.replace(
                    'masses[i] <= 0 || !dommy::bullet::isSafeInverseOperand(masses[i])', 'false', 1)),
                ('inconsistent body/child mass admitted', world.replace(
                    'btFabs(childMass - totalMass) > totalMass * btScalar(1e-4)', 'false', 1), body),
            ]
            for label, candidate_world, candidate_body in cases:
                assert candidate_world != world or candidate_body != body
                try:
                    run(candidate_world, candidate_body, capture=True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(binary), 'Compile failure is not mutation detection'
                    print('Detected native fracture mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected native fracture mutation: ' + label)


if __name__ == '__main__':
    main()
