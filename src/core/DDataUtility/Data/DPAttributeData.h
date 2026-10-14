// Project-owned typed attribute arrays. Public names and numeric tags are
// compatibility interfaces; storage/edit behavior is implemented here.
#ifndef DPAttributeData_h
#define DPAttributeData_h

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include <maya/MMatrix.h>
#include <ImathVec.h>

#include "DType.h"
#include "common/string/DStreamFormat.h"

enum DP_ATTR_DATA_TYPE {
    DP_ATTR_INVALID = -1,
    DP_ATTR_BOOL,
    DP_ATTR_INT,
    DP_ATTR_UINT32,
    DP_ATTR_INT32 = DP_ATTR_UINT32, // Historical alias, not a signed type.
    DP_ATTR_INT64,
    DP_ATTR_FLOAT,
    DP_ATTR_DOUBLE,
    DP_ATTR_VEC2I,
    DP_ATTR_VEC2F,
    DP_ATTR_VEC2D,
    DP_ATTR_VEC3I,
    DP_ATTR_VEC3F,
    DP_ATTR_VEC3D,
    DP_ATTR_STRING,
    DP_ATTR_MATRIX,
    DP_ATTR_INT_ARRAY
};

namespace dommy {
namespace attribute {
namespace detail {

// One catalog defines all three public policies: support, tag and initial value.
// Imath vectors need explicit zero components; their native default is not zero.
template <typename T> struct AttributeTypeInfo : std::false_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_INVALID; }
    static const char* name() { return typeid(T).name(); }
    static T initial() { return T(); }
};

template <> struct AttributeTypeInfo<bool> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_BOOL; }
    static const char* name() { return "bool"; }
    static bool initial() { return bool(); }
};

template <> struct AttributeTypeInfo<int32_t> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_INT; }
    static const char* name() { return "int"; }
    static int32_t initial() { return int32_t(); }
};

template <> struct AttributeTypeInfo<uint32_t> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_UINT32; }
    static const char* name() { return "uint32"; }
    static uint32_t initial() { return uint32_t(); }
};

template <> struct AttributeTypeInfo<int64_t> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_INT64; }
    static const char* name() { return "int64"; }
    static int64_t initial() { return int64_t(); }
};

template <> struct AttributeTypeInfo<float> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_FLOAT; }
    static const char* name() { return "float"; }
    static float initial() { return float(); }
};

template <> struct AttributeTypeInfo<double> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_DOUBLE; }
    static const char* name() { return "double"; }
    static double initial() { return double(); }
};

template <> struct AttributeTypeInfo<Imath::V2i> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC2I; }
    static const char* name() { return "vec2i"; }
    static Imath::V2i initial() { return Imath::V2i(0, 0); }
};

template <> struct AttributeTypeInfo<Imath::V2f> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC2F; }
    static const char* name() { return "vec2f"; }
    static Imath::V2f initial() { return Imath::V2f(0, 0); }
};

template <> struct AttributeTypeInfo<Imath::V2d> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC2D; }
    static const char* name() { return "vec2d"; }
    static Imath::V2d initial() { return Imath::V2d(0, 0); }
};

template <> struct AttributeTypeInfo<Imath::V3i> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC3I; }
    static const char* name() { return "vec3i"; }
    static Imath::V3i initial() { return Imath::V3i(0, 0, 0); }
};

template <> struct AttributeTypeInfo<Imath::V3f> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC3F; }
    static const char* name() { return "vec3f"; }
    static Imath::V3f initial() { return Imath::V3f(0, 0, 0); }
};

template <> struct AttributeTypeInfo<Imath::V3d> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_VEC3D; }
    static const char* name() { return "vec3d"; }
    static Imath::V3d initial() { return Imath::V3d(0, 0, 0); }
};

template <> struct AttributeTypeInfo<std::string> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_STRING; }
    static const char* name() { return "string"; }
    static std::string initial() { return std::string(); }
};

template <> struct AttributeTypeInfo<MMatrix> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_MATRIX; }
    static const char* name() { return "matrix"; }
    static MMatrix initial() { return MMatrix(); }
};

template <> struct AttributeTypeInfo<DIntArray> : std::true_type {
    static DP_ATTR_DATA_TYPE kind() { return DP_ATTR_INT_ARRAY; }
    static const char* name() { return "intarray"; }
    static DIntArray initial() { return DIntArray(); }
};

} // namespace detail
} // namespace attribute
} // namespace dommy

template <typename T>
struct DPAttributeTypeSupported : dommy::attribute::detail::AttributeTypeInfo<T> {};

template <typename T> inline const char* typeNameAsString() {
    static_assert(DPAttributeTypeSupported<T>::value, "Unsupported DP attribute data type");
    return dommy::attribute::detail::AttributeTypeInfo<T>::name();
}

template <typename T> inline T typeDefaultVal() {
    static_assert(DPAttributeTypeSupported<T>::value, "Unsupported DP attribute data type");
    return dommy::attribute::detail::AttributeTypeInfo<T>::initial();
}

class DPAttributeData {
public:
    using Ptr = std::shared_ptr<DPAttributeData>;
    using ConstPtr = std::shared_ptr<const DPAttributeData>;

    DPAttributeData() = default;
    virtual ~DPAttributeData() = default;

    // Polymorphic interface retained for every supported array element type.
    virtual std::string typeName() const = 0;
    virtual DPAttributeData::Ptr copy() const = 0;
    virtual void copy(const DPAttributeData& other) = 0;
    virtual std::string str() const = 0;
    virtual size_t attrSize() const = 0;
    virtual bool appendValues(const DPAttributeData& other) = 0;
    virtual void incrementDefaultValue(const size_t& count) = 0;
    virtual void incrementIndexElement(const size_t& index) = 0;
    virtual void insertDefaultValue(const size_t& count, const uint& index) = 0;
    virtual bool deleteArrayFromIds(const DPIdList& ids) = 0;
    virtual bool cropArrayFromIds(const DPIdList& ids) = 0;
    virtual bool sortArrayFromIds(const DPIndexList& ids) = 0;
    virtual bool remapFromIds(const DPIndexList& ids) = 0;
    virtual DP_ATTR_DATA_TYPE getAttrType() = 0;
    virtual DP_ATTR_DATA_TYPE getAttrType() const = 0;

private:
    DPAttributeData(const DPAttributeData&) = delete;
    DPAttributeData& operator=(const DPAttributeData&) = delete;
};

template <typename T> class TypedDPAttributeData : public DPAttributeData {
public:
    using Ptr = std::shared_ptr<TypedDPAttributeData<T>>;
    using ConstPtr = std::shared_ptr<const TypedDPAttributeData<T>>;

    TypedDPAttributeData() : m_type(typeTag()), m_defaultValue(typeDefaultVal<T>()) {}
    TypedDPAttributeData(const T& defValue) : m_type(typeTag()), m_defaultValue(defValue) {}
    TypedDPAttributeData(const T& value, const T& defValue, const uint size)
        : m_type(typeTag()), m_valueArray(size, value), m_defaultValue(defValue) {}
    TypedDPAttributeData(const std::vector<T>& values, const T& defValue)
        : m_type(typeTag()), m_valueArray(values), m_defaultValue(defValue) {}
    TypedDPAttributeData(const std::vector<T>& values)
        : m_type(typeTag()), m_valueArray(values), m_defaultValue(typeDefaultVal<T>()) {}
    TypedDPAttributeData(const TypedDPAttributeData<T>& other)
        : m_type(other.m_type), m_valueArray(other.m_valueArray),
          m_defaultValue(other.m_defaultValue) {}
    ~TypedDPAttributeData() override = default;

    void init(const std::vector<T>& values) { init(values, typeDefaultVal<T>()); }
    void init(const std::vector<T>& values, const T& defValue) {
        m_valueArray = values;
        m_defaultValue = defValue;
        generateAttrType();
    }

    std::string typeName() const override { return staticTypeName(); }
    static std::string staticTypeName() { return typeNameAsString<T>(); }
    static DPAttributeData::Ptr createDPAttribute() {
        return std::make_shared<TypedDPAttributeData<T>>();
    }
    DPAttributeData::Ptr copy() const override {
        return std::make_shared<TypedDPAttributeData<T> >(*this);
    }
    void copy(const DPAttributeData& other) override {
        const auto* source = dynamic_cast<const TypedDPAttributeData<T>*>(&other);
        if (!source || source == this) return;
        m_valueArray = source->value();
        m_defaultValue = source->defaultValue();
        m_type = source->getAttrType();
    }

    std::string str() const override {
        std::ostringstream ostr;
        ostr << typeName() << '[' << numberOfAttr() << ']';
        return dommy::text::checkedStreamText(ostr, "Attribute value description formatting failed");
    }
    size_t attrSize() const override { return numberOfAttr(); }
    size_t numberOfAttr() const { return m_valueArray.size(); }
    T getDefaultValue() const { return m_defaultValue; }
    const T& defaultValue() const { return m_defaultValue; }
    void generateDefaultVal() { m_defaultValue = typeDefaultVal<T>(); }
    void generateAttrType() { m_type = typeTag(); }
    DP_ATTR_DATA_TYPE getAttrType() override { return m_type; }
    DP_ATTR_DATA_TYPE getAttrType() const override { return m_type; }
    const std::vector<T>& value() const { return m_valueArray; }
    const std::vector<T>& getValueArray() const { return value(); }
    T getValue(const size_t& n) const { return m_valueArray.at(n); }

    void setAllValue(const T& val) {
        std::fill(m_valueArray.begin(), m_valueArray.end(), val);
    }
    bool setValueAt(std::size_t index, const T& value) {
        if (index >= numberOfAttr()) return false;
        try {
            m_valueArray[index] = value;
            return true;
        } catch (...) { return false; }
    }
    bool replaceValues(const std::vector<T>& values, std::size_t expectedSize) {
        if (values.size() != expectedSize) return false;
        try {
            std::vector<T> replacement(values);
            m_valueArray.swap(replacement);
            return true;
        } catch (...) { return false; }
    }
    bool appendValues(const DPAttributeData& other) override {
        const auto* source = dynamic_cast<const TypedDPAttributeData<T>*>(&other);
        if (!source) return false;
        appendRows(source->value());
        return true;
    }
    void appendOtherAttr(const TypedDPAttributeData::ConstPtr& other) {
        if (other) appendRows(other->value());
    }
    void incrementDefaultValue(const size_t& count) override { expandAttrSize(count, false); }
    void incrementIndexElement(const size_t& index) override {
        const T selected = index < numberOfAttr() ? getValue(index) : getDefaultValue();
        m_valueArray.push_back(selected);
    }
    void insertDefaultValue(const size_t& count, const uint& index) override {
        if (index > numberOfAttr() || count == 0) return;
        m_valueArray.insert(m_valueArray.begin() + index, count, m_defaultValue);
    }
    void expandAttrSize(const size_t& count, const bool& isFront) {
        if (count == 0) return;
        const auto insertion = isFront ? m_valueArray.begin() : m_valueArray.end();
        m_valueArray.insert(insertion, count, m_defaultValue);
    }

    bool deleteArrayFromIds(const DPIdList& ids) override { return selectRows(ids, false); }
    bool cropArrayFromIds(const DPIdList& ids) override { return selectRows(ids, true); }
    bool sortArrayFromIds(const DPIndexList& ids) override {
        return ids.size() == numberOfAttr() && remapFromIds(ids);
    }
    bool remapFromIds(const DPIndexList& ids) override {
        for (const auto index : ids) if (index >= numberOfAttr()) return false;
        std::vector<T> replacement;
        replacement.reserve(ids.size());
        for (const auto index : ids) replacement.push_back(getValue(index));
        m_valueArray.swap(replacement);
        return true;
    }

private:
    static DP_ATTR_DATA_TYPE typeTag() {
        static_assert(DPAttributeTypeSupported<T>::value, "Unsupported DP attribute data type");
        return dommy::attribute::detail::AttributeTypeInfo<T>::kind();
    }
    bool selectRows(const DPIdList& mask, bool keepSelected) {
        if (mask.size() != numberOfAttr()) return false;
        std::vector<T> replacement;
        replacement.reserve(numberOfAttr());
        for (size_t index = 0; index != numberOfAttr(); ++index)
            if (bool(mask[index]) == keepSelected) replacement.push_back(getValue(index));
        m_valueArray.swap(replacement);
        return true;
    }
    void appendRows(const std::vector<T>& rows) {
        // Inserting a range from the same vector is not a valid std::vector
        // operation. Snapshot only that aliasing case; normal appends stay lean.
        if (&rows == &m_valueArray) {
            const std::vector<T> snapshot(rows);
            m_valueArray.insert(m_valueArray.end(), snapshot.begin(), snapshot.end());
        } else {
            m_valueArray.insert(m_valueArray.end(), rows.begin(), rows.end());
        }
    }

    DP_ATTR_DATA_TYPE m_type;
    std::vector<T> m_valueArray;
    T m_defaultValue;
};

using DPAttrList = std::map<std::string, DPAttributeData::Ptr>;
using DPAttrIter = DPAttrList::iterator;
using ConstDPAttrIter = DPAttrList::const_iterator;

using DPBoolAttr = TypedDPAttributeData<bool>;
using DPIntAttr = TypedDPAttributeData<int32_t>;
using DPUInt32Attr = TypedDPAttributeData<uint32_t>;
using DPInt64Attr = TypedDPAttributeData<int64_t>;
using DPFloatAttr = TypedDPAttributeData<float>;
using DPDoubleAttr = TypedDPAttributeData<double>;
using DPVec2iAttr = TypedDPAttributeData<Imath::V2i>;
using DPVec2fAttr = TypedDPAttributeData<Imath::V2f>;
using DPVec2dAttr = TypedDPAttributeData<Imath::V2d>;
using DPVec3iAttr = TypedDPAttributeData<Imath::V3i>;
using DPVec3fAttr = TypedDPAttributeData<Imath::V3f>;
using DPVec3dAttr = TypedDPAttributeData<Imath::V3d>;
using DPStringAttr = TypedDPAttributeData<std::string>;
using DPMat4Attr = TypedDPAttributeData<MMatrix>;
using DPIntArrayAttr = TypedDPAttributeData<DIntArray>;

#endif // DPAttributeData_h
