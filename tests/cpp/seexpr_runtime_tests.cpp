#include "common/utility/DExpressionEvaluation.h"
#include "common/utility/DSeExprVariables.h"
#include <SeExprFunc.h>
#include <SeExprNode.h>

#include <cassert>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

// Test the pinned parser/assignment/evaluation implementation without its
// platform-specific noise built-ins or environment-driven plug-in loading.
void SeExprFunc::init() {}
const SeExprFunc* SeExprFunc::lookup(const std::string&) {
    return nullptr;
}
bool SeExprFuncX::prep(SeExprFuncNode*, bool) {
    std::abort();
} // Every test function overrides prep.

namespace {

class Expression : public SeExpression {
    struct FailureFunction : SeExprFuncX {
        bool& failed;
        explicit FailureFunction(bool& state) : SeExprFuncX(true), failed(state) {}
        bool prep(SeExprFuncNode* node, bool) override {
            node->setIsVec(true);
            return node->numChildren() == 0;
        }
        void eval(const SeExprFuncNode*, SeVec3d& result) const override {
            failed = true;
            result = SeVec3d(0, 0, 0);
        }
    };
    bool failed = false;
    FailureFunction failure;
    mutable SeExprFunc function;
    mutable dommy::expression::ScalarVariable id, frame, time;
    mutable dommy::expression::VectorVariable position;
    mutable dommy::expression::VectorVariable userVectors[4];

  public:
    explicit Expression(const std::string& source)
        : SeExpression(source), failure(failed), function(failure, 0, 0) {}
    bool evaluationSucceeded() const {
        return !failed;
    }
    void setUserVector1(const SeVec3d& value) { userVectors[0].val = value; }
    void setUserVector2(const SeVec3d& value) { userVectors[1].val = value; }
    void setUserVector3(const SeVec3d& value) { userVectors[2].val = value; }
    void setUserVector4(const SeVec3d& value) { userVectors[3].val = value; }
    SeExprVarRef* resolveVar(const std::string& name) const override {
        if (name == "id")
            return &id;
        if (name == "frame")
            return &frame;
        if (name == "time")
            return &time;
        if (name == "P")
            return &position;
        for (unsigned int index = 0U; index < 4U; ++index)
            if (name == "userVector" + std::to_string(index + 1U))
                return &userVectors[index];
        return nullptr;
    }
    SeExprFunc* resolveFunc(const std::string& name) const override {
        return name == "fail" ? &function : nullptr;
    }
};

void testDoubleUserVectors() {
    using namespace dommy::expression;
    for (unsigned int index = 0U; index < 4U; ++index) {
        const std::string name = "$userVector" + std::to_string(index + 1U);
        Expression vector(name), scalar(name + "[0]");
        assert(vector.isValid() && scalar.isValid());
        std::array<double, 3> inputs[4] = {
            {{16777217.0, 0.25, -1e100}}, {{1e100, 16777219.0, -0.5}},
            {{-16777217.0, 1e100, 0.75}}, {{1e-100, -16777219.0, 1e100}}};
        assert(setUserVectors(vector, inputs) && setUserVectors(scalar, inputs));
        SeVec3d value(0, 0, 0);
        assert(evaluateChecked(vector, value));
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            assert(value[axis] == inputs[index][axis]);
        const SeVec3d previous = value;
        assert(evaluateChecked(scalar, value) && value[0] == inputs[index][0]);

        // Failure in the last input must not partially update earlier slots.
        inputs[0][0] = 99;
        for (const double invalid : {std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()}) {
            inputs[3][2] = invalid;
            assert(!setUserVectors(vector, inputs));
            assert(evaluateChecked(vector, value));
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                assert(value[axis] == previous[axis]);
        }
    }
}

} // namespace

int main() {
    using namespace dommy::expression;
    testDoubleUserVectors();
    Expression defaults("$id + $frame + $time + $P");
    assert(defaults.isValid());
    SeVec3d value(9, 8, 7);
    assert(evaluateChecked(defaults, value));
    assert(value[0] == 0 && value[1] == 0 && value[2] == 0);

    Expression scalar("$scale = 2; $scale");
    assert(scalar.isValid() && evaluateChecked(scalar, value));
    const auto& raw = scalar.getLocalVars().at("scale");
    assert(value[0] == 2 && value[1] == 2 && value[2] == 2);
    assert(raw.val[0] == 2 && raw.val[1] == 0 && raw.val[2] == 0);
    assert(readScale(raw, value));
    assert(value[0] == 2 && value[1] == 2 && value[2] == 2);

    Expression vector("$scale = [-2, 3, 0]; $scale");
    assert(vector.isValid() && evaluateChecked(vector, value));
    assert(readScale(vector.getLocalVars().at("scale"), value));
    assert(value[0] == -2 && value[1] == 3 && value[2] == 0);

    Expression missingScale("1");
    assert(missingScale.isValid() && evaluateChecked(missingScale, value));
    assert(missingScale.getLocalVars().count("scale") == 0);

    Expression failed("fail()");
    assert(failed.isValid());
    value = SeVec3d(9, 8, 7);
    assert(!evaluateChecked(failed, value));
    assert(value[0] == 9 && value[1] == 8 && value[2] == 7);

    Expression nonFinite("[1, 1 / 0, 3]");
    assert(nonFinite.isValid() && !evaluateChecked(nonFinite, value));
    assert(value[0] == 9 && value[1] == 8 && value[2] == 7);
    std::cout << "Pinned SeExpr runtime boundaries: OK\n";
}
