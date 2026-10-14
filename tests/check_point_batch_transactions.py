#!/usr/bin/env python3
"""Complete point batch transactions with real typed metadata and groups."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run


def bodies(root):
    source = (root / 'src/core/DDataUtility/Data/DPData.cpp').read_text()
    result = [extract(source, signature) for signature in (
        'bool DPData::appendMetadataFrom(', 'MStatus DPData::mergeFrom(',
        'MStatus DPData::copyRepeatedFrom(', 'bool DPData::hasValidPointTopology(',
        'void DPData::setDPData(', 'bool DPData::mergeDPData(')]
    text = (root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text()
    result.append(extract(text, 'bool appendOtherDPAttribute('))
    result.append(extract(text, 'void CopyAttributeToAttribute('))
    result.append(extract((root / 'src/core/Data/DGroupCommon.cpp').read_text(), 'void CopyGroupToGroup('))
    for relative, signature in (
        ('src/core/DDataUtility/Utility/DPMerge.cpp', 'MStatus DPMerge::compute('),
        ('src/modules/point/DPoint/Utility/DPDuplicator.cpp', 'MStatus DPDuplicator::compute('),
        ('src/modules/point/DPoint/Utility/DPCopyTransform.cpp', 'MStatus DPCopyTransform::compute(')):
        node = extract((root / relative).read_text(), signature)
        assert '.mergeDPData(' not in node and '.replacePositions(' not in node, relative
        assert ('outputData.mergeFrom(' if 'DPMerge' in signature else 'output.copyRepeatedFrom(') in node
    return '\n\n'.join(result)


def execute(root, source, capture=False, headers=None):
    run(root, source, suite='point_batch_transactions', arguments=[], capture=capture,
        header_overrides=headers,
        extra_flags=['-I' + str(root / 'src/core/common/utility'),
                     str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])
    limits = (root / 'src/core/common/utility/DGenerationLimits.h').read_text().replace(
        'kMaxGeneratedStringByteCount = 100000000U', 'kMaxGeneratedStringByteCount = 128U')
    overrides = dict(headers or {})
    overrides['common/utility/DGenerationLimits.h'] = limits
    payload = (root / 'src/core/common/utility/DAttributeArrayConversion.h').read_text().replace(
        'maximum = 256U * 1024U * 1024U', 'maximum = 4096U')
    overrides['DAttributeArrayConversion.h'] = payload
    overrides['common/utility/DAttributeArrayConversion.h'] = payload
    run(root, source, suite='point_batch_transactions', arguments=['strings'], capture=capture,
        header_overrides=overrides,
        extra_flags=['-I' + str(root / 'src/core/common/utility'),
                     str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])


def mutations(root):
    source = bodies(root)
    execute(root, source)
    cases = (
        ('placement failure ignored', 'DOMMY_RETURN_IF_FAILURE(operation(point, copyIndex, pointIndex));',
         '(void)operation(point, copyIndex, pointIndex);'),
        ('first batch placement skipped', 'if (operation) DOMMY_RETURN_IF_FAILURE(',
         'if (copyIndex != 0U && operation) DOMMY_RETURN_IF_FAILURE('),
        ('later empty schemas imported', 'if (index != 0U && source.pointArray.empty()) continue;', ''),
        ('missing field defaults omitted', 'field.second->incrementDefaultValue(sourcePointCount);', '(void)sourcePointCount;'),
        ('front defaults omitted', 'copied->insertDefaultValue(destinationPointCount, 0U);', '(void)destinationPointCount;'),
        ('group leading mask omitted', 'copied->insertOtherGrp(destinationPointCount, 0U, false);', '(void)destinationPointCount;'),
        ('group clone alias accepted', 'copied.get() == group.second.get() || ', ''),
        ('attribute clone alias accepted', 'copied.get() == field.second.get() ||', ''),
        ('double bounds update', 'candidate.updateBBox();', 'candidate.updateBBox(); candidate.updateBBox();'),
        ('retained clone budget omitted', 'if (!dommy::point::buildPointCopyPlan(candidate, 1U, finalPlan))', 'if (false)'),
        ('generic copy admission omitted', 'if (!dommy::point::buildPointCopyPlan(data, 1U, copyPlan))', 'if (false)'),
        ('generic cloned copy admission omitted', 'if (!dommy::point::buildPointCopyPlan(candidate, 1U, finalPlan)) {', 'if (false) {'),
    )
    for label, old, new in cases:
        assert old in source, label
        try:
            execute(root, source.replace(old, new), True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_batch_transactions_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'common/utility/DPointMergeBudget.h'
    header = (root / 'src/core' / path).read_text()
    for label, old, new in (
        ('actual repeated strings unbounded',
         '!checkedProduct(rows.stringBytes, copies, kMaxGeneratedStringByteCount, strings)', 'false'),
        ('missing default strings unbounded',
         '!checkedProduct(missing, defaults.stringBytes, kMaxGeneratedStringByteCount, strings)', 'false'),
        ('group work omitted', '!checkedSum(plan.workItems, plan.points, kMaxWorkItemCount, plan.workItems)', 'false'),
        ('payload allowance bypassed', 'plan.estimatedBytes = payload.estimatedBytes();', 'plan.estimatedBytes = 0U;'),
    ):
        assert old in header, label
        changed = header.replace(old, new, 1)
        if 'payload allowance' in label:
            # Remove the shared payload admission itself; keeping add() would
            # merely test an accounting-report field, not memory admission.
            changed = changed.replace('PointPayloadBudget payload;',
                'struct NoBudget { void add(std::size_t,std::size_t){} }; NoBudget payload;')
        try:
            execute(root, source, True, {path: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'point_batch_transactions_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk, paths=None, label='Point batch', extra_flags=()):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    command += list(extra_flags)
    for path in (paths if paths is not None else (
                 'src/core/DDataUtility/Data/DPData.cpp', 'src/core/DDataUtility/Utility/DPMerge.cpp',
                 'src/modules/point/DPoint/Utility/DPDuplicator.cpp',
                 'src/modules/point/DPoint/Utility/DPCopyTransform.cpp')):
        for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                      '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
            subprocess.run(command + flags + [str(root / path)], check=True)
        print(label + ' SDK syntax/analyzer: ' + path, flush=True)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
