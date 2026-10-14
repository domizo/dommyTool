#!/usr/bin/env python3
"""Complete curve transactions; real typed metadata, explicit native boundaries."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run


def bodies(root):
    for name, pairs in (('DMConnectTargets', 1), ('DMConnetedPieces', 2)):
        node = (root / ('src/modules/mesh/DMesh/Utility/' + name + '.cpp')).read_text()
        compute = extract(node, 'MStatus ' + name + '::compute(')
        assert compute.count('DCurveOutputBatch outputBatch;') == 1, name
        assert len(re.findall(r'appendConnectionCurve\(\s*outputBatch\s*,', compute)) == pairs, name
        assert compute.count('outputBatch.publishTo(*output)') == 1, name
        assert compute.index('outputBatch.publishTo(*output)') < compute.index('data.outputValue('), name
        assert 'output->insert(' not in compute and 'output->updateBBox(' not in compute, name
    source = (root / 'src/core/Data/DCurveData.cpp').read_text()
    signatures = (
        'MStatus DCurveData::copyFrom(', 'MStatus DCurveData::copyRepeatedFrom(', 'bool DCurveData::getCheckedTotalCvs(',
        'bool DCurveData::hasValidCurveTopology(',
        'bool DCurveData::insert(const DCurveInternalData',
        'bool DCurveData::insert(const DCItem&', 'bool DCurveData::insert(const ConstDCItem&',
        'bool DCurveData::insert(const DCurveList&',
        'bool DCurveData::appendCurvesTransactional(', 'bool DCurveData::expand(const DCItem&')
    result = [extract(source, 'bool DCurveInternalData::appendOperationCost(')]
    result.extend(extract(source, signature) for signature in signatures)
    for path, signatures in (
        ('src/modules/point/DPoint/Common/DPAttributeCommon.cpp',
         ('void CopyAttributeToAttribute(', 'void ExpandAttributeSize(', 'bool appendOtherDPAttribute(')),
        ('src/core/Data/DGroupCommon.cpp', ('void CopyGroupToGroup(', 'void ExpandGroupListSize('))):
        text = (root / path).read_text()
        result.extend(extract(text, signature) for signature in signatures)
    result.append(extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                          'class DCurveOutputBatch final') + ';')
    builder = (root / 'src/modules/curve/DCurve/Common/DConnectionCurveBuilder.h').read_text()
    result.append('namespace dommy { namespace curve {\n' +
                  extract(builder, 'inline bool appendConnectionCurve(') + '\n} }')
    return '\n\n'.join(result)


def execute(root, source, capture=False, overrides=None):
    limits = (root / 'src/core/common/utility/DGenerationLimits.h').read_text()
    limits = limits.replace('kMaxGeneratedStringByteCount = 100000000U',
                            'kMaxGeneratedStringByteCount = 128U')
    headers = dict(overrides or {})
    headers['common/utility/DGenerationLimits.h'] = limits
    result = run(root, source, suite='curve_transactions', arguments=[], capture=capture,
        header_overrides=headers,
        extra_flags=[str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])
    run(root, source, suite='curve_transactions', arguments=['connections'], capture=capture,
        header_overrides=overrides,
        extra_flags=[str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])
    return result


def mutations(root):
    source = bodies(root)
    execute(root, source)
    cases = (
        ('repeated clones duplicated', 'DCItem copied = curve->copyNewCurve();',
         'DCItem redundant = curve->copyNewCurve(); DCItem copied = curve->copyNewCurve();'),
        ('original batch transformed', 'if (copyIndex != 0U && operation)', 'if (operation)'),
        ('callback failure ignored', 'DOMMY_RETURN_IF_FAILURE(operation(*copied, copyIndex, curveIndex));',
         '(void)operation(*copied, copyIndex, curveIndex);'),
        ('collection rows not repeated',
         'if (!appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first)))',
         'if (false && !appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first)))'),
        ('retained callback budget omitted',
         'if (!dommy::curve::buildCurveCopyPlan(candidate, 1U, finalPlan))', 'if (false)'),
        ('batch component strings omitted', 'components.workItems, components.stringBytes)',
         'components.workItems, 0U)'),
        ('double collection bounds', 'candidate.updateBBox();',
         'candidate.updateBBox(); candidate.updateBBox();'),
        ('collection expansion admission removed',
         '!dommy::curve::validateCurveOperationBudget(plan, mAttributeList, mGroupList,\n'
         '                curvearray.size(), curves.size(), sourceIndices, budget)', 'false'),
        ('output aliasing', 'for (const DCurveInternalData* curve : curves) {\n            DCItem copied = curve->copyNewCurve();',
         'for (const DCurveInternalData* curve : curves) {\n            DCItem copied(const_cast<DCurveInternalData*>(curve), [](DCurveInternalData*) {});'),
        ('admission invalidates cached bounds',
         'addCurveOperationCost(curvearray.cbegin(), curvearray.cend(), plan, budget)',
         'addCurveOperationCost(beginCurves(), endCurves(), plan, budget)'),
    )
    for label, old, new in cases:
        assert old in source, label
        try:
            execute(root, source.replace(old, new), True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_transactions_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'common/utility/DCurveOperationBudget.h'
    header = (root / 'src/core' / path).read_text()
    old = '!geometry::addCopyMetadataCost(*curve, curve->numberOfPoints(), budget)'
    assert old in header
    try:
        execute(root, source, True, {path: header.replace(old, '((void)budget, false)')})
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'curve_transactions_tests', 'Compile failure: component admission removed'
        print('Detected mutation: component admission removed', flush=True)
    else:
        raise AssertionError('Undetected mutation: component admission removed')
    path = 'common/utility/DCurveBatchBudget.h'
    header = (root / 'src/core' / path).read_text()
    for label, old in (
        ('early geometry admission omitted', '!mesh::validMeshOperationStorage(candidate.plan_)'),
        ('early combined work omitted',
         '!generation::checkedSum(candidate.componentWork_, candidate.plan_.workItems,\n'
         '                generation::kMaxWorkItemCount, work)'),
        ('early aggregate strings omitted',
         '!generation::checkedSum(candidate.componentStrings_, componentStrings,\n'
         '                generation::kMaxGeneratedStringByteCount, candidate.componentStrings_)'),
    ):
        assert old in header, label
        changed = header.replace(old, 'false', 1)
        # Keep deliberately omitted inputs used so compilation itself cannot
        # masquerade as a detected runtime regression.
        if 'strings' in label:
            changed = changed.replace('CurveBatchBudget candidate = *this;',
                                      '(void)componentStrings; (void)componentStrings_; CurveBatchBudget candidate = *this;')
        try:
            execute(root, source, True, {path: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_transactions_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def check_sdk(root, sdk):
    includes = ['-I' + str(directory) for directory, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(root / 'third_party/libnoise/source'),
        '-idirafter', str(root / 'tests/stubs')]
    paths = ['src/core/Data/DCurveData.cpp', 'tests/cpp/connection_curve_builder_compile_test.cpp',
             'src/modules/point/DPoint/Utility/DPCopyTransform.cpp']
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
        str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    nanoflann = Path(os.environ.get('NANOFLANN_LOCATION',
        str(dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include')))
    if not (nanoflann / 'nanoflann.hpp').is_file() and (nanoflann / 'include/nanoflann.hpp').is_file():
        nanoflann /= 'include'
    if (seexpr / 'SeExpression.h').is_file() and (nanoflann / 'nanoflann.hpp').is_file():
        command += ['-isystem', str(seexpr), '-isystem', str(nanoflann)]
        paths += ['src/modules/mesh/DMesh/Utility/DMConnectTargets.cpp',
                  'src/modules/mesh/DMesh/Utility/DMConnetedPieces.cpp']
    else:
        print('SKIP complete connection SDK units: pinned SeExpr/nanoflann headers unavailable', flush=True)
    imath = os.environ.get('DOMMY_IMATH_INCLUDE')
    if imath:
        if not (Path(imath) / 'ImathVec.h').is_file():
            raise RuntimeError('DOMMY_IMATH_INCLUDE must contain ImathVec.h')
        command += ['-isystem', imath]
    else:
        print('SDK checks use limited Imath doubles, not the legacy Imath library', flush=True)
    for path in paths:
        source = str(root / path)
        subprocess.run(command + ['-fsyntax-only', source], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', source], check=True)
        print('SDK syntax/analyzer: ' + path, flush=True)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        check_sdk(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
