#!/usr/bin/env python3
"""Range/blend/Signal helpers against exact rational original-input oracles."""
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

ROOT = Path(__file__).resolve().parent.parent
INTERPOLATION = 'src/core/math/DLinearInterpolation.h'
NUMERIC = 'src/core/common/utility/DNumericValidation.h'
SIGNAL = 'src/core/common/utility/DSignalMath.h'
HEADERS = (INTERPOLATION, NUMERIC, SIGNAL,
           'src/core/math/DRemapMath.h', 'src/core/common/utility/DAttributeBlendMath.h',
           'src/core/common/utility/DIntegerBlend.h', 'src/core/common/utility/DPointMeanAccumulator.h',
           'src/core/common/utility/DProductSum.h', 'src/core/common/utility/DScaledProduct.h')
SOURCE = r'''
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DSignalMath.h"
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
double fromBits(std::uint64_t bits) {double value; std::memcpy(&value,&bits,sizeof(value)); return value;}
int main(int argc,char** argv) {
    assert(argc==2); std::ifstream input(argv[1]); assert(input);
    char kind; std::uint64_t bits; bool admitted; std::size_t index=0;
    while(input>>kind) {
        std::array<double,6> a;
        for(auto& value:a) {assert(input>>bits); value=fromBits(bits);}
        assert(input>>admitted>>bits); const double expected=fromBits(bits);
        double result=91.0; bool valid=false; std::feclearexcept(FE_ALL_EXCEPT);
        if(kind=='L') valid=dommy::math::checkedLinearInterpolation(a[0],a[1],a[2],result);
        else if(kind=='M') valid=dommy::attribute::checkedMaskedLinearBlend(a[0],a[1],a[2],a[3],result);
        else if(kind=='F') {float value=91.0f;
            valid=dommy::attribute::checkedMaskedLinearBlend(float(a[0]),float(a[1]),a[2],a[3],value);result=value;}
        else if(kind=='P') valid=dommy::numeric::wrapPeriodic(a[0],a[1],a[2],result);
        else if(kind=='Q') valid=dommy::numeric::wrapPeriodicProductSum(
            std::array<double,3>{{a[0],a[1],a[3]}},std::array<double,3>{{1.0,a[2],1.0}},a[4],a[5],result);
        else if(kind=='S') valid=dommy::signal::outputValue(a[0],a[1],a[2],a[3],a[4],a[5],result);
        else if(kind=='W') valid=dommy::signal::weightMapPosition(a[0],a[1],int(a[2]),result);
        else if(kind=='R') valid=dommy::signal::weightMapDistance(a[0],std::size_t(a[1]),
            std::size_t(a[2]),a[3],a[4]!=0,result);
        else {assert(kind=='A');valid=dommy::signal::applyModulation(a[0],a[1],a[2],a[3],a[4],a[5]!=0,result);}
        if(valid!=admitted || (!valid && result!=91.0) ||
            (valid && (!std::isfinite(result) || std::fabs(result-expected)>
                std::fabs(expected)*(kind=='F'?2e-6:4e-14)+8*std::numeric_limits<double>::denorm_min())) ||
            std::fetestexcept(FE_OVERFLOW|FE_INVALID)!=0) {
            std::cerr<<"Mismatch "<<index<<' '<<kind<<" valid="<<valid<<" expected="<<admitted
                     <<" result="<<result<<" expected="<<expected<<'\n'; assert(false);
        }
        ++index;
    }
    std::cout<<index<<" exact-oracle Signal/blend/periodic cases passed\n";
}
'''


def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def cases():
    maximum = float.fromhex('0x1.fffffffffffffp1023')
    samples = [
        ('L', [1, 1, 1e16]), ('L', [-maximum, maximum, 0.5]),
        ('L', [-maximum, maximum, 0]), ('L', [-maximum, maximum, 1]),
        ('M', [1, 1, 1e308, 2]), ('M', [0, 1e-308, 1e308, 2]),
        ('M', [0, 1e308, 1e-300, 1e-300]),
        ('M', [1, 1, math.inf, 0]), ('M', [math.nan, 1, 0, 0]),
        ('F', [1, 1, 1e308, 2]), ('F', [0, f32(1e-30), 1e308, 1e-300]),
        ('P', [0, -1e308, 1e308]), ('P', [1e308, -1e308, 0]),
        ('P', [1.5e308, -1e308, 1e308]), ('P', [-1.5e308, -1e308, 1e308]),
        ('P', [2**60, 0.5, 3.5]), ('P', [-2**60, 0.5, 3.5]),
        ('P', [2**1000, -0.25, 2.75]), ('P', [-2**1000, -0.25, 2.75]),
        ('S', [-1e308, 1e308, 0, 0, 2, 1]),
        ('S', [0, 1e308, -1e308, 1, 2, 2]), ('S', [3, 1e308, 1e308, 2, 2, 0]),
        ('A', [5, 2, 1e308, 2, 0, 0]), ('A', [5, 2, 1e308, 2, 0, 1]),
        ('A', [0, math.inf, 1e308, 2, 0, 1]),
        ('W', [0.25, 2**60, 2]), ('W', [0.25, -2**60, 2]),
        ('W', [maximum, maximum, 0]), ('W', [maximum, maximum, 1]),
        ('W', [maximum, maximum, 2]), ('W', [math.nan, 0, 1]),
        ('W', [0, math.inf, 2]), ('W', [0, 0, 3]),
        ('R', [0.5, 0, 5, f32(0.2), 0]), ('R', [2**60, 1, 5, 1, 1]),
        ('R', [-2**60, 1, 5, 1, 1]), ('R', [maximum, 0, 2, 0.5, 1]),
        ('R', [0.25, 0, 2, maximum, 1]), ('R', [-maximum, 1, 2, maximum, 1]),
        ('R', [0.5, 0, 5, 2**-23, 1]), ('R', [0, 0, 0, 1, 0]),
        ('R', [0, 5, 5, 1, 1]), ('R', [math.nan, 0, 2, 1, 1]),
        ('Q', [.25, 2**60, 1, .125, 0, 1]),
        ('Q', [.25, -2**60, 1, .125, .5, 3.5]),
        ('Q', [.25, 2**52+1, 2**52+1, .125, .5, 3.5]),
        ('Q', [0, 2**52+1, 2**52+1, 0, 0, 3]),
        ('Q', [.25, maximum, maximum, 0, 0, 3]),
        ('Q', [.25, maximum, maximum, .125, -maximum, maximum]),
        ('Q', [0, 2**-1074, .5, 0, 0, 2**-1074]),
        ('Q', [0, math.inf, 0, 0, 0, 1]),
        ('Q', [0, 1, 1, 0, 1, 1]),
        ('Q', [0, 1, 1, 0, 1, 0]),
    ]
    rng = random.Random(20261002)
    def number():
        return rng.choice((-1,1))*math.ldexp(rng.uniform(0.5,1), rng.choice(
            (-1000,-500,-200,-10,0,10,200,500,1000)))
    for _ in range(650):
        a = [number() for _ in range(6)]
        samples.extend([('L', a[:3]), ('M', a[:4]), ('S', a), ('A', a[:5]+[rng.randrange(2)])])
        first, last = sorted((number(), number()))
        if last-first > 1e-12: samples.append(('P', [number(), first, last]))
        endpoints = [f32(rng.choice((-1,1))*math.ldexp(rng.uniform(0.5,1),
                     rng.choice((-120,-80,-20,0,20,80,120)))) for _ in range(2)]
        samples.append(('F', endpoints+[number(),number()]))
        samples.extend(('W', [number(), number(), mode]) for mode in (0,1,2))
        count=rng.randrange(1,65)
        samples.append(('R', [number(), rng.randrange(count), count,
                             abs(number()), rng.randrange(2)]))
        first,last=sorted((number(),number()))
        samples.append(('Q',[number(),number(),number(),number(),first,last]))
    rows = []
    for kind, values in samples:
        a = [float(value) for value in values]+[0.0]*(6-len(values))
        valid = all(math.isfinite(value) for value in a)
        expected = 91.0
        if valid:
            f = list(map(Fraction, a))
            if kind in ('L','M','F'):
                weight = f[2] if kind=='L' else f[2]*f[3]
                exact = f[0]+(f[1]-f[0])*weight
            elif kind=='P':
                span = f[2]-f[1]
                valid = span > Fraction(1e-12)
                exact = f[1]+(f[0]-f[1]) % span if valid else Fraction(0)
            elif kind=='Q':
                span=f[5]-f[4]
                valid=span>0
                exact=f[4]+(f[0]+f[1]*f[2]+f[3]-f[4]) % span if valid else Fraction(0)
            elif kind=='S':
                exact = f[0]+f[1]*f[4]*f[5]+f[2]*f[3]*f[4]*f[5]
            elif kind=='W':
                valid=a[2] in (0,1,2)
                exact=f[0]+f[1]
                if a[2]==1: exact=max(Fraction(0),min(Fraction(1),exact))
                elif a[2]==2: exact %= 1
            elif kind=='R':
                valid=a[3] > 2**-23 and a[2] > 0 and 0 <= a[1] < a[2]
                if valid:
                    part=Fraction(0 if a[2]==1 else a[1]/(a[2]-1))
                    delta=abs(f[0]-part)
                    if a[4]:
                        phase=delta % (2*f[3])
                        exact=min(phase,2*f[3]-phase)/f[3]
                    else: exact=min(Fraction(1),delta/f[3])
                else: exact=Fraction(0)
            else:
                term = f[1]*f[2]*f[3]*f[4]
                exact = f[0]*term if a[5] else f[0]+term
            if valid:
                try:
                    expected = float(exact)
                    if kind=='F': expected=f32(expected)
                    valid = math.isfinite(expected)
                except (OverflowError, struct.error): valid=False
                if valid and kind=='P' and expected >= a[2]: expected=a[1]
                if valid and kind=='Q' and expected >= a[5]: expected=a[4]
                if valid and kind=='W' and a[2]==2 and expected >= 1: expected=0.0
        if not valid: expected=91.0
        rows.append(' '.join([kind]+[str(bits(value)) for value in a]+
                             [str(int(valid)),str(bits(expected))]))
    return '\n'.join(rows)+'\n'


def run(overrides=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-signal-blend-') as directory:
        temp = Path(directory)
        for path in HEADERS:
            destination = temp / Path(path).relative_to('src/core')
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text((overrides or {}).get(path, (ROOT/path).read_text()))
        source=temp/'test.cpp'; source.write_text(SOURCE)
        fixture=temp/'cases.txt'; fixture.write_text(cases())
        for contraction in ('off','on'):
            binary=temp/('test_'+contraction)
            command=shlex.split(os.environ.get('CXX','clang++'))+[
                '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-ffp-contract='+contraction,
                '-I'+str(temp),'-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'tests/stubs'),
                '-I'+str(ROOT/'src/core/common/utility'),
                '-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all',
                str(source),'-o',str(binary)]
            subprocess.run(command,check=True,capture_output=capture,text=True)
            environment=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
            subprocess.run([str(binary),str(fixture)],check=True,env=environment,
                           capture_output=capture,text=True)


def mutations():
    variants = []
    replacements = (
        ('unsafe interpolation restored', INTERPOLATION, 'bool checkedLinearInterpolation(',
         'bool checkedLinearInterpolation(Value source,Value target,Weight weight,Value& result) '
         '{result=(1-weight)*source+weight*target;return std::isfinite(result);}'),
        ('masked intermediate product restored', INTERPOLATION, 'bool checkedMaskedLinearInterpolation(',
         'bool checkedMaskedLinearInterpolation(Value source,Value target,double weight,double mask,Value& result) '
         '{return checkedLinearInterpolation(source,target,weight*mask,result);}'),
        ('unsafe periodic span restored', NUMERIC, 'inline bool wrapPeriodic(',
         'inline bool wrapPeriodic(double value,double start,double end,double& result) '
         '{double span=end-start,offset=value-start;if(!std::isfinite(span)||!std::isfinite(offset))return false;'
         'result=start+std::fmod(offset,span);return true;}'),
        ('unsafe Signal chain restored', SIGNAL, 'inline bool outputValue(',
         'inline bool outputValue(double base,double mapped,double last,double cycles,double strength,double uniform,double& result)'
         '{result=base+mapped*strength*uniform+last*cycles*strength*uniform;return std::isfinite(result);}'),
        ('unsafe modulation restored', SIGNAL, 'inline bool applyModulation(',
         'inline bool applyModulation(double current,double sample,double amplitude,double strength,double uniform,bool multiply,double& result)'
         '{double value=sample*amplitude*strength*uniform;result=multiply?current*value:current+value;return std::isfinite(result);}'),
        ('WeightMap loop pre-addition restored', SIGNAL, 'inline bool weightMapPosition(',
         'inline bool weightMapPosition(double base,double offset,int mode,double& result)'
         '{double value=base+offset;if(!std::isfinite(base)||!std::isfinite(offset)||!std::isfinite(value)||mode<0||mode>2)return false;'
         'if(mode==2)return numeric::wrapPeriodic(value,0,1,result);'
         'result=mode==1?std::max(0.0,std::min(1.0,value)):value;return true;}'),
        ('WeightMap non-repeat endpoint clamp omitted', SIGNAL, 'inline bool weightMapDistance(',
         'inline bool weightMapDistance(double position,std::size_t index,std::size_t count,double distance,bool repeat,double& result)'
         '{if(!std::isfinite(position)||!std::isfinite(distance)||distance<=std::numeric_limits<float>::epsilon()||count==0||index>=count)return false;'
         'double value=std::fabs(position-(count==1?0.0:double(index)/double(count-1)))/distance;'
         'if(!std::isfinite(value))return false;'
         'if(repeat&&value>1){if(!numeric::wrapPeriodic(value,0,2,value))return false;if(value>1)value=2-value;}'
         'result=value;return true;}'),
        ('WeightMap repeated quotient rounding restored', SIGNAL, 'inline bool weightMapDistance(',
         'inline bool weightMapDistance(double position,std::size_t index,std::size_t count,double distance,bool repeat,double& result)'
         '{if(!std::isfinite(position)||!std::isfinite(distance)||distance<=std::numeric_limits<float>::epsilon()||count==0||index>=count)return false;'
         'double value=std::fabs(position-(count==1?0.0:double(index)/double(count-1)))/distance;'
         'if(!std::isfinite(value))return false;'
         'if(repeat&&value>1){if(!numeric::wrapPeriodic(value,0,2,value))return false;if(value>1)value=2-value;}'
         'result=std::min(1.0,value);return true;}'),
    )
    for label,path,signature,replacement in replacements:
        source=(ROOT/path).read_text(); original=extract(source,signature)
        variants.append((label,{path:source.replace(original,replacement,1)}))
    accumulator='src/core/common/utility/DPointMeanAccumulator.h'
    source=(ROOT/accumulator).read_text()
    broken=source.replace('i + shift / 32U, shift % 32U, divisor[i]',
                          'i + shift / 32U, 0U, divisor[i]')
    assert broken!=source
    variants.append(('periodic divisor bit alignment omitted',{accumulator:broken}))
    source=(ROOT/NUMERIC).read_text()
    for label,old,new in (
        ('periodic input product rounded','value.addProduct(left[i], right[i])','value.add(left[i] * right[i])'),
        ('periodic exact-addition certificate omitted',
         'detail::additionResidual(value, product, sum) == 0.0','true'),
        ('periodic exact-product certificate omitted','std::fma(left[i], right[i], -product) == 0.0','true'),
    ):
        assert old in source,label
        variants.append((label,{NUMERIC:source.replace(old,new)}))
    for label,overrides in variants:
        try: run(overrides,capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.startswith('test_'), 'Compile failure: '+label
            assert any(marker in error.stderr for marker in ('Assertion failed','Assertion ','runtime error:')), error.stderr
            print('Detected mutation: '+label,flush=True)
        else: raise AssertionError('Undetected mutation: '+label)


if __name__=='__main__':
    mutations() if '--mutations' in sys.argv else run()
