#!/usr/bin/env python3
"""Fur/Aho input, frozen initialization and bounded generation checks."""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check
from curve_generator_compute_source import bodies as compute_bodies
from arnold_name_list_source import name_helper

NODES = {
    'DCFurGen': 'src/modules/curve/DCurve/Generate/DCFurGen.cpp',
    'DCAhoGen': 'src/modules/curve/DCurve/Generate/DCAhoGen.cpp',
}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        result += declarations(name, source, True)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)

def inputs(root):
    source = extract((root / NODES['DCFurGen']).read_text(), 'MStatus DCFurGen::compute(')
    assert not re.search(r'inputValue\([^;]+\)\.as', source)
    result = []
    pattern = r'const MDataHandle (\w+) = data.inputValue\((\w+), &status\);'
    for match in re.finditer(pattern, source):
        end = source.index(';', source.index('.as', match.end())) + 1
        check = ''
        if match[1] == 'struggleComponentsHandle':
            end = source.index('}};', end) + 3
            check = 'raw[0]=99;assert(struggleComponents[0]==1);'
        result.append('MStatus probe' + str(len(result)) + '(){MStatus status;MDataBlock data;MObject ' +
                      match[2] + ';' + source[match.start():end] + check + '++admissions;return MS::kSuccess;}')
    assert len(result) == 14
    result.append('MStatus(*probes[])()={' + ','.join('probe'+str(i) for i in range(14)) + '};')
    return '\n'.join(result)


def storage(root):
    result = []
    for header in ('DCurveGenerationStorage.h', 'DFurGenerationPlan.h',
                   'DAhoGenerationPlan.h', 'DPointPayloadBudget.h'):
        result.append(extract((root / 'src/core/common/utility' / header).read_text(),
                              'namespace dommy {'))
    names = (root / 'src/core/common/utility/DMayaNameList.h').read_text()
    result.append(name_helper(names))
    fur = (root / NODES['DCFurGen']).read_text()
    aho = (root / NODES['DCAhoGen']).read_text()
    result += [extract(fur, 'MStatus preflightFurTransfers('),
               'template <typename Value>\n' + extract(fur, 'bool copySelectedAttribute('),
               extract(fur[fur.index('bool copySelectedAttribute(') + 1:],
                       'bool copySelectedAttribute('),
               extract(aho, 'MStatus preflightAhoInputStorage(')]
    # Extract the actual admission sequences, stopping at the first bulk/host
    # allocation. A successful probe marks the point those allocations can run.
    begin = fur.index('const std::size_t pointCount = input->numberOfPoints();')
    end = fur.index('std::vector<float> thinningValues', begin)
    result.append('MStatus furAdmission(const DPData* input, int segmentCount, '
                  'const MString& transferAttributes) try {(void)segmentCount; (void)transferAttributes;' + fur[begin:end] +
                  '++admissions;return MS::kSuccess;}catch(...){return MS::kFailure;}')
    begin = aho.index('dommy::generation::CurveGenerationStorage storage(plan.estimatedBytes);')
    end = aho.index('MRampAttribute struggleRamp', begin)
    result.append('MStatus ahoAdmission(const DCurveData* inputCurves, '
                  'const dommy::generation::AhoGenerationPlan& plan) try {' + aho[begin:end] +
                  '++admissions;return MS::kSuccess;}catch(...){return MS::kFailure;}')
    seed_check = aho[aho.index('static_cast<std::size_t>(seed) >'):aho.index(') {\n        MGlobal::displayError(')]
    result.append('bool ahoSeedAdmission(int seed, const dommy::generation::AhoGenerationPlan& plan) {'
                  'return seed >= 0 && !(' + seed_check + ');}')
    assert fur.index('preflightFurTransfers(*input,') < fur.index('std::vector<float> thinningValues')
    assert aho.index('buildAhoGenerationPlan(') < aho.index('preflightAhoInputStorage(*inputCurves,')
    assert aho.index('preflightAhoInputStorage(*inputCurves,') < aho.index('MRampAttribute struggleRamp')
    assert fur.count('output->insert(') == 1
    assert aho.count('outputCurves->insert(') == 1
    return '\n'.join(result)


def mutations(root):
    init, reads, budget = initializers(root), inputs(root), storage(root)
    cases = [
        ('failed input handle read', 'fur_aho_inputs', reads,
         'DOMMY_RETURN_IF_FAILURE(status);', '', []),
        ('tuple ownership lost', 'fur_aho_inputs', reads,
         'const std::array<float, 3> struggleComponents = {{\n        struggleComponentsRaw[0], struggleComponentsRaw[1], struggleComponentsRaw[2]}};',
         'const float* struggleComponents = struggleComponentsRaw;', []),
        ('null tuple unchecked', 'fur_aho_inputs', reads,
         'if (!struggleComponentsRaw) return MS::kFailure;', '', []),
        ('initializer failure ignored', 'fur_aho_init', init,
         'DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));', '(void)nAttr.setDefault(1);', None),
        ('initializer exception escapes', 'fur_aho_init', init,
         'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', None),
        ('string default failure ignored', 'fur_aho_init', init,
         'const MObject aLengthAttributeDefault = fnStringData.create("", &stat);\n    DOMMY_RETURN_IF_FAILURE(stat);',
         'const MObject aLengthAttributeDefault = fnStringData.create("", &stat);', None),
    ]
    for old, new, label in (
        ('curves, 2048U', 'curves, 0U', 'curve overhead'),
        ('totalPoints, 96U', 'totalPoints, 0U', 'staged and retained CVs'),
        ('pointsPerCurve, 512U', 'pointsPerCurve, 0U', 'per-curve scratch'),
        ('rootScratchCount, 256U', 'rootScratchCount, 0U', 'root scratch'),
        ('std::max(plan.outputCurveCount, plan.inputCurveCount) - 3U',
         'plan.outputCurveCount - 3U', 'zero-copy input seed overflow'),
        ('checkedSum(bytes, extra, kMaxCurveGenerationBytes, total)',
         'checkedSum(0U, extra, kMaxCurveGenerationBytes, total)', 'cumulative curve storage'),
        ('bytes_ * 4U', 'bytes_', 'metadata clone allowance'),
        ('characters, 64U', 'characters, 0U', 'name storage'),
        ('bytes - characters, 64U', 'bytes - characters, 0U', 'multibyte name storage'),
        ('storage.add(1U, attributes.estimatedBytes());', '', 'aggregate metadata'),
        ('workspace.add(1U, width);', '', 'metadata defaults'),
        ('workspace.add(cost.stringBytes - previousStrings, sizeof(char));', '', 'dynamic strings'),
        ('workspace.add(cost.workItems - previousWork - count, sizeof(int));', '', 'dynamic integer arrays'),
        ('if (!dommy::attribute::validateAttribute(field->second, count, cost))', 'if (false)', 'invalid metadata'),
        ('maximumPoints, 128U', 'maximumPoints, 0U', 'native input workspace'),
        ('std::max(maximumPoints, (*curve)->numberOfPoints())',
         'maximumPoints + (*curve)->numberOfPoints()', 'largest input not sum'),
        ('!(*curve)->hasValidCurveTopology()', 'false', 'input curve topology'),
        ('DOMMY_RETURN_IF_FAILURE(preflightAhoInputStorage(*inputCurves, storage));',
         '(void)preflightAhoInputStorage(*inputCurves, storage);', 'input failure propagation'),
        ('preflightFurTransfers(*input, transferAttributes, pointCount, storage, transferNames));',
         'MS::kSuccess);', 'Fur admission call'),
        ('if (!dommy::generation::buildFurGenerationPlan(pointCount, segmentCount, plan))',
         'if (false)', 'Fur count/byte admission'),
    ):
        cases.append((label, 'fur_aho_storage', budget, old, new, []))
    for label, suite, body, old, new, arguments in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), suite=suite, arguments=arguments, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    compute_mutations(root)


def compute_mutations(root):
    source = compute_bodies(root)
    for label, old, new in (
            ('Fur acquisition status',
             'const DPData* input = nullptr;\n    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));',
             'const DPData* input = getInputDP(inDP, data);\n    if (!input) return MS::kInvalidParameter;'),
            ('Aho acquisition status',
             'const DCurveData* inputCurves = nullptr;\n    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));',
             'const DCurveData* inputCurves = getInputDC(inDC, data);\n    if (!inputCurves) return MS::kInvalidParameter;'),
            ('native conversion acquisition status',
             'const DCurveData* inputCurves = nullptr;\n    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));',
             'const DCurveData* inputCurves = getInputDC(inDC, data);\n    if (!inputCurves) return MS::kFailure;')):
        # Aho and conversion have the same admission. Target each complete body.
        name = {'Fur acquisition status': 'DCFurGen', 'Aho acquisition status': 'DCAhoGen',
                'native conversion acquisition status': 'DCToCurves'}[label]
        begin = source.index('MStatus ' + name + '::compute(')
        assert old in source[begin:], label
        changed = source[:begin] + source[begin:].replace(old, new, 1)
        try:
            run(root, changed, suite='curve_generator_compute', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'curve_generator_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='fur_aho_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk = sys.argv[sys.argv.index('--sdk') + 1]
        for name, path in NODES.items():
            sdk_check(root, sdk, path, name)
    elif '--sdk-boundary' in sys.argv:
        from curve_generator_compute_source import sdk_check as compute_sdk_check
        compute_sdk_check(root, sys.argv[sys.argv.index('--sdk-boundary') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='fur_aho_init')
        run(root, inputs(root), suite='fur_aho_inputs', arguments=[])
        run(root, storage(root), suite='fur_aho_storage', arguments=[])
        run(root, compute_bodies(root), suite='curve_generator_compute', arguments=[])
        compute_mutations(root)
        from curve_bend_compute_source import execute as bend_check
        bend_check(root)
