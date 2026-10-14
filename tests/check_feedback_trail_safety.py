#!/usr/bin/env python3
"""Check feedback control flow and Motion Trail input/initialization boundaries.

The feedback helper is executed in full with host data factories/handles
substituted. Motion Trail executes its input prefix and full initializer;
Maya geometry creation, historical DG evaluation and publication remain host
checks. Production algorithm/budget helpers have separate core safety tests.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def generated_methods(root, trail, feedback):
    numeric = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    header = (root / 'src/modules/curve/Curve/Node/DMotionTrailCurveNode.h').read_text()
    attributes = re.findall(r'^MObject\s+DMotionTrailCurveNode::(\w+);', trail, re.M)
    expected = ['inputObject', 'transformMatrix', 'trailType', 'seed', 'randomCount',
                'trailLength', 'timeSampling', 'trailLengthRandom', 'time', 'outputObjects']
    if attributes != expected:
        raise ValueError('Motion Trail attribute declarations changed')
    enum = extract(header, 'enum {') + ';\n'
    # The wrapper publishes captured controls only after the production prefix
    # reaches the geometry-read boundary. No replacement input logic is used.
    compute = extract(trail, 'MStatus DMotionTrailCurveNode::compute(')
    start = compute.index('    MStatus status;')
    end = compute.index('    MDataHandle inputObjectHandle', start)
    inputs = ('MStatus DMotionTrailCurveNode::readInputs(MDataBlock& block, TrailInputs& result) {\n'
              + compute[start:end] + '\n'
              'result = {evaluationTime, seedValue, randomCountValue, trailTypeValue, '
              'trailLengthValue, trailLengthRandomValue, timeSamplingValue};\n'
              'return MS::kSuccess;\n}')
    shared = '\n'.join((extract(numeric, 'template <typename DataBlock, typename Attribute>'),
                        extract(feedback, 'template <typename DataType, typename InputReader, typename Copier>')))
    return ('namespace dommy { namespace maya {\n' + shared + '\n} }\n' + enum +
            '\n'.join('MObject DMotionTrailCurveNode::' + name + ';' for name in attributes) +
            '\n' + extract(trail, 'MStatus DMotionTrailCurveNode::initialize(') + '\n' + inputs)


def main():
    root = Path(__file__).resolve().parent.parent
    trail = (root / 'src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp').read_text()
    feedback = (root / 'src/core/common/utility/DFeedbackSolver.h').read_text()
    methods = generated_methods(root, trail, feedback)
    with tempfile.TemporaryDirectory(prefix='dommy-feedback-trail-') as directory:
        temporary = Path(directory)
        (temporary / 'feedback_trail_methods.inc').write_text(methods)
        binary = temporary / 'feedback_trail_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), '-I' + str(root / 'src'),
            str(root / 'tests/cpp/feedback_trail_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/trail_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()
