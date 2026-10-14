#pragma once

#include "common/utility/DMeshMergePlan.h"

namespace dommy {
namespace curve {

// Count-only admission while generators retain their staged curve objects.
// The same geometry estimate is used by collection publication. This is not
// a process-memory cap; publication still validates the actual copied payload.
class CurveBatchBudget final {
public:
    bool append(std::size_t points, std::size_t knots,
        std::size_t componentWork, std::size_t componentStrings) {
        CurveBatchBudget candidate = *this;
        std::size_t work = 0U;
        if (candidate.plan_.meshes >= generation::kMaxGeneratedCurveCount ||
            !mesh::appendMeshMergePlan(points, 0U, knots, 0U, candidate.plan_) ||
            !mesh::validMeshOperationStorage(candidate.plan_) ||
            !generation::checkedSum(candidate.componentWork_, componentWork,
                generation::kMaxWorkItemCount, candidate.componentWork_) ||
            !generation::checkedSum(candidate.componentWork_, candidate.plan_.workItems,
                generation::kMaxWorkItemCount, work) ||
            !generation::checkedSum(candidate.componentStrings_, componentStrings,
                generation::kMaxGeneratedStringByteCount, candidate.componentStrings_)) return false;
        *this = candidate;
        return true;
    }

private:
    mesh::MeshMergePlan plan_;
    std::size_t componentWork_ = 0U, componentStrings_ = 0U;
};

} // namespace curve
} // namespace dommy
