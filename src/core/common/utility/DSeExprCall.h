#pragma once

#include "DAttributeLookup.h"
#include "DGenerationLimits.h"

namespace dommy {
namespace expression {

// A function helper is shared by all calls of that function in one expression.
// Keep argument names on the AST call, never in the shared helper's state.
template <typename Node> bool readCallName(const Node* node, int argumentCount, std::string& name) {
    if (!node || node->numChildren() != argumentCount || !node->isStrArg(0))
        return false;
    const std::string candidate = node->getStrArg(0);
    if (!attribute::validAttributeName(candidate) ||
        candidate.size() > generation::kMaxGeneratedStringByteCount)
        return false;
    name = candidate;
    return true;
}

template <typename Node> bool prepareNamedCall(Node* node, int argumentCount, bool wantVector) {
    std::string name;
    if (!readCallName(node, argumentCount, name)) {
        if (node)
            node->addError("Invalid argument count or attribute/group name.");
        return false;
    }
    return argumentCount == 1 || node->child(1)->prep(wantVector);
}

} // namespace expression
} // namespace dommy
