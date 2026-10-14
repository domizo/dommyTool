#!/usr/bin/env python3
"""Run complete component-list/sequencer methods with host API doubles.

Checks status propagation, selection and declaration contracts, not Maya's
geometry kernel, data ownership, DG evaluation or registration rollback.
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


SOURCES = {
    'DComponentListGen': 'src/core/UtilityNode/DComponentListGen/DComponentListGen.cpp',
    'DShapeSequencer': 'src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp',
}


def generated_methods(root, sources=None):
    if sources is None:
        sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    expected = {
        'DComponentListGen': ['aComponentType', 'aMode', 'aSize', 'aPattern',
                              'inputDataArray', 'outComponentList'],
        'DShapeSequencer': ['inputObjects', 'seaquence', 'outputObject'],
    }
    bodies = []
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attribute + ';' for attribute in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(bodies)


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-component-sequencer-') as directory:
        temporary = Path(directory)
        (temporary / 'component_sequencer_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'component_sequencer_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unknown-pragmas',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/component_sequencer_safety_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/component_sequencer_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()
