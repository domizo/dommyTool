// Production arithmetic and native-call boundaries. Host vector rotation and
// Perlin sampling below are deterministic doubles, not Maya geometry emulation.
#include "common/utility/DNumericValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
using uint = unsigned int;
struct MPoint {
    double x, y, z, w;
    MPoint(double a=0, double b=0, double c=0):x(a),y(b),z(c),w(1){}
};
struct MVector;
struct MFloatPoint {
    float x, y, z, w;
    MFloatPoint(double a=0, double b=0, double c=0):x(a),y(b),z(c),w(1){}
    void setCast(const MVector& value);
};
struct MQuaternion { double factor; explicit MQuaternion(double f=1):factor(f){} };
struct MTransformationMatrix { enum RotationOrder { kYZX }; };
struct MVector {
    double x, y, z;
    enum Axis {kYaxis};
    MVector(double a=0, double b=0, double c=0):x(a),y(b),z(c){}
    MVector(const MFloatPoint& p):x(p.x),y(p.y),z(p.z){}
    double operator[](unsigned i) const {return i==0?x:i==1?y:z;}
    MVector operator+(const MVector& b) const {return {x+b.x,y+b.y,z+b.z};}
    MVector operator-(const MVector& b) const {return {x-b.x,y-b.y,z-b.z};}
    MVector operator*(double b) const {return {x*b,y*b,z*b};}
    MVector& operator+=(const MVector& b) {*this=*this+b;return *this;}
    MVector& operator/=(double b) {x/=b;y/=b;z/=b;return *this;}
    double length() const {return std::sqrt(x*x+y*y+z*z);}
    MVector rotateBy(Axis, double a) const {return {x*std::cos(a)+z*std::sin(a),y,z*std::cos(a)-x*std::sin(a)};}
    MVector rotateBy(const double* angles, MTransformationMatrix::RotationOrder) const {
        return rotateBy(kYaxis, angles[0]+angles[1]+angles[2]);
    }
    MVector rotateBy(const MQuaternion& q) const {return *this*q.factor;}
    static const MVector zero;
};
const MVector MVector::zero;
void MFloatPoint::setCast(const MVector& v) {x=v.x;y=v.y;z=v.z;}
template <typename T> struct Array:std::vector<T> {
    using std::vector<T>::vector;
    MStatus append(T value) {this->push_back(value);return MS::kSuccess;}
    unsigned length() const {return this->size();}
    T& operator[](std::size_t index) {return this->at(index);}
};
using MFloatPointArray=Array<MFloatPoint>;
using MFloatArray=Array<float>;
using MIntArray=Array<int>;
struct MGlobal {static void displayError(const char*){}};
namespace noise {namespace module {
struct Perlin {
    void SetSeed(int){} void SetFrequency(int){} void SetOctaveCount(int){}
    double GetValue(double x,double y,double z) const {
        assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));return 0.5;
    }
};
}}
struct BubbleSettings {
    int sideVal=4, divideVal=1, midDiv=0, seedVal=1;
    int arcNoiseFrequencyVal=1, heightNoiseFrequencyVal=1;
    double radiusVal=3, arcNoiseAmplitudeVal=0, heightNoiseAmplitudeVal=0;
    double heightNoisePowerVal=1, growthVal=1;
};
static MFloatPointArray bubbleOutput;
static int calls=0, failAt=-1, admissions=0;
static std::vector<double> distances, parameters;
MStatus operation() {return calls++==failAt?MS::kFailure:MS::kSuccess;}
namespace MSpace {enum Space{kWorld};}
const double kMFnNurbsEpsilon=1e-6;
struct Curve {
    double start=0, end=1, lengthValue=2, returnedParameter=0.5, closestParameter=0.5;
    MStatus getKnotDomain(double& a,double& b){a=start;b=end;return operation();}
    double length(double,MStatus* s){*s=operation();return lengthValue;}
    double findParamFromLength(double distance,MStatus* s){
        assert(std::isfinite(distance)&&distance>=0);distances.push_back(distance);
        *s=operation();return returnedParameter;
    }
    MStatus getPointAtParam(double parameter,MPoint& point,MSpace::Space){
        assert(std::isfinite(parameter)&&parameter>=start&&parameter<=end);
        parameters.push_back(parameter);point=MPoint();return operation();
    }
    MPoint closestPoint(MPoint,double* parameter,double,MSpace::Space,MStatus* s){
        *parameter=closestParameter;*s=operation();return MPoint();
    }
};
static Curve crvFn, tanCrvFn;
#include "generator_numerics.inc"

static void reset() {calls=admissions=0;failAt=-1;crvFn=Curve();tanCrvFn=Curve();distances.clear();parameters.clear();}
static void testTransforms() {
    for(double scale:{0.0,-1.25,1.0,123.456})for(double offset:{0.0,-100.25,1e8}) {
        const MVector s(scale,scale*.25,-scale),r(.1,.2,.3),o(offset,offset*.1,-offset);
        const MPoint position(3,5,7);const MQuaternion y(.875),x(1.125);
        const MFloatPoint source(1.2345678,-2.7890123,9.8765432);MFloatPoint expected(source),actual;
        expected.x*=s[0];expected.y*=s[1];expected.z*=s[2];
        double angles[]={r[0],r[1],r[2]};expected.setCast(MVector(expected).rotateBy(angles,MTransformationMatrix::kYZX));
        expected.setCast(MVector(expected).rotateBy(y).rotateBy(x));
        expected.x+=position.x+o[0];expected.y+=position.y+o[1];expected.z+=position.z+o[2];
        assert(transformCurveDuplicatePoint(source,s,r,y,x,position,o,actual));
        assert(actual.x==expected.x&&actual.y==expected.y&&actual.z==expected.z);
    }
    for(double bad:{1e40,-1e40,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        MFloatPoint output(7,8,9);
        assert(!transformCurveDuplicatePoint(MFloatPoint(1,2,3),MVector(bad,1,1),MVector(),MQuaternion(),MQuaternion(),MPoint(),MVector(),output));
        assert(output.x==7&&output.y==8&&output.z==9);
        assert(!transformCurveDuplicatePoint(MFloatPoint(1,2,3),MVector(1,1,1),MVector(),MQuaternion(),MQuaternion(),MPoint(),MVector(bad,0,0),output));
        assert(output.x==7&&output.y==8&&output.z==9);
    }
    MFloatPoint output;
    assert(!transformCurveDuplicatePoint(MFloatPoint(1,2,3),MVector(1,1,1),MVector(),MQuaternion(1e40),MQuaternion(),MPoint(),MVector(),output));
}
static void testSampling() {
    reset();assert(samplingProbe(3,1,.25));assert((distances==std::vector<double>{0,1,2}));
    assert((parameters==std::vector<double>{.75,.75,.75}));
    const int operations=calls;
    for(int at=0;at<operations;++at){reset();failAt=at;assert(!samplingProbe(3,1,.25));assert(admissions==0&&calls==at+1);}
    for(double offset:{-100.0,100.0}) {reset();assert(samplingProbe(1,0,offset));assert(distances[0]==0&&parameters[0]==(offset<0?0:1));}
    reset();assert(!samplingProbe(2,std::numeric_limits<double>::max(),0));assert(distances.empty());
    for(double bad:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        reset();crvFn.lengthValue=bad;assert(!samplingProbe(2,1,0)&&distances.empty());
    }
    reset();crvFn.end=crvFn.start;assert(!samplingProbe(2,1,0)&&distances.empty());
    reset();crvFn.start=std::numeric_limits<double>::quiet_NaN();assert(!samplingProbe(2,1,0)&&distances.empty());
    reset();crvFn.returnedParameter=2;assert(!samplingProbe(2,1,0)&&parameters.empty());
    reset();crvFn.end=std::numeric_limits<double>::max();crvFn.returnedParameter=crvFn.end;
    assert(!samplingProbe(2,1,crvFn.end)&&parameters.empty());
    reset();tanCrvFn.end=0;assert(!samplingProbe(2,1,0,true,2)&&distances.empty());
    reset();assert(tangentProbe(0,1));tanCrvFn.closestParameter=2;assert(!tangentProbe(0,1));
    tanCrvFn.closestParameter=std::numeric_limits<double>::quiet_NaN();assert(!tangentProbe(0,1));
}
static void testBubble() {
    BubbleSettings settings;assert(bubbleProbe(settings));assert(bubbleOutput.size()==9);
    for(const MFloatPoint& p:bubbleOutput) assert(std::isfinite(p.x)&&p.y==0&&std::isfinite(p.z));
    assert(bubbleOutput[1].z==1.5f&&bubbleOutput[5].z==3.0f);
    settings.heightNoiseAmplitudeVal=2;settings.heightNoisePowerVal=2;
    assert(bubbleProbe(settings));assert(bubbleOutput[1].y==.25f&&bubbleOutput[5].y==1.f);
    for(double bad:{1e40,std::numeric_limits<double>::infinity()}) {
        settings=BubbleSettings();settings.heightNoiseAmplitudeVal=bad;assert(!bubbleProbe(settings));
        settings=BubbleSettings();settings.radiusVal=bad;assert(!bubbleProbe(settings));
        settings=BubbleSettings();settings.arcNoiseAmplitudeVal=bad;assert(!bubbleProbe(settings));
    }
    settings=BubbleSettings();settings.midDiv=2;settings.divideVal=3;assert(bubbleProbe(settings));assert(bubbleOutput.size()==49);
    settings.radiusVal=0;assert(bubbleProbe(settings));
}
static void testBudget() {
    using namespace dommy::generation;
    EyelashGenerationPlan plan;
    assert(buildEyelashGenerationPlan(20,8,nullptr,0,plan));
    assert(plan.estimatedBytes==20U*2048U+200U*96U+10U*512U);
    assert(plan.bendWorkItems==20U*10U*9U);
    const std::size_t saved=plan.estimatedBytes;
    assert(!buildEyelashGenerationPlan(1000000,2,nullptr,0,plan)&&plan.estimatedBytes==saved);
    for(int segments:{2,8,100,1000,5000}) {
        const std::size_t points=segments+2U;
        const std::size_t storageCopies=(kMaxEyelashGenerationBytes-points*512U)/(2048U+points*96U);
        const std::size_t workCopies=kMaxWorkItemCount/(points*(points-1U));
        const std::size_t copies=std::min(storageCopies,workCopies);
        assert(copies>=2&&copies<=1000000);
        assert(buildEyelashGenerationPlan(static_cast<int>(copies),segments,nullptr,0,plan));
        assert(plan.estimatedBytes==copies*(2048U+points*96U)+points*512U);
        assert(plan.bendWorkItems==copies*points*(points-1U));
        assert(!buildEyelashGenerationPlan(static_cast<int>(copies+1U),segments,nullptr,0,plan));
    }
    const EyelashGenerationPlan savedPlan=plan;
    for(int segments:{10000,100000,1000000,std::numeric_limits<int>::max()}) {
        assert(!buildEyelashGenerationPlan(2,segments,nullptr,0,plan));
        assert(plan.estimatedBytes==savedPlan.estimatedBytes&&plan.bendWorkItems==savedPlan.bendWorkItems);
    }
    // Below the byte limit, the second curve must still fit the aggregate work limit.
    assert(buildEyelashGenerationPlan(10101,98,nullptr,0,plan));
    assert(plan.bendWorkItems==99999900U);
    assert(!buildEyelashGenerationPlan(10102,98,nullptr,0,plan));
    assert(!buildEyelashGenerationPlan(2,1000000,nullptr,0,plan));
}
int main(){testTransforms();testSampling();testBubble();testBudget();std::cout<<"Generator numeric boundaries, historical rounding, sampling admission and Eyelash storage/work passed\n";}
