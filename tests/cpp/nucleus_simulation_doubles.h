#pragma once

#include "nucleus_particle_doubles.h"
#include "common/utility/DParticleFieldMath.h"

// Extend existing particle/API doubles with a field iterator and checked arrays.
// Native Maya force generation and solver ownership remain integration gates.
struct MVector {
    double x,y,z;
    MVector(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
    bool operator==(const MVector& p)const{return x==p.x&&y==p.y&&z==p.z;}
    static const MVector zero;
};
const MVector MVector::zero;
template<class T> struct FieldArray:std::vector<T> {
    using std::vector<T>::vector;
    unsigned length()const{return this->size();}
    T& operator[](unsigned i){assert(i<this->size());return this->at(i);}
    const T& operator[](unsigned i)const{assert(i<this->size());return this->at(i);}
    MStatus setLength(unsigned n){
        const int i=callCount;const auto s=operation("field.array.resize");
        if(s){this->resize(n);if(i==shortArrayAt&&!this->empty())this->pop_back();}return s;
    }
    MStatus copy(const FieldArray& other){
        const int i=callCount;const auto s=operation("field.array.copy");
        if(s){*this=other;if(i==shortArrayAt&&!this->empty())this->pop_back();}return s;
    }
    MStatus set(const T& value,unsigned i){
        const auto s=operation("field.array.set");
        if(s){assert(i<this->size());(*this)[i]=value;}return s;
    }
};
using MPointArray=FieldArray<MPoint>;
using MVectorArray=FieldArray<MVector>;
using MDoubleArray=FieldArray<double>;
static std::vector<MVectorArray> fields;
static std::vector<MPointArray> sampledPositions;
static std::vector<MVectorArray> sampledVelocities;
static std::vector<MDoubleArray> sampledMasses;
static std::vector<double> sampledSteps;
namespace MFn {enum Type{kField};}
struct MItDag {
    enum Traversal{kDepthFirst};
    bool valid;unsigned index=0;
    MItDag(Traversal,MFn::Type,MStatus* s){*s=operation("field.iterator");valid=bool(*s);}
    bool isDone(MStatus* s){assert(valid);*s=operation("field.done");return index==fields.size();}
    MObject currentItem(MStatus* s){assert(valid&&index<fields.size());*s=operation("field.item");return {*s?text(index):"invalid"};}
    MStatus next(){assert(valid&&index<fields.size());const auto s=operation("field.next");if(s)++index;return s;}
};
struct MFnField {
    bool valid;unsigned index=0;
    MFnField(MObject object,MStatus* s){
        assert(object.name!="invalid");*s=operation("field.attach");valid=bool(*s);
        if(valid)index=std::stoul(object.name);
    }
    MStatus getForceAtPoint(const MPointArray& positions,const MVectorArray& velocities,
                           const MDoubleArray& masses,MVectorArray& forces,double step){
        assert(valid&&index<fields.size());const auto s=operation("field.force");
        if(s){forces=fields[index];sampledPositions.push_back(positions);
            sampledVelocities.push_back(velocities);sampledMasses.push_back(masses);sampledSteps.push_back(step);}
        return s;
    }
};
