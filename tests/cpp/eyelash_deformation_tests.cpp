// Production bend/mapping/publication bodies with failure-injectable host doubles.
// The deterministic rotation below checks control flow and arithmetic ordering;
// it does not claim to reproduce Maya's matrix implementation or DG evaluation.
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DCurveBatchBudget.h"
#include <array>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

static int copies=0, implicitCopies=0, failCopy=-1, shortCopy=-1, throwCopy=-1;
static int operations=0, failOperation=-1, matrixCalls=0, admissions=0, publications=0;
static bool badMatrix=false, badPoint=false, throwCurve=false;
static std::size_t transforms=0;
static std::array<double,3> mappedBends, addedRotation;
struct MPoint {
    double x,y,z,w;
    MPoint(double a=0,double b=0,double c=0):x(a),y(b),z(c),w(1){}
    bool operator==(const MPoint& p) const {return x==p.x&&y==p.y&&z==p.z&&w==p.w;}
};
struct MVector { MVector(double,double,double){} };
struct MPointArray {
    std::vector<MPoint> data;
    MPointArray()=default;
    MPointArray(const MPointArray& other):data(other.data){++implicitCopies;}
    MPointArray& operator=(const MPointArray& other){++implicitCopies;data=other.data;return *this;}
    unsigned length() const {return static_cast<unsigned>(data.size());}
    MPoint& operator[](unsigned i){return data.at(i);}
    const MPoint& operator[](unsigned i)const{return data.at(i);}
    MStatus copy(const MPointArray& other){
        const int call=copies++;
        if(call==throwCopy)throw std::bad_alloc();
        if(call==failCopy){data.clear();return MS::kFailure;}
        data=other.data;
        if(call==shortCopy&&!data.empty())data.pop_back();
        return MS::kSuccess;
    }
};
struct MDoubleArray:std::vector<double>{
    using std::vector<double>::vector;
    unsigned length()const{return static_cast<unsigned>(size());}
    double operator[](unsigned i)const{return at(i);}
};
namespace MSpace {enum Space{kObject};}
struct MMatrix {
    double entries[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    MPoint pivot;
    double angle=0;
    const double* operator[](unsigned i)const{return entries[i];}
};
MStatus operation(){return operations++==failOperation?MS::kFailure:MS::kSuccess;}
struct MTransformationMatrix {
    enum RotationOrder{kXYZ};
    MMatrix matrix;
    MStatus setRotatePivot(const MPoint& p,MSpace::Space,bool){matrix.pivot=p;return operation();}
    MStatus setToRotationAxis(const MVector&,double angle){matrix.angle=angle;return operation();}
    MMatrix asMatrix()const{
        ++matrixCalls;
        MMatrix result=matrix;
        if(badMatrix)result.entries[0][0]=std::numeric_limits<double>::quiet_NaN();
        return result;
    }
    MStatus addRotation(const double* rotation,RotationOrder,MSpace::Space){
        for(unsigned i=0;i<3;++i)assert(std::isfinite(rotation[i]));
        const MStatus status=operation();
        if(status)addedRotation={{rotation[0],rotation[1],rotation[2]}};
        return status;
    }
};
MPoint operator*(const MPoint& p,const MMatrix& m){
    ++transforms;
    if(badPoint)return MPoint(std::numeric_limits<double>::infinity(),0,0);
    const double y=p.y-m.pivot.y,z=p.z-m.pivot.z;
    return MPoint(p.x,m.pivot.y+y*std::cos(m.angle)-z*std::sin(m.angle),
                  m.pivot.z+y*std::sin(m.angle)+z*std::cos(m.angle));
}
struct MGlobal {static void displayError(const char*){}};
struct DCurveInternalData {
    std::vector<MPoint> points;
    std::vector<double> knots;
    unsigned degree;
    DCurveInternalData(const MPointArray& p,const MDoubleArray& k,unsigned d)
        :points(p.data),knots(k.begin(),k.end()),degree(d){
        if(throwCurve)throw std::bad_alloc();
    }
    bool hasValidCurveTopology()const{
        if(points.size()<=degree||knots.size()!=points.size()+degree-1U)return false;
        for(const MPoint& p:points)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
        return true;
    }
    std::size_t numberOfPoints()const{return points.size();}
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const {
        return hasValidCurveTopology() && budget.append(points.size(),points.size()+degree-1U,0U,0U);
    }
};
using DCItem=std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    using DCurveList=std::vector<DCItem>;
    DCurveList curves;
    bool fail=false,throwOnInsert=false;
    unsigned calls=0;
    bool insert(const DCurveList& input){
        ++calls;
        if(throwOnInsert)throw std::bad_alloc();
        if(fail)return false;
        curves=input;
        return true;
    }
};
#include "eyelash_deformation.inc"

void reset(){
    copies=implicitCopies=operations=matrixCalls=admissions=publications=0;
    failCopy=shortCopy=throwCopy=failOperation=-1;
    badMatrix=badPoint=throwCurve=false;transforms=0;
    mappedBends=addedRotation={{7,8,9}};
    std::feclearexcept(FE_ALL_EXCEPT);
}
MPointArray points(unsigned count){
    MPointArray result;
    for(unsigned i=0;i<count;++i)result.data.push_back(MPoint(i*.1,i*.25,-i*.125));
    return result;
}
void testBend(){
    for(unsigned n:{2U,3U,10U,100U})for(double angle:{0.0,-.125,1.75}){
        MPointArray actual=points(n);
        std::vector<MPoint> expected=actual.data;
        MDoubleArray weights(n,0.5);
        for(unsigned i=0;i<n;++i)weights.at(i)=static_cast<double>(i)*.125;
        // Frozen historical pivot/point order, against the same host matrix double.
        for(unsigned i=0;i+1U<n;++i){
            MMatrix matrix;matrix.pivot=expected[i];matrix.angle=angle*weights[i]*.75;
            for(unsigned j=i+1U;j<n;++j)expected[j]=expected[j]*matrix;
        }
        reset();assert(deformBendWithCvs(angle,.75,weights,actual));
        assert(actual.data==expected&&copies==2&&implicitCopies==0);
        assert(transforms==static_cast<std::size_t>(n)*(n-1U)/2U);
    }
    for(int at:{0,1})for(bool silent:{false,true}){
        MPointArray actual=points(4);const auto saved=actual.data;
        reset();if(silent)shortCopy=at;else failCopy=at;
        assert(!deformBendWithCvs(.5,1,MDoubleArray(4,1),actual));
        assert(copies==at+1&&implicitCopies==0);
        if(at==0)assert(actual.data==saved&&operations==0&&transforms==0);
        // A failed final native copy is allowed to leave a partial destination.
    }
    for(int at:{0,1}){
        MPointArray actual=points(4);reset();throwCopy=at;
        bool threw=false;
        try{(void)deformBendWithCvs(.5,1,MDoubleArray(4,1),actual);}
        catch(const std::bad_alloc&){threw=true;}
        assert(threw&&copies==at+1); // node compute catches native exceptions
    }
    for(int at=0;at<6;++at){
        MPointArray actual=points(4);const auto saved=actual.data;
        reset();failOperation=at;
        assert(!deformBendWithCvs(.5,1,MDoubleArray(4,1),actual));
        assert(actual.data==saved&&copies==1&&operations==at+1);
    }
    for(bool matrix:{false,true}){
        MPointArray actual=points(4);const auto saved=actual.data;
        reset();badMatrix=matrix;badPoint=!matrix;
        assert(!deformBendWithCvs(.5,1,MDoubleArray(4,1),actual));
        assert(actual.data==saved&&copies==1);
    }
    for(double bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        MPointArray actual=points(4);reset();
        assert(!deformBendWithCvs(bad,1,MDoubleArray(4,1),actual)&&copies==0);
        assert(!deformBendWithCvs(1,bad,MDoubleArray(4,1),actual)&&copies==0);
        assert(!deformBendWithCvs(0,1,MDoubleArray(4,bad),actual)&&copies==0);
        actual[0].x=bad;
        assert(!deformBendWithCvs(0,1,MDoubleArray(4,1),actual)&&copies==0);
    }
    MPointArray actual=points(4);reset();
    assert(!deformBendWithCvs(1,1,MDoubleArray(2,1),actual)&&copies==0);
    assert(!deformBendWithCvs(std::numeric_limits<double>::max(),2,MDoubleArray(4,1),actual));
    assert(operations==0);
}
using WeightsProbe=MStatus(*)(const std::array<float,3>&,const std::array<float,3>&,
                              const std::array<double,3>&,const std::array<double,3>&);
void testMappings(){
    for(WeightsProbe probe:{nativeWeights,customWeights}){
        for(int i=-100;i<=100;++i){
            const float value=static_cast<float>(i)*.1234567F;
            const std::array<float,3> weights={{value,value*.25F,-value}};
            const std::array<double,3> root={{45,-90,180}},random={{.125,-.5,2}};
            reset();assert(probe(weights,weights,root,random));assert(admissions==1&&operations==1);
            for(unsigned j=0;j<3;++j){
                const double expectedBend=(weights[j]*2)-1.0;
                const float expectedRotation=(weights[j]*2.0)-1.0;
                assert(mappedBends[j]==expectedBend);
                assert(addedRotation[j]==dommy::math::degreesToRadians(root[j]*expectedRotation+random[j]));
            }
        }
        const std::array<float,3> normal={{.5F,.5F,.5F}};
        const std::array<double,3> root={{45,90,180}},zero={{0,0,0}};
        for(unsigned axis=0;axis<3;++axis){
            for(float bad:{std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
                auto weights=normal;weights[axis]=bad;
                reset();assert(!probe(weights,normal,root,zero));assert(operations==0&&admissions==0);
                reset();assert(!probe(normal,weights,root,zero));assert(operations==0&&admissions==0);
                // Reject before an out-of-range double-to-float conversion,
                // not merely after the later angle guard observes infinity.
                assert(std::fetestexcept(FE_OVERFLOW)==0);
            }
            auto weights=normal;weights[axis]=2;
            auto large=root;large[axis]=1e308;
            reset();assert(!probe(normal,weights,large,zero));assert(operations==0&&admissions==0);
            weights[axis]=1;
            auto noise=zero;noise[axis]=1e308;
            reset();assert(!probe(normal,weights,large,noise));assert(operations==0&&admissions==0);
        }
        reset();failOperation=0;assert(!probe(normal,normal,root,zero));
        assert(admissions==0&&operations==1);
    }
    using dommy::generation::remapEyelashBendWeight;
    double output=7;
    assert(!remapEyelashBendWeight(std::numeric_limits<float>::max(),output)&&output==7);
    const float edge=std::numeric_limits<float>::max()/2;
    assert(remapEyelashBendWeight(edge,output));assert(output==(edge*2)-1.0);
    assert(!remapEyelashBendWeight(std::nextafter(edge,std::numeric_limits<float>::max()),output));
}
void testBatch(){
    std::vector<MPointArray> samples{points(4),points(7),points(5)};
    DCurveData output;reset();assert(customBatch(samples,&output));
    assert(output.calls==1&&publications==1&&output.curves.size()==samples.size());
    for(unsigned i=0;i<samples.size();++i){
        assert(output.curves[i]->points==samples[i].data&&output.curves[i]->degree==3);
        assert(output.curves[i]->knots.size()==samples[i].length()+2U);
    }
    const auto saved=output.curves;
    for(unsigned i=0;i<samples.size();++i){
        auto invalid=samples;invalid[i].data[0].x=std::numeric_limits<double>::infinity();
        reset();output.calls=0;assert(!customBatch(invalid,&output));
        assert(publications==0&&output.calls==0&&output.curves==saved);
    }
    reset();output.calls=0;output.fail=true;assert(!customBatch(samples,&output));
    assert(publications==0&&output.calls==1&&output.curves==saved);
    output.fail=false;output.throwOnInsert=true;reset();output.calls=0;
    assert(!customBatch(samples,&output)&&publications==0&&output.calls==1&&output.curves==saved);
    output.throwOnInsert=false;reset();throwCurve=true;output.calls=0;
    assert(!customBatch(samples,&output)&&publications==0&&output.calls==0&&output.curves==saved);
    reset();std::vector<MPointArray> many(1000,points(4));output.calls=0;
    assert(customBatch(many,&output)&&output.calls==1&&output.curves.size()==1000);
}
int main(){testBend();testMappings();testBatch();std::cout<<"Eyelash checked copies, historical math, finite API arguments and single batch publication passed\n";}
