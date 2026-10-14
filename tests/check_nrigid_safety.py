#!/usr/bin/env python3
"""Execute actual nRigid entry points with injectable, bounds-checked host doubles.

No Nucleus simulation or native object lifetime is emulated here.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODE = 'src/modules/mesh/DMesh/Dynamics/nObject/DMnRigidGen'


def bodies(root):
    source = (root / (NODE + '.cpp')).read_text()
    header = (root / (NODE + '.h')).read_text()
    data = (root / 'src/modules/mesh/DMesh/Data/DMeshData.cpp').read_text()
    result = ['namespace dommy { namespace maya {',
              extract((root / 'src/core/common/utility/DMayaMeshImport.h').read_text(),
                      'inline MStatus preflightMeshImport('), '}}',
              extract(data, 'MStatus DMeshInternalData::copyTopology('),
              extract(source, 'namespace {'),
              extract(header, 'class DMnRigidGen final') + ';']
    result += ['MObject DMnRigidGen::' + name + ';' for name in
               re.findall(r'^MObject DMnRigidGen::(\w+);', source, re.M)]
    for signature in ('DMnRigidGen::DMnRigidGen()', 'MStatus DMnRigidGen::initialize(',
                      'MStatus DMnRigidGen::compute('):
        result.append(extract(source, signature))
    result.append('DOMMY_COMPUTE_CATCH("DMnRigidGen")')
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    includes = ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'),
        '-isystem', str(root / 'third_party/libnoise/source'),
        '-idirafter', str(root / 'tests/stubs')] + includes
    path = str(root / (NODE + '.cpp'))
    subprocess.run(command + ['-fsyntax-only', path], check=True)
    subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                              '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, path], check=True)
    print('Complete DMnRigidGen translation unit: Maya SDK syntax/analysis passed (no native execution)')


def mutations(root):
    source = bodies(root)
    run(root, source, suite='nrigid', arguments=[])
    cases = [
        ('point status', 'DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(points, MSpace::kObject));',
         '(void)meshFn.getPoints(points, MSpace::kObject);'),
        ('previous status', 'DOMMY_RETURN_IF_FAILURE(fNObject.getPositions(previous));',
         '(void)fNObject.getPositions(previous);'),
        ('velocity status', 'DOMMY_RETURN_IF_FAILURE(fNObject.setVelocities(velocities));',
         '(void)fNObject.setVelocities(velocities);'),
        ('point cardinality', 'if (points.length() != plan.points) return MS::kFailure;', ''),
        ('previous cardinality', 'if (previous.length() != plan.points) return MS::kFailure;', ''),
        ('topology status', 'DOMMY_RETURN_IF_FAILURE(fNObject.setTopology(', '(void)(fNObject.setTopology('),
        ('edge status', 'DOMMY_RETURN_IF_FAILURE(meshFn.getEdgeVertices(i, edge));',
         '(void)meshFn.getEdgeVertices(i, edge);'),
        ('creation status', 'DOMMY_RETURN_IF_FAILURE(creationStatus_);', ''),
        ('partial state trusted', '\n        topologyInitialized_ = false;', ''),
        ('initializer status', 'DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));',
         '(void)tAttr.setReadable(false);'),
    ]
    for label, old, new in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), suite='nrigid', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'nrigid_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, bodies(root), suite='nrigid', arguments=[])
