#!/usr/bin/env python3
"""Check legacy solver evaluation, bounded stepping and optional native soft forces."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bullet-root', type=Path)
    parser.add_argument('--bullet-build', type=Path)
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    if bool(args.bullet_root) != bool(args.bullet_build):
        parser.error('Supply both --bullet-root and --bullet-build')
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/dynamics/DBullet'
    sources = {name: (folder / name).read_text() for name in (
        'Solver/DBSolverNode.cpp', 'DM/DCBullet2DSolver.cpp', 'DM/DMBulletTest.cpp')}
    main_source = sources['Solver/DBSolverNode.cpp']
    fields = extract(main_source, 'MStatus DBSolverNode::applyFields(')
    assert 'sampledSoftForces.swap(candidateSoftForces)' in fields
    assert '->apply_central_force(softForces' not in fields
    for name, source in sources.items():
        start = source.index('dommy::bullet::SimulationStepPlan step;')
        prepared = source[start:]
        assert prepared.index('prepareSimulationStep(') < prepared.index('setGravity(' if name.startswith('DM/') else 'set_gravity(')
        assert 'canAdvance(step.seconds, step.fixedSeconds, step.substeps)' in prepared or 'can_step_simulation(step.seconds, step.fixedSeconds, step.substeps)' in prepared
        assert 'stepSimulation(' not in source
        assert 'step.seconds, step.fixedSeconds' in prepared
        if name != 'DM/DCBullet2DSolver.cpp':
            assert 'step.seconds, step.fixedSeconds, softForces)' in prepared
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter',
        '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
        '-fno-omit-frame-pointer']
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    step = (folder / 'Solver/DBulletStep.h').read_text()
    world = (folder / 'Solver/DBulletDynamicsWorld.h').read_text()
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-step-') as directory:
        temp = Path(directory)
        (temp / 'Solver').mkdir()
        step_path = temp / 'Solver/DBulletStep.h'
        world_path = temp / 'Solver/DBulletDynamicsWorld.h'
        step_path.write_text(step)
        world_path.write_text(world)
        includes = [temp, root / 'tests/stubs', root / 'src', root / 'src/core', root / 'src/core/math',
                    folder, folder / 'mvl', folder / 'Collision', folder / 'SoftBody']
        command += ['-I' + str(p) for p in includes]
        binary = temp / 'step_tests'

        def run(cmd, output, capture=False):
            subprocess.run(cmd, check=True)
            subprocess.run([str(output)], check=True, env=environment, capture_output=capture)

        portable_command = command + [str(root / 'tests/cpp/bullet_step_tests.cpp'), '-o', str(binary)]
        run(portable_command, binary)
        if args.mutations:
            for label, candidate in (
                ('unsafe integer conversion', step.replace('if (!std::isfinite(quotient) || quotient >= Scalar(available + 1)) return false;', '(void)available;')),
                ('whole-update budget removed', step.replace('available -= count;', '(void)count;')),
                ('residual ignored', step.replace('int available = kMaxSimulationSteps;', 'remainder = 0; int available = kMaxSimulationSteps;'))):
                assert candidate != step
                step_path.write_text(candidate)
                try:
                    run(portable_command, binary, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(binary), 'Compile failure is not mutation detection'
                    print('Detected time mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)
            step_path.write_text(step)

        # Execute the real node-level array publisher with editable alias
        # semantics, not the simplified endpoint writer in the solver fixture.
        rigid_source = (folder / 'RigidBody/DBRigidBodyNode.cpp').read_text()
        array_source = '\n'.join(extract(rigid_source, signature) for signature in (
            'MStatus DBRigidBodyNode::clearContactInfo(',
            'MStatus DBRigidBodyNode::addContactInfo('))
        validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
        (temp / 'bullet_contact_value_validation.inc').write_text('\n'.join(
            extract(validation, signature) for signature in (
                'inline bool isFiniteVector(', 'inline bool isFloatRepresentableVector(')))
        array_path = temp / 'bullet_contact_arrays.inc'
        array_binary = temp / 'contact_array_tests'
        array_command = command + [str(root / 'tests/cpp/bullet_contact_array_tests.cpp'),
                                   '-o', str(array_binary)]

        def run_arrays(candidate, capture=False):
            array_path.write_text(candidate)
            run(array_command, array_binary, capture)

        run_arrays(array_source)
        if args.mutations:
            for label, candidate in (
                ('editable name alias', array_source.replace(
                    'status = stringArrayData.copyTo(stringArray);',
                    'stringArray = stringArrayData.array(&status);', 1)),
                ('editable position alias', array_source.replace(
                    'status = vectorArrayData.copyTo(vectorArray);',
                    'vectorArray = vectorArrayData.array(&status);', 1)),
                ('published count mismatch ignored', array_source.replace(
                    'if (publishedCount != m_contactCount) return MS::kInvalidParameter;', '', 1)),
                ('name count mismatch ignored', array_source.replace(
                    'nameCount != static_cast<unsigned int>(m_contactCount)', 'false', 1)),
                ('position count mismatch ignored', array_source.replace(
                    'if (positionCount != nameCount) return MS::kInvalidParameter;',
                    '(void)positionCount;', 1)),
                ('premature contact count', array_source.replace(
                    'status = plugContactCount.setValue(nextContactCount);',
                    'm_contactCount = nextContactCount; status = plugContactCount.setValue(nextContactCount);', 1))):
                assert candidate != array_source, label
                try:
                    run_arrays(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(array_binary), 'Compile failure is not mutation detection'
                    print('Detected contact-array mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected contact-array mutation: ' + label)
            array_path.write_text(array_source)

        # Execute complete contact refresh/resolution/publication and rigid
        # output callbacks. The host/native doubles model invalidation at writes.
        contact_source = '\n'.join(extract(main_source, signature) for signature in (
            'bool isFinite(const MVector&', 'bool isFinite(const MQuaternion&',
            'bool isFinite(const vec3f&', 'bool isFinite(const quatf&',
            'MStatus DBSolverNode::refreshContactNodes(',
            'MStatus DBSolverNode::resolveRigidBodyNode(',
            'MStatus DBSolverNode::updateContactInfo(',
            'MStatus DBSolverNode::updateActiveRigidBodies(',
            'MStatus DBSolverNode::updateConstraint('))
        contact_header = (folder / 'Solver/DBSolverNode.h').read_text()
        (temp / 'bullet_contact_types.inc').write_text(extract(contact_header, 'struct ContactNode {') + ';')
        contact_path = temp / 'bullet_contact_output.inc'
        contact_binary = temp / 'contact_output_tests'
        contact_command = command + [str(root / 'tests/cpp/bullet_contact_output_tests.cpp'),
                                      '-o', str(contact_binary)]

        def run_contacts(candidate, capture=False):
            contact_path.write_text(candidate)
            run(contact_command, contact_binary, capture)

        run_contacts(contact_source)
        if args.mutations:
            for label, candidate in (
                ('failed refresh retains stale entries', contact_source.replace('m_contactNodes.clear();', '', 1)),
                ('dead Maya handle accepted', contact_source.replace('!entry.node.isAlive()', 'false', 1)),
                ('body generation ignored', contact_source.replace('candidate->cached_rigid_body() != body', 'false', 1)),
                ('contact refresh skipped', contact_source.replace('status = refreshContactNodes(connections);', 'status = MS::kSuccess;', 1)),
                ('post-write node resolution skipped', contact_source.replace(
                    '// callbacks. A partial host write is not a successful solver update.\n'
                    '        status = resolveRigidBodyNode(contact.a, a);\n        if (!status) return status;\n        status = resolveRigidBodyNode(contact.b, b);',
                    'status = MS::kSuccess;', 1)),
                ('nonfinite contacts accepted', contact_source.replace(
                    'if (!isFinite(pointA) || !isFinite(pointB)) return MS::kInvalidParameter;', '', 1)),
                ('unbounded contact snapshot', contact_source.replace(
                    'contacts.size() >= dommy::generation::kMaxGeneratedTransformCount', 'false', 1)),
                ('missing rigid node ignored', contact_source.replace('if (!rbNode) return MS::kFailure;', 'if (!rbNode) continue;')),
                ('nonfinite rigid outputs accepted', contact_source.replace(
                    'if (!isFinite(pos) || !isFinite(rot)) return MS::kInvalidParameter;', '')),
                ('invalid Euler conversion accepted', contact_source.replace(
                    'if (!isFinite(rotationDegrees)) return MS::kInvalidParameter;', '', 1)),
                ('missing constraint node ignored', contact_source.replace(
                    'if (!hingeNode) return MS::kFailure;', 'if (!hingeNode) continue;', 1)),
                ('missing constraint body ignored', contact_source.replace(
                    'if (!hinge) return MS::kFailure;', 'if (!hinge) continue;', 1)),
                ('missing constraint parent ignored', contact_source.replace(
                    'if (parentCount == 0) return MS::kFailure;\n        const MObject parent',
                    'if (parentCount == 0) continue;\n        const MObject parent', 1)),
                ('constraint parent query failure ignored', contact_source.replace(
                    'msgDagNode.parentCount(&status);\n        if (!status) return status;',
                    'msgDagNode.parentCount(&status);', 1)),
                ('constraint type query failure ignored', contact_source.replace(
                    'msgDagNode.typeId(&status);\n        if (!status) return status;',
                    'msgDagNode.typeId(&status);', 1)),
                ('nonfinite constraint frame accepted', contact_source.replace(
                    'if (!isFinite(constrPos) || !isFinite(constrRot)) return MS::kInvalidParameter;', '', 1)),
                ('constraint output failure ignored', contact_source.replace(
                    'status = updateConstraint(node);', 'status = MS::kSuccess;', 1))):
                assert candidate != contact_source, label
                try:
                    run_contacts(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(contact_binary), 'Compile failure is not mutation detection'
                    print('Detected contact/output mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected contact/output mutation: ' + label)
            contact_path.write_text(contact_source)

        # Run complete legacy callbacks without Maya or a Bullet installation.
        # Share the same timeline helper as the two DM adapters.
        legacy = '\n'.join(extract(main_source, signature) for signature in (
            'DBSolverNode::DBSolverNode()', 'DBSolverNode::~DBSolverNode()',
            'bool DBSolverNode::acquireSharedWorld(',
            'MStatus DBSolverNode::updateActiveSoftBodies(',
            'MStatus DBSolverNode::computeRigidBodies('))
        legacy += '\n' + extract(main_source, 'MStatus DBSolverNode::compute(')
        legacy += '\ncatch (...) { return MS::kFailure; }\n'
        legacy_path = temp / 'bullet_legacy_evaluation.inc'
        legacy_binary = temp / 'legacy_evaluation_tests'
        legacy_command = command + [str(root / 'tests/cpp/bullet_legacy_evaluation_tests.cpp'),
                                    '-o', str(legacy_binary)]

        def run_legacy(candidate, capture=False):
            legacy_path.write_text(candidate)
            run(legacy_command, legacy_binary, capture)

        run_legacy(legacy)
        if args.mutations:
            for label, candidate in (
                ('constructor already ready', legacy.replace('m_requiresStartFrame = true;', 'm_requiresStartFrame = false;', 1)),
                ('disabled bypass clears restart', legacy.replace('if (!enabled) {', 'if (!enabled) { m_requiresStartFrame = false;', 1)),
                ('StartTime change ignored', legacy.replace(
                    'if (!m_requiresStartFrame && startTime != m_initializedStartTime)', 'if (false)', 1)),
                ('StartTime units ignored', legacy.replace(
                    'startTime != m_initializedStartTime', 'startTime.value() != m_initializedStartTime.value()', 1)),
                ('StartTime committed before initialization', legacy.replace(
                    'const MTime startTime = startTimeHandle.asTime();',
                    'const MTime startTime = startTimeHandle.asTime(); m_initializedStartTime = startTime;', 1)),
                ('StartTime commit missing', legacy.replace('m_initializedStartTime = startTime;', '', 1)),
                ('unsupported timeline accepted', legacy.replace(
                    'if (action == dommy::bullet::TimelineAction::RestartRequired)', 'if (false)', 1)),
                ('unchecked playback query', legacy.replace(
                    'if (!status) return status;\n    }\n    const dommy::bullet::TimelineAction',
                    '\n    }\n    const dommy::bullet::TimelineAction', 1)),
                ('premature successful time', legacy.replace(
                    'status = updateActiveRigidBodies(rbConnections);',
                    'm_prevTime = time; status = updateActiveRigidBodies(rbConnections);', 1)),
                ('contact failure ignored', legacy.replace('status = updateContactInfo(rbConnections);', 'status = MS::kSuccess;', 1)),
                ('rigid output failure ignored', legacy.replace('status = updateActiveRigidBodies(rbConnections);', 'status = MS::kSuccess;', 1)),
                ('premature initialized time', legacy.replace(
                    'm_reInitialize = true;\n        dbSolver_t::set_split_impulse',
                    'm_prevTime = time; m_reInitialize = true;\n        dbSolver_t::set_split_impulse', 1)),
                ('unchecked soft connection query', legacy.replace(
                    'if (!status) return status;\n            // An intentionally', '// An intentionally', 1)),
                ('missing soft node ignored', legacy.replace(
                    'if (!sbNode) {\n                return MS::kFailure;', 'if (!sbNode) {\n                continue;', 1)),
                ('missing soft body ignored', legacy.replace(
                    'if (!sb || !sb->valid()) {\n                return MS::kFailure;',
                    'if (!sb || !sb->valid()) {\n                continue;', 1)),
                ('stale soft body ignored', legacy.replace(
                    'if (!sb || !sb->valid())', 'if (!sb)', 1)),
                ('wrong output target ignored', legacy.replace(
                    '!connections[0].node().hasFn(MFn::kMesh)) {\n                return MS::kFailure;',
                    '!connections[0].node().hasFn(MFn::kMesh)) {\n                continue;', 1))):
                assert candidate != legacy, label
                try:
                    run_legacy(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(legacy_binary), 'Compile failure is not mutation detection'
                    print('Detected legacy evaluation mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)
            legacy_path.write_text(legacy)
        if not args.bullet_root:
            print('Native Bullet time/force checks require --bullet-root/--bullet-build')
            return
        backend = (folder / 'DB_BulletSolver_t.h').read_text()
        facade = (folder / 'dbSolver_t.cpp').read_text()
        (temp / 'bullet_step_backend.inc').write_text('\n'.join(extract(backend, token) for token in (
            'bool can_step_simulation(', 'bool step_simulation(')))
        (temp / 'bullet_step_facade.inc').write_text('\n'.join(extract(facade, token) for token in (
            'bool dbSolver_t::can_step_simulation(', 'bool dbSolver_t::step_simulation(')))
        gl = '''#pragma once
enum {GL_FRONT_AND_BACK,GL_FILL,GL_LINE,GL_TRIANGLES};
inline void glPushMatrix() {} inline void glPopMatrix() {}
inline void glPolygonMode(int,int) {} inline void glBegin(int) {}
inline void glEnd() {} inline void glVertex3f(float,float,float) {}
'''
        for name in ('GL', 'OpenGL'):
            (temp / name).mkdir()
            (temp / name / 'gl.h').write_text(gl)
            (temp / name / 'glu.h').write_text('#pragma once\n')
        native_binary = temp / 'native_tests'
        libraries = [args.bullet_build / 'src' / name / ('lib' + name + '.a')
                     for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        native_command = command + ['-isystem', str(args.bullet_root / 'src'),
            str(root / 'tests/cpp/bullet_step_native_tests.cpp'),
            str(folder / 'SoftBody/DB_SoftBody_t.cpp'), str(folder / 'SoftBody/dbSoftBody_t.cpp'),
            # Instrument the actual native time-to-int conversion, not only the caller.
            str(args.bullet_root / 'src/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp'),
            *map(str, libraries), '-o', str(native_binary)]
        run(native_command, native_binary)
        if args.mutations:
            for label, candidate in (
                ('wrong Bullet argument slot', world.replace('World::stepSimulation(seconds, kMaxSimulationSteps, fixedSeconds);', 'World::stepSimulation(seconds, fixedSeconds);')),
                ('force applied only once including zero ticks', world.replace('forces_ = &forces;', 'for (const SoftBodyForce& item : forces) item.body->addForce(item.force); forces_ = &forces;').replace('for (const SoftBodyForce& item : *forces_) item.body->addForce(item.force);', '')),
                ('missing exception cleanup', world.replace('~Reset() { slot = nullptr; }', '~Reset() {}'))):
                assert candidate != world
                world_path.write_text(candidate)
                try:
                    run(native_command, native_binary, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(native_binary), 'Compile failure is not mutation detection'
                    print('Detected native step mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)
            world_path.write_text(world)


if __name__ == '__main__':
    main()
