#!/usr/bin/env python3
"""Extract mesh utility boundaries; doubles do not emulate Maya DG/geometry."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NAMES = ('DMNormal', 'DMExtrude', 'DMEdgeDisplaceDir', 'DMCopyUV')
FOLDER = 'src/modules/mesh/DMesh/Utility/'
DATA = 'src/modules/mesh/DMesh/Data/DMeshData.cpp'


def bodies(root, initializers=None):
    common = (root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text()
    result, readers = [extract(common, 'MStatus getValidInputDM(').replace('DMeshData', 'ExtrudeData')], []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&,MDataBlock&);' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(initializers[name] if initializers else source,
                              'MStatus ' + name + '::initialize('))
        pattern = (r'const MDataHandle (\w+) = data\.inputValue\((\w+), &stat\);\s*'
                   r'if \(!stat\) return stat;\s*((?:const \w+(?:::\w+)? )?\w+) = '
                   r'\1\.(as\w+)\(\)(?:\.asChar\(\))?;')
        types = {'asBool': 'bool', 'asFloat': 'float', 'asInt': 'int', 'asShort': 'short',
                 'asDouble': 'double', 'asString': 'std::string'}
        for match in re.finditer(pattern, source):
            block = match[0].replace('inputValue(' + match[2] + ',', 'inputValue(' + name + '::' + match[2] + ',')
            variable = match[3].split()[-1]
            if not match[3].startswith('const '):
                block = block.replace(variable + ' = ', types[match[4]] + ' ' + variable + ' = ')
            reader = 'control' + str(len(readers))
            result.append('MStatus ' + reader + '(MDataBlock& data) {MStatus stat;' + block +
                          '(void)' + variable + ';return MS::kSuccess;}')
            readers.append(reader)
        if name == 'DMEdgeDisplaceDir':
            start = source.index('MFnMesh reference(')
            end = source.index('CreateSearchDataMFP(referencePoints, referenceCloud);', start)
            block = source[start:end + len('CreateSearchDataMFP(referencePoints, referenceCloud);')]
            for attr in attrs:
                block = re.sub(r'\b' + attr + r'\b', name + '::' + attr, block)
            result.append('MStatus referenceSearch() {MStatus stat; MDataBlock data;'
                          'MDataHandle objectHandle=data.inputValue(DMEdgeDisplaceDir::aInObject,&stat);'
                          'if(!stat)return stat;const auto* inputMeshes=&utilityInput;'
                          'MFloatPointArray referencePoints; PointCloud<double> referenceCloud;'
                          'std::string attrName; double minRatio=0;' + block +
                          '(void)attrName;return MS::kSuccess;}')
            assert source.index('validPointSearchStorage') < source.index('reference.getPoints')
        if name == 'DMCopyUV':
            result.append(extract(source, 'MStatus DMCopyUV::compute(').replace('DMeshData', 'ExtrudeData') +
                          '\nDOMMY_COMPUTE_CATCH("DMCopyUV")')
            start = source.index('MDataHandle referenceHandle')
            end = source.index('std::vector<std::vector<int>> destinationFaces', start)
            block = source[start:end].replace('inputValue(aInObject,', 'inputValue(DMCopyUV::aInObject,')
            result.append('MStatus referenceUV() {MStatus status;MDataBlock data;'
                          'const auto* inputMeshes=&utilityInput;' + block + 'return MS::kSuccess;}')
            assert source.index('validMeshUVTransferBudget') < source.index('reference.getUVs')
        if name == 'DMExtrude':
            result.append(extract(source, 'MStatus DMExtrude::compute(').replace('DMeshData', 'ExtrudeData') +
                          '\nDOMMY_COMPUTE_CATCH("DMExtrude")')
            assert source.index('appendExtrusionPlan') < source.index('outputMeshes->copyFrom')
            assert source.index('appendExtrusionPlan') < source.index('meshFn.extrudeFaces')
            assert source.index('meshFn.numFaceVertices') < source.index('setReplaceMeshData(object)')
    assert len(readers) == 14, len(readers)
    result.append('using Reader=MStatus(*)(MDataBlock&);std::vector<Reader> readers={' + ','.join(readers) + '};')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    includes = ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    paths = [DATA] + [FOLDER + n + '.cpp' for n in NAMES if n != 'DMEdgeDisplaceDir']
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                                    str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    nanoflann = dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include'
    if (seexpr / 'SeExpression.h').is_file() and (nanoflann / 'nanoflann.hpp').is_file():
        command += ['-isystem', str(seexpr), '-isystem', str(nanoflann)]
        paths.append(FOLDER + 'DMEdgeDisplaceDir.cpp')
    else:
        print('SKIP complete DMEdgeDisplaceDir SDK unit: pinned SeExpr/nanoflann headers unavailable')
    for path in paths:
        subprocess.run(command + ['-fsyntax-only', str(root / path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(root / path)], check=True)
    print('Mesh utility: ' + str(len(paths)) + ' complete SDK translation units passed syntax/analysis')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    else:
        normal_key=extract((root/'src/modules/mesh/DMesh/common/DMeshNormals.h').read_text(),
                           'inline const char* cornerNormalAttribute(')
        run(root, 'namespace dommy {namespace mesh {'+normal_key+'}}\n'+
            extract((root / DATA).read_text(), 'bool DMeshInternalData::reverseWinding('),
            suite='mesh_reverse', arguments=[])
        source=bodies(root)
        fixture=str(root / 'tests/fixtures/mesh_utility_initialization_contract.txt')
        run(root, source, suite='mesh_utility', arguments=sys.argv[1:] or [fixture])
        if not sys.argv[1:]:
            begin=source.index('MStatus DMCopyUV::compute(')
            old='DOMMY_RETURN_IF_FAILURE(status);\n    if (!outputMeshes) return MS::kFailure;'
            assert old in source[begin:]
            changed=source[:begin]+source[begin:].replace(old,
                'if (!status || !outputMeshes) return MS::kFailure;',1)
            try:
                run(root,changed,suite='mesh_utility',capture=True,arguments=[fixture])
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name=='mesh_utility_tests','Compile failure: UV factory status'
                print('Detected mutation: UV factory status',flush=True)
            else:
                raise AssertionError('Undetected mutation: UV factory status')
