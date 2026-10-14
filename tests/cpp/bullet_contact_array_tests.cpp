// Execute the production contact methods with Maya's editable-array alias
// semantics and failure-injectable host operations. This is not a Maya DG test.
#include "maya/MStatus.h"
#include "common/utility/DNumericValidation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

using MString = std::string;
struct MVector {
    double x,y,z;
    MVector(double a=0,double b=0,double c=0):x(a),y(b),z(c) {}
};
namespace dommy { namespace maya {
#include "bullet_contact_value_validation.inc"
} namespace generation {
// Exercise the same count boundary without allocating millions of records.
const std::size_t kMaxGeneratedPointCount=3;
} }
namespace fixture {
int calls=0, failAt=0, writes=0, firstWrite=0, nullAttribute=0;
bool throwFailure=false;
MStatus operation() {
    if(++calls!=failAt)return MS::kSuccess;
    if(throwFailure)throw std::bad_alloc();
    return MS::kFailure;
}
void reset() {
    calls=failAt=writes=firstWrite=nullAttribute=0;
    throwFailure=false;
}
}
template<class T> struct HostArray {
    std::shared_ptr<std::vector<T>> values=std::make_shared<std::vector<T>>();
    MStatus append(const T& value) {
        const MStatus status=fixture::operation();
        if(status)values->push_back(value);
        return status;
    }
};
using MStringArray=HostArray<MString>;
using MVectorArray=HostArray<MVector>;
struct ArrayData { MStringArray strings; MVectorArray points; };
struct MObject { std::shared_ptr<ArrayData> value; };
namespace fixture {
MObject names,positions;
int count=0;
void seed() {
    reset();
    names={std::make_shared<ArrayData>()};positions={std::make_shared<ArrayData>()};
    names.value->strings.values->push_back("previous");
    positions.value->points.values->push_back(MVector(1,2,3));
    count=1;
}
MStatus write() {
    if(firstWrite==0)firstWrite=calls+1;
    const MStatus status=operation();
    if(status)++writes;
    return status;
}
}
struct MPlug {
    int attribute;
    MPlug(MObject,int a):attribute(a) {}
    bool isNull() const { return fixture::nullAttribute==attribute; }
    MStatus getValue(int& value) const { value=fixture::count;return fixture::operation(); }
    MStatus getValue(MObject& value) const {
        value=attribute==1?fixture::names:fixture::positions;
        return fixture::operation();
    }
    MStatus setValue(int value) const {
        const MStatus status=fixture::write();
        if(status)fixture::count=value;
        return status;
    }
    MStatus setValue(MObject value) const {
        const MStatus status=fixture::write();
        if(status)(attribute==1?fixture::names:fixture::positions)=value;
        return status;
    }
};
template<class Array, Array ArrayData::*Member> struct ArrayFunctionSet {
    MObject object;
    ArrayFunctionSet()=default;
    ArrayFunctionSet(MObject value,MStatus* status):object(value) { *status=fixture::operation(); }
    unsigned length(MStatus* status) const {
        *status=fixture::operation();
        return static_cast<unsigned>((object.value.get()->*Member).values->size());
    }
    Array array(MStatus* status) const {
        *status=fixture::operation();
        // A plain return/copy preserves the alias, as Maya's array() does.
        return object.value.get()->*Member;
    }
    MStatus copyTo(Array& result) const {
        const MStatus status=fixture::operation();
        if(status)*result.values=*(object.value.get()->*Member).values;
        return status;
    }
    MObject create(const Array& input,MStatus* status) const {
        *status=fixture::operation();
        if(!*status)return {};
        MObject result{std::make_shared<ArrayData>()};
        *(result.value.get()->*Member).values=*input.values;
        return result;
    }
};
using MFnStringArrayData=ArrayFunctionSet<MStringArray,&ArrayData::strings>;
using MFnVectorArrayData=ArrayFunctionSet<MVectorArray,&ArrayData::points>;
struct DBRigidBodyNode {
    enum {oa_contactName=1,oa_contactPosition=2,oa_contactCount=3};
    int m_contactCount=1;
    MObject thisMObject() const { return {}; }
    MStatus clearContactInfo();
    MStatus addContactInfo(const MString&,const MVector&);
};
#include "bullet_contact_arrays.inc"

static void checkOriginal(const MObject& names,const MObject& positions) {
    assert(names.value->strings.values->size()==1);
    assert((*names.value->strings.values)[0]=="previous");
    assert(positions.value->points.values->size()==1);
    const MVector& point=(*positions.value->points.values)[0];
    assert(point.x==1&&point.y==2&&point.z==3);
}
int main() {
    int injected=0;
    for(bool clear:{false,true}) {
        int operations,firstWrite;
        {
            fixture::seed();DBRigidBodyNode node;
            const auto oldNames=fixture::names,oldPositions=fixture::positions;
            assert(clear?node.clearContactInfo():node.addContactInfo("new",MVector(4,5,6)));
            operations=fixture::calls;firstWrite=fixture::firstWrite;
            const unsigned count=clear?0:2;
            assert(node.m_contactCount==int(count)&&fixture::count==int(count));
            assert(fixture::names.value->strings.values->size()==count);
            assert(fixture::positions.value->points.values->size()==count);
            checkOriginal(oldNames,oldPositions);
            if(!clear) {
                assert(fixture::names.value->strings.values->back()=="new");
                assert(fixture::positions.value->points.values->back().x==4);
            }
        }
        for(int failure=1;failure<=operations;++failure) for(bool throws:{false,true}) {
            fixture::seed();DBRigidBodyNode node;
            const auto oldNames=fixture::names,oldPositions=fixture::positions;
            fixture::failAt=failure;fixture::throwFailure=throws;
            bool caught=false;
            try {
                assert(!(clear?node.clearContactInfo():node.addContactInfo("new",MVector(4,5,6))));
            } catch(const std::bad_alloc&) { caught=true; }
            assert(caught==throws);
            checkOriginal(oldNames,oldPositions);
            assert(node.m_contactCount==1&&fixture::count==1);
            if(failure<=firstWrite) {
                assert(fixture::writes==0);
                assert(fixture::names.value==oldNames.value&&fixture::positions.value==oldPositions.value);
            }
            // Partial host publication is not rolled back, but a successful
            // clear must recover consistent empty data for the next append.
            fixture::failAt=0;fixture::throwFailure=false;
            assert(node.clearContactInfo());
            assert(node.addContactInfo("recovered",MVector(7,8,9)));
            assert(node.m_contactCount==1&&fixture::count==1);
            ++injected;
        }
    }
    for(int fault=0;fault<13;++fault) {
        fixture::seed();DBRigidBodyNode node;MVector point;
        if(fault==0)node.m_contactCount=-1;
        if(fault==1)node.m_contactCount=3;
        if(fault==2)fixture::count=0;
        if(fault==3)fixture::names.value->strings.values->push_back("extra");
        if(fault==4)fixture::positions.value->points.values->clear();
        if(fault>=5&&fault<=7)fixture::nullAttribute=fault-4;
        if(fault==8)point.x=std::numeric_limits<double>::infinity();
        if(fault==9)point.y=std::numeric_limits<double>::quiet_NaN();
        if(fault==10)point.z=std::numeric_limits<double>::max();
        if(fault==11)fixture::count=-1;
        if(fault==12) {
            fixture::names.value->strings.values->push_back("extra");
            fixture::positions.value->points.values->push_back(MVector());
        }
        assert(!node.addContactInfo("invalid",point)&&fixture::writes==0);
    }
    {
        fixture::seed();DBRigidBodyNode node;
        assert(node.clearContactInfo());
        for(int i=0;i<3;++i)assert(node.addContactInfo("bounded",MVector()));
        const int writes=fixture::writes;
        assert(!node.addContactInfo("over budget",MVector())&&fixture::writes==writes);
    }
    std::printf("Contact arrays: detached preparation, consistency, clear/recovery, limits and %d status/exception failures passed\n",injected);
}
