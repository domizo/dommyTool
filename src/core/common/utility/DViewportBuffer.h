#ifndef DOMMY_DVIEWPORT_BUFFER_H
#define DOMMY_DVIEWPORT_BUFFER_H

#include <maya/MHWGeometry.h>

#include <cstddef>
#include <limits>

namespace dommy {
namespace viewport {

// Visit the requested streams in SDK order without staging or allocating them.
// A rejected descriptor or visitor result stops traversal immediately; callers
// keep their retry state and must not publish/commit buffers after that failure.
// Exceptions propagate to the owning viewport callback's existing guard.
template <typename Visitor>
bool forEachVertexRequirement(
    const MHWRender::MGeometryRequirements& requirements,
    Visitor&& visitor)
{
    const auto& streams = requirements.vertexRequirements();
    const int count = streams.length();
    if (count < 0) return false;

    for (int index = 0; index < count; ++index) {
        MHWRender::MVertexBufferDescriptor stream;
        if (!streams.getDescriptor(index, stream) || !visitor(stream)) {
            return false;
        }
    }
    return true;
}

inline float* acquireFloatBuffer(
    MHWRender::MVertexBuffer* buffer,
    const MHWRender::MVertexBufferDescriptor& descriptor,
    const std::size_t scalarCount,
    const int expectedDimension,
    const bool writeOnly = true)
{
    // Callers write packed float tuples. Do not reinterpret a shader's smaller
    // integer elements or a differently sized tuple as that layout.
    if (!buffer || descriptor.dataType() != MHWRender::MGeometry::kFloat ||
        descriptor.dataTypeSize() != sizeof(float) ||
        expectedDimension < 1 || expectedDimension > 4) {
        return NULL;
    }

    const int descriptorDimension = descriptor.dimension();
    if (descriptorDimension != expectedDimension) {
        return NULL;
    }
    const std::size_t dimension = static_cast<std::size_t>(descriptorDimension);
    if (scalarCount % dimension != 0) {
        return NULL;
    }

    const std::size_t elementCount = scalarCount / dimension;
    if (elementCount > std::numeric_limits<unsigned int>::max()) {
        return NULL;
    }

    return static_cast<float*>(
        buffer->acquire(static_cast<unsigned int>(elementCount), writeOnly));
}

inline unsigned int* acquireIndexBuffer(
    MHWRender::MIndexBuffer* buffer,
    const std::size_t indexCount,
    const bool writeOnly = true)
{
    if (!buffer || indexCount == 0U ||
        indexCount > std::numeric_limits<unsigned int>::max()) {
        return NULL;
    }

    return static_cast<unsigned int*>(
        buffer->acquire(static_cast<unsigned int>(indexCount), writeOnly));
}

inline MHWRender::MIndexBuffer* createIndexBuffer(
    MHWRender::MGeometry& geometry,
    const std::size_t indexCount,
    unsigned int*& indices,
    const bool writeOnly = true)
{
    indices = NULL;
    if (indexCount == 0U ||
        indexCount > std::numeric_limits<unsigned int>::max()) {
        return NULL;
    }

    MHWRender::MIndexBuffer* const buffer = geometry.createIndexBuffer(
        MHWRender::MGeometry::kUnsignedInt32);
    if (!buffer) return NULL;

    indices = acquireIndexBuffer(buffer, indexCount, writeOnly);
    return indices ? buffer : NULL;
}

}  // namespace viewport
}  // namespace dommy

#endif  // DOMMY_DVIEWPORT_BUFFER_H
