#!/usr/bin/env python3
"""Exercise constraint callbacks with host doubles and optionally real Bullet frames."""
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
    args = parser.parse_args()
    if bool(args.bullet_root) != bool(args.bullet_build):
        parser.error('Supply both --bullet-root and --bullet-build')
    root = Path(__file__).resolve().parent.parent
    folder = root / 'src/modules/dynamics/DBullet'
    names = ('Nail', 'Hinge', 'Slider', 'Sixdof')
    sources = {name: (folder / ('Constraint/DB' + name + 'ConstraintNode.cpp')).read_text()
               for name in names}
    utils = (folder / 'Constraint/DBConstraintNodeUtils.h').read_text()
    helpers = '\n'.join(extract(utils, token) for token in (
        'inline MStatus finiteFloat(', 'inline MStatus boolean(', 'inline MStatus finiteVector(',
        'inline bool checkedPosition(', 'inline bool checkedRotation(', 'inline MStatus finiteRotation(',
        'inline MStatus publishBoolean(', 'inline bool finiteFrame(', 'inline MStatus writeFrame(',
        'template <typename ConstraintPointer>'))
    declarations = []
    defaults = []
    attributes = {}
    next_id = 1
    for name in names:
        node = 'DB' + name + 'ConstraintNode'
        ids = re.findall(r'MObject ' + node + r'::(\w+);', sources[name])
        attributes[name] = dict((key, next_id + i) for i, key in enumerate(ids))
        next_id += len(ids)
        declarations.append('struct ' + node + ' {\n enum {' + ','.join(
            key + '=' + str(value) for key, value in attributes[name].items()) + '};\n' + '''
    FakeConstraint::pointer m_constraint;
    vec3f m_pivInA, m_pivInB, m_PivInA, m_PivInB;
    quatf m_RotInA, m_RotInB;
    MObject thisMObject() const { return 0; }
    MStatus computeConstraint(const MPlug&, MDataBlock&);
    MStatus computeConstraintParam(const MPlug&, MDataBlock&);
    MStatus computeWorldMatrix(const MPlug&, MDataBlock&);
    MStatus applyConstraintParameters(MDataBlock&, const FakeConstraint::pointer&);
};''')
        edges = re.findall(r'attributeAffects\((\w+),\s*(\w+)\)', sources[name])
        declarations.append('static const std::pair<int,int> edges' + name + '[] = {' + ','.join(
            '{' + node + '::' + a + ',' + node + '::' + b + '}' for a, b in edges) + '};')
        defaults.append('static void defaults' + name + '() { fixture::reset();')
        for attr, idx in attributes[name].items():
            value = 7 if attr == 'ia_breakThreshold' else .5
            if attr in ('ia_lowerLimit', 'ia_lowerLinLimit', 'ia_lowerAngLimit'): value = -1
            if attr in ('ia_upperLimit', 'ia_upperLinLimit', 'ia_upperAngLimit'): value = 1
            if attr in ('ia_disableCollide', 'ia_enableAngularMotor'): value = 1
            defaults.append('fixture::scalar[' + str(idx) + ']=' + str(value) + ';')
            if attr.startswith('ia_pivotIn'):
                defaults.append('fixture::vectors[' + str(idx) + '][0]=1;')
        defaults.append('}')
    methods = '\n'.join(extract(sources[name], 'MStatus DB' + name + 'ConstraintNode::' + method + '(')
                        for name in names for method in (
                            'computeConstraint', 'computeConstraintParam', 'applyConstraintParameters', 'computeWorldMatrix'))
    solver = (folder / 'Solver/DBSolverNode.cpp').read_text()
    writer = extract(solver, 'void tokenize(') + '\n' + extract(solver, 'bool DBSolverNode::expandFileExpression(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-function',
        '-Wno-unused-parameter', '-fsanitize=address,undefined,float-cast-overflow',
        '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
        '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src'),
        '-I' + str(root / 'src/core'), '-I' + str(root / 'src/core/math'), '-I' + str(folder),
        '-I' + str(folder / 'mvl'), '-I' + str(folder / 'Constraint')]
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-constraints-') as directory:
        temp = Path(directory)
        original = {
            'constraint_declarations.inc': '\n'.join(declarations),
            'constraint_defaults.inc': '\n'.join(defaults),
            'constraint_helpers.inc': helpers,
            'constraint_methods.inc': methods,
            'constraint_writer.inc': writer}
        binary = temp / 'callbacks'
        def portable(files, capture=False):
            for file, text in files.items(): (temp / file).write_text(text)
            subprocess.run(command + ['-I' + str(temp), str(root / 'tests/cpp/bullet_constraint_node_tests.cpp'),
                                      '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env=env, capture_output=capture)
        portable(original)
        if args.mutations:
            changes = [
                ('missing pivot dependency', 'constraint_declarations.inc',
                 '{DBHingeConstraintNode::ia_pivotInA,DBHingeConstraintNode::ca_constraint}',
                 '{DBHingeConstraintNode::ia_pivotInA,DBHingeConstraintNode::ca_constraintParam}'),
                ('missing recreation parameter dependency', 'constraint_declarations.inc',
                 '{DBNailConstraintNode::ia_disableCollide,DBNailConstraintNode::ca_constraintParam}',
                 '{DBNailConstraintNode::ia_disableCollide,DBNailConstraintNode::ca_constraint}'),
                ('unconfigured replacement', 'constraint_methods.inc',
                 'status = applyConstraintParameters(data, replacement);', 'status = MS::kSuccess;'),
                ('ignored native frame failure', 'constraint_methods.inc',
                 'if (!m_constraint->set_world(world)) return MS::kInvalidParameter;',
                 'm_constraint->set_world(world);'),
                ('unchecked double narrowing', 'constraint_helpers.inc',
                 'if (!dommy::numeric::checkedFloatCast(input.x, candidate[0]) ||',
                 'candidate[0] = static_cast<float>(input.x);\n    if (false ||'),
                ('unchecked frame integer cast', 'constraint_writer.inc',
                 'if (!dommy::file::checkedTruncatedFrame(m_prevTime.value(), frame)) return false;',
                 'frame = static_cast<int>(m_prevTime.value());')]
            for label, file, before, after in changes:
                candidate = dict(original)
                assert before in candidate[file], label
                candidate[file] = candidate[file].replace(before, after, 1)
                try:
                    portable(candidate, True)
                except subprocess.CalledProcessError as error:
                    assert error.cmd[0] == str(binary), 'Compile failure is not mutation detection'
                    print('Detected constraint mutation:', label, flush=True)
                else:
                    raise AssertionError('Undetected mutation: ' + label)
            portable(original, True)
        if args.bullet_root:
            setters = []
            for name in names:
                source = (folder / ('Constraint/DB_' + name + '_Constraint_t.h')).read_text()
                setters.append('struct Probe' + name + ' : ProbeBase { using ProbeBase::ProbeBase;\n' +
                               extract(source, 'virtual bool set_world(') + '\n};')
            (temp / 'constraint_native.inc').write_text('\n'.join(setters))
            libraries = [args.bullet_build / 'src' / name / ('lib' + name + '.a')
                         for name in ('BulletDynamics', 'BulletCollision', 'LinearMath')]
            native_binary = temp / 'native'
            native_command = command + ['-iquote', str(temp), '-isystem', str(args.bullet_root / 'src'),
                str(root / 'tests/cpp/bullet_constraint_native_tests.cpp'), *map(str, libraries), '-o', str(native_binary)]
            subprocess.run(native_command, check=True)
            subprocess.run([str(native_binary)], check=True, env=env)
            if args.mutations:
                helper = (folder / 'Constraint/DConstraintFrame.h').read_text()
                for label, guard in (
                    ('partial nail pivot commit', 'if (!isFiniteVec3(candidateA) || !isFiniteVec3(candidateB)) return false;'),
                    ('partial angular frame commit', 'if (!finiteConstraintTransform(candidateA) || !finiteConstraintTransform(candidateB)) return false;')):
                    assert guard in helper
                    (temp / 'DConstraintFrame.h').write_text(helper.replace(guard, '', 1))
                    subprocess.run(native_command, check=True)
                    result = subprocess.run([str(native_binary)], env=env, capture_output=True)
                    assert result.returncode != 0, 'Undetected mutation: ' + label
                    print('Detected native constraint mutation:', label, flush=True)
        else:
            print('Native frame tests require --bullet-root/--bullet-build')


if __name__ == '__main__':
    main()
