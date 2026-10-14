#!/usr/bin/env python3
"""Exercise real mesh-node control flow against failure-injectable host boundaries.

The default test is Maya/OpenSubdiv independent. --opensubdiv also compiles the
real face refinement with pinned OpenSubdiv sources; no Maya DG is emulated.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NAMES = ('DMEdgeDisplace', 'DMSubDivide', 'DMFaceInOut', 'DMExplodeView',
         'DMTransform', 'DMMakePiece', 'DMMerge', 'DMDelete')
FOLDER = 'src/modules/mesh/DMesh/Utility/'


def bodies(root, initializers=None, overrides=None):
    overrides = overrides or {}
    traversal = (root / 'src/core/common/utility/DMayaMeshTraversal.h').read_text()
    result = [extract(traversal, 'namespace dommy {')]
    for name in NAMES:
        source = overrides.get(name, (root / (FOLDER + name + '.cpp')).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();'
                      'MStatus compute(const MPlug&,MDataBlock&);'
                      'MObject thisMObject()const{return {"node"};}'
                      'MString name()const{return "' + name + '";}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(initializers[name] if initializers else source,
                              'MStatus ' + name + '::initialize('))
        if name == 'DMSubDivide':
            result += ['#ifdef DOMMY_TEST_REAL_OPENSUBDIV', extract(source, 'struct Vertex {') + ';',
                       extract(source, 'MStatus refineFaces('), '#else',
                       'MStatus refineFaces(RefinementItem& mesh,MObject object,'
                       'const dommy::mesh::SubdivisionBudget&,std::size_t,int,float,'
                       'Sdc::SchemeType,const Sdc::Options&) {'
                       'DOMMY_RETURN_IF_FAILURE(api("faces.refine"));'
                       'return mesh.setReplaceMeshData(object)?MS::kSuccess:MS::kFailure;}', '#endif',
                       extract(source, 'MStatus checkRefinedMeshCounts('),
                       extract(source, 'MStatus refineEdges(')]
            assert source.index('appendSubdivisionBudget') < source.index('output->copyFrom')
            assert source.index('validMeshOperationStorage(inputBudget)') < source.index('loopObjects[index]')
        if name in NAMES[:3]:
            result.append(extract(source, 'MStatus ' + name + '::compute(') +
                          '\nDOMMY_COMPUTE_CATCH("' + name + '")')
        if name == 'DMMakePiece':
            start = source.index('const MDataHandle groupHandle')
            end = source.index(';', source.index('groupHandle.asString()', start)) + 1
            block = source[start:end]
            # Keep the production attribute spelling, including historical names.
            for attr in attrs:
                block = re.sub(r'(?<!::)\b' + attr + r'\b', 'DMMakePiece::' + attr, block)
            result.append('MStatus makePieceReader(MDataBlock& data){MStatus status;' + block +
                          '(void)pieceName;return MS::kSuccess;}')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    subdiv = dependency / 'OpenSubdiv-3b5c3d99d29ba078385013d15f8b2ebeac0daf45'
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    extra = []
    paths = list(NAMES)
    if (subdiv / 'opensubdiv/far/topologyRefiner.h').is_file():
        extra += ['-isystem', str(subdiv)]  # Before newer OpenSubdiv bundled with Maya.
    else:
        paths.remove('DMSubDivide')
        print('SKIP complete DMSubDivide SDK unit: pinned OpenSubdiv headers unavailable')
    if (seexpr / 'SeExpression.h').is_file():
        extra += ['-isystem', str(seexpr)]
    else:
        paths.remove('DMTransform')
        print('SKIP complete DMTransform SDK unit: pinned SeExpr headers unavailable')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas'] + extra + [
        '-isystem', str(Path(sdk) / 'include')] + [
        '-I' + str(path) for path, _, _ in os.walk(root / 'src')] + [
        '-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    for name in paths:
        path = str(root / (FOLDER + name + '.cpp'))
        subprocess.run(command + ['-fsyntax-only', path], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', path], check=True)
    print('Mesh refinement: ' + str(len(paths)) + ' complete SDK units passed syntax/analysis')


def opensubdiv_check(root, dependency):
    dependency = Path(dependency).resolve()
    version = (dependency / 'opensubdiv/version.h').read_text()
    if '#define OPENSUBDIV_VERSION v3_3_3' not in version:
        raise RuntimeError('Expected OpenSubdiv 3.3.3')
    with tempfile.TemporaryDirectory(prefix='dommy-mesh-refinement-') as directory:
        temporary = Path(directory)
        (temporary / 'mesh_refinement.inc').write_text(bodies(root))
        binary = temporary / 'mesh_refinement_tests'
        sources = sorted(str(p) for part in ('sdc', 'vtr', 'far')
                         for p in (dependency / 'opensubdiv' / part).glob('*.cpp'))
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-O1', '-g', '-DDOMMY_TEST_REAL_OPENSUBDIV',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-isystem', str(dependency),
            '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/mesh_refinement_tests.cpp')] + sources + ['-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary), str(root / 'tests/fixtures/mesh_refinement_initialization_contract.txt')],
                       check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                                          ('0' if sys.platform == 'darwin' else '1')))


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == '--opensubdiv':
        opensubdiv_check(root, sys.argv[2])
    else:
        run(root, bodies(root), suite='mesh_refinement', arguments=sys.argv[1:] or None)
