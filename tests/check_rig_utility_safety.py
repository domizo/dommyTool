#!/usr/bin/env python3
"""Test the Rig input boundary/deformers and three complete initializers.

The generated input probe stops before geometry construction. These doubles
do not implement Maya registration, DG evaluation, transforms or curve creation.
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
    'DRigController': 'src/modules/curve/Curve/Node/DRigController.cpp',
    'DMatToVec': 'src/core/UtilityNode/DMatToVec/DMatToVec.cpp',
    'DNumericToFloatArray': 'src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp',
}


def generated_types(root):
    shape = (root / 'src/modules/curve/Curve/Node/DRigController.h').read_text()
    axes = (root / 'src/modules/point/DPoint/DType.h').read_text()
    return '\n'.join(re.search(r'enum\s*\{[^}]*\};\s*typedef\s+\w+\s+' + name + ';', source).group(0)
                     for source, name in ((shape, 'D_CONTROLLER_SHAPE_TYPE'), (axes, 'D_AXIS_TYPE')))


def generated_methods(root, sources=None):
    if sources is None:
        sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    expected = {
        'DRigController': ['shapeType', 'localScale', 'division', 'offset', 'offsetX', 'offsetY',
                          'offsetZ', 'localRotate', 'localRotateX', 'localRotateY', 'localRotateZ',
                          'keepCenter', 'ratio', 'dirAxis', 'taperAxis', 'taperAmount', 'bendAmount',
                          'bendAxis', 'arrowReverse', 'arrowRatio', 'arrowWidth', 'outputCurve'],
        'DMatToVec': ['inMatrix', 'aType', 'aSpace', 'outVector'],
        'DNumericToFloatArray': ['inputData', 'outDataArray'],
    }
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    bodies = ['namespace dommy { namespace maya {\n' +
              extract(validation, 'inline bool isFinitePoint(') + '\n} }']
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attribute + ';' for attribute in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
    rig = sources['DRigController']
    for method in ('taperDeform', 'bendDeform', 'crossBendDeform'):
        bodies.append(extract(rig, 'bool DRigController::' + method + '('))
    prefix = extract(rig, 'MStatus DRigController::compute(')
    prefix = prefix[:prefix.index('dommy::maya::CheckedArray<MPointArray> cvs;')]
    prefix = prefix.replace('DRigController::compute(', 'DRigController::inputBoundary(', 1)
    # Observe the converted values without replacing any production read or validation.
    prefix += '''
        observed = {static_cast<double>(shapeTypeVal), localScaleVal, localSizeVal,
                    static_cast<double>(divisionVal), offsetVal[0], offsetVal[1], offsetVal[2],
                    localRotateVal[0], localRotateVal[1], localRotateVal[2],
                    static_cast<double>(keepCenterVal), ratioVal, static_cast<double>(dirAxisVal),
                    static_cast<double>(taperAxisVal), taperAmountVal, bendAmountVal,
                    static_cast<double>(bendAxisVal), static_cast<double>(arrowReverseVal),
                    arrowRatioVal, arrowWidthVal};
        observedBudget = generationPlan.maximumPointCount;
        return MS::kSuccess;
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DRigController input boundary")
'''
    bodies.append(prefix)
    return '\n\n'.join(bodies)


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-rig-utility-') as directory:
        temporary = Path(directory)
        (temporary / 'rig_utility_types.inc').write_text(generated_types(root))
        (temporary / 'rig_utility_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'rig_utility_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/rig_utility_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/rig_utility_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()
