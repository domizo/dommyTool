#!/usr/bin/env python3
"""Mesh attribute/control boundaries, independent of the Maya runtime."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_color_noise_safety import palette_helper

NAMES = ('DMColor', 'DMConnetedPieces', 'DMConnectTargets', 'DMInfo')
FOLDER = 'src/modules/mesh/DMesh/Utility/'


def bodies(root, initializers=None, controls=True):
    result = [palette_helper(root)] if controls else []
    for name in NAMES + ('DMRemesh',):
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        if name in NAMES:
            original = (initializers / (FOLDER + name + '.cpp')).read_text() if initializers else source
            result.append(extract(original, 'MStatus ' + name + '::initialize('))
        if not controls:
            continue
        if name == 'DMColor':
            start = source.index('MColorArray plColArray;')
            end = source.index('if (assigneType==', start)
            block = source[start:end]
            block = block.replace('MColorArray plColArray;', '')
            block = block.replace('aPaletteColor', 'DMColor::aPaletteColor')
            result.append('MStatus readPalette(MDataBlock& data,MColorArray& plColArray){MStatus stat;' +
                          block + 'return MS::kSuccess;}')
        if name in ('DMConnectTargets', 'DMRemesh'):
            # Execute the actual read statements, including each intervening status check.
            expressions = list(re.finditer(r'const MDataHandle (\w+) =\s*data.inputValue\((\w+), &status\);\s*'
                r'CHECK_MSTATUS_AND_RETURN_IT\(status\);\s*const (short|int|double) (\w+) =\s*[^;]+;', source))
            assert len(expressions) == (7 if name == 'DMConnectTargets' else 4)
            block = '\n'.join(m.group(0) for m in expressions)
            for attr in attrs:
                block = re.sub(r'\b' + attr + r'\b', name + '::' + attr, block)
            result.append('MStatus read' + name + '(MDataBlock& data){MStatus status;' + block +
                          'return MS::kSuccess;}')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    extra = [dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr',
             dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include']
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas']
    for path in extra + [Path(sdk) / 'include']:
        command += ['-isystem', str(path)]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    paths = [FOLDER + name + '.cpp' for name in NAMES + ('DMRemesh',)]
    paths += ['src/modules/mesh/DMesh/common/DMeshUtil.cpp',
              'src/modules/point/DPoint/Utility/DPRay.cpp']
    if not (extra[0] / 'SeExpression.h').is_file():
        paths.remove(FOLDER + 'DMColor.cpp')
        paths.remove('src/modules/point/DPoint/Utility/DPRay.cpp')
        print('SKIP complete DMColor/DPRay SDK units: pinned SeExpr headers unavailable')
    if not (extra[1] / 'nanoflann.hpp').is_file():
        paths.remove(FOLDER + 'DMConnetedPieces.cpp')
        paths.remove(FOLDER + 'DMConnectTargets.cpp')
        print('SKIP complete mesh connection SDK units: pinned nanoflann headers unavailable')
    imath = os.environ.get('DOMMY_IMATH_INCLUDE')
    if imath:
        if not (Path(imath) / 'ImathVec.h').is_file():
            raise RuntimeError('DOMMY_IMATH_INCLUDE must contain ImathVec.h')
        command += ['-isystem', imath]
        if (extra[0] / 'SeExpression.h').is_file():
            paths.append('src/modules/point/DPoint/math/DPMathUtility.cpp')
    else:
        print('SKIP complete DPMathUtility SDK unit: set DOMMY_IMATH_INCLUDE to real Imath headers')
        print('Other units use the limited Imath test double, not the legacy Imath library')
    for path in paths:
        subprocess.run(command + ['-fsyntax-only', str(root / path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(root / path)], check=True)
    print('Mesh controls: %d complete SDK units passed syntax/analysis' % len(paths))


def attribute_bodies(root):
    source = (root / 'src/modules/mesh/DMesh/common/DMeshUtil.h').read_text()
    pattern = r'template <typename T>\ninline void getDM(?:P)?(?:SingleValueToFloat|TwoArray|Vec2ToVectorArray|Vec3ToVectorArray)\([^\n]+\)'
    signatures = re.findall(pattern, source)
    assert len(signatures) == 14
    return '\n\n'.join(extract(source, signature) for signature in signatures)


def mutations(root):
    control = bodies(root)
    attribute = attribute_bodies(root)
    cases = [
        ('initializer ignores status', 'mesh_controls', control.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('palette ignores allocation', 'mesh_controls', control.replace(
            'return maya::buildCheckedArray(count,', '(void)maya::buildCheckedArray(count,').replace(
            '}, palette);', '}, palette); return MS::kSuccess;')),
        ('palette reads invalid handle', 'mesh_controls', control.replace(
            'DOMMY_RETURN_IF_FAILURE(status);\n            const MVector value = handle.asFloatVector();',
            'const MVector value = handle.asFloatVector();\n            DOMMY_RETURN_IF_FAILURE(status);')),
        ('typed Maya arrays ignore status', 'mesh_attribute', attribute.replace(
            'dommy::attribute::requireArrayStatus(', 'static_cast<void>(')),
    ]
    signature = 'inline void getDMPVec3ToVectorArray(const DMItem &mesh, const std::string &name, DVec3DataList &vecArray)'
    unchecked = signature + ''' {
        if (!mesh) throw std::invalid_argument("null");
        const auto source = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(mesh->getAttr(name));
        if (!source || source->value().size() != mesh->numberOfPoints()) throw std::invalid_argument("size");
        vecArray.resize(source->value().size());
        for (std::size_t i=0; i<vecArray.size(); ++i) {
            const auto& value=source->value()[i];
            vecArray[i]=Imath::V3f(float(value[0]),float(value[1]),float(value[2]));
        }
    }'''
    cases.append(('unchecked float narrowing', 'mesh_attribute', attribute.replace(
        extract(attribute, signature), unchecked)))
    for label, suite, source in cases:
        assert source != (control if suite == 'mesh_controls' else attribute), label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=[] if suite == 'mesh_attribute' else None)
        except subprocess.CalledProcessError as error:
            # Compilation errors do not count as a detected runtime regression.
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) > 1 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) > 1 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run(root, bodies(root), suite='mesh_controls')
        run(root, attribute_bodies(root), suite='mesh_attribute', arguments=[])
