#!/usr/bin/env python3
"""Production force/range helpers against exact rational reference inputs."""
from fractions import Fraction
import math
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_normal_numeric_safety import scaled_fraction

ROOT = Path(__file__).resolve().parent.parent
ACCUMULATOR = 'src/core/common/utility/DPointMeanAccumulator.h'
FORCE = 'src/core/common/utility/DForceMath.h'
FIELD = 'src/core/common/utility/DParticleFieldMath.h'
REMAP = 'src/core/math/DRemapMath.h'
NUMERIC = 'src/core/common/utility/DNumericValidation.h'
HEADERS = (ACCUMULATOR, FORCE, FIELD, REMAP, NUMERIC,
           'src/core/math/DLinearInterpolation.h',
           'src/core/common/utility/DGoalMath.h', 'src/core/common/utility/DProductSum.h',
           'src/core/common/utility/DScaledProduct.h')

SOURCE = r'''
#include "common/utility/DGoalMath.h"
#include "common/utility/DParticleFieldMath.h"
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
int main(int argc,char** argv) {
    assert(argc==2); std::ifstream input(argv[1]); assert(input);
    char kind=0; unsigned int count=0; std::size_t index=0;
    while(input>>kind>>count) {
        assert(count>0U && count<=16U);
        std::array<double,16> a,b,c,d;
        std::uint64_t bits=0U; bool admitted=false; int expectedExponent=0,exponent=0;
        for(auto* values:{&a,&b,&c,&d})
            for(double& value:*values) { assert(input>>bits); value=fromBits(bits); }
        assert(input>>admitted>>bits>>expectedExponent);
        const double expected=fromBits(bits); double result=91.0; bool valid=false;
        std::feclearexcept(FE_ALL_EXCEPT);
        if(kind=='Q') {
            dommy::numeric::detail::BinaryAccumulator<268U,-4296> sum;
            valid=true;
            for(unsigned int i=0U;i<count;++i)
                if(!sum.addProduct4(a[i],b[i],c[i],d[i])) {valid=false;break;}
            if(valid) sum.scaledValue(result,exponent);
        } else if(kind=='S') {
            valid=dommy::numeric::checkedSpringDampingResponse(a[0],a[1],a[2],a[3],a[4],a[5],a[6],result);
        } else if(kind=='G') {
            float candidate=91.0f;
            valid=dommy::point::computeGoalVelocityComponent(a[0],a[1],a[2],a[3],a[4],a[5],a[6],candidate);
            result=candidate;
        } else if(kind=='I') {
            valid=dommy::field::integrateFieldVelocity(a[0],a[1],a[2],a[3],result);
        } else if(kind=='P') {
            valid=dommy::field::computeNoiseFieldForceComponent(a[0],a[1],a[2],a[3],result);
        } else if(kind=='V') {
            valid=dommy::field::computeVectorFieldForceComponent(a[0],a[1],a[2],result);
        } else if(kind=='R') {
            valid=dommy::math::normalizeRange(a[0],a[1],a[2],result);
        } else if(kind=='N') {
            valid=dommy::numeric::normalizeRange(a[0],a[1],a[2],result);
        } else {
            assert(kind=='F'); float candidate=91.0f;
            valid=dommy::math::normalizeRange(float(a[0]),float(a[1]),float(a[2]),candidate);
            result=candidate;
        }
        if(valid!=admitted) { std::cerr<<"Admission mismatch "<<index<<' '<<kind<<'\n'; assert(false); }
        if(kind=='Q') {
            assert(toBits(result)==toBits(expected) && exponent==expectedExponent);
        } else if(!valid) {
            assert(result==expected);
        } else {
            const double tolerance=(kind=='F'||kind=='G')?3e-6:3e-14;
            assert(std::isfinite(result));
            if(std::fabs(result-expected)>std::fabs(expected)*tolerance+
                8*std::numeric_limits<double>::denorm_min()) {
                std::cerr<<"Value mismatch "<<index<<' '<<kind<<'\n'; assert(false);
            }
        }
        assert(std::fetestexcept(FE_OVERFLOW|FE_INVALID)==0); ++index;
    }
    assert(input.eof() && index>2000U);
    std::cout<<index<<" four-factor, force, integration and range oracle cases passed\n";
}
'''


def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def cases():
    rng = random.Random(2019202610024)

    def finite(single=False):
        width, mask, flip, fmt = (32, 0x7f800000, 0x00800000, '>f') if single else (
            64, 0x7ff0000000000000, 0x0010000000000000, '>d')
        value = rng.getrandbits(width)
        if value & mask == mask:
            value ^= flip
        return struct.unpack(fmt, value.to_bytes(width // 8, 'big'))[0]

    records = []
    for _ in range(512):
        products = [tuple(finite() for _ in range(4)) for _ in range(16)]
        if rng.randrange(2):
            products[1] = (-products[0][0],) + products[0][1:]
        records.append(('Q', products))
        records.append(('R', [finite() for _ in range(3)]))
        records.append(('F', [finite(True) for _ in range(3)]))
        records.append(('P', [finite(), finite(), finite(), abs(finite())]))
    for _ in range(256):
        records.append(('S', [1e200] + [finite() for _ in range(6)]))
        records.append(('I', [finite(), finite(), abs(finite()), abs(finite())]))
        records.append(('N', [finite() for _ in range(3)]))
    maximum = sys.float_info.max
    tiny = float.fromhex('0x0.0000000000001p-1022')
    for products in (
        [(maximum,)*4, (-maximum,maximum,maximum,maximum), (tiny,)*4],
        [(tiny,)*4], [(maximum,)*4],
        [(1,1,1,1),(2**-53,1,1,1),(tiny,)*4],
        [(0,math.inf,0,0)], [(math.nan,0,0,0)], [(0,0,0,math.inf)],
    ):
        records.append(('Q', products))
    for kind, inputs in (
        ('S',[0,1,1e308,0,2,1e-308,1]), ('S',[-1e308,1e308,0,1,0,.25,1]),
        ('S',[0,1,1e308,0,2,0,1]), ('S',[0,1,math.inf,0,2,0,1]),
        ('S',[1e308,1e308,1,1e308,1,1,-2]),
        ('G',[0,1,1e308,0,2,0,1]), ('G',[0,1,math.inf,0,2,0,1]),
        ('G',[-1e308,1e308,0,1e-308,0,1,-2]),
        ('I',[1,1e308,2,1e-308]), ('I',[-1e308,1e308,2,1]),
        ('I',[1,math.inf,0,1]), ('I',[1,1,-1,1]), ('I',[1,1,1,0]),
        ('P',[1e308,2,.25,1]), ('P',[math.inf,2,0,1]), ('P',[1,1,1,-1]),
        ('V',[-1e308,1e308,2]), ('V',[maximum,maximum,2]),
        ('R',[0,-maximum,maximum]), ('R',[maximum,-maximum,maximum]),
        ('R',[0,maximum,-maximum]), ('R',[maximum,0,1]),
        ('R',[maximum,0,tiny]), ('R',[0,0,0]), ('R',[0,math.inf,1]),
        ('R',[tiny,0,2*tiny]), ('R',[1,1,math.nextafter(1,math.inf)]),
        ('N',[0,-maximum,maximum]), ('N',[0,maximum,-maximum]),
        ('N',[0,0,1e-12]), ('N',[0,0,math.nextafter(1e-12,math.inf)]),
        ('F',[0,-finite_float_max(),finite_float_max()]),
    ):
        records.append((kind, inputs))
    rows = []
    for kind, inputs in records:
        arrays = [[], [], [], []]
        count, exponent, expected = 1, 0, (0.0 if kind in ('R','F') else 91.0)
        if kind == 'Q':
            count = len(inputs)
            arrays = list(map(list, zip(*inputs)))
            valid = all(math.isfinite(value) for product in inputs for value in product)
            if valid:
                exact = sum((math_product(product) for product in inputs), Fraction())
                expected, exponent = scaled_fraction(exact)
        else:
            arrays[0] = inputs
            valid = all(math.isfinite(value) for value in inputs)
            if kind == 'P':
                valid = valid and inputs[3] >= 0
            if kind == 'I':
                valid = valid and inputs[2] >= 0 and inputs[3] > 0
            if kind in ('R','F','N'):
                valid = valid and inputs[1] != inputs[2]
                if kind == 'N':
                    valid = valid and Fraction(inputs[2])-Fraction(inputs[1]) > Fraction(1e-12)
            if valid:
                p = list(map(Fraction, inputs))
                if kind in ('S','G'):
                    exact = ((p[1]-p[0])*p[3]-p[2]*p[4])*p[5]*p[6]
                elif kind == 'I':
                    exact = p[0]+p[1]*p[2]*p[3]
                elif kind == 'P':
                    exact = math_product(inputs)
                elif kind == 'V':
                    exact = p[0]+p[1]*p[2]
                else:
                    exact = (p[0]-p[1])/(p[2]-p[1])
                limit = finite_float_max() if kind in ('F','G') else maximum
                if abs(exact) > Fraction(limit):
                    valid = False
                else:
                    expected = float(exact)
                    if kind in ('F','G'):
                        expected = struct.unpack('>f', struct.pack('>f', expected))[0]
        padded = [values+[0.0]*(16-len(values)) for values in arrays]
        rows.append(' '.join([kind,str(count)]+[str(bits(v)) for values in padded for v in values]+
                             [str(int(valid)),str(bits(expected)),str(exponent)]))
    return '\n'.join(rows)+'\n'


def finite_float_max():
    return float.fromhex('0x1.fffffep127')


def math_product(values):
    result = Fraction(1)
    for value in values:
        result *= Fraction(value)
    return result


def run(overrides=None, capture=False):
    overrides = overrides or {}
    with tempfile.TemporaryDirectory(prefix='dommy-force-range-') as directory:
        temp = Path(directory)
        for relative in HEADERS:
            path = temp/Path(relative).relative_to('src/core')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(overrides.get(relative, (ROOT/relative).read_text()))
        source = temp/'test.cpp'; source.write_text(SOURCE)
        fixture = temp/'cases.txt'; fixture.write_text(cases()); binary = temp/'test'
        flags = ['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
                 '-I'+str(temp),'-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'tests/stubs'),
                 '-I'+str(ROOT/'src/core/common/utility'),
                 '-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        subprocess.run(shlex.split(os.environ.get('CXX','clang++'))+flags+
                       [str(source),'-o',str(binary)], check=True, capture_output=capture, text=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        return subprocess.run([str(binary),str(fixture)],capture_output=capture,text=True,env=environment)


def mutations():
    variants = []
    for label, path, old, new in (
        ('fourth product sign ignored',ACCUMULATOR,
         'std::signbit(third) ^ std::signbit(fourth)', 'std::signbit(third) ^ false'),
        ('four-factor carry truncated',ACCUMULATOR,
         'static_cast<std::uint32_t>(term >> 32U)', 'static_cast<std::uint32_t>(term >> 33U)'),
        ('zero conceals invalid fourth factor',ACCUMULATOR,
         '!std::isfinite(third) || !std::isfinite(fourth)', '!std::isfinite(third)'),
        ('spring final mass omitted',FORCE,'sum.addProduct4(target, spring, scale, mass)',
         'sum.addProduct4(target, spring, scale, 1.0)'),
        ('integration time factor omitted',FORCE,'sum.addProduct3(force, inverseMass, deltaTime)',
         'sum.addProduct3(force, inverseMass, 1.0)'),
        ('disabled force hides nonfinite input',FORCE,'if (!std::isfinite(input)) return false;',
         'if (false && !std::isfinite(input)) return false;'),
        ('noise intermediate overflow restored',FIELD,
         'return numeric::scaledProduct(std::array<double, 4>{{velocity, noise, magnitude, mass}}, result);',
         'result=velocity*noise*magnitude*mass; return std::isfinite(result);'),
        ('ascending range policy removed',NUMERIC,'rangeEnd <= rangeStart',
         'rangeEnd == rangeStart'),
        ('range final exponent admission removed',REMAP,
         'if (exponent > maximumExponent ||\n        (exponent == maximumExponent && std::fabs(fraction) > maximumFraction))',
         'if (false && (exponent > maximumExponent || (exponent == maximumExponent && std::fabs(fraction) > maximumFraction)))'),
    ):
        source = (ROOT/path).read_text(); assert old in source, label
        variants.append((label,{path:source.replace(old,new,1)}))
    source = (ROOT/REMAP).read_text(); original = extract(source,'inline bool normalizeRange(')
    unsafe = '''inline bool normalizeRange(const T value,const T minimum,const T maximum,T& normalized) {
        const T span=maximum-minimum; const T candidate=(value-minimum)/span;
        if(!std::isfinite(candidate)||span==T(0)){normalized=T(0);return false;}
        normalized=candidate;return true;
    }'''
    variants.append(('unscaled range subtraction restored',{REMAP:source.replace(original,unsafe,1)}))
    for label, overrides in variants:
        result = run(overrides,capture=True)
        assert result.returncode != 0, 'Undetected mutation: '+label
        assert any(marker in result.stderr for marker in
                   ('Assertion failed','Assertion ','runtime error:','ERROR: AddressSanitizer')), result.stderr
        print('Detected mutation: '+label,flush=True)


if __name__ == '__main__':
    if '--mutations' in sys.argv:
        mutations()
    else:
        result = run()
        sys.exit(result.returncode)
