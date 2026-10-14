#pragma once

#include "DGenerationLimits.h"
#include "DStatusOperation.h"
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MObject.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <string>
#include <vector>

namespace dommy {
namespace maya {

struct NameListBudget {
    std::size_t bytes = 0U, names = 0U, workItems = 0U;
};

struct NameListScan {
    const char* text = nullptr;
    std::size_t bytes = 0U, names = 0U;
    NameListBudget admitted;
};

// No token allocation. Maya's native split keeps its own result semantics;
// count all possible fields for that path, including delimiter-empty fields,
// rather than assuming the SDK trims them. This may conservatively reject an
// oversized whitespace-heavy list before invoking the SDK.
inline MStatus admitNameList(
    const MString& value, std::size_t repetitions,
    const NameListBudget& budget, bool countEmptyFields, NameListScan& scan)
{
    const std::size_t encodedBytes = value.length();
    if (encodedBytes > generation::kMaxGeneratedStringByteCount ||
        budget.bytes > generation::kMaxGeneratedStringByteCount ||
        budget.names > generation::kMaxGeneratedStringCount ||
        budget.workItems > generation::kMaxWorkItemCount) {
        return MS::kInvalidParameter;
    }
    const char* const text = value.asChar();
    if (!text) {
        return MS::kInvalidParameter;
    }
    const std::size_t remainingBytes =
        generation::kMaxGeneratedStringByteCount - budget.bytes;
    const std::size_t remainingNames = generation::kMaxGeneratedStringCount - budget.names;
    const std::size_t remainingWork = generation::kMaxWorkItemCount - budget.workItems;
    std::size_t bytes = 0U;
    std::size_t count = countEmptyFields && text[0] != '\0' ? 1U : 0U;
    if (count > remainingNames) {
        return MS::kInvalidParameter;
    }
    bool insideName = false;
    while (text[bytes] != '\0') {
        if (bytes >= encodedBytes || bytes >= remainingBytes || bytes >= remainingWork / 3U) {
            return MS::kInvalidParameter;
        }
        if (text[bytes] == ' ') {
            if (countEmptyFields) {
                if (count >= remainingNames) {
                    return MS::kInvalidParameter;
                }
                ++count;
            }
            insideName = false;
        } else if (!countEmptyFields && !insideName) {
            if (count >= remainingNames) {
                return MS::kInvalidParameter;
            }
            ++count;
            insideName = true;
        }
        ++bytes;
    }
    // length() describes asChar() bytes, not numChars(). Reject an embedded
    // terminator or inconsistent view before a native split can see more data.
    if (bytes != encodedBytes) {
        return MS::kInvalidParameter;
    }
    NameListBudget candidate = budget;
    std::size_t parseWork = 0U, repeatedWork = 0U;
    // Two scans and at most one copy of each byte, plus repeated name work.
    if (!generation::checkedSum(candidate.bytes, bytes,
            generation::kMaxGeneratedStringByteCount, candidate.bytes) ||
        !generation::checkedSum(candidate.names, count,
            generation::kMaxGeneratedStringCount, candidate.names) ||
        !generation::checkedProduct(bytes, 3U, generation::kMaxWorkItemCount, parseWork) ||
        !generation::checkedProduct(count, repetitions, generation::kMaxWorkItemCount, repeatedWork) ||
        !generation::checkedSum(candidate.workItems, parseWork,
            generation::kMaxWorkItemCount, candidate.workItems) ||
        !generation::checkedSum(candidate.workItems, repeatedWork,
            generation::kMaxWorkItemCount, candidate.workItems)) {
        return MS::kInvalidParameter;
    }
    NameListScan staged;
    staged.text = text;
    staged.bytes = bytes;
    staged.names = count;
    staged.admitted = candidate;
    scan = staged;
    return MS::kSuccess;
}

// Space-delimited renderer identifiers: ignore empty tokens, preserve bytes,
// duplicates and order. Caller output/budget commit together after allocation.
inline MStatus splitCheckedNonemptyNameList(
    const MString& value, std::size_t repetitions,
    NameListBudget& budget, std::vector<std::string>& names)
{
    NameListScan scan;
    const MStatus status = admitNameList(value, repetitions, budget, false, scan);
    if (status != MS::kSuccess) {
        return status;
    }
    std::vector<std::string> staged;
    staged.reserve(scan.names);
    std::size_t start = 0U;
    for (std::size_t end = 0U; end <= scan.bytes; ++end) {
        if (end == scan.bytes || scan.text[end] == ' ') {
            if (end != start) {
                staged.emplace_back(scan.text + start, end - start);
            }
            start = end + 1U;
        }
    }
    names.swap(staged);
    budget = scan.admitted;
    return MS::kSuccess;
}

// Call with fresh local arrays and discard all staged work on failure.
// Keep Maya's space-splitting semantics; do not reinterpret names as patterns.
inline MStatus splitCheckedNameList(const MString& value, std::size_t repetitions,
                                   NameListBudget& budget, MStringArray& names)
{
    NameListScan scan;
    DOMMY_RETURN_IF_FAILURE(admitNameList(value, repetitions, budget, true, scan));
    DOMMY_RETURN_IF_FAILURE(value.split(' ', names));
    const std::size_t actualCount = names.length();
    if (actualCount > scan.names) {
        return MS::kInvalidParameter;
    }
    // Native empty-field behavior must not change the returned array. Refund
    // conservative count/repetition admission; scan work is still charged.
    NameListBudget candidate = scan.admitted;
    const std::size_t unused = scan.names - actualCount;
    candidate.names -= unused;
    // The upper count x repetitions was already checked by admitNameList.
    candidate.workItems -= unused * repetitions;
    budget = candidate;
    return MS::kSuccess;
}

inline MStatus readCheckedNameList(MDataBlock& data, const MObject& attribute,
                                  std::size_t repetitions, NameListBudget& budget,
                                  MStringArray& names)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    return splitCheckedNameList(handle.asString(), repetitions, budget, names);
}

} // namespace maya
} // namespace dommy
