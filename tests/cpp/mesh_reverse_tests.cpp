// Exercise the actual reverseWinding transaction with small host/storage doubles.
// Maya edge IDs are represented by traversal-order IDs, deliberately permuted
// after reversal. Real SDK binding is checked separately; not a Maya emulator.
#include "common/utility/DMeshUVTransfer.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

template<class T> struct Array:std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length() const{return static_cast<unsigned int>(this->size());}
};
using MIntArray=Array<int>;
using MFloatArray=Array<float>;
using DPointList=std::vector<int>;
using DPIndexList=std::vector<std::size_t>;
using MeshEdgeKey=std::pair<int,int>;
using MeshEdgeIndexMap=std::map<MeshEdgeKey,std::size_t>;
static bool failCopy=false,failSort=false,throwSort=false,throwGroup=false;
static int mapCalls=0,failMap=0,corruptMap=0,validCalls=0,failValid=0;
struct Attribute {
    std::vector<int> values;
    int defaultValue=73;
    bool sortArrayFromIds(const DPIndexList& ids) {
        if(throwSort)throw std::bad_alloc();
        if(failSort)return false;
        std::vector<int> candidate;for(auto id:ids)candidate.push_back(values.at(id));
        values.swap(candidate);return true;
    }
};
using Attributes=std::map<std::string,std::shared_ptr<Attribute>>;
using Groups=std::map<std::string,std::vector<int>>;
static bool buildMeshEdgeIndexMap(const DPointList&,const MIntArray& counts,
                                 const MIntArray& ids,MeshEdgeIndexMap& edges) {
    if(++mapCalls==failMap)return false;
    std::size_t offset=0;
    for(int count:counts){for(int i=0;i<count;++i){
        const MeshEdgeKey key=std::minmax(ids.at(offset+i),ids.at(offset+(i+1)%count));
        if(!edges.count(key)){const auto id=edges.size();edges[key]=id;}
    }offset+=count;}
    if(mapCalls==corruptMap && !edges.empty())edges.begin()->second=edges.size();
    return true;
}
static void RemapFromGroupUseIds(Groups& groups,const DPIndexList& ids) {
    if(throwGroup)throw std::bad_alloc();
    for(auto& group:groups){std::vector<int> candidate;for(auto id:ids)candidate.push_back(group.second.at(id));
        group.second.swap(candidate);}
}
struct DMeshInternalData {
    struct Arrays {
        MIntArray mFaceList,mConnectList,m_uv_count_list,m_uv_connect_list;
        MFloatArray m_u_vals,m_v_vals;
    };
    std::unique_ptr<Arrays> arrays_{new Arrays};
    DPointList pointArray;
    Attributes mAttributeList,mFaceAttributeList,mPVtxAttributeList;
    Groups mGrooupList,mFaceGroupList,mEdgeGroupList;
    int m_mat=51;
    DMeshInternalData()=default;
    DMeshInternalData(const DMeshInternalData& source) {
        if(failCopy)throw std::bad_alloc();
        *arrays_=*source.arrays_;pointArray=source.pointArray;m_mat=source.m_mat;
        for(auto pair:{std::make_pair(&mAttributeList,&source.mAttributeList),
                       std::make_pair(&mFaceAttributeList,&source.mFaceAttributeList),
                       std::make_pair(&mPVtxAttributeList,&source.mPVtxAttributeList)})
            for(auto& item:*pair.second)(*pair.first)[item.first]=std::make_shared<Attribute>(*item.second);
        mGrooupList=source.mGrooupList;mFaceGroupList=source.mFaceGroupList;mEdgeGroupList=source.mEdgeGroupList;
    }
    std::size_t numberOfConnections()const{return arrays_->mConnectList.size();}
    bool hasUV()const{return !arrays_->m_u_vals.empty();}
    bool hasValidMeshData()const {
        if(++validCalls==failValid)return false;
        std::size_t total=0;for(int face:arrays_->mFaceList){if(face<3)return false;total+=face;}
        if(total!=numberOfConnections())return false;
        for(int id:arrays_->mConnectList)if(id<0 || static_cast<std::size_t>(id)>=pointArray.size())return false;
        if(!dommy::mesh::validFullMeshUVArrays(arrays_->mFaceList,arrays_->m_u_vals,arrays_->m_v_vals,
                arrays_->m_uv_count_list,arrays_->m_uv_connect_list))return false;
        for(auto& a:mPVtxAttributeList)if(!a.second || a.second->values.size()!=total)return false;
        return true;
    }
    void swapState(DMeshInternalData& other)noexcept {
        arrays_.swap(other.arrays_);pointArray.swap(other.pointArray);std::swap(m_mat,other.m_mat);
        mAttributeList.swap(other.mAttributeList);mFaceAttributeList.swap(other.mFaceAttributeList);
        mPVtxAttributeList.swap(other.mPVtxAttributeList);mGrooupList.swap(other.mGrooupList);
        mFaceGroupList.swap(other.mFaceGroupList);mEdgeGroupList.swap(other.mEdgeGroupList);
    }
    bool reverseWinding();
};
#include "mesh_reverse.inc"
static void reset(){failCopy=failSort=throwSort=throwGroup=false;mapCalls=failMap=corruptMap=validCalls=failValid=0;}
static std::shared_ptr<Attribute> attr(std::initializer_list<int> values){
    auto result=std::make_shared<Attribute>();result->values=values;return result;
}
static DMeshInternalData fixture() {
    reset();DMeshInternalData mesh;
    mesh.pointArray={10,20,30,40,50};mesh.arrays_->mFaceList={4,3};
    mesh.arrays_->mConnectList={0,1,2,3,3,2,4};
    mesh.arrays_->m_u_vals={0,1,1,0,.5f,.7f};mesh.arrays_->m_v_vals={0,0,1,1,.5f,.7f};
    mesh.arrays_->m_uv_count_list={4,3};mesh.arrays_->m_uv_connect_list={0,1,2,3,4,2,5};
    mesh.mAttributeList["point"]=attr({1,2,3,4,5});mesh.mFaceAttributeList["face"]=attr({6,7});
    mesh.mPVtxAttributeList["corner"]=attr({10,11,12,13,14,15,16});
    mesh.mPVtxAttributeList["other"]=attr({20,21,22,23,24,25,26});
    mesh.mGrooupList["point"]={1,0,1,0,1};mesh.mFaceGroupList["outer"]={1,0};
    mesh.mEdgeGroupList["edge"]={1,0,0,1,0,1};return mesh;
}
static bool sameAttributes(const Attributes& a,const Attributes& b) {
    if(a.size()!=b.size())return false;
    for(const auto& item:a){const auto other=b.find(item.first);
        if(other==b.end() || item.second->values!=other->second->values ||
           item.second->defaultValue!=other->second->defaultValue)return false;}
    return true;
}
static bool same(const DMeshInternalData& a,const DMeshInternalData& b) {
    return a.pointArray==b.pointArray && a.m_mat==b.m_mat &&
        a.arrays_->mFaceList==b.arrays_->mFaceList && a.arrays_->mConnectList==b.arrays_->mConnectList &&
        a.arrays_->m_u_vals==b.arrays_->m_u_vals && a.arrays_->m_v_vals==b.arrays_->m_v_vals &&
        a.arrays_->m_uv_count_list==b.arrays_->m_uv_count_list &&
        a.arrays_->m_uv_connect_list==b.arrays_->m_uv_connect_list &&
        sameAttributes(a.mAttributeList,b.mAttributeList) && sameAttributes(a.mFaceAttributeList,b.mFaceAttributeList) &&
        sameAttributes(a.mPVtxAttributeList,b.mPVtxAttributeList) && a.mGrooupList==b.mGrooupList &&
        a.mFaceGroupList==b.mFaceGroupList && a.mEdgeGroupList==b.mEdgeGroupList;
}
int main() {
    {auto withNormals=fixture();withNormals.mPVtxAttributeList["__dommy_corner_normal"]=attr({1,2,3,4,5,6,7});
     assert(withNormals.reverseWinding());assert(!withNormals.mPVtxAttributeList.count("__dommy_corner_normal"));}
    auto mesh=fixture();const DMeshInternalData original(mesh);
    assert(mesh.reverseWinding());
    assert((mesh.arrays_->mConnectList==MIntArray{3,2,1,0,4,2,3}));
    assert((mesh.arrays_->m_uv_connect_list==MIntArray{3,2,1,0,5,2,4}));
    assert((mesh.mPVtxAttributeList.at("corner")->values==std::vector<int>{13,12,11,10,16,15,14}));
    assert((mesh.mEdgeGroupList.at("edge")==std::vector<int>{0,0,1,1,0,1}));
    assert(sameAttributes(mesh.mAttributeList,original.mAttributeList));
    assert(mesh.reverseWinding() && same(mesh,original));
    for(int mode=0;mode<9;++mode){auto failed=fixture();const DMeshInternalData before(failed);
        if(mode==0)failCopy=true;if(mode==1)failSort=true;if(mode==2)throwSort=true;
        if(mode==3)failMap=1;if(mode==4)failMap=2;if(mode==5)corruptMap=2;
        if(mode==6)throwGroup=true;if(mode==7)failValid=1;if(mode==8)failValid=2;
        assert(!failed.reverseWinding());assert(same(failed,before));reset();}
    auto bare=fixture();bare.mEdgeGroupList.clear();bare.mPVtxAttributeList.clear();
    bare.arrays_->m_u_vals.clear();bare.arrays_->m_v_vals.clear();
    bare.arrays_->m_uv_count_list.clear();bare.arrays_->m_uv_connect_list.clear();
    const DMeshInternalData before(bare);assert(bare.reverseWinding() && mapCalls==0);
    assert(bare.reverseWinding() && same(bare,before));
    std::cout<<"Winding reversal preserves metadata/UVs and rolls back 9 injected failures\n";
}
