#include "arnold_mesh_metadata_host.h"
#include "metadata_methods.inc"

void resetMetadata() {
    calls.clear();failAt=-1;failureKind=0;tableArray=true;tableCount=3;
    reportedStringLength=0U;oversizedString.clear();
    rows.assign(3,Row());rows[0].enabled=false;rows[1].name="sparse-7";rows[2].name="sparse-123";
    nativeEdges=3;endpoint=1;nullOwner=nullMesh=false;
    fixtureFaces=1;mattePublished=creasePublished=999;visibility=1;errors=0;
}
MeshOwner fixtureMeshes() {
    MeshOwner result;
    for(int i=0;i<3;++i) {
        auto mesh=std::make_shared<MetadataMesh>();
        auto group=std::make_shared<DPGroupData>();group->ids={true,false,true};mesh->group=group;
        if(i==0)mesh->exists=false;
        result.meshes.push_back(mesh);
    }
    return result;
}
template<class Callback>void metadataFaults(Callback callback) {
    resetMetadata();callback(false);const auto count=calls.size();
    for(std::size_t i=0;i<count;++i)for(int kind=1;kind<=3;++kind) {
        resetMetadata();failAt=int(i);failureKind=kind;callback(true);
        resetMetadata();callback(false);
    }
}
void rowsAndFaults() {
    metadataFaults([](bool failure){
        std::vector<GroupMatteRow> out(1);out[0].name="retained";
        bool failed=false;try{failed=!readGroupMatteRows(MPlug(),1,3,out);}catch(const std::exception&){failed=true;}
        assert(failed==failure);
        if(failed)assert(out.size()==1&&out[0].name=="retained");
        else {assert(out.size()==2&&out[0].name=="sparse-7"&&out[1].groups->length()==3);
            assert(std::string((*out[1].groups)[2].asChar())=="third");}
    });
    resetMetadata();std::vector<GroupMatteRow> out;MPlug absent;absent.null=true;
    assert(readGroupMatteRows(absent,1,1,out)&&out.empty()&&calls.empty());
    resetMetadata();rows[0].name.clear();rows[0].groups.clear();rows[1].groups="  ";rows[2].name.clear();
    assert(readGroupMatteRows(MPlug(),1,3,out)&&out.empty());
    resetMetadata();rows.assign(3,Row());for(auto& row:rows)row.enabled=false;
    assert(readGroupMatteRows(MPlug(),1,3,out)&&out.empty());
    for(const auto& call:calls)assert(call!="row.string"&&call!="names.append");
    resetMetadata();tableCount=dommy::generation::kMaxGeneratedStringCount+1;
    assert(!readGroupMatteRows(MPlug(),1,3,out)&&calls.size()==2);
    resetMetadata();tableArray=false;assert(!readGroupMatteRows(MPlug(),1,3,out)&&calls.size()==1);
    resetMetadata();assert(!readGroupMatteRows(MPlug(),3000000,3,out));
    resetMetadata();assert(!readGroupMatteRows(MPlug(),20000000,3,out));
    resetMetadata();reportedStringLength=dommy::generation::kMaxGeneratedStringByteCount+1;
    oversizedString="sparse-7";
    assert(!readGroupMatteRows(MPlug(),1,3,out));
    resetMetadata();reportedStringLength=dommy::generation::kMaxGeneratedStringByteCount+1;
    oversizedString="selected";
    MStringArray names;bool more=false;
    assert(!splitRenderGroupNames("selected",1,names,more)&&calls.empty());
    metadataFaults([](bool failure){MeshOwner owner=fixtureMeshes();AtNode node;
        bool failed=false;try{matteAdmission(&node,&owner);failed=mattePublished==999;}catch(const std::exception&){failed=true;}
        assert(failed==failure);if(failed)assert(mattePublished==999);else assert(mattePublished==2);
    });
}
void creasesAndFaults() {
    const DPIndexList expected={3,4,5,4,6,7,8,7};
    metadataFaults([&](bool failure){MeshOwner owner=fixtureMeshes();DPIndexList out{99};
        bool failed=false;try{failed=!collectCreaseIndices(owner,"selected",out);}catch(const std::exception&){failed=true;}
        assert(failed==failure);assert(out==(failed?DPIndexList{99}:expected));
    });
    resetMetadata();auto owner=fixtureMeshes();DPIndexList out{99};
    owner.meshes[1]->group.reset();assert(!collectCreaseIndices(owner,"selected",out)&&out==DPIndexList{99});
    resetMetadata();owner=fixtureMeshes();auto group=std::make_shared<DPGroupData>();group->ids={true,false};owner.meshes[1]->group=group;
    assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();owner=fixtureMeshes();endpoint=3;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();endpoint=-1;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();nativeEdges=-1;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();nullOwner=true;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();nullMesh=true;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();owner.meshes[1]->faces=-1;assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();owner=fixtureMeshes();owner.meshes[1].reset();assert(!collectCreaseIndices(owner,"selected",out));
    resetMetadata();owner=fixtureMeshes();owner.meshes[1]->points=3000000;
    assert(!collectCreaseIndices(owner,"selected",out)&&calls.empty());
    resetMetadata();owner=fixtureMeshes();owner.meshes[0]->points=dommy::generation::kMaxGeneratedPointCount;
    assert(!collectCreaseIndices(owner,"selected",out)&&calls.empty());
    resetMetadata();owner=fixtureMeshes();owner.meshes.resize(2);owner.meshes[1]->faces=0;
    auto empty=std::make_shared<DPGroupData>();owner.meshes[1]->group=empty;
    assert(collectCreaseIndices(owner,"selected",out)&&out.empty()&&calls.empty());
    metadataFaults([](bool failure){MeshOwner owner=fixtureMeshes();AtNode node;
        bool failed=false;try{creaseAdmission(&node,&owner);failed=creasePublished==999;}catch(const std::exception&){failed=true;}
        assert(failed==failure);if(failed)assert(creasePublished==999);else assert(creasePublished==8);
    });
}
int main(){rowsAndFaults();creasesAndFaults();}
