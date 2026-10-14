#!/usr/bin/env python3
"""Check the complete motion/rotation compute and initializer bodies with doubles.

Typed fields use production classes, validation and the extracted legacy writer.
The doubles test control flow, array boundaries and API argument/result traces;
they do not certify Maya math, native MPxData ownership, time contexts or TBB scheduling.
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
from check_path_reservation_exception_safety import disable_core_dumps

SOURCES = {name: 'src/modules/point/DPoint/animation/' + name + '.cpp'
           for name in ('DPMotionCreate', 'DPRotation')}


def payload_methods(root):
    folder = root / 'src/core/common/utility'
    budget = (folder / 'DPointPayloadBudget.h').read_text()
    return '\n'.join([
        '#ifndef DOMMY_TYPED_ROTATION_ATTRIBUTES\nnamespace dommy {namespace attribute {',
        extract((folder / 'DAttributeArrayConversion.h').read_text(),
                'inline bool validArrayConversionBudget('),
        '}}\n#endif\nnamespace dommy {namespace point {', extract(budget, 'class PointPayloadBudget') + ';',
        '''template<class Owner> void preflightMetadataPayload(const Owner& input,
             std::size_t, PointPayloadBudget& budget) {
            budget.add(input.metadataBytes, 1U);
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
            for(const auto& field:input.fields) {
                const auto width=field.second->getAttrType()==DP_ATTR_MATRIX ? sizeof(MMatrix) : sizeof(Imath::V3d);
                budget.add(field.second->attrSize()+1U,width);
                budget.add(field.first.size(),sizeof(char));
            }
#else
            for(const auto& field:input.vectors) {
                budget.add(field.second.size()+1U,sizeof(Imath::V3d));
                budget.add(field.first.size(),sizeof(char));
            }
            for(const auto& field:input.matrices) {
                budget.add(field.second.size()+1U,sizeof(MMatrix));
                budget.add(field.first.size(),sizeof(char));
            }
            for(const auto& field:input.scalars) {
                budget.add(field.second.size()+1U,sizeof(double));
                budget.add(field.first.size(),sizeof(char));
            }
#endif
        }''', extract(budget, 'template <typename Owner>\nvoid preflightPointPayload('),
        extract(budget, 'inline void preflightGeneratedPointAttribute('), '}}'])


def generated_methods(root, overrides=None):
    overrides = overrides or {}
    bodies = [extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                      'void AddAttributeList('),
              payload_methods(root), 'namespace dommy { namespace maya {\n' + extract(
        (root / 'src/core/common/utility/DTimeSampling.h').read_text(),
        'inline bool computeSampleTime(') + '\n} }']
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    bodies.append('namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(')
        for name in ('isFiniteVector', 'isFloatRepresentableVector')) + '\n} }')
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    constant = re.search(r'const double kDegreesToRadians = [^;]+;', math).group()
    bodies.append('namespace dommy { namespace math {\n' + constant + '\n' +
                  extract(math, 'double degreesToRadians(') + '\n} }')
    bodies.append(extract((root / 'src/core/common/object/DNodeOperation.cpp').read_text(),
                          'MStatus getAlignSpaceMatrix('))
    for name, path in SOURCES.items():
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        bodies.append('struct ' + name + ': AnimationNode {\n'
                      'static MStatus initialize();\nMStatus compute(const MPlug&, MDataBlock&);\n' +
                      ''.join('static MObject ' + attr + ';\n' for attr in attrs) + '};')
        bodies.extend('MObject ' + name + '::' + attr + ';' for attr in attrs)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(bodies)


def run(root, methods, arguments, capture=False, budget=False):
    with tempfile.TemporaryDirectory(prefix='dommy-motion-rotation-') as directory:
        temp = Path(directory)
        (temp / 'motion_rotation_methods.inc').write_text(methods)
        (temp / 'maya').mkdir()
        (temp / 'maya/MMatrix.h').write_text('// Matrix transport seam is defined by the fixture.\n')
        if budget:
            path = temp / 'common/utility/DAttributeArrayConversion.h'
            path.parent.mkdir(parents=True)
            path.write_text((root / 'src/core/common/utility/DAttributeArrayConversion.h').read_text().replace(
                'maximum = 256U * 1024U * 1024U', 'maximum = 16384U'))
        binary = temp / 'motion_rotation_tests'
        subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-DDOMMY_TYPED_ROTATION_ATTRIBUTES',
            '-Wno-unknown-pragmas', '-Wno-unused-function',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            '-I' + str(root / 'src/core/common/utility'),
            str(root / 'tests/cpp/motion_rotation_safety_tests.cpp'),
            '-o', str(binary)], check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        return subprocess.run([str(binary)] + arguments, check=True, env=environment,
                              capture_output=capture, text=True, cwd=str(temp), **options)


def main():
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], tuple(SOURCES.values()), 'Animation')
        return
    def execute(methods, capture=False):
        run(root, methods, [str(root / 'tests/fixtures/motion_rotation_initialization_contract.txt'),
                            str(root / 'tests/fixtures/motion_rotation_result_contract.txt')], capture)
        run(root, methods, ['--budget'], capture, budget=True)
    source = generated_methods(root)
    execute(source)
    cases = [('rotation output collision admitted', source.replace(
        'outputType == 1 && !fwAttrName.empty() && fwAttrName == upAttrName', 'false', 1))]
    for name in ('fwAttrName', 'upAttrName', 'matAttrName'):
        pattern = (r'if \(!dommy::attribute::commitStagedAttribute\(\*outputDPData, ' + name +
                   r',\s*candidate,\s*ptCount\)\)\s*return MS::kFailure;')
        match = re.search(pattern, source)
        assert match, name
        changed = source[:match.start()] + ('(void)dommy::attribute::commitStagedAttribute('
            '*outputDPData, ' + name + ', candidate, ptCount);') + source[match.end():]
        cases.append((name + ' write failure ignored', changed))
    for label, changed in cases:
        assert changed != source
        try:
            run(root, changed, [str(root / 'tests/fixtures/motion_rotation_initialization_contract.txt'),
                               str(root / 'tests/fixtures/motion_rotation_result_contract.txt')], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'motion_rotation_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected rotation-output mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    if '--mutations' in sys.argv:
        for label, old, new in (
            ('motion aggregate admission omitted', 'dommy::point::preflightPointPayload(\n',
             'if(false) dommy::point::preflightPointPayload(\n'),
            ('rotation aggregate admission omitted', 'dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);', ''),
            ('motion connection error ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n    if (matrixConnected)', 'if (matrixConnected)'),
            ('rotation connection error ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n        if (matrixConnected)', 'if (matrixConnected)'),
            ('child connection error ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n                    if (childConnected)', 'if (childConnected)'),
            ('parent connection error ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n                if (rotationConnected', 'if (rotationConnected'),
            ('motion coordinate placement corrupted', 'point = stagedPoints[index];',
             'point = stagedPoints[index - index];'),
            ('motion redundant bounds update', '}));\n    MDataHandle output = data.outputValue(outDP, &stat);',
             '}));\n    outputDPData->updateBBox();\n    MDataHandle output = data.outputValue(outDP, &stat);'),
        ):
            assert old in source, label
            try:
                execute(source.replace(old, new, 1), True)
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'motion_rotation_tests', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    main()
