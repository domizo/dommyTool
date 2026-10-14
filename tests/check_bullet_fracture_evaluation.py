#!/usr/bin/env python3
"""Execute the experimental Maya fracture callback without requiring Maya."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--input-mutations', action='store_true',
                        help='Verify every compute input getter checks acquisition first')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    utility = root / 'src/core/common/utility'
    source = (root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp').read_text()
    header = (root / 'src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.h').read_text()
    state_members = header[header.index('MTime m_prevTime;'):header.index('std::unique_ptr<btCollisionShape>')]
    compute = extract(source, 'MStatus DBulletFractureSolver::compute(')
    compute += '\ncatch (...) { return MS::kFailure; }\n'
    initializer = extract(source, 'MStatus DBulletFractureSolver::initialize(')
    dependencies = 'MStatus fractureDependencies() try {\n' + initializer[initializer.index('const MObject simulationInputs[]'):]
    adapters = '\n'.join(extract((utility / name).read_text(), 'namespace dommy {') for name in (
        'DMayaPluginData.h', 'DMayaPluginDataOutput.h', 'DMayaObjectArrayOutput.h'))
    validation = (utility / 'DMayaValueValidation.h').read_text()
    adapters += '\nnamespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(') for name in (
            'isFiniteVector', 'isFloatRepresentableVector')) + '\n}}\n'
    adapters += extract((root / 'src/modules/dynamics/DBullet/Solver/DBulletMayaState.h').read_text(), 'namespace dommy {')
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
    with tempfile.TemporaryDirectory(prefix='dommy-fracture-evaluation-') as directory:
        temp = Path(directory)
        compute_path = temp / 'fracture_compute.inc'
        adapters_path = temp / 'fracture_adapters.inc'
        dependencies_path = temp / 'fracture_dependencies.inc'
        compute_path.write_text(compute)
        adapters_path.write_text(adapters)
        dependencies_path.write_text(dependencies)
        (temp / 'fracture_state_members.inc').write_text(state_members)
        binary = temp / 'evaluation'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/bullet_fracture_evaluation_tests.cpp'), '-o', str(binary)]

        def run(capture=False):
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True, env=environment, capture_output=capture)

        run()
        cases = []
        if args.mutations or args.input_mutations:
            pattern = re.compile(
                r'(?P<acquire>^[ \t]*const MDataHandle (?P<handle>\w+) = data\.inputValue\([^;\n]+;\n)'
                r'(?P<check>^[ \t]*DOMMY_RETURN_IF_FAILURE\(status\);\n)'
                r'(?P<getter>^[ \t]*[^\n]*\b(?P=handle)\.as\w+\(\);\n)', re.M)
            matches = list(pattern.finditer(compute))
            assert len(matches) == 6
            for match in matches:
                candidate = compute[:match.start()] + match['acquire'] + match['getter'] + \
                    match['check'] + compute[match.end():]
                cases.append(('getter before acquisition check ' + match['handle'], compute_path, compute, candidate))
        if args.mutations:
            cases += [
                ('connected-array count replaced by all slots', compute_path, compute,
                 compute.replace('inputs.numConnectedElements(&status)', 'inputs.numElements(&status)', 1)),
                ('world guard never armed', compute_path, compute, compute.replace('mutationGuard.arm();', '')),
                ('time committed before output', compute_path, compute, compute.replace(
                    '    DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput',
                    '    m_prevTime = candidateTime;\n    DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput', 1)),
                ('restart state ignored', compute_path, compute, compute.replace('!m_requiresStartFrame && bool(m_dynamicsWorld)', 'bool(m_dynamicsWorld)', 1)),
                ('changed world settings ignored', compute_path, compute, compute.replace('candidateSettings != m_worldSettings', 'false', 1)),
                ('gravity validation removed', compute_path, compute, compute.replace('!dommy::maya::isFloatRepresentableVector(gravity)', 'false', 1)),
                ('tiny variable step accepted', compute_path, compute, compute.replace('btFuzzyZero(seconds)', 'false', 1)),
                ('duplicate glue dispatch', compute_path, compute, compute.replace(
                    'm_dynamicsWorld->stepSimulation(seconds, 0);',
                    'm_dynamicsWorld->stepSimulation(seconds, 0);\n            m_dynamicsWorld->glueCallback();', 1)),
                ('disabled evaluation commits time', compute_path, compute, compute.replace(
                    '    mutationGuard.commit();', '    if (!enabled) m_prevTime = MTime(99);\n    mutationGuard.commit();', 1)),
                ('mesh copy failure swallowed', compute_path, compute, compute.replace(
                    'DOMMY_RETURN_IF_FAILURE(output.copyFrom(*inputData));', '(void)output.copyFrom(*inputData);', 1)),
                ('output element failure swallowed', adapters_path, adapters, adapters.replace(
                    'status = element.set(objects[index]);\n        if (!status) return status;',
                    '(void)element.set(objects[index]);', 1)),
                ('plugin type query failure swallowed', adapters_path, adapters, adapters.replace(
                    'const MTypeId actualTypeId = pluginData.typeId(&status);\n    if (!status) return status;',
                    'const MTypeId actualTypeId = pluginData.typeId(&status);', 1)),
                ('dependency failure swallowed', dependencies_path, dependencies, dependencies.replace(
                    'DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outDM));', '(void)attributeAffects(input, outDM);', 1)),
            ]
        for label, path, baseline, candidate in cases:
            assert candidate != baseline, label
            path.write_text(candidate)
            try:
                run(capture=True)
            except subprocess.CalledProcessError as error:
                assert error.cmd[0] == str(binary), 'Compile failure is not mutation detection'
                print('Detected fracture adapter mutation:', label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)
            finally:
                path.write_text(baseline)


if __name__ == '__main__':
    main()
