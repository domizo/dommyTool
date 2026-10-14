#!/usr/bin/env python3
"""Execute production Curl field bodies with failure-injectable API doubles.

This checks control flow and real Perlin math, not Maya's DG or ramp behavior.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / 'src/modules/particles/particle/field/DCurlNoiseField.cpp').read_text()
    for required in ('kMaximumPositionNoiseFrequency = noise::kMaxPerlinOctaves',
                     'setMax(kMaximumPositionNoiseFrequency)', 'perlin.tryNoise(',
                     'falloffCurve(ratio, &status)', 'dommy::maya::publishFieldForces('):
        if required not in source:
            raise SystemExit('Missing Curl field guard: ' + required)
    for forbidden in (r'\bapply(?:NoMaxDist|MaxDist)\(', r'\bperlin\.noise\(',
                      r'\bfloat\s+(?:vx|vy|vz)', r'\bsetZeroForces\('):
        if re.search(forbidden, source):
            raise SystemExit('Unsafe/duplicated Curl field path: ' + forbidden)
    helpers = source[source.index('namespace {'):source.index('MObject DCurlNoiseField::curlSpeed;')]
    numeric_input = (root / 'src/core/common/utility/DMayaNumericInput.h').read_text()
    field_data = (root / 'src/core/common/utility/DMayaFieldData.h').read_text()
    helpers = (extract(field_data, 'namespace dommy {') + '\n' +
               'namespace dommy { namespace maya {\n' +
               extract(numeric_input, 'template <typename DataBlock, typename Attribute>') +
               '\n} }\n' + helpers)
    signatures = ('calculateForces', 'ownerPosition', 'getWorldPosition', 'compute')
    bodies = [extract(source, 'MStatus DCurlNoiseField::' + name + '(') for name in signatures]
    # The function-try boundary is covered by the project-wide entry-point guard
    # tests; this fixture injects returned API failures into the unmodified body.
    bodies[-1] = bodies[-1].replace(') try {', ') {', 1)
    with tempfile.TemporaryDirectory(prefix='dommy-curl-field-') as directory:
        temporary = Path(directory)
        (temporary / 'curl_field_methods.inc').write_text(helpers + '\n\n'.join(bodies))
        binary = temporary / 'curl_field_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'src'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/curl_field_safety_tests.cpp'),
            str(root / 'src/core/noise/PerlinNoise.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)


if __name__ == '__main__':
    main()
