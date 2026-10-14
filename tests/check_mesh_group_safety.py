#!/usr/bin/env python3
"""Mesh-group compute boundaries and unchanged conversion/group initialization."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODES = (('DMToMesh', 'Convert'), ('DMGroupToIndex', 'Group'),
         ('DMGroupFromDP', 'Group'), ('DMGroupDelete', 'Data'))
FOLDER = 'src/modules/mesh/DMesh/'


def initializers(root):
    result = []
    for name, folder in NODES:
        source = (root / (FOLDER + folder + '/' + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def index_body(root):
    common = (root / 'src/core/common/object/DNodeOperation.cpp').read_text()
    source = (root / (FOLDER + 'Group/DMGroupToIndex.cpp')).read_text()
    return ('template <typename FunctionSet, typename Array>\n' +
            extract(common, 'MStatus outputMayaArray(') + '\n' +
            extract(common, 'MStatus DOutputDataMayaIntArray(') + '\n' +
            extract(source, 'MStatus DMGroupToIndex::compute(') +
            '\nDOMMY_COMPUTE_CATCH("DMGroupToIndex")')


def compute_bodies(root):
    common = (root / (FOLDER + 'common/DMeshUtil.cpp')).read_text()
    result = [extract((root / ('src/core/common/utility/' + header)).read_text(), 'namespace dommy {')
              for header in ('DGroupGenerationPlan.h', 'DMayaNameList.h')]
    result.append(extract(common, 'MStatus getValidInputDM('))
    for name, folder in NODES[2:]:
        source = (root / (FOLDER + folder + '/' + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ' {MStatus compute(const MPlug&, MDataBlock&);' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def to_mesh_body(root):
    source = (root / (FOLDER + 'Convert/DMToMesh.cpp')).read_text()
    common = (root / (FOLDER + 'common/DMeshUtil.cpp')).read_text()
    return '\n'.join([extract(common, 'MStatus getValidInputDM('),
                      extract(source, 'namespace {'),
                      extract(source, 'MStatus DMToMesh::compute('),
                      'DOMMY_COMPUTE_CATCH("DMToMesh")'])


def sdk_check(root, sdk):
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    nanoflann = dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include'
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'),
                '-idirafter', str(root / 'tests/stubs')]
    nodes = list(NODES)
    if (nanoflann / 'nanoflann.hpp').is_file() and (seexpr / 'SeExpression.h').is_file():
        command += ['-isystem', str(nanoflann), '-isystem', str(seexpr)]
    else:
        nodes.remove(('DMGroupFromDP', 'Group'))
        print('SKIP complete DMGroupFromDP SDK unit: pinned nanoflann/SeExpr unavailable')
    for name, folder in nodes:
        source = root / (FOLDER + folder + '/' + name + '.cpp')
        subprocess.run(command + ['-fsyntax-only', str(source)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)], check=True)
    print('Mesh groups: %d complete SDK units passed syntax/analysis (limited Imath test double)' % len(nodes))


def mutations(root):
    init, index, compute = initializers(root), index_body(root), compute_bodies(root)
    cases = [
        ('missing group drops offset', 'mesh_group_index', index.replace(
            'offset += selection.count;', 'if (selection.group) offset += selection.count;')),
        ('mixed component types accepted', 'mesh_group_index', index.replace(
            'else if (type != localType)', 'else if (false)')),
        ('group array length unchecked', 'mesh_group_index', index.replace(
            'if (indices.length() != selectedCount) return MS::kFailure;', '')),
        ('failed group read ignored', 'mesh_group_index', index.replace(
            'DOMMY_RETURN_IF_FAILURE(status);', 'CHECK_MSTATUS(status);', 1)),
        ('initializer create failure ignored', 'mesh_group_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(status);', 'CHECK_MSTATUS(status);', 1)),
        ('initializer exception escapes', 'mesh_group_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('final group budget skipped', 'mesh_group_compute', compute.replace(
            'if (!validGeneratedGroupBudget(input, primitiveCount, candidate)) return false;',
            'if (false && !validGeneratedGroupBudget(input, primitiveCount, candidate)) return false;')),
        ('replacement counted twice', 'mesh_group_compute', compute.replace(
            'if (!input.isExistGrp(name) &&', 'if (true &&')),
        ('attribute budget ignored', 'mesh_group_compute', compute.replace(
            '!dommy::attribute::validateAttribute(attribute->second, primitiveCount, budget)', 'false')),
        ('repeated group names unbounded', 'mesh_group_compute', compute.replace(
            '!generation::checkedProduct(prefixBytes, generatedCount,\n            generation::kMaxGeneratedStringByteCount, nameBytes)',
            '(static_cast<void>(prefixBytes), false)')),
        ('from-DP input read failure ignored', 'mesh_group_compute', compute.replace(
            'const MDataHandle groupHandle = data.inputValue(aGroupName, &status);\n    DOMMY_RETURN_IF_FAILURE(status);',
            'const MDataHandle groupHandle = data.inputValue(aGroupName, &status);')),
        ('delete input read failure ignored', 'mesh_group_compute', compute.replace(
            'const MDataHandle enableHandle = data.inputValue(aIsEnable, &status);\n        DOMMY_RETURN_IF_FAILURE(status);',
            'const MDataHandle enableHandle = data.inputValue(aIsEnable, &status);')),
        ('split failure ignored', 'mesh_group_compute', compute.replace(
            "DOMMY_RETURN_IF_FAILURE(value.split(' ', names));",
            "(void)value.split(' ', names);")),
        ('delete work budget skipped', 'mesh_group_compute', compute.replace(
            'domain == 0U ? 1U : meshCount,', '1U,')),
        ('distance filtering lost', 'mesh_group_compute', compute.replace(
            'if (useDistance && distance > maximumDistance) continue;', '')),
    ]
    baselines = {'mesh_group_init': init, 'mesh_group_index': index, 'mesh_group_compute': compute}
    for label, suite, source in cases:
        assert source != baselines[suite], label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=None if suite == 'mesh_group_init' else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    original = to_mesh_body(root)
    for label, old, new in [
        ('invalid mesh accepted', 'if (!inputData || !inputData->hasValidMeshTopology())',
         'if (!inputData)'),
        ('failed control handle consumed', 'DOMMY_RETURN_IF_FAILURE(status);\n    const short convertType',
         'const short convertType'),
        ('failed group split ignored', "DOMMY_RETURN_IF_FAILURE(groupNamesValue.split(' ', groupNames));",
         "(void)groupNamesValue.split(' ', groupNames);"),
    ]:
        assert old in original, label
        try:
            run(root, original.replace(old, new), capture=True, suite='mesh_to_maya', arguments=[])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1, 'Compile failure is not detection: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run(root, initializers(root), suite='mesh_group_init')
        run(root, index_body(root), suite='mesh_group_index', arguments=[])
        run(root, compute_bodies(root), suite='mesh_group_compute', arguments=[])
        run(root, to_mesh_body(root), suite='mesh_to_maya', arguments=[])
