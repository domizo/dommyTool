#!/usr/bin/env python3
"""Exercise production affine math against exact rational binary64 results."""
import math
import itertools
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile
from fractions import Fraction

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
TRANSFORM = 'src/core/common/utility/DPointTransform.h'
PRODUCT = 'src/core/common/utility/DProductSum.h'
ACCUMULATOR = 'src/core/common/utility/DPointMeanAccumulator.h'

PREFIX = r'''
#include "common/utility/DPointTransform.h"
#include <maya/MMatrix.h>
#include <array>
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
double fromBits(std::uint64_t bits) { double value; std::memcpy(&value,&bits,sizeof(value)); return value; }
std::uint64_t toBits(double value) { std::uint64_t bits; std::memcpy(&bits,&value,sizeof(bits)); return bits; }
'''
MAIN = r'''
int main(int argc,char** argv) {
    assert(argc==2);
    testExtremeAffineProductSums();
    std::ifstream input(argv[1]); assert(input);
    char kind=0; unsigned int count=0;
    std::size_t index=0;
    while(input>>kind>>count) {
        assert(count==16U);
        std::array<double,16> left,right;
        std::uint64_t bits=0U,expectedBits=0U; bool expected=false;
        for(double& value:left) { assert(input>>bits); value=fromBits(bits); }
        for(double& value:right) { assert(input>>bits); value=fromBits(bits); }
        assert(input>>expected>>expectedBits);
        std::feclearexcept(FE_ALL_EXCEPT);
        double result=91.0; bool valid=true;
        if(kind=='S') valid=dommy::numeric::checkedProductSum(left,right,result);
        else {
            assert(kind=='M');
            dommy::numeric::detail::PointMeanAccumulator sum;
            for(double value:left) if(!sum.add(value)) { valid=false; break; }
            if(valid) valid=sum.mean(count,result);
        }
        if(valid!=expected || toBits(result)!=expectedBits) {
            std::cerr<<"Exact oracle mismatch "<<index<<' '<<kind<<" valid="<<valid
                     <<" bits="<<toBits(result)<<" expected="<<expectedBits<<'\n';
            assert(false);
        }
        assert(std::fetestexcept(FE_OVERFLOW|FE_INVALID)==0);
        ++index;
    }
    assert(input.eof() && index>1000U);
    std::cout<<index<<" exact product-sum/mean cases and affine failure regressions passed\n";
}
'''

def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def cases():
    rng = random.Random(20192026)

    def finite():
        value = rng.getrandbits(64)
        if value & 0x7ff0000000000000 == 0x7ff0000000000000:
            value ^= 0x0010000000000000
        return struct.unpack('>d', struct.pack('>Q', value))[0]

    result = []
    maximum = sys.float_info.max
    tiny = float.fromhex('0x0.0000000000001p-1022')
    for _ in range(512):
        result.append(('S', [(math.ldexp(rng.uniform(-1.0, 1.0), rng.randrange(-80, 81)),
                              math.ldexp(rng.uniform(-1.0, 1.0), rng.randrange(-80, 81)))
                             for _ in range(16)]))
    for order in itertools.permutations((1e16, 1.0, -1e16, 10000.0)):
        result.append(('S', [(value, 1.0) for value in order] + [(0.0, 1.0)] * 12))
    for _ in range(768):
        pairs = [(finite(), finite()) for _ in range(16)]
        # Force the exact path; other randomly generated terms exercise carry,
        # sign, wide exponents and finite/nonrepresentable final magnitudes.
        pairs[0] = (maximum, finite())
        if rng.randrange(2):
            pairs[1] = (-pairs[0][0], pairs[0][1])
        result.append(('S', pairs))
        values = [(finite(), 0.0) for _ in range(16)]
        if rng.randrange(2):
            values[1] = (-values[0][0], 0.0)
        result.append(('M', values))
    for pairs in (
        [(maximum, maximum), (-maximum, maximum), (tiny, 1.0)],
        [(tiny, 0.5), (tiny, tiny)],
        [(tiny, 0.5), (-tiny, tiny)],
        [(maximum, 1.0)], [(maximum, float.fromhex('0x1.fffffffffffffp-1'))],
        [(maximum, 2.0)] * 16,
        [(0.0, math.inf)], [(math.nan, 0.0)],
    ):
        result.append(('S', pairs + [(0.0, 1.0)] * (16 - len(pairs))))
    rows = []
    for kind, pairs in result:
        left, right = zip(*pairs)
        admitted = all(math.isfinite(value) for value in left)
        if kind == 'S':
            admitted = admitted and all(math.isfinite(value) for value in right)
        expected = 91.0
        if admitted:
            exact = sum((Fraction(a) * Fraction(b) if kind == 'S' else Fraction(a)
                         for a, b in pairs), Fraction())
            if kind == 'M':
                exact /= 16
            try:
                expected = float(exact)
            except OverflowError:
                admitted = False
                expected = 91.0
        rows.append(' '.join([kind, '16'] + [str(bits(v)) for v in left] +
                             [str(bits(v)) for v in right] +
                             [str(int(admitted)), str(bits(expected))]))
    return '\n'.join(rows) + '\n'


def run(overrides=None, capture=False):
    overrides = overrides or {}
    regression = extract((ROOT/'tests/cpp/expression_sort_tests.cpp').read_text(),
                         'void testExtremeAffineProductSums(')
    with tempfile.TemporaryDirectory(prefix='dommy-affine-numeric-') as directory:
        temp = Path(directory)
        # Stage the transitive numeric dependency beside the overridden headers
        # so pragma-once never sees both original and copied accumulators.
        for relative in (TRANSFORM, PRODUCT, ACCUMULATOR,
                         'src/core/common/utility/DNumericValidation.h'):
            destination = temp/Path(relative).relative_to('src/core')
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(overrides.get(relative, (ROOT/relative).read_text()))
        source = temp/'test.cpp'
        source.write_text(PREFIX + regression + MAIN)
        fixture = temp/'cases.txt'
        fixture.write_text(cases())
        binary = temp/'test'
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-I'+str(temp),'-I'+str(ROOT/'src/core'),
                 '-I'+str(ROOT/'src/core/common/utility'),'-I'+str(ROOT/'tests/stubs'),
                 '-fsanitize=address,undefined,float-cast-overflow',
                 '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        subprocess.run(shlex.split(os.environ.get('CXX','clang++'))+flags+
                       [str(source),'-o',str(binary)], check=True, capture_output=capture, text=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        return subprocess.run([str(binary),str(fixture)], capture_output=capture,
                              text=True, env=environment)


def mutations():
    transform = (ROOT/TRANSFORM).read_text()
    point = extract(transform,'bool checkedTransformPoint(')
    direction = extract(transform,'bool checkedTransformDirection(')
    pivot = extract(transform,'bool composeWorldPivotTransform(')
    variants = []
    point_old = point.replace('double value = 0.0;',
        'double value = left[0]*right[0]+left[1]*right[1]+left[2]*right[2]+right[3];').replace(
        '!checkedProductSum(left, right, value) || ', '')
    variants.append(('point double product overflow restored', {TRANSFORM:transform.replace(point,point_old,1)}))
    direction_old = direction.replace('double value = 0.0;',
        'long double value = static_cast<long double>(left[0])*right[0]+static_cast<long double>(left[1])*right[1]+static_cast<long double>(left[2])*right[2];').replace(
        '!checkedProductSum(left, right, value) || ', '')
    variants.append(('direction long-double dependence restored', {TRANSFORM:transform.replace(direction,direction_old,1)}))
    pivot_old = pivot.replace('if (!checkedProductSum(left, right, value)) return false;',
        'value=0.0; for(std::size_t k=0U;k<left.size();++k) value+=left[k]*right[k];')
    variants.append(('pivot product overflow restored', {TRANSFORM:transform.replace(pivot,pivot_old,1)}))
    partial = point.replace('std::array<Scalar, 3> candidate;', 'std::array<Scalar, 3> candidate = output;').replace(
        'return false;', 'return false; output = candidate;')
    variants.append(('partial point output published', {TRANSFORM:transform.replace(point,partial,1)}))
    product = (ROOT/PRODUCT).read_text()
    variants.append(('extreme exact fallback disabled', {PRODUCT:product.replace('if (!exact) {','if (true) {')}))
    cancellation = 'exact = errorBound >= 0.5 * std::min(below, above);'
    assert cancellation in product
    variants.append(('rounding-error admission disabled', {PRODUCT:product.replace(cancellation,
        'exact = false; (void)below; (void)above;')}))
    accumulator = (ROOT/ACCUMULATOR).read_text()
    for label, old, new in (
        ('product signs inverted','std::signbit(left) != std::signbit(right)',
         'std::signbit(left) == std::signbit(right)'),
        ('subnormal single rounding removed',
         'const unsigned int subnormalShift = static_cast<unsigned int>(smallestExponent - BaseExponent);',
         'const unsigned int subnormalShift = 0U; (void)smallestExponent;'),
        ('overflow checked after floating conversion',
         'if (significand != 0U &&\n            significandExponent + outputExponent > std::numeric_limits<Scalar>::max_exponent)',
         'if (false && significand != 0U && significandExponent + outputExponent > std::numeric_limits<Scalar>::max_exponent)'),
    ):
        assert old in accumulator, label
        variants.append((label,{ACCUMULATOR:accumulator.replace(old,new,1)}))
    for label, overrides in variants:
        result = run(overrides, capture=True)
        assert result.returncode != 0, 'Undetected mutation: '+label
        assert any(marker in result.stderr for marker in
                   ('Assertion failed', 'Assertion ', 'runtime error:', 'ERROR: AddressSanitizer')), result.stderr
        print('Detected mutation: '+label, flush=True)


if __name__ == '__main__':
    if '--mutations' in sys.argv:
        mutations()
    else:
        result = run()
        if result.returncode:
            raise SystemExit(result.returncode)
