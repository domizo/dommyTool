#pragma once

#include <string>
#include "DAttributeLookup.h"
#include "DGenerationLimits.h"

namespace dommy {
namespace attribute {

// Commit a newly built, validated attribute to an unpublished owner. Unlike
// legacy addAttr, this deliberately permits replacing the stored type. The old
// field stays available to readers until the candidate is complete.
template <typename Owner, typename AttributePointer>
bool commitStagedAttribute(Owner& owner, const std::string& name, AttributePointer candidate,
                           std::size_t expectedCount) {
    if (!validAttributeName(name) || name.size() > generation::kMaxGeneratedStringByteCount ||
        expectedCount > generation::kMaxGeneratedPointCount || !candidate ||
        candidate->attrSize() != expectedCount)
        return false;
    for (auto it = owner.beginAttr(); it != owner.endAttr(); ++it) {
        if (it->first == name) {
            it->second.swap(candidate);
            return true;
        }
    }
    owner.addAttr(name, *candidate);
    const auto written = owner.getAttr(name);
    return written && written->typeName() == candidate->typeName() &&
           written->attrSize() == expectedCount;
}

// Owners expose getAttr/addAttr/removeAttr. addAttr is a legacy void API:
// it silently ignores incompatible types, so preflight and verify its copy
// before removing anything. Owners must be local, staged output data.
template <typename Owner, typename Attribute>
bool publishAttributeReplacement(Owner& owner, const std::string& sourceName,
                                 const std::string& outputName, const Attribute& replacement,
                                 bool deleteOriginal) {
    if (!validAttributeName(sourceName) || !validAttributeName(outputName)) {
        return false;
    }
    const auto source = owner.getAttr(sourceName);
    const auto previous = owner.getAttr(outputName);
    if (!source || source->attrSize() != replacement.attrSize() ||
        (previous && previous->typeName() != replacement.typeName())) {
        return false;
    }

    owner.addAttr(outputName, replacement);
    const auto written = owner.getAttr(outputName);
    if (!written || written == previous || written->typeName() != replacement.typeName() ||
        written->attrSize() != replacement.attrSize()) {
        return false;
    }
    if (deleteOriginal && sourceName != outputName) {
        owner.removeAttr(sourceName);
    }
    return true;
}

// A blend has two inputs. Neither may be removed before a checked write, and
// output aliases of either input must remain available.
template <typename Owner, typename Attribute>
bool publishBlendedAttribute(Owner& owner, const std::string& sourceName,
                             const std::string& targetName, const std::string& outputName,
                             const Attribute& replacement, bool deleteOriginal) {
    if (!validAttributeName(targetName))
        return false;
    const auto target = owner.getAttr(targetName);
    if (!target || target->attrSize() != replacement.attrSize())
        return false;
    if (!publishAttributeReplacement(owner, sourceName, outputName, replacement, false))
        return false;
    if (deleteOriginal) {
        if (sourceName != outputName)
            owner.removeAttr(sourceName);
        if (targetName != outputName && targetName != sourceName)
            owner.removeAttr(targetName);
    }
    return true;
}

} // namespace attribute
} // namespace dommy
