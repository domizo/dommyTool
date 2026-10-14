#!/usr/bin/env python3
"""Test sparse soft-body preparation and factory ownership; optionally run real Bullet."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bullet-root', type=Path)
    parser.add_argument('--bullet-build', type=Path)
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--connection-mutations', action='store_true')
    args = parser.parse_args()
    if bool(args.bullet_root) != bool(args.bullet_build):
        parser.error('Supply both --bullet-root and --bullet-build')
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/dynamics/DBullet'
    source = (folder / 'dbSolver_t.cpp').read_text()
    methods = []
    for match in re.finditer(r'\w+::pointer\s+dbSolver_t::create_\w+\(', source):
        if '::create_soft_body(' in match.group():
            continue
        # The two-body overloads share a prefix; extract from this offset instead.
        methods.append(extract(source[match.start():], match.group()))
    assert len(methods) == 16
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-function',
        '-Wno-deprecated-declarations', '-fsanitize=address,undefined,float-cast-overflow',
        '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    includes = [root / 'tests/stubs', root / 'src', root / 'src/core', root / 'src/core/math',
                folder, folder / 'mvl', folder / 'Collision', folder / 'SoftBody']
    command += ['-I' + str(p) for p in includes]
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-soft-body-') as directory:
        temp = Path(directory)
        declarations = '\n'.join('static ' + method[:method.index('{')].replace('dbSolver_t::', '') + ';'
                                 for method in methods)
        (temp / 'bullet_factory_declarations.inc').write_text(declarations)
        factories = '\n'.join(methods)
        path = temp / 'bullet_factories.inc'
        binary = temp / 'preparation_tests'
        def portable(candidate, capture=False):
            path.write_text(candidate)
            subprocess.run(command + ['-I' + str(temp), str(root / 'tests/cpp/bullet_soft_preparation_tests.cpp'),
                                     '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env=environment, capture_output=capture)
        portable(factories)
        node = (folder / 'SoftBody/DBSoftBodyNode.cpp').read_text()
        lifecycle = '\n'.join(extract(node, prefix) for prefix in (
            'void DBSoftBodyNode::destroySoftBody(',
            'MStatus DBSoftBodyNode::readSoftBodyParameters(',
            'MStatus DBSoftBodyNode::computeSoftBody(',
            'MStatus DBSoftBodyNode::computeSoftBodyParam(',
            'void DBSoftBodyNode::set_soft_body('))
        lifecycle += '\n' + extract((folder / 'Solver/DBSolverNode.cpp').read_text(),
                                     'MStatus DBSolverNode::initSoftBody(')
        lifecycle_path = temp / 'bullet_soft_lifecycle.inc'
        lifecycle_binary = temp / 'lifecycle_tests'
        def run_lifecycle(candidate, capture=False):
            lifecycle_path.write_text(candidate)
            subprocess.run(command + ['-I' + str(temp), str(root / 'tests/cpp/bullet_soft_lifecycle_tests.cpp'),
                                     '-o', str(lifecycle_binary)], check=True)
            subprocess.run([str(lifecycle_binary)], check=True, env=environment, capture_output=capture)
        run_lifecycle(lifecycle)
        if args.connection_mutations:
            checks=list(re.finditer(r'isConnected\(&status\);\n[ \t]*if \(!status\) return status;',lifecycle))
            assert len(checks)==3
            for index,check in enumerate(checks):
                candidate=lifecycle[:check.start()]+check.group().split('\n')[0]+lifecycle[check.end():]
                try:
                    run_lifecycle(candidate,True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0]==str(lifecycle_binary),'Compile failure is not detection'
                    print('Detected soft connection mutation:',index,flush=True)
                else:raise AssertionError('Undetected soft connection mutation: '+str(index))
            lifecycle_path.write_text(lifecycle)
        if args.mutations:
            for label, candidate in (
                ('default mesh configuration', lifecycle.replace(
                    'create_soft_body(triVertCoords, triVertIndices, parameters)',
                    'create_soft_body(triVertCoords, triVertIndices)', 1)),
                ('default StartTime configuration', lifecycle.replace(
                    'status = sbNode->readSoftBodyParameters(parameters);', 'status = MS::kSuccess;', 1)),
                ('lost failed-output restoration', lifecycle.replace(
                    'sbNode->set_soft_body(previousSoftBody);', 'sbNode->set_soft_body(newSoftBody);', 1)),
                ('restored stale world body', lifecycle.replace(
                    'if (previousSoftBody && !previousSoftBody->valid()) previousSoftBody.reset();', '', 1))):
                assert candidate != lifecycle
                try:
                    run_lifecycle(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(lifecycle_binary), 'Compile failure is not detection'
                    print('Detected soft lifecycle mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected lifecycle mutation: ' + label)
        if args.mutations:
            candidate = factories.replace('shared_ptr<rigid_body_impl_t> body(m_impl->create_rigid_body(cs->impl()));',
                                          'rigid_body_impl_t* body = m_impl->create_rigid_body(cs->impl());', 1)
            assert candidate != factories
            try:
                portable(candidate, True)
            except subprocess.CalledProcessError as error:
                assert error.cmd[0] == str(binary), 'Compile failure is not detection'
                print('Detected factory mutation: unowned intermediate body', flush=True)
            else:
                raise AssertionError('Factory ownership mutation survived')
        cpp = (folder / 'SoftBody/DB_SoftBody_t.cpp').read_text()
        assert 'CreateFromTriMesh' not in cpp and 'generateBendingConstraints' not in cpp
        assert 'setTotalMass' not in cpp
        cluster_calls = re.findall(r'generateClusters\([^;]+;', cpp)
        assert len(cluster_calls) == 2 and all(
            'dommy::bullet::kMaxSoftClusterGenerationIterations' in call for call in cluster_calls)
        if not args.bullet_root:
            print('Native soft body tests require --bullet-root/--bullet-build')
            return
        # Only OpenGL is doubled: compile the entire production soft-body unit.
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
        native = temp / 'DB_SoftBody_t.cpp'
        native_binary = temp / 'native_tests'
        libraries = [args.bullet_build / 'src' / name / ('lib' + name + '.a')
                     for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        native_command = command + ['-I' + str(temp), '-isystem', str(args.bullet_root / 'src'),
            str(root / 'tests/cpp/bullet_soft_body_tests.cpp'), str(native), *map(str, libraries), '-o', str(native_binary)]
        def run_native(candidate, capture=False):
            native.write_text(candidate)
            subprocess.run(native_command, check=True)
            subprocess.run([str(native_binary)], check=True, env=environment, capture_output=capture)
        run_native(cpp)
        # Exercise the complete production factory/world lifecycle and backend
        # membership checks with actual Bullet, including instrumented native
        # worldInfo reads in predictMotion. Unrelated virtual APIs are omitted.
        backend = (folder / 'DB_BulletSolver_t.cpp').read_text()
        factory = '\n'.join(extract(backend, signature) for signature in (
            'DB_BulletSolver_t::DB_BulletSolver_t()', 'DB_BulletSolver_t::~DB_BulletSolver_t()',
            'void DB_BulletSolver_t::destroyWorld()', 'void DB_BulletSolver_t::createWorld()',
            'dbSoftBody_t::pointer DB_BulletSolver_t::create_soft_body('))
        backend_header = (folder / 'DB_BulletSolver_t.h').read_text()
        registration = '\n'.join(extract(backend_header, signature).replace(' override', '')
                                 for signature in ('bool add_soft_body(', 'void remove_soft_body('))
        factory_path = temp / 'bullet_soft_world_factory.inc'
        registration_path = temp / 'bullet_soft_world_registration.inc'
        bullet_object = temp / 'btSoftBody.o'
        # Bullet 2.88 itself has legacy compiler warnings; project units retain
        # warning-as-error checks. Instrument the native lifetime-sensitive unit.
        subprocess.run(command + ['-w', '-isystem', str(args.bullet_root / 'src'), '-c',
            str(args.bullet_root / 'src/BulletSoftBody/btSoftBody.cpp'), '-o', str(bullet_object)], check=True)
        lifetime_binary = temp / 'world_lifetime_tests'
        lifetime_command = command + ['-I' + str(temp), '-isystem', str(args.bullet_root / 'src'),
            str(root / 'tests/cpp/bullet_soft_world_lifetime_tests.cpp'), str(native),
            str(folder / 'SoftBody/dbSoftBody_t.cpp'), str(bullet_object),
            *map(str, libraries), '-o', str(lifetime_binary)]

        def run_lifetime(factory_candidate, registration_candidate, owner_candidate, capture=False):
            factory_path.write_text(factory_candidate)
            registration_path.write_text(registration_candidate)
            native.write_text(owner_candidate)
            subprocess.run(lifetime_command, check=True)
            subprocess.run([str(lifetime_binary)], check=True, env=environment, capture_output=capture)

        run_lifetime(factory, registration, cpp)
        if args.mutations:
            for label, changed_factory, changed_registration, changed_owner in (
                ('missing explicit world invalidation', factory.replace(
                    'if (m_softBodyWorldReference) m_softBodyWorldReference->invalidate();', '', 1), registration, cpp),
                ('foreign world registration accepted', factory, registration.replace(
                    '!bt_body->belongs_to_world(m_softBodyWorldReference)', '!bt_body->body()', 1), cpp),
                ('foreign world removal accepted', factory, registration.replace(
                    'if (!bt_body || !bt_body->belongs_to_world(m_softBodyWorldReference)) return;',
                    'if (!bt_body || !bt_body->body()) return;', 1), cpp),
                ('stale body access allowed', factory, registration, cpp.replace(
                    'if (m_managedWorld) {', 'if (false) {', 1))):
                assert (changed_factory, changed_registration, changed_owner) != (factory, registration, cpp)
                try:
                    run_lifetime(changed_factory, changed_registration, changed_owner, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(lifetime_binary), 'Compile failure is not detection'
                    print('Detected soft world lifetime mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected world lifetime mutation: ' + label)
            native.write_text(cpp)
        if args.mutations:
            for label, candidate in (
                ('zero mass reciprocal', cpp.replace('m_body->setMass(int(i),values[i]);',
                    'm_body->m_nodes[int(i)].m_im=1.0f/values[i];', 1)),
                ('lost zero recovery weights', cpp.replace('m_massWeights.swap(weights);',
                    'if(mass==0) for(double& w:weights) w=0; m_massWeights.swap(weights);', 1)),
                ('mass change bypasses cluster work budget', cpp.replace(
                    'if (mass > 0 && !dommy::bullet::softBodyClusterWorkBudget(',
                    'if (false && !dommy::bullet::softBodyClusterWorkBudget(', 1)),
                ('fixed body retains velocity', cpp.replace('node.m_v.setZero();', '', 1)),
                ('fixed body retains force', cpp.replace('node.m_f.setZero();', '', 1)),
                ('fixed body retains stale integration position', cpp.replace('node.m_q=node.m_x;', '', 1)),
                ('mass edit marks rest geometry dirty', cpp.replace(
                    'm_body->m_bUpdateRtCst=pendingInitialization;',
                    'm_body->m_bUpdateRtCst=true; (void)pendingInitialization;', 1)),
                ('mass edit leaves stale link coefficients', cpp.replace(
                    'if (massChanged) m_body->updateLinkConstants();', '(void)massChanged;', 1)),
                ('unchecked configured margin', cpp.replace(
                    '!dommy::bullet::validSoftBodyParameters(parameters)', 'false')),
                ('unchecked direct margin', cpp.replace(
                    'body() && dommy::bullet::isSafeSoftBodyCollisionMargin(margin)',
                    'body() && std::isfinite(margin) && margin>=0', 1)),
                ('missing bending flag', cpp.replace('m_body->m_links[m_body->m_links.size()-1].m_bbending=-1;', '(void)pm;', 1))):
                assert candidate != cpp
                try:
                    run_native(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(native_binary), 'Compile failure is not detection'
                    print('Detected soft-body mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()
