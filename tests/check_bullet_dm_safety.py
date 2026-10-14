#!/usr/bin/env python3
"""Execute DM solvers/rope callbacks; optionally verify fields and gravity with native Bullet."""
import argparse
import os
import re
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals


def sdk_admission(root,sdk):
    methods=[]
    for name in ('DCBullet2DSolver','DMBulletTest'):
        source=(root/('src/modules/dynamics/DBullet/DM/'+name+'.cpp')).read_text()
        callback=extract(source,'MStatus '+name+'::compute(')
        prefix=callback[:callback.index('const MDataHandle enabledInput')]
        clean=strip_comments_and_literals(prefix)
        observed=''.join('(void)'+value+';' for value in
            ('kinemaCount','ropecount','constCount','isCheckPanicSb','candidateTime')
            if re.search(r'\b'+value+r'\s*=',clean))
        depth=clean.count('{')-clean.count('}')
        assert depth==2,name
        prefix+=observed+'return MS::kSuccess;}return MS::kUnknownParameter;}'
        methods+=[prefix,'DOMMY_COMPUTE_CATCH("'+name+'")']
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-admission-sdk-') as directory:
        unit=Path(directory)/'sdk.cpp'
        unit.write_text('#include "bullet_dm_admission_sdk.h"\n'+'\n'.join(methods))
        command=shlex.split(os.environ.get('CXX','clang++'))+[
            '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-parameter','-Wno-unused-variable',
            '-isystem',str(sdk/'include'),'-I'+str(root/'tests/cpp'),'-I'+str(root/'src'),'-I'+str(root/'src/core')]
        subprocess.run(command+['-fsyntax-only',str(unit)],check=True)
        subprocess.run(command+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
            '-analyzer-werror','-o',os.devnull,str(unit)],check=True)
    print('Both production DM input admission prefixes: Maya SDK syntax/analysis passed; native payload/Bullet boundaries declared')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bullet-root', type=Path)
    parser.add_argument('--bullet-build', type=Path)
    parser.add_argument('--sdk',type=Path)
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--connection-mutations', action='store_true')
    parser.add_argument('--admission-mutations', action='store_true')
    parser.add_argument('--input-mutations', action='store_true',
                        help='Verify every compute/settings input getter checks acquisition first')
    args = parser.parse_args()
    if bool(args.bullet_root) != bool(args.bullet_build):
        parser.error('Supply both --bullet-root and --bullet-build')
    root = Path(__file__).resolve().parent.parent
    if args.sdk:
        sdk_admission(root,args.sdk)
        return
    folder = root / 'src/modules/dynamics/DBullet'
    methods = []
    dependencies = []
    for name in ('DCBullet2DSolver', 'DMBulletTest'):
        source = (folder / ('DM/' + name + '.cpp')).read_text()
        initializer = extract(source, 'MStatus ' + name + '::initialize(')
        dependencies.append('MStatus ' + name + 'Dependencies() try {\n' +
                            initializer[initializer.index('const MObject simulationInputs[]'):])
        method = extract(source, 'MStatus ' + name + '::compute(')
        assert 'CHECK_MSTATUS' not in method
        assert method.index('DOMMY_RETURN_IF_FAILURE(checkRigidBodies());') < method.index('->stepChecked(')
        assert 'return dommy::bullet::applyWorldFields(' in extract(source, 'MStatus ' + name + '::applyFields(')
        methods.append(method + '\ncatch (...) { return MS::kFailure; }')
        if name == 'DMBulletTest':
            mesh_compute = method
            methods.insert(0, extract(source, 'MStatus DMBulletTest::appendContactSamples('))
    compute = '\n'.join(methods)
    output = extract((root / 'src/core/common/utility/DMayaObjectArrayOutput.h').read_text(), 'namespace dommy {')
    fields = extract((folder / 'Solver/DBulletWorldFields.h').read_text(), 'namespace dommy {')
    evaluation = (folder / 'Solver/DBulletEvaluation.h').read_text()
    companion = root / 'src/modules/mesh/DMesh/Dynamics/Bullet'
    for name in ('DBPointConstraint', 'DBLinkConstraint', 'DMbtFracture'):
        source = (companion / (('' if name == 'DMbtFracture' else 'Constraint/') + name + '.cpp')).read_text()
        method = extract(source, 'MStatus ' + name + '::' +
                         ('resetRigidBodies(' if name == 'DMbtFracture' else 'createConstraint('))
        assert method.index('evalParms(') < method.index('detachFromWorld();')
        assert method.index('prepareConstraintFrames(') < method.index('detachFromWorld();')
        if name == 'DMbtFracture':
            compute_fracture = extract(source, 'MStatus DMbtFracture::compute(')
            assert 'detachFromWorld' not in compute_fracture
            assert 'prepareCollisionGeometry' not in compute_fracture
    rope = (companion / 'DCbtRope.cpp').read_text()
    reset_rope = extract(rope, 'MStatus DCbtRope::resetRopes(')
    rope_initial = reset_rope[reset_rope.index('std::vector<btVector3> rootPositions'):
                              reset_rope.index('// Preflight every segment')]
    rope_update = extract(rope, 'MStatus DCbtRope::updateLockPosition(')
    rope_flags = extract(rope, 'MStatus DCbtRope::updateRopes(')
    rope_parameters = extract(rope, 'MStatus DCbtRope::evalParms(')
    rope_position_guard = rope_parameters[rope_parameters.index('// Validate double-valued Maya positions'):rope_parameters.rfind('}')]
    assert reset_rope.index('ropeCapsuleHeight(') < reset_rope.index('detachFromWorld();')
    assert 'capsuleHeights[segmentIndex++]' in reset_rope
    soft = (companion / 'DMbtSoft.cpp').read_text()
    for name, source in (('DMbtSoft', soft), ('DCbtRope', rope)):
        output_compute = extract(source, 'MStatus ' + name + '::compute(')
        for side_effect in ('releaseSoftBodies', 'detachFromWorld', 'currentTime', 'getEvaluationTime'):
            assert side_effect not in output_compute, (name, side_effect)
    soft_callbacks = '\n'.join(extract(soft, signature) for signature in (
        'MStatus readSoftBodyTopology(', 'bool areValidSoftBodyParameters(', 'MStatus softBodyWorkBudget(', 'MStatus DMbtSoft::evalParms(',
        'void DMbtSoft::releaseSoftBodies(', 'MStatus DMbtSoft::resetSoftBodies(',
        'MStatus DMbtSoft::updateSoftBodies('))
    soft_creation = extract(soft, 'std::unique_ptr<db_softbody_base> createSoftBodyShape(')
    assert soft_creation.index('softBodyNativeWorkBudget(') < soft_creation.index('new db_softbody_base(')
    assert 'generateClusters(sParam->m_numClusters,\n                dommy::bullet::kMaxSoftClusterGenerationIterations)' in soft_creation
    dispatch_source = (folder / 'DM/DMBulletTest.cpp').read_text()
    dispatch = '\n'.join(extract(dispatch_source, 'MStatus DMBulletTest::' + name + '(')
                         for name in ('solverProcessFromPlug', 'initRigidBodies', 'checkRigidBodies'))
    dispatch_2d_source = (folder / 'DM/DCBullet2DSolver.cpp').read_text()
    dispatch += '\n' + '\n'.join(extract(dispatch_2d_source, 'MStatus DCBullet2DSolver::' + name + '(')
                         for name in ('solverProcessFromPlug', 'initRigidBodies', 'checkRigidBodies'))
    transforms = (companion / 'commonBulletRigidBody.h').read_text()
    gravity_start = mesh_compute.index('m_dynamicsWorld->setGravity(')
    gravity_update = mesh_compute[gravity_start:mesh_compute.index('m_dynamicsWorld->getSolverInfo()', gravity_start)]
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-dm-') as directory:
        temp = Path(directory)
        rigid_header = (companion / 'commonBulletRigidBody.h').read_text()
        rigid_output = extract(rigid_header, 'inline MStatus UpdateBulletGeometryTransform(') + '\n' + \
            'template <class Visitor>\n' + extract(rigid_header, 'MStatus VisitBulletRigidGeometry(')
        rigid_output_path = temp / 'dm_rigid_output_helpers.inc'
        rigid_output_path.write_text(rigid_output)
        output_callbacks = '\n'.join(extract((folder / ('DM/' + name + '.cpp')).read_text(),
            'MStatus ' + name + '::updateRigidBodies(') for name in ('DCBullet2DSolver', 'DMBulletTest'))
        (temp / 'dm_rigid_output_callbacks.inc').write_text(output_callbacks)
        (temp / 'dm_body_mode.inc').write_text(extract(rigid_header, 'enum class DBulletBodyMode') + ';')
        (temp / 'dm_soft_topology.inc').write_text(extract(
            (companion / 'DMbtSoft.h').read_text(), 'struct DBulletSoftTopology') + ';')
        (temp / 'dm_rope_topology.inc').write_text(extract(rope, 'MStatus readRopeTopology('))
        (temp / 'dm_rope_lifecycle.inc').write_text(extract(rope, 'void DCbtRope::detachFromWorld(') + '\n' + rope_flags)
        (temp / 'Solver').mkdir()
        compute_path = temp / 'dm_compute.inc'
        output_path = temp / 'dm_output.inc'
        fields_path = temp / 'dm_fields.inc'
        evaluation_path = temp / 'Solver/DBulletEvaluation.h'
        compute_path.write_text(compute)
        output_path.write_text(output)
        fields_path.write_text(fields)
        evaluation_path.write_text(evaluation)
        rope_initial_path = temp / 'dm_rope_initial.inc'
        rope_update_path = temp / 'dm_rope_update.inc'
        gravity_path = temp / 'dm_gravity_update.inc'
        rope_initial_path.write_text(rope_initial)
        rope_update_path.write_text(rope_update)
        gravity_path.write_text(gravity_update)
        (temp / 'dm_rope_positions.inc').write_text(extract(rope, 'MStatus getLockPosition('))
        rope_flags_path = temp / 'dm_rope_flags.inc'
        rope_flags_path.write_text(rope_flags)
        (temp / 'dm_rope_position_guard.inc').write_text(rope_position_guard)
        (temp / 'dm_capsule.inc').write_text(extract((companion / 'db_collision_base_shapes.h').read_text(), 'class db_collision_capsule:') + ';')
        soft_path = temp / 'dm_soft_callbacks.inc'
        soft_path.write_text(soft_callbacks)
        dispatch_path = temp / 'dm_dispatch.inc'
        dispatch_path.write_text(dispatch)
        (temp / 'dm_soft_params.inc').write_text(extract((companion / 'commonBulletSoftBody.h').read_text(), 'struct btSoftParams') + ';')
        attributes = re.findall(r'MPlug\(thisMObject\(\),\s*(\w+)\)', extract(soft, 'MStatus DMbtSoft::evalParms('))
        (temp / 'dm_soft_attributes.inc').write_text('enum {' + ','.join(dict.fromkeys(attributes)) + '};')
        (temp / 'dm_rope_transform.inc').write_text('\n'.join(extract(transforms, signature) for signature in (
            'inline void getDBtTransformTR(', 'inline void set_DB_RigidTransform(')))
        state = extract((folder / 'Solver/DBulletMayaState.h').read_text(), 'namespace dommy {')
        state_path = temp / 'dm_state.inc'
        state_path.write_text(state)
        (temp / 'dm_dependencies.inc').write_text('\n'.join(dependencies))
        header = (folder / 'DM/DMBulletTest.h').read_text()
        (temp / 'dm_contact_members.inc').write_text(extract(header, 'struct ContactSamples {') + ''';
        MStatus appendContactSamples(ContactSamples&) const;
        ContactSamples m_contactSamples;
        bool m_contactSamplesValid = false;
        ''')
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-deprecated-declarations',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer']
        command += ['-I' + str(p) for p in (temp, root / 'tests/stubs', root / 'src', root / 'src/core', root / 'src/core/math', folder)]
        binary = temp / 'evaluation'
        portable = command + [str(root / 'tests/cpp/bullet_dm_evaluation_tests.cpp'), '-o', str(binary)]

        def run(cmd, binary, capture=False):
            subprocess.run(cmd, check=True)
            subprocess.run([str(binary)], check=True, env=environment, capture_output=capture)

        def mutate(path, baseline, candidate, label, cmd, binary):
            assert baseline != candidate, label
            path.write_text(candidate)
            try:
                run(cmd, binary, True)
            except subprocess.CalledProcessError as error:
                assert error.cmd[0] == str(binary), 'Compile failure is not mutation detection'
                print('Detected DM mutation:', label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)
            finally:
                path.write_text(baseline)

        run(portable, binary)
        if args.admission_mutations:
            variants=[]
            for match in re.finditer(r'numConnectedElements\(&stat\)',compute):
                variants.append(('connected count '+str(len(variants)),
                    compute[:match.start()]+'numElements(&stat)'+compute[match.end():]))
            assert len(variants)==6
            for label,pattern,expected in (
                ('count ceiling',r'if \(\w+ > dommy::generation::kMaxConnectedGeometryCount\)\s*return MS::kInvalidParameter;',6),
                ('topology',r'if \(!dommy::maya::validatePluginData\(\*\w+\)\)\s*return MS::kInvalidParameter;',6),
                ('batch budget',r'if \(!dommy::bullet::admit(?:Mesh|Curve)Input\([^;]+\)\)\s*return MS::kInvalidParameter;',6),
                ('UUID',r'if \(!cuuid.valid\(\)\) return MS::kInvalidParameter;',4)):
                matches=list(re.finditer(pattern,compute));assert len(matches)==expected,(label,len(matches))
                for index,match in enumerate(matches):
                    variants.append((label+' '+str(index),
                        compute[:match.start()]+'if (false) return MS::kInvalidParameter;'+compute[match.end():]))
            pattern=r'DOMMY_RETURN_IF_FAILURE\((?P<call>dommy::maya::getPluginDataFromPlug\([^;]+?\))\);'
            matches=list(re.finditer(pattern,compute));assert len(matches)==6
            for index,match in enumerate(matches):
                variants.append(('payload status '+str(index),
                    compute[:match.start()]+'(void)'+match['call']+';'+compute[match.end():]))
            for label,candidate in variants:
                mutate(compute_path,compute,candidate,label,portable,binary)
            print('DM admission runtime mutations detected:',len(variants),flush=True)
        if args.connection_mutations:
            checks=list(re.finditer(r'isConnected\(&stat\);\n[ \t]*DOMMY_RETURN_IF_FAILURE\(stat\);',compute))
            assert len(checks)==6
            for index,check in enumerate(checks):
                candidate=compute[:check.start()]+check.group().split('\n')[0]+compute[check.end():]
                mutate(compute_path,compute,candidate,'DM connection '+str(index)+' failure ignored',portable,binary)
        callback_binary = temp / 'callbacks'
        callback_command = command + [str(root / 'tests/cpp/bullet_dm_callbacks_tests.cpp'), '-o', str(callback_binary)]
        run(callback_command, callback_binary)
        if args.mutations or args.input_mutations:
            pattern = re.compile(
                r'(?P<acquire>^[ \t]*const MDataHandle (?P<handle>\w+) = data\.inputValue\([^;\n]+;\n)'
                r'(?P<check>^[ \t]*(?:DOMMY_RETURN_IF_FAILURE\((?:stat|status)\);|'
                r'if \(!(?:stat|status)\) return (?:stat|status);)\n)'
                r'(?P<getter>^[ \t]*[^\n]*\b(?P=handle)\.as\w+\(\);\n)', re.M)
            for path, baseline, expected in ((compute_path, compute, 18), (state_path, state, 3)):
                matches = list(pattern.finditer(baseline))
                assert len(matches) == expected, (path.name, len(matches))
                for index, match in enumerate(matches):
                    candidate = baseline[:match.start()] + match['acquire'] + match['getter'] + \
                        match['check'] + baseline[match.end():]
                    mutate(path, baseline, candidate,
                           path.name + ' getter before acquisition check ' + match['handle'] + ' ' + str(index),
                           portable, binary)
        if args.mutations:
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                'if (!dommy::numeric::checkedFloatCast(value, converted) ||\n            (value != 0 && !std::isnormal(converted)))',
                'if (!std::isfinite(value))'),
                'soft double values admitted without float conversion validation', callback_command, callback_binary)
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                'count < 0 || count > 1024', 'count < 0'),
                'soft solver iteration cap removed', callback_command, callback_binary)
            for token in ('softBodyWorkBudget(topology, 0, sParam, work)',
                          'softBodyWorkBudget(topology, preSoftBodyCount, sParam, work)'):
                mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                    'DOMMY_RETURN_IF_FAILURE(' + token + ');', ''),
                    'soft preflight work budget bypassed: ' + token, callback_command, callback_binary)
            mutate(compute_path, compute, compute.replace(
                'DOMMY_RETURN_IF_FAILURE(updateRigidBodies(&outDMeshList, &nodeIdList));',
                '(void)updateRigidBodies(&outDMeshList, &nodeIdList);').replace(
                'DOMMY_RETURN_IF_FAILURE(updateRigidBodies(&outDCurveList, &nodeIdList));',
                '(void)updateRigidBodies(&outDCurveList, &nodeIdList);'),
                'rigid output failure swallowed', portable, binary)
            for node, method, expected in (
                    ('fractureNode', 'resetRigidBodies', 1),
                    ('fractureNode', 'updateRigidBodies', 1),
                    ('cNode', 'createConstraint', 2),
                    ('cNode', 'updateConstraint', 2)):
                pattern = r'DOMMY_RETURN_IF_FAILURE\((' + node + '->' + method + r'\([^;]+?)\);'
                matches = list(re.finditer(pattern, dispatch))
                assert len(matches) == expected, (node, method)
                for index, match in enumerate(matches):
                    candidate = dispatch[:match.start()] + '(void)' + match.group(1) + ';' + dispatch[match.end():]
                    mutate(dispatch_path, dispatch, candidate,
                           node + ' ' + method + ' failure swallowed ' + str(index),
                           callback_command, callback_binary)
            for method in ('resetRigidBodies', 'resetRigidBodiesKinematic', 'updateRigidBodies',
                           'updateCollisionShape', 'updateRigidBodiesKinematic'):
                pattern = r'DOMMY_RETURN_IF_FAILURE\((dmRbNode->' + method + r'\([^;]+?)\);'
                matches = list(re.finditer(pattern, dispatch))
                assert len(matches) == 2, method
                for index, match in enumerate(matches):
                    candidate = dispatch[:match.start()] + '(void)' + match.group(1) + ';' + dispatch[match.end():]
                    mutate(dispatch_path, dispatch, candidate,
                           ('3D' if index == 0 else '2D') + ' ' + method + ' failure swallowed',
                           callback_command, callback_binary)
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                'DOMMY_RETURN_IF_FAILURE(dMeshHndOwner.read(dmPlug));',
                'if (dMeshHndOwner.read(dmPlug) != MS::kSuccess) return MS::kSuccess;'),
                'soft input failure reported as success', callback_command, callback_binary)
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                '    btSoftParams sParam;', '    releaseSoftBodies();\n    btSoftParams sParam;', 1),
                'soft reset destroys old bodies before parameter validation', callback_command, callback_binary)
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                '        releaseSoftBodies();', '        // stale soft bodies retained'),
                'soft empty update retains native bodies', callback_command, callback_binary)
            mutate(soft_path, soft_callbacks, soft_callbacks.replace(
                'if (topology.size() < m_inputTopology.size() ||\n        !std::equal(m_inputTopology.begin(), m_inputTopology.end(), topology.begin()))',
                'if (false)'), 'soft topology changes silently accepted', callback_command, callback_binary)
            mutate(dispatch_path, dispatch, dispatch.replace(
                'DBulletBodyMode::Kinematic : DBulletBodyMode::Dynamic',
                'DBulletBodyMode::Dynamic : DBulletBodyMode::Dynamic'),
                'kinematic additions dispatched as dynamic bodies', callback_command, callback_binary)
            for name in ('initRigidBodies', 'checkRigidBodies'):
                mutate(compute_path, compute, compute.replace('DOMMY_RETURN_IF_FAILURE(' + name + '());', '(void)' + name + '();'),
                       name + ' failure swallowed by compute', portable, binary)
            mutate(dispatch_path, dispatch, dispatch.replace(
                'DOMMY_RETURN_IF_FAILURE(ropeNode->updateRopes(\n                                m_dynamicsWorld.get(), m_worldInfo.get()));',
                '(void)ropeNode->updateRopes(m_dynamicsWorld.get(), m_worldInfo.get());'),
                'rope failure swallowed by dispatch', callback_command, callback_binary)
            mutate(evaluation_path, evaluation, evaluation.replace('if (armed_) {', 'if (false) {'),
                   'failed world remains reusable', portable, binary)
            mutate(evaluation_path, evaluation, evaluation.replace('if (deltaFrames == 0)', 'if (deltaFrames <= 0)'),
                   'rewind treated as same frame', portable, binary)
            mutate(compute_path, compute, compute.replace('candidateTime = time;', 'candidateTime = time; m_prevTime = time;'),
                   'timestamp committed before output', portable, binary)
            mutate(output_path, output, output.replace('if (!status) return status;', '(void)status;'),
                   'array output failure swallowed', portable, binary)
            mutate(output_path, output, output.replace('if (!markClean) return MS::kSuccess;', '(void)markClean;'),
                   'outputs cleaned before all values published', portable, binary)
            mutate(compute_path, compute, compute.replace(' || plug == outRopeDC || plug == outContactDP', ''),
                   'rope and contact output requests rejected', portable, binary)
            mutate(compute_path, compute, compute.replace('reuseContacts = m_contactSamplesValid;', 'reuseContacts = false;'),
                   'same-frame contact batch recollected', portable, binary)
            mutate(compute_path, compute, compute.replace('        DOMMY_RETURN_IF_FAILURE(data.setClean(plug));',
                   '        if (replaceContacts) m_contactSamples = candidateContacts;\n        DOMMY_RETURN_IF_FAILURE(data.setClean(plug));'),
                   'contact cache committed before output success', portable, binary)
            mutate(compute_path, compute, compute.replace('candidateSettings != m_worldSettings', 'false'),
                   'initialization-only settings do not require restart', portable, binary)
            dependency_source = '\n'.join(dependencies)
            mutate(temp / 'dm_dependencies.inc', dependency_source,
                   dependency_source.replace('inDC, inKinematicDCs, inConst,', 'inKinematicDCs, inConst,'),
                   'input geometry dependency missing', portable, binary)
            mutate(temp / 'dm_dependencies.inc', dependency_source,
                   dependency_source.replace('outDM, outSoftDM, outRopeDC, outContactDP', 'outDM, outSoftDM, outRopeDC'),
                   'contact output dependency missing', portable, binary)
            mutate(compute_path, compute, compute.replace('m_worldInfo->m_gravity = m_dynamicsWorld->getGravity();', ''),
                   'soft gravity not updated on advance', portable, binary)
            mutate(compute_path, compute, compute.replace(' && bool(m_worldInfo)', ''),
                   'missing soft-world descriptor accepted', portable, binary)

        rope_binary = temp / 'rope'
        rope_command = command + [str(root / 'tests/cpp/bullet_dm_rope_tests.cpp'), '-o', str(rope_binary)]
        run(rope_command, rope_binary)
        if args.connection_mutations:
            positions=extract(rope,'MStatus getLockPosition(')
            mutate(temp/'dm_rope_positions.inc',positions,
                   positions.replace('if (!status) return status;','',1),
                   'rope connection failure ignored',rope_command,rope_binary)

        def mutate_rope(cmd, target):
            mutate(rope_flags_path, rope_flags,
                   rope_flags.replace('constraint->constraint()->setEnabled(param.m_lock_tip);', '(void)constraint;'),
                   'tip lock flag ignored', cmd, target)
            mutate(rope_flags_path, rope_flags,
                   rope_flags.replace('if (!rbList.empty() &&', 'if (false &&'),
                   'missing endpoint constraint silently accepted', cmd, target)
            mutate(rope_initial_path, rope_initial,
                   rope_initial.replace('getLockPosition(tipPositions,', 'getLockPosition(rootPositions,'),
                   'tip input overwrites root positions', cmd, target)
            for occurrence, label in ((0, 'root'), (1, 'tip')):
                token = 'btRigidBody& body ='
                offset = rope_update.index(token) if occurrence == 0 else rope_update.rindex(token)
                candidate = rope_update[:offset] + rope_update[offset:].replace(token, 'btRigidBody body =', 1)
                mutate(rope_update_path, rope_update, candidate,
                       label + ' constrained body copied', cmd, target)

        if args.mutations:
            mutate_rope(rope_command, rope_binary)
        if not args.bullet_root:
            print('Native field/rope/gravity checks require --bullet-root/--bullet-build')
            return
        soft_owner = extract((companion / 'commonBulletSoftBody.h').read_text(), 'class db_softbody_base') + ';'
        soft_owner_path = temp / 'dm_soft_owner.inc'
        soft_owner_path.write_text(soft_owner)
        native_creation = extract(soft, 'bool areValidSoftBodyParameters(') + '\n' + soft_creation
        creation_path = temp / 'dm_soft_creation.inc'
        creation_path.write_text(native_creation)
        creation_binary = temp / 'native_soft_creation'
        # Instrument the native body implementation as well as the adapter;
        # the other native libraries are prebuilt and not fully instrumented.
        # Upstream 2.88 warnings are not project warnings: compile it separately
        # so the production adapter/fixture still retain -Werror.
        native_soft_object = temp / 'btSoftBody.o'
        subprocess.run(command + ['-DDEBUG', '-w', '-isystem', str(args.bullet_root / 'src'),
            '-c', str(args.bullet_root / 'src/BulletSoftBody/btSoftBody.cpp'),
            '-o', str(native_soft_object)], check=True)
        creation_command = command + ['-DDEBUG', '-Wno-unused-function',
            '-isystem', str(args.bullet_root / 'src'), '-I' + str(folder / 'mvl'),
            str(root / 'tests/cpp/bullet_dm_soft_creation_tests.cpp'),
            str(native_soft_object)]
        creation_command += [str(args.bullet_build / 'src' / name / ('lib' + name + '.a'))
                             for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        creation_command += ['-o', str(creation_binary)]
        run(creation_command, creation_binary)
        if args.mutations:
            mutate(creation_path, native_creation, native_creation.replace(
                'dommy::bullet::isSafeSoftBodyCollisionMargin(params.m_collisionMargin)',
                '(params.m_collisionMargin >= 0)'),
                'DM oversized collision margin admitted', creation_command, creation_binary)
            mutate(soft_owner_path, soft_owner, soft_owner.replace(
                'dommy::bullet::isSafeSoftBodyCollisionMargin(margin)', 'margin >= 0'),
                'DM wrapper oversized collision margin admitted', creation_command, creation_binary)
            mutate(creation_path, native_creation, native_creation.replace(
                'sb->body()->setMass(static_cast<int>(i), masses[i]);',
                'sb->body()->m_nodes[static_cast<int>(i)].m_im = 1.0f / masses[i];'),
                'DM zero mass produces infinite inverse masses', creation_command, creation_binary)
            cluster_block = extract(soft_creation, 'if (useClusters) {')
            candidate = native_creation.replace(cluster_block, '').replace(
                'sb->body()->scale(btVector3(', cluster_block + '\n        sb->body()->scale(btVector3(')
            mutate(creation_path, native_creation, candidate,
                'DM clusters cache pre-scale and pre-mass state', creation_command, creation_binary)
            mutate(creation_path, native_creation, native_creation.replace(
                'sb->body()->m_bUpdateRtCst = false;', 'sb->body()->m_bUpdateRtCst = true;'),
                'DM first tick erases rest length scale', creation_command, creation_binary)
            mutate(creation_path, native_creation, native_creation.replace(
                'sb->body()->setRestLengthScale(sParam->m_rest_length_scale);',
                'sb->body()->setRestLengthScale(sParam->m_rest_length_scale);\n        sb->body()->resetLinkRestLengths();'),
                'DM final reset erases rest length scale', creation_command, creation_binary)
            mutate(creation_path, native_creation, native_creation.replace(
                'sb->body()->initializeFaceTree();', '(void)0;'),
                'DM initialization omits soft contact face tree', creation_command, creation_binary)
            mutate(soft_owner_path, soft_owner, soft_owner.replace(
                'if (m_body->getTotalMass() == 0 || (!volume && !frame)) return;', ''),
                'DM zero mass enters native pose initialization', creation_command, creation_binary)
            mutate(creation_path, native_creation, native_creation.replace(
                'sParam->m_is_cluster && sParam->m_mass > 0', 'sParam->m_is_cluster'),
                'DM zero mass incorrectly requires cluster inertia', creation_command, creation_binary)
        rigid_header = (companion / 'commonBulletRigidBody.h').read_text()
        (temp / 'dm_rigid_parameters.inc').write_text(extract(rigid_header, 'struct btRigidParams') + ';\n' +
            extract(rigid_header, 'inline bool GetBulletCollisionScale(') + '\n' +
            extract(rigid_header, 'inline bool ValidateBulletRigidParameters('))
        (temp / 'dm_rigid_kinematic.inc').write_text(extract(rigid_header, 'inline void set_kinematic(') + '\n' +
            extract(rigid_header, 'inline void ConfigureBulletBodyMotion('))
        common_header = (companion / 'commonDBullet.h').read_text()
        constraint_header = (companion / 'db_constraint_base.h').read_text()
        constraint_header_path = temp / 'db_constraint_base.h'
        constraint_header_path.write_text(constraint_header)
        (temp / 'dm_rigid_world_reference.inc').write_text(extract(common_header, 'class DBulletWorldReference final') + ''';
using DBulletWorldReferencePtr=std::shared_ptr<DBulletWorldReference>;
using DBulletWeakWorldReference=std::weak_ptr<DBulletWorldReference>;
''' + extract(common_header, 'inline btDynamicsWorld* ResolveBulletWorld('))
        common_source = (companion / 'commonDBullet.cpp').read_text()
        (temp / 'dm_rigid_ownership.inc').write_text('''
DBulletRigidBodyPtr MakeBulletRigidBody(std::unique_ptr<btMotionState>,
    const btRigidBody::btRigidBodyConstructionInfo&, std::unique_ptr<customDBData> = {});
''' + '\n'.join(extract(common_source, signature) for signature in (
            'void DBulletRigidBodyDeleter::operator()(', 'DBulletRigidBodyPtr MakeBulletRigidBody(',
            'void AddBulletRigidBody(', 'void ReleaseBulletRigidBodies(', 'void DetachAndReleaseBulletRigidBodies(')))
        shape_helpers = '\n'.join(extract(common_source, 'bool ' + name + '(')
                                  for name in ('PrepareBulletCollisionShape', 'ReplaceBulletCollisionShape'))
        shape_helpers_path = temp / 'dm_rigid_shape_helpers.inc'
        shape_helpers_path.write_text(shape_helpers)
        rigid_callbacks = '\n'.join(extract((companion / (name + '.cpp')).read_text(), 'MStatus ' + name + '::' + method + '(')
            for name in ('DMbtRigid', 'DCbtRigid2D') for method in (
                'evalParms', 'resetRigidBodies', 'resetRigidBodiesKinematic', 'updateRigidBodies',
                'updateCollisionShape', 'updateRigidBodiesKinematic'))
        rigid_callbacks_path = temp / 'dm_rigid_callbacks.inc'
        rigid_callbacks_path.write_text(rigid_callbacks)
        attrs = ['inDM', 'inDC'] + re.findall(r'MPlug\(thisMObject\(\),\s*(\w+)\)', rigid_callbacks)
        (temp / 'dm_rigid_attributes.inc').write_text('enum {' + ','.join(dict.fromkeys(attrs)) + '};')
        rigid_binary = temp / 'native_rigid'
        rigid_command = command + ['-DDEBUG', '-isystem', str(args.bullet_root / 'src'),
            '-I' + str(companion),
            str(root / 'tests/cpp/bullet_dm_rigid_tests.cpp')]
        rigid_command += [str(args.bullet_build / 'src' / name / ('lib' + name + '.a'))
                         for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        rigid_command += ['-o', str(rigid_binary)]
        run(rigid_command, rigid_binary)
        if args.mutations:
            mutate(rigid_callbacks_path, rigid_callbacks, rigid_callbacks.replace(
                'static_cast<int>(collisionShapes[i]->itemIndex())', 'static_cast<int>(i)'),
                '2D output uses compact slot instead of original input ID', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers, shape_helpers.replace(
                'replacement->set_itemIndex(owner->itemIndex());', ''),
                'deformed replacement loses original input ID', rigid_command, rigid_binary)
            mutate(rigid_output_path, rigid_output, rigid_output.replace(
                'MTransformationMatrix candidate(source);', 'MTransformationMatrix candidate;'),
                'output discards source scale', rigid_command, rigid_binary)
            mutate(rigid_output_path, rigid_output, rigid_output.replace(
                'if (metadata) {', 'if (metadata && !body->getCollisionShape()->isCompound()) {'),
                'single-input compound ignores root output metadata', rigid_command, rigid_binary)
            mutate(rigid_callbacks_path, rigid_callbacks, rigid_callbacks.replace(
                'ConfigureBulletBodyMotion(*rb, rParam, mode,',
                'ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Dynamic,'),
                'added kinematic bodies configured as dynamic', rigid_command, rigid_binary)
            mutate(rigid_callbacks_path, rigid_callbacks, rigid_callbacks.replace(
                'if (!compoundCollisionLists.empty()) return MS::kInvalidParameter;', ''),
                'compound kinematic reset silently drops bodies', rigid_command, rigid_binary)
        legacy_names = ('BTP2PConstraint', 'BTFixConstraint', 'BTConeTwistConstraint', 'BT6DofConstraint')
        legacy_sources = {name: (companion / 'Constraint' / (name + '.cpp')).read_text()
                          for name in legacy_names + ('DBPointConstraint', 'DBLinkConstraint')}
        legacy_sources['DMbtFracture'] = (companion / 'DMbtFracture.cpp').read_text()
        legacy_attributes = []
        for name, source in legacy_sources.items():
            legacy_attributes += re.findall(r'MObject ' + name + r'::(\w+);', source)
        (temp / 'dm_legacy_attributes.inc').write_text('enum {' +
            ','.join(dict.fromkeys(legacy_attributes)) + '};')
        (temp / 'dm_legacy_parameters.inc').write_text('\n'.join(
            extract((companion / 'Constraint' / (name + '.h')).read_text(), 'struct ' + param) + ';'
            for name, param in (('DBPointConstraint', 'DBPointConstParams'), ('DBLinkConstraint', 'DBLinkConstParams'))))
        validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
        (temp / 'dm_legacy_vector_validation.inc').write_text('namespace dommy { namespace maya {\n' +
            extract(validation, 'inline bool isFiniteVector(') + '\n' +
            extract(validation, 'inline bool isFloatRepresentableVector(') + '\n}}')
        (temp / 'dm_legacy_settings.inc').write_text('\n'.join(
            extract(legacy_sources[name], 'MStatus ' + name + '::evalParms(') for name in legacy_names))
        helper_source = (companion / 'commonDBulletConstraint.h').read_text()
        (temp / 'dm_legacy_inputs.inc').write_text('\n'.join(extract(helper_source, token) for token in (
            'inline MStatus getConstraintInfo(', 'inline MStatus getDPointPositionToBTVec(',
            'inline MStatus getRBPositionToBTVecList(', 'inline MStatus getRBPositionToBTVec(',
            'inline db_constraint_base* makeNewConstraint(')))
        legacy_ownership = '\n'.join(extract(common_source, token) for token in (
            'DBulletRigidBodyPtr MakeSimpleKinematicRigidBody(', 'void AddBulletRigidBodyWithShape(',
            'void AddBulletConstraint(', 'void ReleaseBulletCollisionShapes(', 'void ReleaseBulletConstrains(',
            'void DetachAndReleaseBulletConstraints('))
        (temp / 'dm_legacy_ownership.inc').write_text(legacy_ownership)
        legacy_callbacks = '\n'.join(extract(legacy_sources[name], signature + name + '::' + method + '(')
            for name in ('DBPointConstraint', 'DBLinkConstraint', 'DMbtFracture')
            for signature, method in (('MStatus ', 'evalParms'), ('void ', 'detachFromWorld'),
                ('MStatus ', 'resetRigidBodies' if name == 'DMbtFracture' else 'createConstraint'),
                ('MStatus ', 'updateRigidBodies' if name == 'DMbtFracture' else 'updateConstraint')))
        (temp / 'dm_legacy_callbacks.inc').write_text(legacy_callbacks)
        legacy_binary = temp / 'native_legacy'
        legacy_command = command + ['-DDEBUG', '-Wno-unused-function', '-isystem', str(args.bullet_root / 'src'),
            '-I' + str(companion), '-I' + str(root / 'src/modules/point'),
            str(root / 'tests/cpp/bullet_dm_legacy_tests.cpp'),
            str(args.bullet_root / 'src/BulletDynamics/Dynamics/btRigidBody.cpp'),
            str(args.bullet_root / 'src/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp'),
            str(root / 'src/core/math/dommyMath.cpp')]
        legacy_command += [str(args.bullet_build / 'src' / name / ('lib' + name + '.a'))
                           for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        legacy_command += ['-o', str(legacy_binary)]
        run(legacy_command, legacy_binary)
        geometry_helpers = (companion / 'DBulletGeometryPreparation.h').read_text()
        geometry_helpers = geometry_helpers[geometry_helpers.index('inline MStatus ReadBulletGeometryTransform('):]
        (temp / 'dm_geometry_helpers.inc').write_text(geometry_helpers)
        (temp / 'dm_geometry_release.inc').write_text('\n'.join(extract(common_source, token) for token in (
            'void ReleaseBulletCollisionShapes(', 'void ReleaseBulletConstrains(',
            'void DetachAndReleaseBulletConstraints(')))
        geometry_callbacks = '\n'.join(extract((companion / (name + '.cpp')).read_text(),
            signature + name + '::' + method + '(')
            for name in ('DMbtRigid', 'DCbtRigid2D', 'DMbtFracture')
            for signature, method in (('void ', 'detachFromWorld'), ('void ', 'clearCollisionGeometry'),
                                      ('MStatus ', 'prepareCollisionGeometry')))
        (temp / 'dm_geometry_callbacks.inc').write_text(geometry_callbacks)
        for name in ('DMbtRigid', 'DCbtRigid2D', 'DMbtFracture', 'DMbtSoft', 'DCbtRope'):
            geometry_compute = extract((companion / (name + '.cpp')).read_text(), 'MStatus ' + name + '::compute(')
            assert 'detachFromWorld' not in geometry_compute and 'collisionShapes' not in geometry_compute
            assert 'currentTime' not in geometry_compute and 'prepareCollisionGeometry' not in geometry_compute
            assert 'releaseSoftBodies' not in geometry_compute
        geometry_binary = temp / 'native_geometry'
        geometry_command = [str(root / 'tests/cpp/bullet_dm_geometry_tests.cpp') if token ==
            str(root / 'tests/cpp/bullet_dm_legacy_tests.cpp') else str(geometry_binary) if token ==
            str(legacy_binary) else token for token in legacy_command]
        geometry_command += ['-I' + str(folder / 'mvl')]
        run(geometry_command, geometry_binary)
        if args.mutations:
            mutate(temp / 'dm_geometry_callbacks.inc', geometry_callbacks, geometry_callbacks.replace(
                'if (grouped && mode == DBulletBodyMode::Kinematic)', 'if (false)'),
                'compound kinematic preparation mutates geometry', geometry_command, geometry_binary)
            ownership_path = temp / 'dm_rigid_ownership.inc'
            ownership = ownership_path.read_text()
            mutate(ownership_path, ownership,
                   ownership.replace('worldReference->detachConstraintsForBody(body);', ''),
                   'body deleted before dependent constraints', legacy_command, legacy_binary)
            mutate(constraint_header_path, constraint_header,
                   constraint_header.replace('setAngularLowerLimit(angularLowerLimitVec)',
                                             'setLinearLowerLimit(angularLowerLimitVec)'),
                   '6DoF angular limits overwrite linear limits', legacy_command, legacy_binary)
            mutate(constraint_header_path, constraint_header,
                   constraint_header.replace('btVector3 transStiff(params.m_translate_stiffness[0],',
                       'btVector3 transStiff(dommy::math::degreesToRadians(params.m_translate_stiffness[0]),'),
                   'translation stiffness converted as angle', legacy_command, legacy_binary)
            mutate(temp / 'dm_geometry_callbacks.inc', geometry_callbacks,
                   geometry_callbacks.replace('const bool replace = rebuild || count == 0;',
                                              'const bool replace = count == 0;'),
                   'solver-requested geometry rebuild ignored', geometry_command, geometry_binary)
            mutate(temp / 'dm_geometry_callbacks.inc', geometry_callbacks,
                   geometry_callbacks.replace('ReleaseBulletCollisionShapes(collisionShapes);', ''),
                   'empty geometry retains collision shapes', geometry_command, geometry_binary)
            mutate(dispatch_path, dispatch,
                   dispatch.replace('prepareCollisionGeometry(true)', 'prepareCollisionGeometry(false)'),
                   'initialization uses incremental geometry', callback_command, callback_binary)
            mutate(ownership_path, ownership, ownership.replace('world.removeRigidBody(rawBody);', ''),
                   'failed body registration leaves a dangling world pointer', legacy_command, legacy_binary)
            mutate(temp / 'dm_legacy_ownership.inc', legacy_ownership,
                   legacy_ownership.replace('world.removeConstraint(rawConstraint->constraint());', ''),
                   'failed constraint registration leaves a dangling world pointer', legacy_command, legacy_binary)
            for name in ('DMbtRigid', 'DCbtRigid2D'):
                method = extract(rigid_callbacks, 'MStatus ' + name + '::updateRigidBodiesKinematic(')
                candidate = method.replace('vec3f position;', 'rot1 = rot0;\n                    vec3f position;')
                mutate(rigid_callbacks_path, rigid_callbacks, rigid_callbacks.replace(method, candidate),
                       name + ' target rotation ignored', rigid_command, rigid_binary)
            for name in ('DBPointConstraint', 'DBLinkConstraint', 'DMbtFracture'):
                method_name = 'resetRigidBodies' if name == 'DMbtFracture' else 'createConstraint'
                method = extract(legacy_callbacks, 'MStatus ' + name + '::' + method_name + '(')
                if name == 'DMbtFracture':
                    candidate = method.replace('first, second, connection.frameA, connection.frameB',
                                               'btTransform::getIdentity(), btTransform::getIdentity(), connection.frameA, connection.frameB')
                else:
                    candidate = method.replace('source->getWorldTransform()', 'btTransform::getIdentity()')
                mutate(temp / 'dm_legacy_callbacks.inc', legacy_callbacks, legacy_callbacks.replace(method, candidate),
                       name + ' body-to-local frame omitted', legacy_command, legacy_binary)
                declaration = {'DBPointConstraint': 'DBPointConstParams param;',
                               'DBLinkConstraint': 'DBLinkConstParams param;',
                               'DMbtFracture': 'btRigidParams params;'}[name]
                candidate = method.replace(declaration, 'detachFromWorld();\n    ' + declaration)
                mutate(temp / 'dm_legacy_callbacks.inc', legacy_callbacks, legacy_callbacks.replace(method, candidate),
                       name + ' detaches before preflight', legacy_command, legacy_binary)
            inputs_path = temp / 'dm_legacy_inputs.inc'
            inputs = inputs_path.read_text()
            mutate(inputs_path, inputs, inputs.replace('DOMMY_RETURN_IF_FAILURE(owner.read(dpPlug));',
                   '(void)owner.read(dpPlug);'), 'connected point read failure swallowed', legacy_command, legacy_binary)
            mutate(rigid_callbacks_path, rigid_callbacks,
                rigid_callbacks.replace('DOMMY_RETURN_IF_FAILURE(evalParms(rParam));', '(void)evalParms(rParam);'),
                'rigid parameter failure swallowed before detach', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers,
                shape_helpers.replace('body.setMassProps(effectiveMass, inertia);', ''),
                'deformed body inertia not applied', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers,
                shape_helpers.replace('shape->setLocalScaling(scale);', ''),
                'rigid collision scale ignored', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers,
                shape_helpers.replace('body.updateInertiaTensor();', ''),
                'deformed world inertia tensor stale', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers,
                shape_helpers.replace('body.setGravity(candidateGravity);', 'body.setGravity(gravity);'),
                'newly dynamic body retains static gravity', rigid_command, rigid_binary)
            mutate(shape_helpers_path, shape_helpers,
                shape_helpers.replace('for (const auto& child : children) child.first->setMargin(margin);', ''),
                'compound child margin ignored', rigid_command, rigid_binary)
        native_binary = temp / 'fields'
        native = command + ['-isystem', str(args.bullet_root / 'src'),
                            str(root / 'tests/cpp/bullet_dm_field_tests.cpp')]
        native += [str(args.bullet_build / 'src' / n / ('lib' + n + '.a'))
                   for n in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        native += ['-o', str(native_binary)]
        run(native, native_binary)
        if args.mutations:
            mutate(fields_path, fields, fields.replace('if (!fieldVector(rigidTotals[i], rigidForces[i])) return MS::kInvalidParameter;',
                'rigidForces[i] = btVector3(float(rigidTotals[i].x), float(rigidTotals[i].y), float(rigidTotals[i].z));').replace(
                'if (!numeric::checkedFloatCast(double(rigidForces[i][axis]) *\n                    rigidBodies[i]->getLinearFactor()[axis], product)) return MS::kInvalidParameter;', '(void)product;'),
                'unchecked double narrowing', native, native_binary)
            mutate(fields_path, fields, fields.replace('DOMMY_RETURN_IF_FAILURE(fieldNode.getForceAtPoint(positions, velocities, masses, forces, dt));',
                '(void)fieldNode.getForceAtPoint(positions, velocities, masses, forces, dt);'),
                'field API failure swallowed', native, native_binary)
            mutate(fields_path, fields, fields.replace('rigidBodies[i]->clearForces();', '(void)rigidBodies[i];'),
                'stale rigid forces accumulate', native, native_binary)
        native_rope_binary = temp / 'native_rope'
        native_rope = command + ['-DDEBUG', '-DDOMMY_NATIVE_BULLET', '-isystem', str(args.bullet_root / 'src'),
                                 str(root / 'tests/cpp/bullet_dm_rope_tests.cpp')]
        native_rope += [str(args.bullet_build / 'src' / name / ('lib' + name + '.a'))
                       for name in ('BulletSoftBody', 'BulletDynamics', 'BulletCollision', 'LinearMath')]
        native_rope += ['-o', str(native_rope_binary)]
        run(native_rope, native_rope_binary)
        if args.mutations:
            mutate_rope(native_rope, native_rope_binary)
            mutate(gravity_path, gravity_update,
                   gravity_update.replace('m_worldInfo->m_gravity = m_dynamicsWorld->getGravity();', ''),
                   'native soft gravity remains at its initial value', native_rope, native_rope_binary)

        fracture_command = [sys.executable, str(root / 'tests/check_bullet_fracture_safety.py'),
                            '--bullet-root', str(args.bullet_root),
                            '--bullet-build', str(args.bullet_build)]
        if args.mutations:
            fracture_command.append('--mutations')
        subprocess.run(fracture_command, check=True)


if __name__ == '__main__':
    main()
