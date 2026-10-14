// Production polygon/UV/mesh validation; metadata is a cardinality boundary.
#include "common/utility/DMeshUVTransfer.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

static std::size_t connectionReads=0U;
template<typename T>struct Array:std::vector<T>{
    using std::vector<T>::vector;
    bool track=false;
    unsigned int length()const{return this->size();}
    const T& operator[](unsigned int i)const{if(track)++connectionReads;return this->at(i);}
    T& operator[](unsigned int i){if(track)++connectionReads;return this->at(i);}
};
struct Group {
    std::vector<bool> ids;
    explicit Group(std::size_t n):ids(n,false){}
    const std::vector<bool>& getGroupIds()const{return ids;}
};
using Groups=std::map<std::string,std::shared_ptr<Group>>;
using ConstDPGrpIter=Groups::const_iterator;
struct Matrix {bool finite=true;};
namespace dommy {namespace attribute {
struct ValidationBudget{std::size_t workItems;explicit ValidationBudget(std::size_t n):workItems(n){}};
bool isFiniteMatrix(const Matrix& m){return m.finite;}
bool validateAttributeList(const Groups& groups,std::size_t count,ValidationBudget& budget){
    for(const auto& g:groups)
        if(!g.second||g.second->ids.size()!=count||
           !generation::checkedSum(budget.workItems,count,generation::kMaxWorkItemCount,budget.workItems))return false;
    return true;
}
}}
struct DMeshInternalData {
    struct Arrays{
        Array<int> mFaceList{3},mConnectList{0,1,2},m_uv_count_list,m_uv_connect_list;
        Array<float> m_u_vals,m_v_vals;
        Arrays(){mConnectList.track=true;}
    };
    std::shared_ptr<Arrays> arrays_{new Arrays};
    std::vector<std::array<float,3>> points{{{0,0,0}},{{1,0,0}},{{0,1,0}}};
    Matrix m_mat;
    Groups mAttributeList,mFaceAttributeList,mPVtxAttributeList,mGrooupList,mFaceGroupList,mEdgeGroupList;
    using ConstDPointIter=decltype(points)::const_iterator;
    std::size_t numberOfPoints()const{return points.size();}
    ConstDPointIter beginPt()const{return points.begin();}
    ConstDPointIter endPt()const{return points.end();}
    bool hasValidPolygonTopology()const;
    bool hasValidUVTopology()const;
    bool hasValidMeshData()const;
};
#include "mesh_validation.inc"

int main(){
    DMeshInternalData mesh;
    connectionReads=0;assert(mesh.hasValidMeshData());assert(connectionReads==3U);
    mesh.mEdgeGroupList["edges"]=std::make_shared<Group>(3);
    connectionReads=0;assert(mesh.hasValidMeshData());assert(connectionReads==9U);
    mesh.mEdgeGroupList["edges"]->ids.pop_back();assert(!mesh.hasValidMeshData());
    mesh.mEdgeGroupList.clear();
    for(Groups* groups:{&mesh.mAttributeList,&mesh.mFaceAttributeList,&mesh.mPVtxAttributeList,
                       &mesh.mGrooupList,&mesh.mFaceGroupList}){
        (*groups)["invalid"]=nullptr;assert(!mesh.hasValidMeshData());
        (*groups)["invalid"]=std::make_shared<Group>(10);assert(!mesh.hasValidMeshData());groups->clear();
    }
    mesh.m_mat.finite=false;assert(!mesh.hasValidMeshData());mesh.m_mat.finite=true;
    mesh.arrays_->m_u_vals.push_back(1);assert(!mesh.hasValidMeshData());mesh.arrays_->m_u_vals.clear();
    mesh.arrays_->mConnectList[0]=-1;assert(!mesh.hasValidMeshData());mesh.arrays_->mConnectList[0]=0;
    mesh.points[0][0]=std::numeric_limits<float>::infinity();assert(!mesh.hasValidMeshData());
    std::cout<<"Mesh validation: empty-edge-group fast path retains topology/UV/metadata rejection\n";
}
