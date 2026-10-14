#!/usr/bin/env python3
"""DNoise's complete initializer, frozen schema and failure/exception retries."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/deformers/Deformer/noiseDeformer.cpp'


def initializer(root, override=None):
    source = override if override is not None else (root / SOURCE).read_text()
    result = [extract((root / 'src/modules/deformers/Deformer/noiseDeformer.h').read_text(),
                      'enum {') + ';',
              'const int kMaxNoiseOctaves = noise::kMaxPerlinOctaves;']
    parts = declarations('noiseDeformer', source, True)
    parts[0] = parts[0].replace('static MStatus initialize();',
                               'static MStatus initialize();static MObject outputGeom;')
    result.extend(parts)
    result.append('MObject noiseDeformer::outputGeom{"outputGeom"};')
    result.append(extract(source, 'MStatus noiseDeformer::initialize('))
    return '\n\n'.join(result)


def mutations(root):
    body = initializer(root)
    for label, old, new in (
        ('create status ignored', 'DOMMY_RETURN_IF_FAILURE(status);', '(void)status;'),
        ('setter status ignored', 'DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));', '(void)nAttr.setDefault(1);'),
        ('enum field status ignored', 'DOMMY_RETURN_IF_FAILURE(enumFn.addField("Constant", DT_DROPOFF_CONSTANT));',
         '(void)enumFn.addField("Constant", DT_DROPOFF_CONSTANT);'),
        ('attribute registration status ignored', 'DOMMY_RETURN_IF_FAILURE(addAttribute(seed));', '(void)addAttribute(seed);'),
        ('dependency status ignored', 'DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::seed, noiseDeformer::outputGeom));',
         '(void)attributeAffects(noiseDeformer::seed, noiseDeformer::outputGeom);'),
        ('initializer exception escapes', 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
        ('default schema changed', 'nAttr.setDefault(1.0)', 'nAttr.setDefault(2.0)'),
    ):
        assert old in body, label
        try:
            run(root, body.replace(old, new), capture=True, suite='noise_deformer_init')
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'noise_deformer_init_tests', 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    (label == 'initializer exception escapes' and 'uncaught exception' in error.stderr)), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    source = '''#include "noiseDeformer.h"
#include "PerlinNoise.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
const int kMaxNoiseOctaves = noise::kMaxPerlinOctaves;
''' + extract((root / SOURCE).read_text(), 'MStatus noiseDeformer::initialize(')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags + ['-x', 'c++', '-'], input=source, text=True, check=True)
    print('DNoise: complete initializer with actual class/SDK headers passed syntax/analysis; no full node link/runtime')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run(root, initializer(root), suite='noise_deformer_init', arguments=['--emit'])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    else:
        run(root, initializer(root), suite='noise_deformer_init')
