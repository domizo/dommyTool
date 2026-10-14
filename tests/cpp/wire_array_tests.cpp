// Native Maya allocation/ownership is not emulated by these array seams.
#include <maya/MStatus.h>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

int calls=0, failAt=-1, throwAt=-1, shortAt=-1, publications=0;
MS::MStatusCode injectedStatus=MS::kFailure;
MStatus operation(bool publication=false) {
    if (publication) ++publications;
    const int point=calls++;
    if (point==throwAt) throw std::bad_alloc();
    return point==failAt ? injectedStatus : MS::kSuccess;
}
void reset(int failure=-1,int throwing=-1,int shortened=-1) {
    calls=publications=0;failAt=failure;throwAt=throwing;shortAt=shortened;
}
template <typename T> struct Array {
    std::vector<T> values;
    bool output=false;
    unsigned int length() const {return static_cast<unsigned int>(values.size());}
    T& operator[](unsigned int index) {return values.at(index);}
    const T& operator[](unsigned int index) const {return values.at(index);}
    MStatus setLength(unsigned int count) {
        const MStatus status=operation();
        if (status) values.resize(count-(calls-1==shortAt && count>0));
        return status;
    }
    MStatus copy(const Array& other) {
        const MStatus status=operation(output);
        // Native failed copy need not preserve its destination.
        if (!status) {values.assign(1,T());return status;}
        values=other.values;
        if (calls-1==shortAt && !values.empty()) values.pop_back();
        return status;
    }
};
using MIntArray=Array<int>;
using MFloatArray=Array<float>;
#include "DMayaWireTopology.h"

struct Outputs {
    MIntArray counts,indices,uvCounts,uvIndices;
    MFloatArray u,v;
    Outputs() {
        counts.values=indices.values=uvCounts.values=uvIndices.values={99};
        u.values=v.values={99};
        counts.output=indices.output=uvCounts.output=uvIndices.output=u.output=v.output=true;
    }
    bool untouched() const {
        return counts.values==std::vector<int>({99}) && indices.values==std::vector<int>({99}) &&
            uvCounts.values==std::vector<int>({99}) && uvIndices.values==std::vector<int>({99}) &&
            u.values==std::vector<float>({99}) && v.values==std::vector<float>({99});
    }
    bool equals(const dommy::mesh::WireTopologyPlan& plan) const {
        return counts.values==plan.faceCounts && indices.values==plan.faceVertexIndices &&
            uvCounts.values==plan.uvCounts && uvIndices.values==plan.uvIndices &&
            u.values==plan.uCoordinates && v.values==plan.vCoordinates;
    }
};
MStatus invoke(const dommy::mesh::WireTopologyPlan& plan,Outputs& out) {
    try {
        return dommy::maya::copyWireTopologyToMaya(
            plan,out.counts,out.indices,out.u,out.v,out.uvCounts,out.uvIndices);
    } catch (...) {return MS::kFailure;}
}
int main() {
    dommy::mesh::WireTopologyPlan plan;
    assert(dommy::mesh::checkedWireTopologyPlan(3,5,.25,-.5,2,-3,100,100,1000,plan));
    Outputs output;
    reset();
    assert(invoke(plan,output)==MS::kSuccess && output.equals(plan));
    assert(calls==18 && publications==6);
    for (MS::MStatusCode failure : {MS::kFailure,MS::kInvalidParameter}) {
        injectedStatus=failure;
        for (int point=0;point<18;++point) {
            Outputs rejected;
            reset(point);
            assert(invoke(plan,rejected)==failure && calls==point+1);
            if (point<12) assert(publications==0 && rejected.untouched());
            reset();
            assert(invoke(plan,rejected)==MS::kSuccess && rejected.equals(plan));
        }
    }
    for (int point=0;point<18;++point) {
        for (bool shortened : {false,true}) {
            Outputs rejected;
            reset(-1,shortened ? -1 : point,shortened ? point : -1);
            assert(invoke(plan,rejected)==MS::kFailure && calls==point+1);
            if (point<12) assert(publications==0 && rejected.untouched());
            reset();
            assert(invoke(plan,rejected)==MS::kSuccess && rejected.equals(plan));
        }
    }
    // A second valid plan replaces, rather than appends to, all six outputs.
    assert(dommy::mesh::checkedWireTopologyPlan(2,3,0,0,1,1,100,100,1000,plan));
    reset();assert(invoke(plan,output)==MS::kSuccess && output.equals(plan));
    dommy::mesh::WireTopologyPlan invalid;
    Outputs unchanged;
    reset();assert(invoke(invalid,unchanged)==MS::kInvalidParameter);
    assert(calls==0 && unchanged.untouched());
    std::cout << "Production wire arrays: exact six-array replacement, 18 failure/exception/short-array "
                 "points, pre-publication preservation and recovery passed (Maya storage seam)\n";
}
