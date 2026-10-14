#!/usr/bin/env python3
"""Complete delete/sort/smooth bodies, frozen schemas and failure injection."""
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_point_utility_safety import declarations

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPDelete', 'DPSort', 'DPSmooth')}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    groups = (root / 'src/core/Data/DGroupCommon.cpp').read_text()
    result = [extract((root / 'src/core/common/object/DNodeOperation.cpp').read_text(),
                      'void mergeUnionIdListOp('),
              extract(groups, 'bool getPatternStrIndex('),
              extract(groups, 'void checkDPGroupActiveIdsFromString(')]
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, False)
        if name == 'DPSmooth':
            declared[0] = declared[0].replace(' {', ' {MObject thisMObject() const {return MObject{"node"};}', 1)
        result.extend(declared)
        if 'namespace {' in source:
            result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def run_edit(root, source, **kwargs):
    return run(root, source, extra_flags=[
        '-I' + str(root / 'src/core/common/utility'),
        '-I' + str(root / 'src/core/Utility'),
        '-I' + str(root / 'src/core/Utility/3d/Math')], **kwargs)


def check_compute(root, source, capture=False):
    run_edit(root, source, suite='point_edit_compute', arguments=[], capture=capture)
    path = 'common/utility/DAttributeArrayConversion.h'
    header = (root / 'src/core' / path).read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    sort_path = 'common/utility/DAttributeSort.h'
    sort_header = (root / 'src/core' / sort_path).read_text().replace(
        '#include "DAttributeArrayConversion.h"',
        '#include "common/utility/DAttributeArrayConversion.h"')
    run_edit(root, source, suite='point_edit_compute', arguments=['budget'],
             header_overrides={path: header, sort_path: sort_header}, capture=capture)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('number-only selection deletes all', 'point_edit_compute', compute.replace(
            'if (hasGroups || !hasPattern)', 'if (true)', 1)),
        ('empty-selector compatibility lost', 'point_edit_compute', compute.replace(
            'if (hasGroups || !hasPattern)', 'if (hasGroups)', 1)),
        ('failed source handle accessed', 'point_edit_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const DPData* input = nullptr;',
            'const DPData* input = nullptr;', 1)),
        ('failed delete input accessed', 'point_edit_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n        groupNames = groupsHandle.asString();',
            'groupNames = groupsHandle.asString();\n        DOMMY_RETURN_IF_FAILURE(status);', 1)),
        ('failed sort input accessed', 'point_edit_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n    const short sortType = sortHandle.asShort();',
            'const short sortType = sortHandle.asShort();\n    DOMMY_RETURN_IF_FAILURE(status);', 1)),
        ('failed smoothing input accessed', 'point_edit_compute', compute.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n        radius = radiusHandle.asFloat();',
            'radius = radiusHandle.asFloat();\n        DOMMY_RETURN_IF_FAILURE(status);', 1)),
        ('double target narrowed to float', 'point_edit_compute', compute.replace(
            'difference(points[i], target, delta)',
            'difference(points[i], DPoint(target[0], target[1], target[2]), delta)', 1)),
        ('float distance overflow restored', 'point_edit_compute', compute.replace(
            'keys[i] = delta.length;',
            'keys[i] = (DPoint(target[0], target[1], target[2]) - points[i]).length();', 1)),
        ('final reverse ignored', 'point_edit_compute', compute.replace(
            'if (reverseHandle.asBool()) std::reverse(indices.begin(), indices.end());',
            '(void)reverseHandle.asBool();', 1)),
        ('smoothing update omitted', 'point_edit_compute', compute.replace(
            'if (count > 0U) newPtArray[i] += velocity / static_cast<DReal>(count);',
            '(void)count;', 1)),
        ('smoothing iteration limit omitted', 'point_edit_compute', compute.replace(
            '!dommy::generation::checkedProduct(ptCount, iterationCount,\n                dommy::generation::kMaxWorkItemCount, workCount)',
            'false', 1)),
        ('smoothing aggregate admission omitted', 'point_edit_compute', compute.replace(
            'dommy::point::preflightPointPayload(*input,',
            'if (false) dommy::point::preflightPointPayload(*input,', 1)),
        ('smoothing grid cells omitted', 'point_edit_compute', compute.replace(
            'workspace.add(cells, 2U * sizeof(uint));', '(void)cells;', 1)),
        ('smoothing coordinate placement omitted', 'point_edit_compute', compute.replace(
            'point = newPtArray[index];', '(void)point; (void)index; (void)newPtArray;', 1)),
        ('sort remap failure ignored', 'point_edit_compute', compute.replace(
            'if (reorder && !output.sortPointFromIds(indices)) return MS::kFailure;',
            'if (reorder) (void)output.sortPointFromIds(indices);', 1)),
        ('initializer failure ignored', 'point_edit_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', 'point_edit_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, suite, source in cases:
        assert source != (init if suite.endswith('_init') else compute), label
        try:
            if suite.endswith('_init'):
                run_edit(root, source, capture=True, suite=suite)
            else:
                check_compute(root, source, True)
        except subprocess.CalledProcessError as error:
            # A compilation failure does not prove the test detected a bug.
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        import check_group_transfer_safety as shared
        shared.sdk_check(root, sys.argv[2], NODES, 'Point editing')
    elif len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run_edit(root, initializers(root), suite='point_edit_init', arguments=['--emit'])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run_edit(root, initializers(root), suite='point_edit_init')
        check_compute(root, compute_bodies(root))
