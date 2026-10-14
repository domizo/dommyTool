#include "common/utility/DAttributeArrayConversion.h"
#ifndef DOMMY_TEST_EXPORT_MUTATION
#include "common/utility/DAttributeExport.h"
#endif
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

struct DPData {
    std::size_t count = 1U;
    int faces = 1;
    int numFace() const { return faces; }
    std::map<std::string, DPAttributeData::ConstPtr> attributes;
    std::size_t numberOfPoints() const { return count; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? DPAttributeData::ConstPtr() : found->second;
    }
};
struct DCurveData {
    std::vector<std::shared_ptr<DPData>> owners;
    auto beginCurves() const -> decltype(owners.begin()) { return owners.begin(); }
    auto endCurves() const -> decltype(owners.end()) { return owners.end(); }
};
struct DMeshData {
    std::vector<std::shared_ptr<DPData>> owners;
    const std::vector<std::shared_ptr<DPData>>* getMeshListConst() const { return &owners; }
};
#include "alembic_attribute.inc"

template <typename T> std::shared_ptr<DPData> owner(const std::vector<T>& values, T fallback) {
    auto result = std::make_shared<DPData>();
    result->count = values.size();
    result->attributes["field"] = std::make_shared<TypedDPAttributeData<T>>(values, fallback);
    return result;
}
template <typename Run> void rejected(Run run) {
    bool failed = false;
    try { run(); } catch (const std::invalid_argument&) { failed = true; }
    assert(failed);
}
template <typename T> bool equal(const T& left, const T& right) {
    using Parts = dommy::numeric::ValueComponents<T>;
    for (unsigned int axis = 0U; axis < Parts::count; ++axis)
        if (Parts::get(left, axis) != Parts::get(right, axis)) return false;
    return true;
}
template <typename T, typename U> void roundTrip(const T& value, const T& fallback) {
    const auto present = owner<T>({value}, fallback);
    auto missing = std::make_shared<DPData>();
    DMeshData meshes{{missing, present, missing}};
    DCurveData curves{{missing, present, missing}};
    const auto type = present->getAttr("field")->getAttrType();
    std::vector<U> output{U()};
    DGetAbcDMPScalarAttribute<T>(meshes, type, "field", output);
    assert(output.size() == 3U);
    U convertedValue, convertedDefault;
    assert(dommy::numeric::checkedValueCast(value, convertedValue));
    assert(dommy::numeric::checkedValueCast(fallback, convertedDefault));
    assert(equal(output[0], convertedDefault) && equal(output[1], convertedValue) && equal(output[2], convertedDefault));
    DGetAbcDCPScalarAttribute<T>(&curves, type, "field", output);
    assert(equal(output[0], convertedDefault) && equal(output[1], convertedValue) && equal(output[2], convertedDefault));
    DGetAbcDPScalarAttribute<T>(present.get(), type, "field", output);
    assert(output.size() == 1U && equal(output[0], convertedValue));
    // Complete primitive-to-face helper and throwing production wrapper.
    present->faces=2;missing->faces=4;
    meshes.owners={present,missing};
    const auto primitive=std::make_shared<TypedDPAttributeData<T>>(
        std::vector<T>{value,fallback},fallback);
    DGetAbcDMPFaceAttribute<T>(meshes,primitive,output);
    assert(output.size()==6);
    for(std::size_t i=0;i<output.size();++i)
        assert(equal(output[i],i<2?convertedValue:convertedDefault));
    present->faces=0;DGetAbcDMPFaceAttribute<T>(meshes,primitive,output);
    assert(output.size()==4&&equal(output[0],convertedDefault));
}

int main() {
    roundTrip<bool, std::uint8_t>(true, false);
    roundTrip<std::int32_t, std::int32_t>(7, -3);
    roundTrip<std::uint32_t, std::uint32_t>(4000000000U, 3U);
    roundTrip<std::int64_t, std::int64_t>(INT64_MAX, INT64_MIN);
    roundTrip<float, float>(2.5f, -1.5f);
    roundTrip<double, double>(2.5, -1.5);
    roundTrip<Imath::V2i, Imath::V2i>(Imath::V2i(2,3), Imath::V2i(-1,-2));
    roundTrip<Imath::V2f, Imath::V2f>(Imath::V2f(2,3), Imath::V2f(-1,-2));
    roundTrip<Imath::V2d, Imath::V2d>(Imath::V2d(2,3), Imath::V2d(-1,-2));
    roundTrip<Imath::V3i, Imath::V3i>(Imath::V3i(2,3,4), Imath::V3i(-1,-2,-3));
    roundTrip<Imath::V3f, Imath::V3f>(Imath::V3f(2,3,4), Imath::V3f(-1,-2,-3));
    roundTrip<Imath::V3d, Imath::V3d>(Imath::V3d(2,3,4), Imath::V3d(-1,-2,-3));
    auto present = owner<int>({7}, 0);
    DMeshData meshes{{present}};
    std::vector<int> output{91};
    for (std::size_t count : {0U, 2U, 10000001U}) {
        present->count = count;
        rejected([&] { DGetAbcDMPScalarAttribute<int>(meshes, DP_ATTR_INT, "field", output); });
        assert(output == std::vector<int>{91});
    }
    present->count = 1U;
    meshes.owners.push_back(std::shared_ptr<DPData>());
    rejected([&] { DGetAbcDMPScalarAttribute<int>(meshes, DP_ATTR_INT, "field", output); });
    assert(output == std::vector<int>{91});
    present->count=1;present->faces=1;meshes.owners={present};
    for(int count:{-1,2000001}) {
        present->faces=count;
        rejected([&]{DGetAbcDMPFaceAttribute<int>(meshes,present->getAttr("field"),output);});
        assert(output==std::vector<int>{91});
    }
    present->faces=2;
    rejected([&]{DGetAbcDMPFaceAttribute<double>(meshes,present->getAttr("field"),output);});
    rejected([&]{DGetAbcDMPFaceAttribute<int>(meshes,DPAttributeData::ConstPtr(),output);});
    meshes.owners.push_back(present);
    rejected([&]{DGetAbcDMPFaceAttribute<int>(meshes,present->getAttr("field"),output);});
    meshes.owners={present,std::shared_ptr<DPData>()};
    rejected([&]{DGetAbcDMPFaceAttribute<int>(meshes,present->getAttr("field"),output);});
    assert(output==std::vector<int>{91});
    meshes.owners = {present, owner<float>({1}, 0)};
    rejected([&] { DGetAbcDMPScalarAttribute<int>(meshes, DP_ATTR_INT, "field", output); });
    assert(output == std::vector<int>{91});
    rejected([&] { DGetAbcDPScalarAttribute<int>(nullptr, DP_ATTR_INT, "field", output); });
    rejected([&] { DGetAbcDCPScalarAttribute<int>(nullptr, DP_ATTR_INT, "field", output); });
    auto huge = owner<double>({std::numeric_limits<double>::max()}, 0);
    std::vector<float> floats{91};
    rejected([&] { DGetAbcDPScalarAttribute<double>(huge.get(), DP_ATTR_DOUBLE, "field", floats); });
    assert(floats == std::vector<float>{91});
    auto nan = owner<double>({std::numeric_limits<double>::quiet_NaN()}, 0);
    rejected([&] { DGetAbcDPScalarAttribute<double>(nan.get(), DP_ATTR_DOUBLE, "field", floats); });
    meshes.owners={nan};
    rejected([&] { DGetAbcDMPFaceAttribute<double>(meshes,nan->getAttr("field"),floats); });
    meshes.owners={huge};
    rejected([&] { DGetAbcDMPFaceAttribute<double>(meshes,huge->getAttr("field"),floats); });
    assert(floats==std::vector<float>{91});
    std::cout << "Alembic field alignment, 12 types, bounds and transactional rejection passed\n";
}
