#!/usr/bin/env python3
"""Production normal/mean/product boundaries with rational and decimal oracles."""
from decimal import Decimal, localcontext
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
TRANSFORM = 'src/core/common/utility/DPointTransform.h'
ACCUMULATOR = 'src/core/common/utility/DPointMeanAccumulator.h'
SCALED = 'src/core/common/utility/DScaledProduct.h'
AVERAGE = 'src/modules/point/DPoint/math/DVectorNodeMath.h'
HEADERS = (TRANSFORM, ACCUMULATOR, SCALED, AVERAGE,
           'src/core/common/utility/DProductSum.h', 'src/core/math/DVectorNormalization.h',
           'src/core/common/utility/DNumericValidation.h',
           'src/core/common/utility/DAttributeValueConversion.h',
           'src/core/common/utility/DAttributeMathInput.h',
           'src/core/common/utility/DAttributeMathPublication.h')

PREFIX = r'''
#include "common/utility/DPointTransform.h"
#include "common/utility/DScaledProduct.h"
#include "math/DVectorNodeMath.h"
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
    assert(argc==2); testNormalAndScaledProductExtremes();
    std::ifstream input(argv[1]); assert(input);
    char kind=0; unsigned int count=0; std::size_t index=0;
    while(input>>kind>>count) {
        assert(count>0U && count<=16U);
        std::array<double,16> a,b,c;
        std::array<double,3> expected,result{{91,92,93}};
        std::uint64_t bits=0U; bool admitted=false; int exponent=0,expectedExponent=0;
        for(double& value:a) { assert(input>>bits); value=fromBits(bits); }
        for(double& value:b) { assert(input>>bits); value=fromBits(bits); }
        for(double& value:c) { assert(input>>bits); value=fromBits(bits); }
        assert(input>>admitted);
        for(double& value:expected) { assert(input>>bits); value=fromBits(bits); }
        assert(input>>expectedExponent); std::feclearexcept(FE_ALL_EXCEPT);
        bool valid=true;
        if(kind=='T') {
            dommy::numeric::detail::BinaryAccumulator<200U,-3222> sum;
            for(unsigned int i=0U;i<count;++i)
                if(!sum.addProduct3(a[i],b[i],c[i])) {valid=false;break;}
            if(valid) {
                sum.scaledValue(result[0],exponent);
                assert(sum.sign()==(expected[0]>0.0?1:(expected[0]<0.0?-1:0)));
            }
        } else if(kind=='A') {
            const DMathVec3Data source(a[0],a[1],a[2]),target(0,0,0);
            valid=dommy::math::vectorNodeValue(dommy::math::VectorNodeKind::Average,
                                              source,count,target,result[0]);
        } else if(kind=='P') {
            assert(count==16U); valid=dommy::numeric::scaledProduct(a,result[0]);
        } else {
            assert(kind=='N'); MMatrix matrix;
            for(unsigned int row=0U;row<3U;++row) for(unsigned int col=0U;col<3U;++col)
                matrix[row][col]=a[row*3U+col];
            valid=dommy::numeric::checkedTransformNormal(std::array<double,3>{{b[0],b[1],b[2]}},matrix,result);
        }
        assert(valid==admitted);
        if(kind=='N' && valid) {
            for(unsigned int axis=0U;axis<3U;++axis) {
                assert(std::isfinite(result[axis]));
                if(std::fabs(result[axis]-expected[axis])>3e-12) {
                    std::cerr<<"Normal oracle mismatch "<<index<<' '<<axis<<'\n'; assert(false);
                }
            }
        } else if(kind=='P' && valid && expected[0]!=0.0) {
            assert(std::isfinite(result[0]));
            assert(std::fabs(result[0]-expected[0])<=std::fabs(expected[0])*2e-14+
                8*std::numeric_limits<double>::denorm_min());
        } else {
            assert(toBits(result[0])==toBits(expected[0]));
            if(kind=='T' && valid) assert(exponent==expectedExponent);
            if(kind=='N' && !valid) assert(result[1]==92.0 && result[2]==93.0);
        }
        assert(std::fetestexcept(FE_OVERFLOW|FE_INVALID)==0); ++index;
    }
    assert(input.eof() && index>2000U);
    std::cout<<index<<" exact triple-product/mean, scaled-product and normal oracle cases passed\n";
}
'''

def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]


def power(exponent):
    return Fraction(1 << exponent) if exponent>=0 else Fraction(1,1 << -exponent)


def scaled_fraction(value):
    if not value:
        return 0.0,0
    absolute=abs(value)
    exponent=absolute.numerator.bit_length()-absolute.denominator.bit_length()+1
    if absolute<power(exponent-1):
        exponent-=1
    fraction=float(value/power(exponent))
    if abs(fraction)==1.0:
        fraction*=0.5; exponent+=1
    return fraction,exponent


def normal_reference(matrix,normal):
    m=[list(map(Fraction,matrix[row*3:row*3+3])) for row in range(3)]
    n=list(map(Fraction,normal))
    if not any(n):
        return True,[0.0]*3
    cofactors=[[m[(i+1)%3][(j+1)%3]*m[(i+2)%3][(j+2)%3]-
                m[(i+1)%3][(j+2)%3]*m[(i+2)%3][(j+1)%3]
                for j in range(3)] for i in range(3)]
    determinant=sum((m[0][j]*cofactors[0][j] for j in range(3)),Fraction())
    if not determinant:
        return False,[91.0,92.0,93.0]
    values=[sum((n[i]*cofactors[i][j] for i in range(3)),Fraction()) for j in range(3)]
    with localcontext() as context:
        context.prec=100
        values=[Decimal(v.numerator)/Decimal(v.denominator) for v in values]
        length=sum((v*v for v in values),Decimal()).sqrt()
        direction=1 if determinant>0 else -1
        return True,[float(v/length*direction) for v in values]


def cases():
    rng=random.Random(201920261002)
    def finite():
        value=rng.getrandbits(64)
        if value & 0x7ff0000000000000==0x7ff0000000000000:
            value^=0x0010000000000000
        return struct.unpack('>d',struct.pack('>Q',value))[0]
    records=[]
    for iteration in range(512):
        triples=[(finite(),finite(),finite()) for _ in range(16)]
        if iteration%2:
            triples[1]=(-triples[0][0],triples[0][1],triples[0][2])
        a,b,c=map(list,zip(*triples))
        exact=sum((Fraction(x)*Fraction(y)*Fraction(z) for x,y,z in triples),Fraction())
        fraction,exponent=scaled_fraction(exact)
        records.append(('T',16,a,b,c,True,[fraction,0.0,0.0],exponent))
        count=2+iteration%2; values=[finite() for _ in range(3)]
        if iteration%4==1:
            values=[1e308,finite(),-1e308]
        average=float(sum(map(Fraction,values[:count]),Fraction())/count)
        records.append(('A',count,values,[0.0]*3,[0.0]*3,True,[average,0.0,0.0],0))
        if iteration%2:
            matrix=[finite() for _ in range(9)]; normal=[finite() for _ in range(3)]
        else:
            matrix=[rng.uniform(-3,3) for _ in range(9)]; normal=[rng.uniform(-3,3) for _ in range(3)]
        if iteration%16==0:
            matrix[3:6]=matrix[0:3]
        valid,reference=normal_reference(matrix,normal)
        records.append(('N',3,matrix,normal,[0.0]*3,valid,reference,0))
        factors=[math.ldexp(rng.uniform(0.5,1.0),rng.randrange(-500,501))*rng.choice((-1,1)) for _ in range(16)]
        exact=Fraction(1)
        for factor in factors:
            exact*=Fraction(factor)
        try:
            expected=float(exact); valid=True
        except OverflowError:
            expected=91.0; valid=False
        records.append(('P',16,factors,[0.0]*3,[0.0]*3,valid,[expected,0.0,0.0],0))
    maximum=sys.float_info.max; tiny=float.fromhex('0x0.0000000000001p-1022')
    for delta in (1e-8, 1e-10, 1e-12):
        matrix=[1,2,3,4,5,7,4.2+delta,6-2*delta,8.6+3*delta]
        normal=[1+2*delta,4-delta,4.2+4*delta]
        valid,reference=normal_reference(matrix,normal)
        records.append(('N',3,matrix,normal,[],valid,reference,0))
    for _ in range(256):
        first=[rng.uniform(-2,2) for _ in range(3)]
        second=[rng.uniform(-2,2) for _ in range(3)]
        delta=rng.choice((1e-8,1e-10,1e-12))
        third=[a+0.8*b+delta*rng.uniform(-1,1) for a,b in zip(first,second)]
        matrix=first+second+third
        normal=[matrix[row*3]+delta*rng.uniform(-1,1) for row in range(3)]
        valid,reference=normal_reference(matrix,normal)
        records.append(('N',3,matrix,normal,[],valid,reference,0))
    for triples in (
        [(maximum,maximum,maximum),(-maximum,maximum,maximum),(tiny,tiny,tiny)],
        [(tiny,tiny,tiny)], [(maximum,maximum,maximum)],
        [(1.0,1.0,1.0),(2**-53,1.0,1.0),(tiny,tiny,tiny)],
    ):
        a,b,c=map(list,zip(*triples)); exact=sum((Fraction(x)*Fraction(y)*Fraction(z) for x,y,z in triples),Fraction())
        fraction,exponent=scaled_fraction(exact)
        records.append(('T',len(triples),a,b,c,True,[fraction,0.0,0.0],exponent))
    for values in ([1e308,1e-300,-1e308],[-1e308,-1e-300,1e308]):
        average=float(sum(map(Fraction,values),Fraction())/3)
        records.append(('A',3,values,[],[],True,[average,0.0,0.0],0))
    records.append(('T',1,[0.0],[math.inf],[0.0],False,[91.0,0.0,0.0],0))
    for matrix,normal in (
        ([1,1,0,1,1+sys.float_info.epsilon,0,0,0,1],[1,1,1]),
        ([tiny,0,0,0,maximum,0,0,0,1],[1,1,1]),
        ([-tiny,0,0,0,maximum,0,0,0,1],[1,1,1]),
        ([maximum,0,0,0,maximum,0,0,0,maximum],[maximum]*3),
        ([tiny,0,0,0,tiny,0,0,0,tiny],[1,1,1]),
        ([0]*9,[0]*3), ([0]*9,[0,0,1]),
    ):
        valid,reference=normal_reference(matrix,normal)
        records.append(('N',3,matrix,normal,[],valid,reference,0))
    rows=[]
    for kind,count,a,b,c,valid,expected,exponent in records:
        arrays=[values+[0.0]*(16-len(values)) for values in (a,b,c)]
        rows.append(' '.join([kind,str(count)]+[str(bits(v)) for values in arrays for v in values]+
                             [str(int(valid))]+[str(bits(v)) for v in expected]+[str(exponent)]))
    return '\n'.join(rows)+'\n'


def run(overrides=None,capture=False):
    overrides=overrides or {}
    regression=extract((ROOT/'tests/cpp/expression_sort_tests.cpp').read_text(),
                       'void testNormalAndScaledProductExtremes(')
    with tempfile.TemporaryDirectory(prefix='dommy-normal-numeric-') as directory:
        temp=Path(directory)
        for relative in HEADERS:
            destination=temp/Path(relative).relative_to('src')
            destination.parent.mkdir(parents=True,exist_ok=True)
            destination.write_text(overrides.get(relative,(ROOT/relative).read_text()))
        source=temp/'test.cpp'; source.write_text(PREFIX+regression+MAIN)
        fixture=temp/'cases.txt'; fixture.write_text(cases()); binary=temp/'test'
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
               '-I'+str(temp/'core'),'-I'+str(temp/'modules/point/DPoint'),
               '-I'+str(ROOT/'tests/stubs'),
               '-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'src/modules/point/DPoint'),
               '-I'+str(ROOT/'src/core/common/utility'),
               '-I'+str(ROOT/'src/modules/point/DPoint/math'),
               '-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer']
        subprocess.run(shlex.split(os.environ.get('CXX','clang++'))+flags+[str(source),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        environment=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        return subprocess.run([str(binary),str(fixture)],capture_output=capture,text=True,env=environment)


def mutations():
    variants=[]
    transform=(ROOT/TRANSFORM).read_text()
    old='if (exact && !detail::exactNormalDirection(normal, matrix, direction)) return false;'
    assert old in transform
    variants.append(('extreme normal fallback removed',{TRANSFORM:transform.replace(old,'if (exact) return false;')}))
    old='largestError > directionScale * 1.0e-13'
    assert old in transform
    variants.append(('normal direction error budget ignored',
        {TRANSFORM:transform.replace(old,'(false && largestError > directionScale * 1.0e-13)')}))
    for label,old,new in (
        ('exact reflection sign ignored','value *= determinantSign','value *= 1'),
        ('singular normal admitted','if (determinantSign == 0) return false;',
         'if (determinantSign == 0) { output={{0,0,1}}; return true; }'),
    ):
        assert old in transform,label; variants.append((label,{TRANSFORM:transform.replace(old,new,1)}))
    accumulator=(ROOT/ACCUMULATOR).read_text()
    for label,old,new in (
        ('triple product signs inverted',
         'std::signbit(first) ^ std::signbit(second) ^ std::signbit(third)',
         '!(std::signbit(first) ^ std::signbit(second) ^ std::signbit(third))'),
        ('scaled-value nearest-even rounding omitted',
         'if (bit(value, shift - 1U) && (sticky || (significand & 1U))) ++significand;',
         'if (false && bit(value, shift - 1U) && (sticky || (significand & 1U))) ++significand;'),
    ):
        assert old in accumulator,label; variants.append((label,{ACCUMULATOR:accumulator.replace(old,new,1)}))
    average=(ROOT/AVERAGE).read_text(); start=average.index('    case VectorNodeKind::Average: {')
    end=average.index('    case VectorNodeKind::Dot:',start)
    replacement='''    case VectorNodeKind::Average: {
        double scale=0.0,sum=0.0;
        for(unsigned int axis=0U;axis<dimensions;++axis) scale=std::max(scale,std::fabs(source[axis]));
        if(scale!=0.0) {for(unsigned int axis=0U;axis<dimensions;++axis) sum+=source[axis]/scale;value=scale*(sum/dimensions);}
        break;
    }
'''
    variants.append(('scaled component mean restored',{AVERAGE:average[:start]+replacement+average[end:]}))
    scaled=(ROOT/SCALED).read_text()
    for label,old,new in (
        ('unsafe fast multiplication restored',
         'if (productExponent > std::numeric_limits<double>::max_exponent ||\n            productExponent < std::numeric_limits<double>::min_exponent + 1)',
         'if (false && (productExponent > std::numeric_limits<double>::max_exponent || productExponent < std::numeric_limits<double>::min_exponent + 1))'),
        ('final exponent preflight removed',
         'if (exponent > std::numeric_limits<double>::max_exponent) return false;',
         'if (false && exponent > std::numeric_limits<double>::max_exponent) return false;'),
        ('subnormal scaled products discarded','exponent < -1074','exponent < -1021'),
        ('zero hides nonfinite factor','if (!std::isfinite(factor))','if (false && !std::isfinite(factor))'),
    ):
        assert old in scaled,label; variants.append((label,{SCALED:scaled.replace(old,new,1)}))
    for label,overrides in variants:
        result=run(overrides,capture=True)
        assert result.returncode!=0,'Undetected mutation: '+label
        assert any(marker in result.stderr for marker in
                   ('Assertion failed','Assertion ','runtime error:','ERROR: AddressSanitizer')),result.stderr
        print('Detected mutation: '+label,flush=True)


if __name__=='__main__':
    result=mutations() if '--mutations' in sys.argv else run()
    if result is not None and result.returncode:
        raise SystemExit(result.returncode)
