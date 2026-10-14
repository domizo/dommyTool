#pragma once

#include <cstddef>
#include <vector>

namespace MHWRender {

struct MGeometryTypes {
    enum DataType { kFloat, kDouble, kChar, kUnsignedChar, kInt16,
                    kUnsignedInt16, kInt32, kUnsignedInt32 };
};

class MVertexBufferDescriptor
{
public:
    explicit MVertexBufferDescriptor(int dimension = 0,
        MGeometryTypes::DataType type = MGeometryTypes::kFloat)
        : dimension_(dimension), type_(type) {}

    int dimension() const { return dimension_; }
    MGeometryTypes::DataType dataType() const { return type_; }
    unsigned int dataTypeSize() const {
        return type_ == MGeometryTypes::kDouble ? 8U :
               (type_ == MGeometryTypes::kChar || type_ == MGeometryTypes::kUnsignedChar
                    ? 1U : (type_ == MGeometryTypes::kInt16 ||
                            type_ == MGeometryTypes::kUnsignedInt16 ? 2U : 4U));
    }

private:
    int dimension_;
    MGeometryTypes::DataType type_;
};

class MVertexBufferDescriptorList
{
public:
    std::vector<MVertexBufferDescriptor> values;
    int length() const { return static_cast<int>(values.size()); }
    bool getDescriptor(int index, MVertexBufferDescriptor& result) const {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size()) return false;
        result = values[static_cast<std::size_t>(index)];
        return true;
    }
};

class MGeometryRequirements
{
public:
    MVertexBufferDescriptorList streams;
    const MVertexBufferDescriptorList& vertexRequirements() const { return streams; }
};

class MVertexBuffer
{
public:
    MVertexBuffer() : lastCount_(0), writeOnly_(false) {}

    void* acquire(unsigned int count, bool writeOnly)
    {
        lastCount_ = count;
        writeOnly_ = writeOnly;
        return storage_;
    }

    unsigned int lastCount() const { return lastCount_; }
    bool writeOnly() const { return writeOnly_; }

private:
    unsigned int lastCount_;
    bool writeOnly_;
    float storage_[16];
};

class MIndexBuffer
{
public:
    MIndexBuffer() : lastCount_(0), writeOnly_(false) {}

    void* acquire(unsigned int count, bool writeOnly = true)
    {
        lastCount_ = count;
        writeOnly_ = writeOnly;
        return storage_;
    }

    unsigned int lastCount() const { return lastCount_; }
    bool writeOnly() const { return writeOnly_; }

private:
    unsigned int lastCount_;
    bool writeOnly_;
    unsigned int storage_[16];
};

class MGeometry : public MGeometryTypes
{
public:
    MIndexBuffer* createIndexBuffer(DataType) { return &indexBuffer_; }

private:
    MIndexBuffer indexBuffer_;
};

}  // namespace MHWRender
