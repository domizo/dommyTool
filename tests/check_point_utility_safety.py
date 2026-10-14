#!/usr/bin/env python3
"""Particle attribute transactions, fill reads, bounded merge and initialization."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODES = {
    'DPFillSpace': 'src/core/DDataUtility/Utility/DPFillSpace.cpp',
    'DPMerge': 'src/core/DDataUtility/Utility/DPMerge.cpp',
    'DPParticleAttribute': 'src/core/DDataUtility/Particle/utility/DPParticleAttribute.cpp',
}


def declarations(name, source, initialize):
    attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
    result = ['struct ' + name + (':AttributeNode' if initialize else '') +
              ' {' + ('static MStatus initialize();' if initialize else
                       'MStatus compute(const MPlug&, MDataBlock&);') +
              ''.join('static MObject ' + a + ';' for a in attrs) + '};']
    result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
    return result


def initializers(root):
    header = (root / 'src/core/DDataUtility/Utility/DPFillSpace.h').read_text()
    result = [re.search(r'enum\s*\{[^}]+\};typedef int DP_FILL_SAMPLE_TYPE;', header).group(0)]
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    common = (root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text()
    result = ['#include "common/utility/DPointMergeBudget.h"',
              extract(common, 'MStatus getValidInputDP('),
              extract(common, 'const DPData* getInputDP(')]
    for name, path in NODES.items():
        source = (root / path).read_text()
        if name != 'DPMerge':
            result.append(extract(source, 'namespace {'))
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    import check_group_transfer_safety as shared
    shared.sdk_check(root, sdk, NODES, 'Point utilities')


def fill_budget(root, source=None, capture=False):
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text()
    reduced = header.replace('maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    assert reduced != header
    run(root, source or compute_bodies(root), suite='point_utility_compute', arguments=['fill-budget'],
        capture=capture, header_overrides={path: reduced},
        extra_flags=['-I' + str(root / 'src/core/common/utility')])


def fill_mutations(root):
    source = compute_bodies(root)
    for label, old, new in (
        ('Fill copies points before planning',
         'const std::size_t pointCount = inputData->numberOfPoints();',
         '(void)inputData->getMPosition();\n    const std::size_t pointCount = inputData->numberOfPoints();'),
        ('Fill known output admission omitted',
         'plannedWorkspace.add(plan.minimumOutputPointCount - pointCount,\n                         sizeof(MPoint) + sizeof(DPoint));', ''),
        ('Fill incremental output admission omitted',
         'workspace.add(sampleCount, sizeof(MPoint) + sizeof(DPoint));', '(void)workspace;'),
        ('Fill input status discarded',
         'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputData));',
         'if (!getValidInputDP(inDP, data, inputData)) return MS::kFailure;'),
    ):
        assert old in source, label
        try:
            changed = source.replace(old, new, 1)
            run(root, changed, suite='point_utility_compute', arguments=[], capture=True)
            fill_budget(root, changed, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_utility_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('same-name source removed before copy', 'point_utility_compute', compute.replace(
            'if (!particleSourceValues(output.getAttr(sourceName), count, values))',
            'output.removeAttr(name);\n        if (!particleSourceValues(output.getAttr(sourceName), count, values))')),
        ('invalid mode accepted', 'point_utility_compute', compute.replace(
            'if (mode != 0 && mode != 1) return MS::kInvalidParameter;', '')),
        ('failed fill handle read', 'point_utility_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const int samplingTypeValue = samplingTypeHandle.asInt();',
            'const int samplingTypeValue = samplingTypeHandle.asInt();\n    DOMMY_RETURN_IF_FAILURE(status);')),
        ('merge preflight omitted', 'point_utility_compute', compute.replace(
            'if (!dommy::point::buildPointMergeBudget(inputData, workItems))', 'if (false)')),
        ('failed attribute publication ignored', 'point_utility_compute', compute.replace(
            '? MStatus(MS::kSuccess) : MStatus(MS::kFailure);',
            '? MStatus(MS::kSuccess) : MStatus(MS::kSuccess);')),
        ('array count failure ignored', 'point_utility_compute', compute.replace(
            'const unsigned int inputCount = inputs.elementCount(&status);\n    DOMMY_RETURN_IF_FAILURE(status);',
            'const unsigned int inputCount = inputs.elementCount(&status);')),
        ('initializer failure ignored', 'point_utility_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', 'point_utility_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, suite, source in cases:
        assert source != (init if suite.endswith('_init') else compute), label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'common/utility/DPointMergeBudget.h'
    header = (root / 'src/core' / path).read_text()
    for label, old, new in (
        ('vector components undercounted',
         'case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 3U;',
         'case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 1U;'),
        ('missing dynamic defaults unbounded',
         '!checkedProduct(missing, defaults.stringBytes, kMaxGeneratedStringByteCount, strings)', 'false'),
        ('group work omitted', '!checkedSum(plan.workItems, plan.points, kMaxWorkItemCount, plan.workItems)', 'false'),
    ):
        assert old in header, label
        try:
            run(root, compute, capture=True, suite='point_utility_compute', arguments=[],
                header_overrides={path: header.replace(old, new, 1)})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_utility_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
        fill_budget(root)
        fill_mutations(root)
    elif len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run(root, initializers(root), suite='point_utility_init', arguments=['--emit'])
    else:
        run(root, initializers(root), suite='point_utility_init')
        run(root, compute_bodies(root), suite='point_utility_compute', arguments=[])
        fill_budget(root)
        fill_mutations(root)
