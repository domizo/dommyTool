#pragma once

#include <maya/MStatus.h>
#include "DGenerationLimits.h"

#include <stdexcept>
#include <limits>
#include <utility>

namespace dommy {
namespace maya {

namespace detail {
template <typename Array>
auto copyArrayContents(const Array& source, Array& output, int)
    -> decltype(output.copy(source))
{
    return output.copy(source);
}

// MStringArray has no copy() status API. Its setLength/set operations do.
// A failure during publication can leave a partial destination, as can a
// failed copy() for the other Maya arrays; callers must discard that result.
template <typename Array>
auto copyArrayContents(const Array& source, Array& output, long)
    -> decltype(output.set(source[0U], 0U))
{
    if (&source == &output) return MS::kSuccess;
    const unsigned int count = source.length();
    MStatus status = output.setLength(count);
    if (status != MS::kSuccess) return status;
    if (output.length() != count) return MS::kFailure;
    for (unsigned int i = 0U; i < count; ++i) {
        status = output.set(source[i], i);
        if (status != MS::kSuccess) return status;
    }
    return MS::kSuccess;
}
} // namespace detail

template <typename Array>
MStatus copyCheckedArray(const Array& source, Array& output)
{
    const unsigned int count = source.length();
    const MStatus status = detail::copyArrayContents(source, output, 0);
    if (status != MS::kSuccess) return status;
    return output.length() == count ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

// Stage allocation and all element reads before copying to the caller's array.
// Maya reports allocation/copy failures with MStatus, not C++ exceptions.
template <typename Array, typename Fill>
MStatus buildCheckedArray(std::size_t count, Fill fill, Array& output)
{
    if (count > generation::kMaxGeneratedPointCount ||
        count > std::numeric_limits<unsigned int>::max()) return MS::kInvalidParameter;
    Array candidate;
    MStatus status = candidate.setLength(static_cast<unsigned int>(count));
    if (status != MS::kSuccess) return status;
    if (candidate.length() != count) return MS::kFailure;
    for (unsigned int i = 0U; i < candidate.length(); ++i) {
        status = fill(i, candidate[i]);
        if (status != MS::kSuccess) return status;
    }
    return copyCheckedArray(candidate, output);
}

// Adapter for legacy Maya-array builders that append incrementally. Maya's
// array append methods report allocation failure through MStatus; historical
// generators often discarded it. This adapter preserves the array interface
// while turning a discarded failure into an exception caught at the node's
// compute boundary.
template <typename Array>
class CheckedArray final {
public:
    explicit CheckedArray(unsigned int maximumLength =
            std::numeric_limits<unsigned int>::max())
        : maximumLength_(maximumLength) {}

    typedef decltype(std::declval<Array&>()[0U]) Reference;
    typedef decltype(std::declval<const Array&>()[0U]) ConstReference;

    unsigned int length() const { return values_.length(); }

    template <typename Value>
    MStatus append(const Value& value)
    {
        if (values_.length() >= maximumLength_) {
            throw std::length_error("Maya array exceeds its generation plan");
        }
        const MStatus status = values_.append(value);
        if (status != MS::kSuccess) {
            throw std::runtime_error("Maya array append failed");
        }
        return status;
    }

    MStatus setLength(unsigned int length)
    {
        if (length > maximumLength_) {
            throw std::length_error("Maya array exceeds its generation plan");
        }
        const MStatus status = values_.setLength(length);
        if (status != MS::kSuccess) {
            throw std::runtime_error("Maya array allocation failed");
        }
        return status;
    }

    Reference operator[](unsigned int index) { return values_[index]; }
    ConstReference operator[](unsigned int index) const {
        return values_[index];
    }

    operator Array&() { return values_; }
    operator const Array&() const { return values_; }

private:
    unsigned int maximumLength_;
    Array values_;
};

} // namespace maya
} // namespace dommy
