#!/usr/bin/env python3
"""Complete DPNoise compute and retained-metadata/workspace admission.

Maya publication and noise sampling are explicit boundaries. The production
noise algorithms and owning DPData transaction have separate regression suites.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run


def bodies(root):
    source = (root / 'src/modules/point/DPoint/Utility/DPNoise.cpp').read_text()
    for path, name in (('src/modules/point/DPoint/Utility/DPPyroNoise.cpp', 'DPPyroNoise'),
                       ('src/modules/curve/DCurve/Utility/DCCopy.cpp', 'DCCopy')):
        compute = extract((root / path).read_text(), 'MStatus ' + name + '::compute(')
        assert 'getPositionConst()' not in compute and 'getPositionPtrConst()' in compute, path
    header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    result = []
    for name in ('NoiseBaseType', 'NoiseDeformType'):
        match = re.search(r'enum\s*\{[^}]*\};typedef short ' + name + ';', header)
        assert match, name
        result.append(match.group())
    result.append(extract(header, 'struct NoiseBaseParams') + ';')
    result.append(extract((root / 'src/core/common/object/DNoiseOp.cpp').read_text(),
                          'MStatus validateNoiseBaseParams('))
    declared = declarations('DPNoise', source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MStatus evalNoiseBaseParms(MDataBlock&, NoiseBaseParams&); MStatus compute(')
    result.extend(declared)
    result.append(extract(source, 'MStatus DPNoise::evalNoiseBaseParms('))
    result.append('''MStatus createNoiseFromPosition(const NoiseBaseParams&,
        const DPointList&, DScalarDataList& values) {
        const MStatus status = operation("noise.sample");
        if (!status) return status;
        for (std::size_t index = 0; index < values.size(); ++index)
            values[index] = static_cast<float>(index + 1U) * 0.25f;
        return MS::kSuccess;
    }''')
    result.append(extract(source, 'MStatus DPNoise::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DPNoise")')
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    flags = ['-isystem', str(root / 'third_party/libnoise/source'),
             '-I' + str(root / 'src/core/common/utility')]
    run(root, source, suite='point_coordinate_output', arguments=[],
        extra_flags=flags, capture=capture)
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    run(root, source, suite='point_coordinate_output', arguments=['budget'],
        extra_flags=flags, header_overrides={path: header}, capture=capture)


def mutations(root):
    source = bodies(root)
    for label, old, new in (
        ('workspace admission omitted', 'dommy::point::preflightPointPayload(',
         'if (false) dommy::point::preflightPointPayload('),
        ('new field admission omitted', 'workspace.add(count, sizeof(DScalarData));', ''),
        ('coordinate placement omitted', 'point = points[index];', '(void)point; (void)index;'),
        ('duplicate bounds calculation', 'return status ? data.setClean(plug) : status;',
         'if (status) outputPoints.updateBBox(); return status ? data.setClean(plug) : status;'),
        ('batch failure ignored', 'DOMMY_RETURN_IF_FAILURE(output.copyRepeatedFrom(',
         '(void)output.copyRepeatedFrom('),
    ):
        assert old in source, label
        changed = source.replace(old, new, 1)
        if label == 'batch failure ignored':
            changed = changed.replace('}));', '});', 1)
        try:
            execute(root, changed, True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_coordinate_output_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        from check_point_batch_transactions import sdk_check
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], (
            'src/modules/point/DPoint/Utility/DPNoise.cpp',
            'src/modules/point/DPoint/Utility/DPPyroNoise.cpp',
            'src/modules/curve/DCurve/Utility/DCCopy.cpp',
        ), 'Point coordinate output')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
