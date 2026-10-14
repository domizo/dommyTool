#pragma once

#include <SeExpression.h>
#include <cmath>
#include <type_traits>

namespace dommy {
namespace expression {

struct ScalarVariable : public SeExprScalarVarRef {
    double val = 0.0;
    void eval(const SeExprVarNode*, SeVec3d& result) override {
        result = SeVec3d(val, val, val);
    }
};

struct VectorVariable : public SeExprVectorVarRef {
    SeVec3d val = SeVec3d(0.0, 0.0, 0.0);
    void eval(const SeExprVarNode*, SeVec3d& result) override {
        result = val;
    }
};

// UserVector attributes are double-backed. Validate the entire input before
// binding any variable, and do not narrow through the float point-storage type.
template <typename Expression, typename Vector>
bool setUserVectors(Expression& expression, const Vector (&values)[4]) {
    static_assert(std::is_same<typename std::decay<decltype(values[0][0])>::type, double>::value,
                  "UserVector inputs must retain double components");
    SeVec3d candidates[4];
    for (unsigned int index = 0U; index < 4U; ++index) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!std::isfinite(values[index][axis]))
                return false;
            candidates[index][axis] = values[index][axis];
        }
    }
    expression.setUserVector1(candidates[0]);
    expression.setUserVector2(candidates[1]);
    expression.setUserVector3(candidates[2]);
    expression.setUserVector4(candidates[3]);
    return true;
}

} // namespace expression
} // namespace dommy
