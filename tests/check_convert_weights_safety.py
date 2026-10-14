#!/usr/bin/env python3
"""Complete weights-node bodies with injectable Maya array/plug boundaries."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile
import re

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/point/DPoint/Deform/DPConvertWeights.cpp'


def bodies(root, initialize):
    source = (root / SOURCE).read_text()
    declared = declarations('DPConvertWeights', source, initialize)
    if not initialize:
        declared[0] = declared[0].replace('MStatus compute(',
            'MObject thisMObject()const{return {"node"};} MStatus compute(')
    signature = 'initialize(' if initialize else 'compute('
    result = declared + [extract(source, 'MStatus DPConvertWeights::' + signature)]
    if not initialize:
        result.append('DOMMY_COMPUTE_CATCH("DPConvertWeights")')
    return '\n'.join(result)


def sdk_check(root, sdk):
    # These complete methods use only Maya types, apart from the separately
    # tested input reader. Check them even without the archived dependency stack.
    source = (root / SOURCE).read_text()
    includes = '\n'.join('#include <maya/' + name + '.h>' for name in (
        'MArrayDataBuilder', 'MArrayDataHandle', 'MDataBlock', 'MDataHandle',
        'MFloatArray', 'MFnCompoundAttribute', 'MFnGenericAttribute',
        'MFnNumericAttribute', 'MPlug'))
    cpp = includes + '\n#include "' + SOURCE[:-4] + '.h"\n'
    cpp += '#include "common/utility/DComputeGuard.h"\n'
    cpp += '#include "common/utility/DStatusOperation.h"\n'
    cpp += '#include "common/utility/DGenerationLimits.h"\n#include <cmath>\n'
    common=(root/'src/modules/point/DPoint/Common/DPointCommon.h').read_text()
    reader=re.search(r'MStatus getFloatArrayFromData\(MDataBlock& block, const MObject& attribute, MFloatArray& output\);',common)
    assert reader,'Missing production block-array declaration'
    cpp += reader.group(0)+'\n'
    cpp += extract(source, 'MStatus DPConvertWeights::compute(')
    cpp += '\nDOMMY_COMPUTE_CATCH("DPConvertWeights")\n'
    cpp += extract(source, 'MStatus DPConvertWeights::initialize(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root),
        '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-weights-sdk-') as directory:
        path = Path(directory) / 'weights.cpp'
        path.write_text(cpp)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('DPConvertWeights: public header and complete methods passed SDK syntax/analysis')
    print('The input reader is declaration-only here; no native linking or DG execution')


def mutations(root):
    initial, compute = bodies(root, True), bodies(root, False)
    cases = [
        ('initializer create status', initial, 'DOMMY_RETURN_IF_FAILURE(status);', '', True),
        ('initializer setter status', initial, 'DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));',
         '(void)generic.setStorable(false);', True),
        ('initializer exception', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', True),
        ('child requests rejected', compute, 'if (plug != aWeightsList && plug != aWeights)',
         'if (plug != aWeightsList)', False),
        ('unsupported row accepted', compute, 'if (rowIndex != 0U) return MS::kUnknownParameter;',
         '(void)rowIndex;', False),
        ('wrong parent accepted', compute, 'if (rowPlug != aWeightsList) return MS::kUnknownParameter;',
         '', False),
        ('unindexed parent accepted', compute, '} else if (plug == aWeights) {',
         '} else if (false) {', False),
        ('input status ignored', compute,
         'DOMMY_RETURN_IF_FAILURE(getFloatArrayFromData(block, inputDataArray, values));',
         '(void)getFloatArrayFromData(block, inputDataArray, values);', False),
        ('retained outer rows', compute,
         'MArrayDataBuilder rows(&block, aWeightsList, 1U, &status);',
         'MArrayDataBuilder rows = output.builder(&status);', False),
        ('weight value omitted', compute, 'element.set(values[i]);', '', False),
        ('builder status ignored', compute,
         'MArrayDataBuilder elements(&block, aWeights, values.length(), &status);\n'
         '    DOMMY_RETURN_IF_FAILURE(status);',
         'MArrayDataBuilder elements(&block, aWeights, values.length(), &status);', False),
        ('inner assignment ignored', compute, 'DOMMY_RETURN_IF_FAILURE(weights.set(elements));',
         '(void)weights.set(elements);', False),
        ('publication failure ignored', compute, 'DOMMY_RETURN_IF_FAILURE(output.set(rows));',
         '(void)output.set(rows);', False),
        ('clean before publication', compute, 'DOMMY_RETURN_IF_FAILURE(output.set(rows));',
         'DOMMY_RETURN_IF_FAILURE(output.setAllClean());\n'
         '    DOMMY_RETURN_IF_FAILURE(output.set(rows));', False),
        ('clean failure ignored', compute, 'DOMMY_RETURN_IF_FAILURE(committedWeights.setAllClean());',
         '(void)committedWeights.setAllClean();', False),
        ('non-finite input', compute, 'if (!std::isfinite(values[i])) return MS::kInvalidParameter;',
         '(void)values[i];', False),
        ('count limit ignored', compute, 'dommy::generation::kMaxGeneratedPointCount)',
         'std::numeric_limits<unsigned int>::max())', False),
    ]
    for label, source, old, new, initialize in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), capture=True,
                suite='convert_weights_init' if initialize else 'convert_weights_compute',
                arguments=None if initialize else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--emit' in sys.argv:
        run(root, bodies(root, True), suite='convert_weights_init', arguments=['--emit'])
    else:
        run(root, bodies(root, True), suite='convert_weights_init')
        run(root, bodies(root, False), suite='convert_weights_compute', arguments=[])
