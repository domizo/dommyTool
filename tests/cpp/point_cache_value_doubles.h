#pragma once
// Typed DP attributes, validation and budgets are production code. The point
// owner and OpenVDB leaf/attribute APIs below are explicit host-free doubles.
#include "common/utility/DAttributeArrayConversion.h"
#include <maya/MString.h>
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>

using DPoint = Imath::V3f;
struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const {
        return ids;
    }
};
struct DPData {
    std::size_t count = 2U;
    bool valid = true;
    DPAttrList attributes;
    std::map<std::string, std::shared_ptr<Group>> groups;
    std::size_t numberOfPoints() const {
        return count;
    }
    bool hasValidPointTopology() const {
        return valid;
    }
    auto beginAttr() const -> decltype(attributes.begin()) {
        return attributes.begin();
    }
    auto endAttr() const -> decltype(attributes.end()) {
        return attributes.end();
    }
    auto beginGrp() const -> decltype(groups.begin()) {
        return groups.begin();
    }
    auto endGrp() const -> decltype(groups.end()) {
        return groups.end();
    }
    DPAttributeData::ConstPtr getAttr(const std::string& key) const {
        const auto found = attributes.find(key);
        return found == attributes.end() ? nullptr : found->second;
    }
    void addAttr(const std::string& key, const DPAttributeData& value) {
        attributes[key] = value.copy();
    }
};
struct MStringArray {
    std::vector<MString> data;
    unsigned int length() const {
        return data.size();
    }
    const MString& operator[](unsigned int index) const {
        return data.at(index);
    }
};

namespace openvdb {
using Index = uint32_t;
using Index32 = uint32_t;
using Vec3R = Imath::V3d;
struct Mat4d {
    double data[16] = {};
    double* operator[](unsigned int row) {
        assert(row < 4);
        return data + row * 4;
    }
    const double* operator[](unsigned int row) const {
        assert(row < 4);
        return data + row * 4;
    }
};
struct Metadata {
    virtual ~Metadata() {}
};
template <typename T> struct TypedMetadata : Metadata {
    T value;
    explicit TypedMetadata(const T& value) : value(value) {}
    std::shared_ptr<Metadata> copy() const {
        return std::make_shared<TypedMetadata>(*this);
    }
};
struct NamePair {};
namespace points {
struct NullCodec {};
struct AttributeArray {
    virtual ~AttributeArray() {}
    template <typename T> bool isType() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
};
template <typename T, typename Codec> struct TypedAttributeArray : AttributeArray {
    std::vector<T> values;
    static NamePair attributeType() {
        return {};
    }
};
template <typename T> struct AttributeHandle {
    const TypedAttributeArray<T, NullCodec>& array;
    explicit AttributeHandle(const AttributeArray& array)
        : array(dynamic_cast<const TypedAttributeArray<T, NullCodec>&>(array)) {}
    T get(std::size_t i) const {
        return array.values.at(i);
    }
};
struct AttributeSet {
    static const std::size_t INVALID_POS = static_cast<std::size_t>(-1);
    std::map<std::string, std::shared_ptr<AttributeArray>> arrays;
    std::map<std::string, std::shared_ptr<Metadata>> defaults;
    std::size_t find(const std::string& name) const {
        return arrays.count(name) ? 0U : INVALID_POS;
    }
    const AttributeSet& descriptor() const {
        return *this;
    }
    template <typename T> T getDefaultValue(const std::string& name) const {
        return dynamic_cast<const TypedMetadata<T>&>(*defaults.at(name)).value;
    }
};
struct IndexIter {
    std::size_t index, count;
    explicit operator bool() const {
        return index < count;
    }
    std::size_t operator*() const {
        return index;
    }
    IndexIter& operator++() {
        ++index;
        return *this;
    }
};
struct Leaf {
    AttributeSet attributes;
    std::size_t count = 2;
    const AttributeArray& constAttributeArray(const std::string& name) const {
        return *attributes.arrays.at(name);
    }
    const AttributeSet& attributeSet() const {
        return attributes;
    }
    IndexIter beginIndexOn() const {
        return {0U, count};
    }
};
struct LeafIter {
    const Leaf* leaf;
    explicit operator bool() const {
        return leaf != nullptr;
    }
    const Leaf* operator->() const {
        assert(leaf);
        return leaf;
    }
    LeafIter& operator++() {
        leaf = nullptr;
        return *this;
    }
};
struct PointDataTree {
    Leaf leaf;
    bool hasLeaf = true;
    LeafIter cbeginLeaf() const {
        return {hasLeaf ? &leaf : nullptr};
    }
};
struct PointDataGrid {
    using Ptr = std::shared_ptr<PointDataGrid>;
    PointDataTree data;
    PointDataTree& tree() {
        return data;
    }
    const PointDataTree& tree() const {
        return data;
    }
};
template <typename T> struct PointAttributeVector {
    using Value = T;
    const std::vector<T>& values;
    explicit PointAttributeVector(const std::vector<T>& values) : values(values) {}
};
inline void appendAttribute(PointDataTree& tree, const std::string& name, const NamePair&, int,
                            bool, std::shared_ptr<Metadata> value) {
    tree.leaf.attributes.defaults[name] = value;
}
} // namespace points
namespace tools {
struct PointIndexTree {
    std::vector<std::size_t> order{1U, 0U};
};
struct PointIndexGrid {
    using Ptr = std::shared_ptr<PointIndexGrid>;
    PointIndexTree data;
    PointIndexTree& tree() {
        return data;
    }
};
} // namespace tools
namespace points {
template <typename Tree, typename IndexTree, typename Wrapper>
void populateAttribute(Tree& tree, const IndexTree& indices, const std::string& name,
                       const Wrapper& wrapper) {
    auto array = std::make_shared<TypedAttributeArray<typename Wrapper::Value, NullCodec>>();
    for (auto id : indices.order)
        array->values.push_back(wrapper.values.at(id));
    tree.leaf.attributes.arrays[name] = array;
}
} // namespace points
} // namespace openvdb

template <typename T>
using VDBArray = openvdb::points::TypedAttributeArray<T, openvdb::points::NullCodec>;
using DVDBAttrBool = VDBArray<bool>;
using DVDBAttrInt = VDBArray<int32_t>;
using DVDBAttrInt32 = VDBArray<uint32_t>;
using DVDBAttrInt64 = VDBArray<int64_t>;
using DVDBAttrFloat = VDBArray<float>;
using DVDBAttrDouble = VDBArray<double>;
using DVDBAttrVec2i = VDBArray<Imath::V2i>;
using DVDBAttrVec2f = VDBArray<Imath::V2f>;
using DVDBAttrVec2d = VDBArray<Imath::V2d>;
using DVDBAttrVec3i = VDBArray<Imath::V3i>;
using DVDBAttrVec3f = VDBArray<Imath::V3f>;
using DVDBAttrVec3d = VDBArray<Imath::V3d>;
using DVDBAttrString = VDBArray<std::string>;
using DVDBAttrMat = VDBArray<openvdb::Mat4d>;
