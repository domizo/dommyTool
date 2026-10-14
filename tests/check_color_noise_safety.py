#!/usr/bin/env python3
"""Color palette boundaries and frozen color/point-noise initialization."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from color_compute_source import bodies as compute_bodies

def check_computes(root, overrides=None, arguments=(), capture=False):
    return run(root, compute_bodies(root, overrides), suite='color_compute',
               arguments=list(arguments), capture=capture)

def compute_mutations(root):
    cases = []
    for name, path in COLORS.items():
        source = (root / path).read_text()
        kind = {'DPColor': 'DP', 'DCColor': 'DC', 'DMColor': 'DM'}[name]
        data_type = {'DP': 'DPData', 'DC': 'DCurveData', 'DM': 'DMeshData'}[kind]
        variable = 'input' + kind + 'Data'
        old = ('const ' + data_type + '* ' + variable + ' = nullptr;\n'
               '        stat = getValidInput' + kind + '(in' + kind + ', data, ' + variable + ');\n'
               '        if (!stat) return stat;')
        assert source.count(old) == 1, name
        for label, new in (
                ('legacy input status loss', 'const ' + data_type + '* ' + variable +
                 ' = getInput' + kind + '(in' + kind + ', data);'),
                ('checked input status ignored', old.replace('if (!stat) return stat;', '(void)stat;'))):
            cases.append((name + ': ' + label, path, source.replace(old, new, 1)))
        statements = re.findall(r'DOMMY_RETURN_IF_FAILURE\(\s*dommy::geometry::copyPointsToMayaArray\([\s\S]*?\)\);', source)
        assert len(statements) == (2 if name == 'DPColor' else 1), name
        for index, statement in enumerate(statements):
            cases.append((name + ': point copy status ignored ' + str(index), path,
                          source.replace(statement, statement.replace('DOMMY_RETURN_IF_FAILURE(', 'static_cast<void>(', 1), 1)))
            arguments = statement[statement.index('copyPointsToMayaArray(') + len('copyPointsToMayaArray('):statement.rindex('))')]
            points, array = (value.strip() for value in arguments.rsplit(',', 1))
            unsafe = ('const auto& uncheckedPoints = ' + points + ';\n'
                      'MFloatPointArray ' + array + '(uncheckedPoints.size(), MFloatPoint(0, 0, 0));\n'
                      'for (unsigned int i = 0; i < uncheckedPoints.size(); ++i)\n'
                      '    ' + array + '[i] = MFloatPoint(uncheckedPoints[i][0], uncheckedPoints[i][1], uncheckedPoints[i][2]);')
            # Replace the declaration too, so a mutation must compile and fail
            # runtime assertions; duplicate declarations cannot count as evidence.
            start = source.rfind('MFloatPointArray ' + array + ';', 0, source.index(statement))
            assert start >= 0, name
            end = source.index(statement) + len(statement)
            cases.append((name + ': unchecked coordinate array restored ' + str(index), path,
                          source[:start] + unsafe + source[end:]))
    path = COLORS['DPColor']; source = (root / path).read_text()
    old = '"Failed to create a new DPData object");\n            return stat;'
    assert source.count(old) == 1
    cases.append(('DPColor: factory status replaced', path, source.replace(old, old.replace('return stat;', 'return MS::kFailure;'), 1)))
    for array in ('uArray', 'vArray'):
        old = 'DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(count, fillZero, ' + array + '));'
        assert source.count(old) == 1
        cases.append(('DPColor: zero UV array status ignored ' + array, path,
                      source.replace(old, old.replace('DOMMY_RETURN_IF_FAILURE(', 'static_cast<void>(', 1), 1)))
    for label, path, changed in cases:
        try:
            check_computes(root, {path: changed}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'color_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr, label
            print('Detected color-compute mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected color-compute mutation: ' + label)
    print('Color compute mutations: all {} detected at runtime'.format(len(cases)))

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPColor', 'DPNoise', 'DPDFNoise', 'DPPyroNoise', 'DPFastNoise')}
NODES.update({name: 'src/modules/curve/DCurve/Utility/' + name + '.cpp'
              for name in ('DCColor', 'DCNoise')})
COLORS = {name: NODES[name] for name in ('DPColor', 'DCColor')}
COLORS['DMColor'] = 'src/modules/mesh/DMesh/Utility/DMColor.cpp'

def enums(root):
    header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    result = [re.search(r'enum\s*\{[^}]*\};typedef short ' + alias + ';', header).group()
              for alias in ('NoiseBaseType', 'NoiseDeformType', 'NoiseCalcPositionType',
                  'FastNoiseType', 'FNInterpType', 'FNFractalType',
                  'FNCellularDistanceType', 'FNCellularReturnType')]
    result.append(re.search(r'enum\s*\{[^}]*DP_ATTR_PRE_FLOAT[^}]*\};',
        (root / 'src/modules/point/DPoint/DType.h').read_text()).group())
    return result

def initializers(root, base=None):
    result = enums(root)
    for name, path in NODES.items():
        source = (root / path).read_text()
        declared = declarations(name, source, True)
        if base:
            declared[0] = declared[0].replace(':AttributeNode', ':' + base)
            declared[1:] = [re.sub(r'\{"[^"]*"\}', '', line) for line in declared[1:]]
        result += declared + [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n\n'.join(result)

def palette_helper(root):
    budget = (root / 'src/core/common/utility/DAttributeArrayConversion.h').read_text()
    return ('namespace dommy { namespace attribute {\n' +
            extract(budget, 'inline bool validArrayConversionBudget(') + '\n}}\n' +
            extract((root / 'src/core/common/utility/DColorPalette.h').read_text(), 'namespace dommy {'))

def palette_bodies(root, originals=None):
    result = [palette_helper(root), 'template<typename T>\n' + extract(
        (root / 'src/core/common/object/DNoiseOp.h').read_text(), 'void DGenerateRandomValue(')]
    for name, path in COLORS.items():
        source = ((originals / (name + '.cpp')) if originals else root / path).read_text()
        branch = extract(source, 'else if(colorAddTypeVal==dommy::color::kPalette)')
        body = branch[branch.index('{') + 1:-1]
        if not originals:
            assert body.count('dommy::color::readPalette(') == 1, name
            assert 'plColArray.append(' not in body, name
        result.append('MStatus palette' + name + '(MDataBlock& data, int assigneType, ColorGeometry& result) try {' +
            'MStatus stat;(void)assigneType;const std::string colAttrName="Cd";'
            'ColorGeometry* outputDPData=&result;ColorGeometry* outDCrvs=&result;'
            'ColorGeometry* outDMeshes=&result;' + body +
            '\nreturn MS::kSuccess;\n} DOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)

def sdk_check(root, sdk):
    # The optional backend stays OFF. Its complete initializer is checked with
    # real Maya declarations independently of the external FastNoise algorithm.
    includes = ('MPxNode', 'MFnNumericAttribute', 'MFnGenericAttribute', 'MFnTypedAttribute',
                'MFnEnumAttribute', 'MFnStringData', 'MTypeId', 'MObject', 'MRampAttribute')
    preamble = '\n'.join('#include <maya/' + name + '.h>' for name in includes)
    preamble += '\n#include "common/utility/DComputeGuard.h"\n#include "common/utility/DStatusOperation.h"\n'
    for index, name in enumerate(('DPData', 'DCurveData', 'DMeshData'), 1):
        preamble += ('struct ' + name + '{static const MTypeId id;};const MTypeId ' +
                     name + '::id(' + str(index) + ');\n')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-color-noise-sdk-') as directory:
        source = Path(directory) / 'initializers.cpp'
        source.write_text(preamble + initializers(root, 'MPxNode'))
        subprocess.run(command + ['-fsyntax-only', str(source)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)], check=True)
    print('Color/noise: 7 complete initializers passed real Maya SDK syntax/analysis')
    from check_group_transfer_safety import sdk_check as full_units
    nodes = {name: path for name, path in NODES.items() if name != 'DPFastNoise'}
    nodes['DMColor'] = COLORS['DMColor']
    full_units(root, sdk, nodes, 'Color/noise')
    print('Optional FastNoise algorithm/full unit not covered; default OFF is unchanged')

def compute_sdk_check(root, sdk):
    from check_group_transfer_safety import sdk_check as shared_sdk_check
    code = '#include "' + str(root / 'tests/cpp/color_compute_sdk.h') + '"\n'
    code += compute_bodies(root, sdk=True)
    shared_sdk_check(root, sdk, nodes={}, label='Three complete color callbacks',
                     boundary_source=code)

def mutations(root):
    init, palette = initializers(root), palette_bodies(root)
    cases = [
        ('failed palette handle accessed', False, palette.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n            const MVector value = handle.asFloatVector();',
            'const MVector value = handle.asFloatVector();\n            DOMMY_RETURN_IF_FAILURE(status);')),
        ('palette array failure ignored', False, palette.replace(
            'return maya::buildCheckedArray(count,', '(void)maya::buildCheckedArray(count,').replace(
            '}, palette);', '}, palette); return MS::kSuccess;')),
        ('oversized palette accepted', False, palette.replace(
            '!attribute::validArrayConversionBudget(count, sizeof(MColor))', 'false')),
        ('nonfinite palette accepted', False, palette.replace(
            'if (!isFiniteTriple(value[0], value[1], value[2]))', 'if (false)')),
        ('palette read failure bypassed at call sites', False, palette.replace(
            'DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(\n',
            'static_cast<void>(dommy::color::readPalette(\n')),
        ('palette physical progression omitted', False, palette.replace(
            'if (index + 1U < count) DOMMY_RETURN_IF_FAILURE(elements.next());', '(void)index;')),
        ('point order reversed', False, palette.replace('i%palletCount', '(palletCount-1)-(i%palletCount)')),
        ('primitive random seed changed', False, palette.replace(
            'randomGenerator(static_cast<unsigned int>(seedVal))',
            'randomGenerator(static_cast<unsigned int>(seedVal)+1U)')),
        ('create status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('setter status ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));',
                                                   'tAttr.setReadable(false);', 1)),
        ('string default status ignored', True, init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);\n    aColorName =', 'CHECK_MSTATUS(stat);\n    aColorName =', 1)),
        ('initializer exception escapes', True, init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initializer, source in cases:
        assert source != (init if initializer else palette), label
        try:
            run(root, source, capture=True, suite='color_noise_init' if initializer else 'color_palette',
                arguments=None if initializer else [])
        except subprocess.CalledProcessError as error:
            # Compiler errors are not evidence that a regression was detected.
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif sys.argv[1:2] == ['--compute']:
        check_computes(root, arguments=sys.argv[2:])
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        compute_sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        mutations(root)
        compute_mutations(root)
    elif '--emit-palettes' in sys.argv:
        originals = Path(sys.argv[2]) if len(sys.argv) > 2 else None
        run(root, palette_bodies(root, originals), suite='color_palette', arguments=['--emit'])
    else:
        run(root, initializers(root), suite='color_noise_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            run(root, palette_bodies(root), suite='color_palette', arguments=[])
            check_computes(root)
            compute_mutations(root)
