#!/usr/bin/env python3
"""Test Bullet ownership/numerics portably; optionally run native shapes and bodies."""
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
    parser.add_argument('--sdk', type=Path, help='Additionally syntax-check/analyze against this Maya SDK')
    parser.add_argument('--seexpr-include', type=Path,
                        help='With --sdk, check full Bullet node/solver units using these legacy SeExpr headers')
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    if bool(args.bullet_root) != bool(args.bullet_build):
        parser.error('--bullet-root and --bullet-build must be supplied together')
    if args.sdk and not args.bullet_root:
        parser.error('--sdk requires --bullet-root and --bullet-build')
    if args.seexpr_include and not args.sdk:
        parser.error('--seexpr-include requires --sdk')
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/dynamics/DBullet'
    cpp = (folder / 'Collision/convex_decomposition_hacd.cpp').read_text()
    source = (extract(cpp, 'namespace {') + '\n' +
              extract(cpp, 'btCompoundShape* convex_decomposition_hacd::ConvexDecomp('))
    includes = [root / 'tests/stubs', root / 'src', root / 'src/core',
                root / 'src/core/math', folder, folder / 'mvl',
                folder / 'Collision', folder / 'Mesh', folder / 'Draw', folder / 'BaseShape/Box']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-unused-function', '-fsanitize=address,undefined,float-cast-overflow',
        '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    command += ['-I' + str(p) for p in includes]
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                       ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-shapes-') as directory:
        temp = Path(directory)
        path = temp / 'bullet_hacd.inc'
        path.write_text(source)
        sphere_source = (folder / 'BaseShape/Shpere/DB_SphereShape_t.h').read_text()
        (temp / 'bullet_sphere.inc').write_text(extract(sphere_source, 'class DB_SphereShape_t:') + ';')
        binary = temp / 'ownership_tests'

        def execute(candidate, capture=False):
            path.write_text(candidate)
            subprocess.run(command + ['-I' + str(temp),
                str(root / 'tests/cpp/bullet_shape_ownership_tests.cpp'),
                '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env=environment,
                           capture_output=capture, text=True)

        execute(source)
        shape_input_source = extract(
            (folder / 'Collision/DBCollisionShapeInput.h').read_text(),
            'namespace dommy {')
        node_source = '\n'.join([
            shape_input_source,
            extract((folder / 'RigidBody/DBRigidBodyNode.cpp').read_text(),
                    'MStatus DBRigidBodyNode::computeRigidBody('),
            extract((folder / 'RigidBody/DBRigidBodyNode.cpp').read_text(),
                    'MStatus DBRigidBodyNode::computeWorldMatrix('),
            extract((folder / 'RigidBody/DBRigidBodyNode.cpp').read_text(),
                    'MStatus DBRigidBodyNode::computeRigidBodyParam('),
            extract((folder / 'RigidBody/DBRigidBodyArrayNode.cpp').read_text(),
                    'MStatus DBRigidBodyArrayNode::computeRigidBodyParam('),
            extract((folder / 'Collision/DBCollisionShapeNode.cpp').read_text(),
                    'MStatus DBCollisionShapeNode::computeCollisionShapeParam('),
            extract((folder / 'SoftBody/DBSoftBodyNode.cpp').read_text(),
                    'MStatus DBSoftBodyNode::readSoftBodyParameters('),
            extract((folder / 'SoftBody/DBSoftBodyNode.cpp').read_text(),
                    'MStatus DBSoftBodyNode::computeSoftBodyParam(')])
        node_path = temp / 'bullet_node_methods.inc'
        node_binary = temp / 'node_tests'
        def execute_nodes(candidate, capture=False):
            node_path.write_text(candidate)
            subprocess.run(command + ['-I' + str(temp),
                str(root / 'tests/cpp/bullet_scale_node_tests.cpp'), '-o', str(node_binary)], check=True)
            subprocess.run([str(node_binary)], check=True, env=environment, capture_output=capture)
        execute_nodes(node_source)
        array_source = '\n'.join([
            shape_input_source,
            extract((folder / 'RigidBody/DBRigidBodyArrayNode.cpp').read_text(),
                    'MStatus DBRigidBodyArrayNode::computeWorldMatrix('),
            extract((folder / 'RigidBody/DBRigidBodyArrayNode.cpp').read_text(),
                    'MStatus DBRigidBodyArrayNode::computeRigidBodies(')])
        array_path = temp / 'bullet_array_methods.inc'
        array_binary = temp / 'array_tests'

        def execute_arrays(candidate, capture=False, native=False):
            array_path.write_text(candidate)
            extra = []
            if native:
                extra = ['-DDOMMY_NATIVE_BULLET', '-isystem', str(args.bullet_root / 'src'),
                         str(args.bullet_build / 'src/BulletDynamics/libBulletDynamics.a'),
                         str(args.bullet_build / 'src/BulletCollision/libBulletCollision.a'),
                         str(args.bullet_build / 'src/LinearMath/libLinearMath.a')]
            subprocess.run(command + ['-I' + str(temp),
                str(root / 'tests/cpp/bullet_array_translation_tests.cpp'),
                *extra, '-o', str(array_binary)], check=True)
            subprocess.run([str(array_binary)], check=True, env=environment, capture_output=capture)

        execute_arrays(array_source)
        if args.mutations:
            array_cases = [
                ('physical rather than logical indices', array_source.replace(
                    'hInitPosArray.jumpToElement(', 'hInitPosArray.jumpToArrayElement(', 1)),
                ('native world not restored', array_source.replace(
                    'previous.nativeBody->setWorldTransform(previous.previousWorld);', '', 1)),
                ('interpolation restored from current world', array_source.replace(
                    'previous.previousInterpolation);', 'previous.previousWorld);', 1)),
                ('initial positions not restored', array_source.replace(
                    'previous.previousInitialPosition[0],', 'previous.initialPosition[0],', 1)),
                ('rollback disabled before clean', array_source.replace(
                    'status = hInitPosArray.setAllClean();',
                    'restore.touched = 0U;\n    status = hInitPosArray.setAllClean();', 1)),
                ('matrix cache advanced before clean', array_source.replace(
                    'status = hInitPosArray.setAllClean();',
                    'm_worldMatrix = parentMatrix;\n    status = hInitPosArray.setAllClean();', 1)),
                ('failed shape query ignored', array_source.replace(
                    'if (!status) return status;\n\n    dbCollisionShape_t::pointer candidate;',
                    '\n    dbCollisionShape_t::pointer candidate;', 1)),
                ('missing connection source uses sphere', array_source.replace(
                    'if (!connected && sources.length() == 0U)',
                    'if (!connected || sources.length() == 0U)', 1)),
                ('invalid connected shape uses sphere', array_source.replace(
                    'if (!candidate) return MS::kFailure;',
                    'if (!candidate) candidate = dbSolver_t::create_sphere_shape();\n'
                    '    if (!candidate) return MS::kFailure;', 1)),
                ('array rebuild accepts nonfinite position', array_source.replace(
                    '!dommy::bullet::isFiniteVec3(savedPositions[i])', 'false', 1)),
                ('array rebuild accepts zero quaternion', array_source.replace(
                    '!dommy::bullet::hasValidQuaternion(savedRotations[i])', 'false', 1)),
            ]
            for label, candidate in array_cases:
                assert candidate != array_source
                try:
                    execute_arrays(candidate, capture=True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(array_binary), 'Compile failure is not detection'
                    print('Detected array/input mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected array/input mutation: ' + label)
            array_path.write_text(array_source)
        if args.mutations:
            node_cases = [
                ('stale body inertia', node_source.replace(
                    'body->collision_shape()->local_inertia(), inertia)',
                    'vec3f(1,2,3), inertia)', 1)),
                ('stale new-body inertia', node_source.replace(
                    'parameters.mass, collision_shape->local_inertia(), inertia)',
                    'parameters.mass, vec3f(1,2,3), inertia)', 1)),
                ('shape committed before failed publication', node_source.replace(
                    'if (!dbSolver_t::add_rigid_body(',
                    'scaleUpdate.accept();\n    if (!dbSolver_t::add_rigid_body(', 1)),
                ('shape committed before failed reconfiguration', node_source.replace(
                    'if (!dbSolver_t::reconfigure_rigid_body(',
                    'scaleUpdate.accept();\n    if (!dbSolver_t::reconfigure_rigid_body(', 1)),
                ('rejected scale reported successful', node_source.replace(
                    'if (!m_collision_shape->set_scale(vec3f(scale[0], scale[1], scale[2])))',
                    'if ((m_collision_shape->set_scale(vec3f(scale[0], scale[1], scale[2])), false))', 1)),
                ('pose rollback disabled before cleaning', node_source.replace(
                    'stat = data.setClean(plug);', 'restore.accepted = true;\n    stat = data.setClean(plug);', 1)),
                ('single initial rotation not restored', node_source.replace(
                    'i < handles.size(); ++i) {\n                    handles[i].set3Float',
                    'i < 1U; ++i) {\n                    handles[i].set3Float', 1)),
                ('single interpolation restored from world', node_source.replace(
                    'native->setInterpolationWorldTransform(interpolation);',
                    'native->setInterpolationWorldTransform(world);', 1)),
                ('constraint frames not restored', node_source.replace(
                    'for (auto& restore : constraints) restore();', '', 1)),
                ('parent transform not restored', node_source.replace(
                    'if (parentTouched) {', 'if (false && parentTouched) {', 1)),
                ('single initial rotation not published', node_source.replace(
                    'initialHandles[1].set3Float(initialRotation[0], initialRotation[1], initialRotation[2]);',
                    '(void)initialRotation;', 1)),
            ]
            for label, candidate in node_cases:
                assert candidate != node_source
                try:
                    execute_nodes(candidate, capture=True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(node_binary), 'Compile failure is not detection'
                    print('Detected node mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected node mutation: ' + label)
            node_path.write_text(node_source)
            cases = [
                ('lost child ownership', source.replace(
                    'm_children.push_back(std::move(child));\n        addChildShape(transform, m_children.back().get());',
                    'addChildShape(transform, child.release());', 1)),
                ('lost compound ownership', source.replace(
                    'std::unique_ptr<DOwningCompoundShape> compound(new DOwningCompoundShape());',
                    'DOwningCompoundShape* compound = new DOwningCompoundShape();', 1).replace(
                    'return compound.release();', 'return compound;', 1)),
                ('ignored GetCH failure', source.replace(
                    'if (!decomposition.GetCH(cluster, hullPoints.data(), hullTriangles.data())) return nullptr;',
                    'decomposition.GetCH(cluster, hullPoints.data(), hullTriangles.data());', 1)),
            ]
            for label, candidate in cases:
                assert candidate != source
                try:
                    execute(candidate, capture=True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(binary), 'Compile failure is not detection'
                    print('Detected mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)
            path.write_text(source)
        if args.bullet_root:
            body_source = (folder / 'RigidBody/DB_RigidBody_t.h').read_text()
            (temp / 'bullet_array_native_transforms.inc').write_text('\n'.join(
                extract(body_source, token) for token in (
                    'virtual void get_transform(vec3f', 'virtual void set_transform(vec3f')))
            execute_arrays(array_source, native=True)
            (temp / 'bullet_pose_native_body.inc').write_text('\n'.join(
                extract(body_source, token) for token in (
                    'virtual void get_transform(vec3f', 'virtual void set_transform(vec3f',
                    'virtual void set_interpolation_transform(vec3f',
                    'virtual void update_constraint()',
                    'std::vector<std::function<void()> > capture_constraint_frames()')))
            single_source = (folder / 'RigidBody/DBRigidBodyNode.cpp').read_text()
            array_node_source = (folder / 'RigidBody/DBRigidBodyArrayNode.cpp').read_text()
            restore_path = temp / 'bullet_pose_native_restore.inc'
            restore_source = '\n'.join([
                extract(single_source, 'struct RestorePose {') + ';',
                extract(array_node_source, 'struct BodyUpdate {') + ';',
                extract(array_node_source, 'struct RestoreUpdates {') + ';'])
            restore_path.write_text(restore_source)
            constraint_classes = []
            for name in ('Nail', 'Hinge', 'Slider', 'Sixdof'):
                constraint_source = (folder / ('Constraint/DB_' + name + '_Constraint_t.h')).read_text()
                constraint_classes.append('struct Probe' + name + ' : ConstraintBase {\n'
                    'using ConstraintBase::ConstraintBase;\n' +
                    extract(constraint_source, 'virtual void update_constraint(') + '\n};')
            (temp / 'bullet_pose_native_constraints.inc').write_text('\n'.join(constraint_classes))
            pose_binary = temp / 'pose_native_tests'
            pose_command = command + ['-I' + str(temp), '-isystem', str(args.bullet_root / 'src'),
                str(root / 'tests/cpp/bullet_pose_native_tests.cpp'),
                str(args.bullet_build / 'src/BulletDynamics/libBulletDynamics.a'),
                str(args.bullet_build / 'src/BulletCollision/libBulletCollision.a'),
                str(args.bullet_build / 'src/LinearMath/libLinearMath.a'), '-o', str(pose_binary)]
            subprocess.run(pose_command, check=True)
            subprocess.run([str(pose_binary)], check=True, env=environment)
            if args.mutations:
                pose_path = temp / 'bullet_pose_native_body.inc'
                pose_source = pose_path.read_text()
                pose_cases = [
                    ('world inertia not refreshed after pose', pose_source.replace(
                        'm_body->updateInertiaTensor();', '', 1)),
                    ('pose setter overwrites interpolation velocities', pose_source.replace(
                        'm_body->setWorldTransform(xform);', 'm_body->setCenterOfMassTransform(xform);', 1)),
                    ('point B pivot not restored', pose_source.replace('point->setPivotB(b);', '(void)b;', 1)),
                    ('slider B frame not restored', pose_source.replace('slider->getFrameOffsetB() = b;', '(void)b;', 1)),
                    ('constraint changed flags not restored', pose_source.replace(
                        'state->setPivotChanged(changed);', '(void)state; (void)changed;')),
                ]
                for label, candidate in pose_cases:
                    assert candidate != pose_source
                    pose_path.write_text(candidate)
                    try:
                        subprocess.run(pose_command, check=True)
                        subprocess.run([str(pose_binary)], check=True, env=environment, capture_output=True)
                    except subprocess.CalledProcessError as error:
                        assert error.cmd[0] == str(pose_binary), 'Compile failure is not detection'
                        print('Detected native pose mutation:', label, flush=True)
                    else:
                        raise AssertionError('Undetected native pose mutation: ' + label)
                pose_path.write_text(pose_source)
                restore_cases = [
                    ('single rollback leaves rotated inertia', restore_source.replace(
                        'native->updateInertiaTensor();', '', 1)),
                    ('array rollback leaves rotated inertia', restore_source.replace(
                        'previous.nativeBody->updateInertiaTensor();', '', 1)),
                ]
                for label, candidate in restore_cases:
                    assert candidate != restore_source
                    restore_path.write_text(candidate)
                    try:
                        subprocess.run(pose_command, check=True)
                        subprocess.run([str(pose_binary)], check=True, env=environment, capture_output=True)
                    except subprocess.CalledProcessError as error:
                        assert error.cmd[0] == str(pose_binary), 'Compile failure is not detection'
                        print('Detected native pose rollback mutation:', label, flush=True)
                    else:
                        raise AssertionError('Undetected native pose rollback mutation: ' + label)
                restore_path.write_text(restore_source)
            libraries = [args.bullet_build / 'Extras/HACD/libHACD.a',
                         args.bullet_build / 'src/BulletCollision/libBulletCollision.a',
                         args.bullet_build / 'src/LinearMath/libLinearMath.a']
            for library in libraries:
                if not library.is_file():
                    raise FileNotFoundError(library)
            binary = temp / 'frames_tests'
            native_command = command + ['-iquote', str(temp), '-isystem', str(args.bullet_root / 'src'),
                '-isystem', str(args.bullet_root / 'Extras'),
                str(root / 'tests/cpp/bullet_shape_frames_tests.cpp'),
                *map(str, libraries), '-o', str(binary)]
            subprocess.run(native_command, check=True)
            subprocess.run([str(binary)], check=True, env=environment)
            body_source = (folder / 'RigidBody/DB_RigidBody_t.h').read_text()
            backend_source = (folder / 'DB_BulletSolver_t.h').read_text()
            registration_source = (folder / 'dbSolver_t.cpp').read_text()
            (temp / 'bullet_native_body.inc').write_text('\n'.join(extract(body_source, token) for token in (
                'virtual float get_mass(', 'virtual void set_kinematic(', 'virtual void set_mass(',
                'virtual void set_inertia(', 'bool set_mass_properties(', 'void get_mass_properties(')))
            (temp / 'bullet_native_solver.inc').write_text('\n'.join(extract(backend_source, token) for token in (
                'bool add_rigid_body(', 'void remove_rigid_body(', 'bool can_set_mass_properties(', 'bool set_gravity(')))
            (temp / 'bullet_mass_registration.inc').write_text('\n'.join(extract(registration_source, token) for token in (
                'bool dbSolver_t::add_rigid_body(', 'bool dbSolver_t::reconfigure_rigid_body(',
                'bool dbSolver_t::reconfigure_rigid_bodies(', 'bool dbSolver_t::set_gravity(')))
            mass_binary = temp / 'mass_tests'
            mass_command = command + ['-I' + str(temp), '-isystem', str(args.bullet_root / 'src'),
                str(root / 'tests/cpp/bullet_body_mass_tests.cpp'),
                str(args.bullet_build / 'src/BulletDynamics/libBulletDynamics.a'),
                *map(str, libraries[1:]), '-o', str(mass_binary)]
            subprocess.run(mass_command, check=True)
            subprocess.run([str(mass_binary)], check=True, env=environment)
            if args.mutations:
                body_path = temp / 'bullet_native_body.inc'
                backend_path = temp / 'bullet_native_solver.inc'
                registration_path = temp / 'bullet_mass_registration.inc'
                body_original, backend_original, registration_original = (
                    body_path.read_text(), backend_path.read_text(), registration_path.read_text())
                mass_cases = [
                    ('inverse inertia admission removed', body_path, body_original.replace(
                        '!dommy::bullet::safeBodyMassProperties(mass, inertia)', 'false', 1)),
                    ('kinematic flag not cleared', body_path, body_original.replace(
                        'flags & ~btCollisionObject::CF_KINEMATIC_OBJECT', 'flags', 1)),
                    ('gravity preflight skipped', backend_path, backend_original.replace(
                        'i < objects.size()', 'i < 0', 1)),
                    ('earlier batch members not restored', registration_path, registration_original.replace(
                        'std::size_t j = 0; j < i;', 'std::size_t j = i; j < i;', 1)),
                ]
                for label, file, candidate in mass_cases:
                    original=file.read_text()
                    assert candidate != original
                    file.write_text(candidate)
                    try:
                        subprocess.run(mass_command, check=True)
                        subprocess.run([str(mass_binary)], check=True, env=environment, capture_output=True)
                    except subprocess.CalledProcessError as error:
                        assert error.cmd[0] == str(mass_binary), 'Compile failure is not detection'
                        print('Detected native mass mutation:', label, flush=True)
                    else:
                        raise AssertionError('Undetected native mass mutation: ' + label)
                    finally:
                        file.write_text(original)
            if args.mutations:
                hull = (folder / 'Collision/bt_convex_hull_shape.h').read_text()
                hacd = (folder / 'Mesh/DB_HacdShape_t.h').read_text()
                mesh = (folder / 'Mesh/DB_MeshShape_t.h').read_text()
                composite = (folder / 'Collision/bt_composite_shape.h').read_text()
                box = (folder / 'BaseShape/Box/DB_BoxShape_t.h').read_text()
                cases = [
                    ('constructor inertia in wrong axes', 'bt_convex_hull_shape.h', hull.replace(
                        'm_local_inertia = diag(I);',
                        'mat3x3f Q; q_to_mat(m_rotation,Q); m_local_inertia = diag(prod(trans(Q),mat3x3f(prod(I,Q))));', 1)),
                    ('normal frame conversion removed', 'bt_convex_hull_shape.h', hull.replace(
                        'q_to_mat(qconj(m_rotation), parentToLocal);',
                        'q_to_mat(mvl::qidentity<float>(), parentToLocal);', 1)),
                    ('HACD center applied twice', 'DB_HacdShape_t.h', hacd.replace(
                        '// HACD and the display buffer both retain input-mesh coordinates.',
                        'glTranslatef(m_center[0],m_center[1],m_center[2]);', 1)),
                    ('HACD relative compound scaling restored', 'DB_HacdShape_t.h', hacd.replace(
                        '// Keep the compound\'s own scale at identity.',
                        'compound->setLocalScaling(btVector3(s[0], s[1], s[2]));\n        // Keep the compound\'s own scale at identity.', 1)),
                    ('GImpact quantized refit restored', 'DB_MeshShape_t.h', mesh.replace(
                        'new dommy::bullet::RebuildingGImpactMeshShape(m_tiva.get())',
                        'new btGImpactMeshShape(m_tiva.get())', 1)),
                    ('GImpact parent bounds update removed', 'DB_MeshShape_t.h', mesh.replace(
                        'compound->updateChildTransform(0, transform);', '(void)transform;', 1)),
                    ('convex compound tree update removed', 'bt_convex_hull_shape.h', hull.replace(
                        'compound->updateChildTransform(0, tr);', 'compound->recalculateLocalAabb();', 1)),
                    ('composite ownership lost', 'bt_composite_shape.h', composite.replace(
                        'std::unique_ptr<btCompoundShape> compound(new btCompoundShape());',
                        'btCompoundShape* compound = new btCompoundShape();', 1).replace(
                        'compound.release()', 'compound').replace('compound.get()', 'compound')),
                    ('compound child commit omitted', 'bt_composite_shape.h', composite.replace(
                        'staged->children[i]->commit();', '(void)staged->children[i];', 1)),
                    ('Box relative scale division restored', 'BaseShape/Box/DB_BoxShape_t.h', box.replace(
                        'm_localScaling = scale.absolute();',
                        'btBoxShape::setLocalScaling(scale);\n        return;\n        m_localScaling = scale.absolute();', 1)),
                ]
                for label, name, candidate in cases:
                    assert candidate != {'bt_convex_hull_shape.h': hull, 'DB_HacdShape_t.h': hacd,
                                         'DB_MeshShape_t.h': mesh, 'bt_composite_shape.h': composite,
                                         'BaseShape/Box/DB_BoxShape_t.h': box}[name]
                    header = temp / name
                    header.parent.mkdir(parents=True, exist_ok=True)
                    header.write_text(candidate)
                    try:
                        subprocess.run(native_command, check=True)
                        subprocess.run([str(binary)], check=True, env=environment, capture_output=True)
                    except subprocess.CalledProcessError as error:
                        assert error.cmd[0] == str(binary), 'Compile failure is not detection'
                        print('Detected native mutation:', label, flush=True)
                    else:
                        raise AssertionError('Undetected native mutation: ' + label)
                    finally:
                        header.unlink()
        else:
            print('Native Bullet shape construction is not run; supply --bullet-root/--bullet-build')
        if args.sdk:
            sdk_source = temp / 'shape_sdk.cpp'
            sdk_source.write_text('''
#include <maya/MGL.h>
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "dbCollisionShape_t.h"
#include "bt_convex_hull_shape.h"
#include "DB_MeshShape_t.h"
#include "DB_HacdShape_t.h"
#include "bt_composite_shape.h"
#include "BaseShape/Box/DB_BoxShape_t.h"
#include "BaseShape/Shpere/DB_SphereShape_t.h"
#include "BaseShape/Plane/DB_PlaneShape_t.h"
class DB_BulletSolver_t {
public:
    static void check(const vec3f* p, const vec3f* n, const unsigned int* t) {
        bt_convex_hull_shape_t hull(p,8,n,t,36);
        DB_MeshShape_t mesh(p,8,n,t,36,true);
        DB_HacdShape_t hacd(p,8,n,t,36,true);
        hull.set_scale(vec3f(1,1,1));
        mesh.set_scale(vec3f(1,1,1));
        hacd.set_scale(vec3f(1,1,1));
        bt_composite_shape_t compound(nullptr, nullptr, nullptr, 0);
        DB_BoxShape_t box(vec3f(1,2,3));
        DB_SphereShape_t sphere(1);
        DB_PlaneShape_t plane(vec3f(0,1,0),0);
        box.set_scale(vec3f(0,1,1)); box.set_scale(vec3f(1,1,1));
        sphere.set_scale(vec3f(0,1,1)); sphere.set_scale(vec3f(1,1,1));
        plane.set_scale(vec3f(1,1,1));
    }
};
''')
            sdk_command = shlex.split(os.environ.get('CXX', 'clang++')) + [
                '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-Wno-deprecated-declarations', '-isystem', str(args.sdk / 'include'),
                '-isystem', str(args.bullet_root / 'src'),
                '-isystem', str(args.bullet_root / 'Extras')]
            sdk_command += ['-I' + str(p) for p in includes if p != root / 'tests/stubs']
            for file in (sdk_source, folder / 'Collision/convex_decomposition_hacd.cpp'):
                subprocess.run(sdk_command + ['-fsyntax-only', str(file)], check=True)
                subprocess.run(sdk_command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(file)], check=True)
            print('Shape headers/constructors and complete HACD source: Maya SDK syntax/static analysis passed')
            if args.seexpr_include:
                if not (args.seexpr_include / 'SeExpression.h').is_file():
                    raise FileNotFoundError(args.seexpr_include / 'SeExpression.h')
                full_command = sdk_command + ['-std=c++14', '-Wno-unused-parameter',
                    '-isystem', str(args.seexpr_include), '-isystem', str(root / 'third_party/libnoise/source'),
                    '-idirafter', str(root / 'tests/stubs')]
                full_command += ['-I' + str(p) for p in sorted((root / 'src').rglob('*')) if p.is_dir()]
                for file in (folder / 'RigidBody/DBRigidBodyNode.cpp',
                             folder / 'RigidBody/DBRigidBodyArrayNode.cpp', folder / 'Solver/DBSolverNode.cpp',
                             folder / 'dbSolver_t.cpp', folder / 'Collision/DBCollisionShapeNode.cpp', folder / 'DB_BulletSolver_t.cpp',
                             folder / 'SoftBody/DBSoftBodyNode.cpp', folder / 'SoftBody/DB_SoftBody_t.cpp',
                             folder / 'SoftBody/dbSoftBody_t.cpp',
                             folder / 'Constraint/DBNailConstraintNode.cpp',
                             folder / 'Constraint/DBHingeConstraintNode.cpp',
                             folder / 'Constraint/DBSliderConstraintNode.cpp',
                             folder / 'Constraint/DBSixdofConstraintNode.cpp',
                             folder / 'DM/DCBullet2DSolver.cpp', folder / 'DM/DMBulletTest.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRope.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DMbtSoft.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DMbtRigid.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRigid2D.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTP2PConstraint.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTFixConstraint.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTConeTwistConstraint.cpp',
                             root / 'src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BT6DofConstraint.cpp'):
                    subprocess.run(full_command + ['-fsyntax-only', str(file)], check=True)
                    subprocess.run(full_command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(file)], check=True)
                print('Complete Bullet node/solver units: SDK syntax/static analysis passed (legacy auxiliary headers/stub fallbacks; not linking)')


if __name__ == '__main__':
    main()
