// Geometry, native knots and bounds are scalar transport doubles. Typed
// attributes/groups and the complete production collection transactions run
// unchanged. This does not emulate Maya geometry or claim runtime coverage.
#include "mesh_metadata_fixture.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DCurveOperationBudget.h"
#include "common/utility/DCurveBatchBudget.h"
#include <maya/MPoint.h>

// Native array allocation and point conversion are explicit host boundaries;
// the production builder, per-point metadata and batch transaction run below.
struct MPointArray {
    std::vector<MPoint> values;
    MStatus append(const MPoint& point) {values.push_back(point); return MS::kSuccess;}
};
namespace dommy { namespace maya {
bool isFloatRepresentablePoint(const MPoint& value) {
    const double maximum = std::numeric_limits<float>::max();
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
        std::abs(value.x) <= maximum && std::abs(value.y) <= maximum && std::abs(value.z) <= maximum;
}
} }

using DPrimAttrListIter = DPAttrList::iterator;
using DPrimGrpListIter = DPGrpList::iterator;
static std::size_t curveCopies = 0U, curveChecks = 0U, curveBounds = 0U;
static int curveFault = 0;
struct DCurveInternalData {
    std::size_t points = 2U, knots = 2U;
    DPAttrList attrs;
    DPGrpList groups;
    std::vector<MPoint> coordinates;
    bool valid = true;
    DCurveInternalData() = default;
    DCurveInternalData(const MPointArray& input, int) : coordinates(input.values) {points = knots = input.values.size();}
    DCurveInternalData(const DCurveInternalData& source) : points(source.points), knots(source.knots),
        coordinates(source.coordinates), valid(source.valid) {
        ++curveCopies;
        if (curveFault == 1) throw std::bad_alloc();
        CopyAttributeToAttribute(attrs, source.attrs); CopyGroupToGroup(groups, source.groups);
        if (curveFault == 3) valid = false;
        if (curveFault == 5) {
            for (auto& field : attrs) {
                auto strings = std::dynamic_pointer_cast<DPStringAttr>(field.second);
                if (strings) field.second = std::make_shared<DPStringAttr>(
                    std::vector<std::string>(points, std::string(40U, 'x')), strings->defaultValue());
            }
        }
    }
    std::size_t numberOfPoints() const {return points;}
    std::size_t numberOfKnots() const {return knots;}
    bool hasValidCurveTopology() const {
        ++curveChecks;
        dommy::attribute::ValidationBudget budget;
        if (!valid || points > dommy::generation::kMaxGeneratedPointCount ||
            !dommy::attribute::validateAttributeList(attrs, points, budget)) return false;
        for (const auto& group : groups)
            if (!group.second || group.second->getGroupIds().size() != points) return false;
        return true;
    }
    bool appendOperationCost(dommy::curve::CurveBatchBudget&) const;
    std::shared_ptr<const DCurveInternalData> copy() const {return std::make_shared<DCurveInternalData>(*this);}
    std::shared_ptr<DCurveInternalData> copyNewCurve() const {
        if (curveFault == 2) {++curveCopies; return {};}
        return std::make_shared<DCurveInternalData>(*this);
    }
    ConstDPAttrIter beginAttr() const {return attrs.begin();}
    ConstDPAttrIter endAttr() const {return attrs.end();}
    ConstDPGrpIter beginGrp() const {return groups.begin();}
    ConstDPGrpIter endGrp() const {return groups.end();}
    void addAttr(const std::string& name, const DPAttributeData& value) {attrs[name] = value.copy();}
};
using DCItem = std::shared_ptr<DCurveInternalData>;
using ConstDCItem = std::shared_ptr<const DCurveInternalData>;
using DCurveList = std::vector<DCItem>;
using ConstDCurveList = std::vector<ConstDCItem>;
using ConstDCurveIter = DCurveList::const_iterator;
void ExpandAttributeSize(DPAttrList&, const std::size_t&);
struct DCurveData {
    using DCurveList = ::DCurveList;
    using CurveSourceList = std::vector<const DCurveInternalData*>;
    DCurveList curvearray;
    DPAttrList mAttributeList;
    DPGrpList mGroupList;
    std::size_t mBBox = 99U;
    bool mBBoxDirty = true;
    bool hasValidCurveTopology() const;
    bool getCheckedTotalCvs(std::size_t&) const;
    bool insert(const DCurveInternalData&);
    bool insert(const DCItem&);
    bool insert(const ConstDCItem&);
    bool insert(const DCurveList&);
    bool expand(const DCItem&, std::size_t);
    bool appendCurvesTransactional(const CurveSourceList&, const DPIndexList* = nullptr);
    MStatus copyFrom(const DCurveData&);
    using CurveCopyOperation = std::function<MStatus(DCurveInternalData&,std::size_t,std::size_t)>;
    MStatus copyRepeatedFrom(const DCurveData&,std::size_t,const CurveCopyOperation& = CurveCopyOperation());
    ConstDCurveIter beginCurves() const {return curvearray.begin();}
    ConstDCurveIter endCurves() const {return curvearray.end();}
    DCurveList::iterator beginCurves() {mBBoxDirty = true; return curvearray.begin();}
    DCurveList::iterator endCurves() {mBBoxDirty = true; return curvearray.end();}
    ConstDPAttrIter beginAttr() const {return mAttributeList.begin();}
    ConstDPAttrIter endAttr() const {return mAttributeList.end();}
    ConstDPGrpIter beginGrp() const {return mGroupList.begin();}
    ConstDPGrpIter endGrp() const {return mGroupList.end();}
    std::size_t numberOfCurves() const {return curvearray.size();}
    void updateBBox() {++curveBounds; if (curveFault == 4) throw std::bad_alloc(); mBBox = curvearray.size(); mBBoxDirty = false;}
};
#include "curve_transactions.inc"

static DCItem curve() {
    auto result = std::make_shared<DCurveInternalData>();
    result->attrs["p"] = attr<int>({1, 2});
    result->groups["g"] = std::make_shared<TrackedGroup>(DPIdList{true, false});
    return result;
}
static void counters() {reset(); curveCopies = curveChecks = curveBounds = 0U; curveFault = 0;}
static void unchangedCurve(const DCurveData& value, const DCurveData& old) {
    assert(value.curvearray == old.curvearray && value.mAttributeList == old.mAttributeList &&
        value.mGroupList == old.mGroupList && value.mBBox == old.mBBox && value.mBBoxDirty == old.mBBoxDirty);
}
static void transactions() {
    DCurveData empty, copiedEmpty;
    empty.mAttributeList["v"] = attr<std::string>({}, "empty");
    empty.mGroupList["g"] = std::make_shared<TrackedGroup>(DPIdList());
    counters(); assert(copiedEmpty.copyFrom(empty));
    assert(curveCopies == 0U && curveBounds == 1U && copiedEmpty.hasValidCurveTopology());
    assert(std::dynamic_pointer_cast<DPStringAttr>(copiedEmpty.mAttributeList.at("v"))->defaultValue() == "empty");
    assert(copiedEmpty.mGroupList.at("g")->getGroupIds().empty());
    auto source = curve(); DCurveData value;
    counters(); assert(value.insert(*source)); assert(curveCopies == 1U && curveBounds == 1U);
    assert(value.curvearray[0] != source && value.curvearray[0]->attrs.at("p") != source->attrs.at("p"));
    value.mAttributeList["v"] = attr<int>({7}, -1);
    value.mGroupList["g"] = std::make_shared<DPGroupData>(DPIdList{true});
    DCurveData copied; counters(); assert(copied.copyFrom(value));
    assert(curveCopies == 1U && curveBounds == 1U && copied.hasValidCurveTopology());
    assert(copied.mAttributeList.at("v") != value.mAttributeList.at("v"));
    const auto old = copied; counters(); assert(copied.copyFrom(copied));
    unchangedCurve(copied, old); assert(curveCopies == 0U && curveBounds == 0U);
    counters(); assert(copied.expand(source, 0U)); assert(curveCopies == 1U && curveBounds == 1U);
    assert(std::dynamic_pointer_cast<DPIntAttr>(copied.mAttributeList.at("v"))->value() == std::vector<int>({7, 7}));
    assert(copied.mGroupList.at("g")->getGroupIds() == DPIdList({true, true}));
    counters(); ConstDCItem constSource = source; assert(copied.insert(constSource));
    assert(curveCopies == 1U && curveBounds == 1U);
    assert(std::dynamic_pointer_cast<DPIntAttr>(copied.mAttributeList.at("v"))->value() == std::vector<int>({7, 7, -1}));
    assert(copied.mGroupList.at("g")->getGroupIds() == DPIdList({true, true, false}));
    for (int mode : {1, 2, 3, 4}) {
        const auto before = copied; counters(); curveFault = mode;
        assert(!copied.insert(source)); unchangedCurve(copied, before);
        assert(!copied.copyFrom(value)); unchangedCurve(copied, before);
        assert(!copied.expand(source, 0U)); unchangedCurve(copied, before);
    }
    for (int mode : {1, 2, 3}) for (bool group : {false, true}) {
        const auto before = copied; counters(); metadataFailure = mode;
        if (group) failGroupAt = 1; else failAttributeAt = 1;
        assert(!copied.insert(source)); unchangedCurve(copied, before);
    }
    counters(); const auto before = copied;
    assert(!copied.insert(DCItem())); assert(!copied.insert(ConstDCItem()));
    assert(!copied.expand(source, 99U)); unchangedCurve(copied, before); assert(curveCopies == 0U);
    DCurveData invalid; invalid.curvearray.push_back(DCItem());
    assert(!invalid.insert(DCurveList()));
}
static void admission() {
    auto payload = curve(); payload->attrs.clear();
    payload->attrs["s"] = attr<std::string>({std::string(60U, 'a'), std::string(60U, 'b')});
    DCurveData one; counters(); assert(one.insert(payload));
    const auto old = one; counters(); assert(!one.insert(payload)); unchangedCurve(one, old);
    assert(curveCopies == 0U && attributeCopies == 0 && curveBounds == 0U);
    DCurveData two; two.curvearray = {payload, payload}; assert(two.hasValidCurveTopology());
    counters(); assert(!one.copyFrom(two)); unchangedCurve(one, old); assert(curveCopies == 0U);
    counters(); assert(!one.expand(payload, 0U)); unchangedCurve(one, old); assert(curveCopies == 0U);
    auto exact = curve(); exact->attrs.clear();
    exact->attrs["s"] = attr<std::string>({std::string(63U, 'a'), std::string(63U, 'b')});
    DCurveData boundary; counters(); assert(boundary.insert(exact)); assert(curveCopies == 1U);
    exact->attrs["s"] = attr<std::string>({std::string(63U, 'a'), std::string(64U, 'b')});
    DCurveData rejected; counters(); assert(!rejected.insert(exact)); assert(curveCopies == 0U);
    DCurveData defaults; defaults.curvearray.push_back(curve());
    defaults.mAttributeList["v"] = attr<std::string>({std::string(50U, 'a')}, std::string(50U, 'd'));
    const auto before = defaults; counters(); assert(!defaults.insert(curve()));
    unchangedCurve(defaults, before); assert(curveCopies == 0U && attributeCopies == 0);
    auto huge = std::make_shared<DCurveInternalData>(); huge->points = 3000000U;
    counters(); assert(!rejected.insert(huge)); assert(curveCopies == 0U && curveBounds == 0U);
    counters(); assert(!rejected.insert(*huge)); assert(curveCopies == 0U && curveBounds == 0U);
    auto exactGeometry = std::make_shared<DCurveInternalData>();
    exactGeometry->points = 1677720U; exactGeometry->knots = 1677724U;
    dommy::mesh::MeshMergePlan exactPlan;
    assert(dommy::mesh::appendMeshMergePlan(exactGeometry->points, 0U, exactGeometry->knots, 0U, exactPlan));
    std::size_t bytes = 0U; assert(dommy::mesh::meshOperationStorageBytes(exactPlan, bytes));
    assert(bytes == dommy::mesh::kMaxMeshOperationBytes);
    DCurveData exactOutput; counters(); assert(exactOutput.insert(exactGeometry));
    assert(curveCopies == 1U && curveBounds == 1U);
    ++exactGeometry->points; ++exactGeometry->knots;
    counters(); assert(!rejected.insert(exactGeometry)); assert(curveCopies == 0U && curveBounds == 0U);
    auto growing = curve(); growing->attrs.clear();
    growing->attrs["s"] = attr<std::string>({std::string(25U, 'a'), std::string(25U, 'b')});
    DCurveData small; small.curvearray = {growing, growing};
    counters(); curveFault = 5;
    assert(!rejected.copyFrom(small)); assert(rejected.curvearray.empty() && curveBounds == 0U);
    // Each copied curve remains valid; only the combined candidate exceeds
    // the policy. A virtual metadata copy must not bypass final admission.
    counters(); curveFault = 5;
    assert(!rejected.insert(DCurveList{growing, growing}));
    assert(rejected.curvearray.empty() && curveBounds == 0U);
    counters();curveFault=5;
    assert(!rejected.copyRepeatedFrom(small,1U));
    assert(rejected.curvearray.empty() && curveBounds==0U);
}
static void batches() {
    for (std::size_t count : {100U, 200U, 1000U}) {
        auto source = std::make_shared<DCurveInternalData>(); DCurveList batch(count, source);
        DCurveData output; counters(); assert(output.insert(batch));
        assert(curveCopies == count && curveBounds == 1U && curveChecks <= 4U * count);
        for (const auto& item : output.curvearray) assert(item != source);
        std::cout << "Curve batch " << count << ": " << curveChecks << " primitive validations\n";
    }
}
static void repeatedCopies() {
    DCurveData source;source.curvearray={curve(),curve()};
    source.mAttributeList["v"]=attr<int>({7,8},-1);
    source.mGroupList["g"]=std::make_shared<DPGroupData>(DPIdList{true,false});
    DCurveData output;std::size_t calls=0U;
    counters();assert(output.copyRepeatedFrom(source,3U,
        [&](DCurveInternalData& copied,std::size_t step,std::size_t index) {
            assert(step==1U+calls/2U && index==calls%2U);++calls;
            copied.attrs["p"]=attr<int>({static_cast<int>(step),static_cast<int>(index)});
            return MStatus(MS::kSuccess);
        }));
    assert(calls==4U && curveCopies==6U && curveBounds==1U);
    assert(std::dynamic_pointer_cast<DPIntAttr>(output.mAttributeList.at("v"))->value()==std::vector<int>({7,8,7,8,7,8}));
    assert(output.mGroupList.at("g")->getGroupIds()==DPIdList({true,false,true,false,true,false}));
    for(std::size_t i=0;i<6U;++i)assert(output.curvearray[i]!=source.curvearray[i%2U]);
    assert(std::dynamic_pointer_cast<DPIntAttr>(output.curvearray[0]->attrs.at("p"))->value()==std::vector<int>({1,2}));
    const auto old=output;
    for(int mode:{1,2,3,4}) {
        counters();curveFault=mode;assert(!output.copyRepeatedFrom(source,3U));unchangedCurve(output,old);
    }
    for(int mode:{1,2,3})for(bool group:{false,true}) {
        counters();metadataFailure=mode;
        if(group)failGroupAt=1;else failAttributeAt=1;
        assert(!output.copyRepeatedFrom(source,3U));unchangedCurve(output,old);
    }
    for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
        counters();assert(output.copyRepeatedFrom(source,3U,
            [code](DCurveInternalData&,std::size_t,std::size_t){return MStatus(code);})==code);
        unchangedCurve(output,old);
    }
    counters();assert(!output.copyRepeatedFrom(source,3U,
        [](DCurveInternalData&,std::size_t,std::size_t)->MStatus{throw std::bad_alloc();}));unchangedCurve(output,old);
    counters();assert(!output.copyRepeatedFrom(source,3U,
        [](DCurveInternalData& value,std::size_t,std::size_t) {
            value.points=value.knots=2000000U;value.attrs.clear();value.groups.clear();return MStatus(MS::kSuccess);
        }));unchangedCurve(output,old);assert(curveBounds==0U);
    for(std::size_t count:{0U,1000001U}) {
        counters();assert(!output.copyRepeatedFrom(source,count));unchangedCurve(output,old);assert(curveCopies==0U);
    }
    DCurveData aliased;counters();assert(aliased.copyFrom(source));const auto original=aliased;
    counters();assert(aliased.copyRepeatedFrom(aliased,2U));assert(curveCopies==4U&&curveBounds==1U);
    assert(std::dynamic_pointer_cast<DPIntAttr>(aliased.mAttributeList.at("v"))->value()==std::vector<int>({7,8,7,8}));
    assert(aliased.curvearray[0]!=original.curvearray[0]&&original.curvearray.size()==2U);
    DCurveData empty;empty.mAttributeList["s"]=attr<std::string>({},"default");
    counters();assert(output.copyRepeatedFrom(empty,3U));assert(output.curvearray.empty()&&curveCopies==0U&&curveBounds==1U);
    assert(std::dynamic_pointer_cast<DPStringAttr>(output.mAttributeList.at("s"))->defaultValue()=="default");
    for(std::size_t count:{100U,200U,1000U}) {
        DCurveData many;many.curvearray={std::make_shared<DCurveInternalData>()};
        counters();assert(output.copyRepeatedFrom(many,count));assert(curveCopies==count&&curveBounds==1U&&curveChecks<=3U*count);
    }
}
static void connections() {
    DCurveOutputBatch batch; counters();
    for (int i = 0; i < 1000; ++i)
        assert(dommy::curve::appendConnectionCurve(batch, MPoint(i, 1, 2), MPoint(i + 1, 3, 4), i, -i));
    assert(curveCopies == 0U && curveBounds == 0U);
    DCurveData output; assert(batch.publishTo(output));
    assert(curveCopies == 1000U && curveBounds == 1U && curveChecks <= 5000U);
    for (int i = 0; i < 1000; ++i) {
        const auto& item = output.curvearray[i];
        assert(item->coordinates[0].x == i && item->coordinates[1].x == i + 1);
        assert(std::dynamic_pointer_cast<DPIntAttr>(item->attrs.at("prim_index"))->value() == std::vector<int>({i, -i}));
    }
    DCurveOutputBatch invalid;
    assert(!dommy::curve::appendConnectionCurve(invalid, MPoint(std::numeric_limits<double>::infinity(), 0, 0), MPoint(), 0, 0));
    assert(!dommy::curve::appendConnectionCurve(invalid, MPoint(), MPoint(std::numeric_limits<double>::max(), 0, 0), 0, 0));
    const auto old = output;
    counters(); curveFault = 3; assert(!batch.publishTo(output)); unchangedCurve(output, old);
    std::cout << "Connection batch: ordered endpoints/prim_index, one publication, rollback and linear validation pass\n";
}
static void batchAdmission() {
    auto large=std::make_shared<DCurveInternalData>();large->points=large->knots=1000000U;
    DCurveOutputBatch geometry;assert(geometry.append(large));assert(!geometry.append(large));
    DCurveData output;counters();assert(geometry.publishTo(output));assert(output.curvearray.size()==1U&&curveCopies==1U);
    auto strings=std::make_shared<DCurveInternalData>();
    strings->attrs["s"]=attr<std::string>({std::string(25U,'a'),std::string(25U,'b')},std::string(60U,'d'));
    DCurveOutputBatch metadata;assert(metadata.append(strings));assert(!metadata.append(strings));
    assert(!metadata.append(DCItem()));
    output=DCurveData();counters();assert(metadata.publishTo(output));assert(output.curvearray.size()==1U&&curveCopies==1U);
    const auto before=output;
    // Publication rechecks shared staging objects; they are never adopted.
    strings->attrs["s"]=attr<std::string>({std::string(70U,'a'),std::string(70U,'b')});
    counters();assert(!metadata.publishTo(output));unchangedCurve(output,before);assert(curveCopies==0U);
    auto exact=std::make_shared<DCurveInternalData>();exact->points=1677720U;exact->knots=1677724U;
    DCurveOutputBatch boundary;assert(boundary.append(exact));assert(!boundary.append(curve()));
    dommy::curve::CurveBatchBudget work;
    assert(work.append(2U,2U,dommy::generation::kMaxWorkItemCount-6U,0U));
    assert(!work.append(2U,2U,0U,0U));
    dommy::curve::CurveBatchBudget bytes;
    assert(bytes.append(2U,2U,0U,dommy::generation::kMaxGeneratedStringByteCount));
    assert(!bytes.append(2U,2U,0U,1U));
}
int main(int argc, char**) {
    if (argc > 1) {connections(); return 0;}
    transactions(); admission(); batches();repeatedCopies();batchAdmission();
    std::cout << "Curve transactions: aggregate admission, one clone/bounds, indexed/default rows and rollback pass\n";
}
