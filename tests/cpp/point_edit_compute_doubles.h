#pragma once
// Reuse the failure-injectable point ownership/DG boundary. Sorting, attribute
// remapping, selection parsing, direction math and the hash grid are production.
#include "copy_utility_compute_doubles.h"
#include "common/utility/DAttributeSort.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DDirectionMath.h"
#include "Utility/3d/DataStruct/DHashGrid.h"
#include <random>

using DVec3Data=Imath::V3f;
struct DPGroupData {using ConstPtr=std::shared_ptr<const Group>;};
struct EditString;
struct MStringArray {
    std::vector<EditString> values;
    unsigned int length()const{return static_cast<unsigned int>(values.size());}
    EditString operator[](unsigned int index)const;
};
struct EditString : CopyString {
    EditString()=default;
    EditString(const char* text){value=text;}
    EditString(const CopyString& text){value=text.value;}
    int index(char character)const {
        const auto i=value.find(character);return i==std::string::npos?-1:static_cast<int>(i);
    }
    EditString substring(int first,int last)const {
        return EditString(value.substr(first,last-first+1).c_str());
    }
    void split(char delimiter,MStringArray& output)const {
        output.values.clear();std::istringstream stream(value);std::string token;
        while(std::getline(stream,token,delimiter))if(!token.empty())output.values.emplace_back(token.c_str());
    }
};
inline EditString MStringArray::operator[](unsigned int index)const{return values.at(index);}
#undef MString
#define MString EditString

// Only the rotated Y key is consumed. This records the rotate-to-Y convention,
// not the unobserved X/Z output or Maya's native quaternion implementation.
struct MQuaternion {Imath::V3d direction;};
struct EditVector : Imath::V3d {
    EditVector(double x=0,double y=0,double z=0):Imath::V3d(x,y,z){}
    MQuaternion rotateTo(const EditVector& target)const {
        assert(target.x==0&&target.y==1&&target.z==0);
        return MQuaternion{Imath::V3d(x/length(),y/length(),z/length())};
    }
    EditVector rotateBy(const MQuaternion& q)const{return EditVector(x,dot(q.direction),z);}
};
#define MVector EditVector
static bool inputConnected=true;
struct EditPlug : MPlug {
    explicit EditPlug(const MObject& attr):MPlug(attr){}
    EditPlug(const MObject&,const MObject& attr):MPlug(attr){}
    bool isConnected(MStatus* status)const{*status=operation("input.connected");return inputConnected;}
};
#define MPlug EditPlug
