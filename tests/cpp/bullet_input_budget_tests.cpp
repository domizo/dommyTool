#include "mesh_metadata_fixture.h"
#include "bullet_input_budget.inc"
#include "modules/dynamics/DBullet/DM/DBulletInputBudget.h"

struct BudgetCurve : DMeshInternalData {
    std::size_t numberOfKnots()const{return 5U;}
};
struct BudgetCurves {
    std::vector<std::shared_ptr<BudgetCurve>> curves;
    DPAttrList attrs;DPGrpList groups;
    auto beginCurves()const -> decltype(curves.begin()){return curves.begin();}
    auto endCurves()const -> decltype(curves.end()){return curves.end();}
    auto beginAttr()const -> decltype(attrs.begin()){return attrs.begin();}
    auto endAttr()const -> decltype(attrs.end()){return attrs.end();}
    auto beginGrp()const -> decltype(groups.begin()){return groups.begin();}
    auto endGrp()const -> decltype(groups.end()){return groups.end();}
    std::size_t numberOfCurves()const{return curves.size();}
};

int main(){
    using dommy::geometry::GeometryBatchBudget;
    GeometryBatchBudget batch;auto mesh=fixture();
    BudgetCurves curves;curves.curves.push_back(std::make_shared<BudgetCurve>());
    curves.curves[0]->pointAttrs["label"]=attr<std::string>({"a","b","c"});
    curves.attrs["payload"]=attr<DIntArray>({DIntArray{1,2,3}});
    curves.groups["selected"]=std::make_shared<DPGroupData>(DPIdList{false});
    assert(dommy::bullet::admitMeshInput(mesh,batch));
    assert(dommy::bullet::admitCurveInput(curves,batch));
    reset();assert(meshCopies==0&&attributeCopies==0&&groupCopies==0);
    // Individually admissible collections jointly exceed a reduced string
    // allowance. A rejected append does not corrupt the prior batch state.
    GeometryBatchBudget metadata;
    curves.attrs["payload"]=attr<std::string>({std::string(50U,'x')});
    assert(dommy::bullet::admitCurveInput(curves,metadata));
    assert(!dommy::bullet::admitCurveInput(curves,metadata));
    BudgetCurves empty;assert(dommy::bullet::admitCurveInput(empty,metadata));
    curves.curves[0]->pointAttrs["invalid"]=attr<int>({1});
    GeometryBatchBudget invalid;assert(!dommy::bullet::admitCurveInput(curves,invalid));
    mesh.meshItemArray[0]->cornerAttrs["bad"]=attr<int>({1});
    assert(!dommy::bullet::admitMeshInput(mesh,invalid));
    dommy::mesh::MeshMergePlan half;half.meshes=1;half.points=1200000U;half.workItems=half.points;
    GeometryBatchBudget geometry;
    assert(geometry.append(half,0,0));assert(!geometry.append(half,0,0));
    // Borrowed kinematic payloads consume traversal work but do not get charged
    // as retained copies. They still share the evaluation's aggregate limits.
    assert(geometry.append(half,0,0,false));
    GeometryBatchBudget work;dommy::mesh::MeshMergePlan noGeometry;
    assert(work.append(noGeometry,70000000U,0U,false));
    assert(!work.append(noGeometry,70000000U,0U,false));
    assert(work.append(noGeometry,1U,0U,false));
    GeometryBatchBudget bytes;const std::size_t maximum=dommy::mesh::kMaxMeshOperationBytes;
    assert(bytes.append(noGeometry,(maximum-128U)/32U,0U));
    assert(!bytes.append(noGeometry,1U,0U));
}
