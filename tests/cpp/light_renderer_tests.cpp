#include "arnold_export_host.h"
#include "light_transform_methods.inc"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DStatusOperation.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "integrations/arnold/DArnoldResources.h"

bool accepted = false;
unsigned int retireCalls = 0U;
MMatrix submittedMatrix;
static_assert(DP_MAT_APPLY_NONE == 0 && DP_MAT_FULL_MAT_IGNORE_ALL == 1 &&
    DP_MAT_FULL_MAT_IGNORE_DIR == 2 && DP_MAT_PRE_TRANSLATE == 3 &&
    DP_MAT_LOOKAT == 4 && DP_MAT_APPLY_TYPE_COUNT == 5, "archived Light mode values");

#ifndef DOMMY_SDK_CHECK
void AiMsgError(const char*, ...) { ++reports; }
struct AtNode {};
AtNode* AiNode(const char*) { ++liveNodes; return new AtNode; }
bool AiNodeDestroy(AtNode* node) { --liveNodes; delete node; return true; }
bool failLink = false;
bool AiNodeLink(AtNode*, const char*, AtNode*) { return !failLink; }
#endif

#include "light_renderer_methods.inc"

#ifndef DOMMY_SDK_CHECK
void resetStage() {
    assert(liveNodes == 0);
    operations.clear(); failOperation = throwOperation = -1;
    accepted = false; reports = 0; retireCalls = 0U;
    submittedMatrix = MMatrix(); submittedMatrix[3][0] = 123;
}

void sharedTransformTransactions() {
    const MVector position(1,2,3),direction(0,0,-1),scale(2,3,4),target(10,20,30);
    MMatrix animation; animation[3][0] = 10;
    for (int mode=0; mode<DP_MAT_APPLY_TYPE_COUNT; ++mode) {
        resetStage(); MMatrix output; output[3][0] = 123;
        assert(dommy::light::buildTransform(3,mode,position,direction,scale,1,animation,target,output));
        const auto events = operations;
        for (std::size_t i=0; i<events.size(); ++i) {
            if (events[i] != "transform.scale" && events[i] != "transform.rotation" &&
                events[i] != "transform.translate" && events[i] != "transform.getTranslation") continue;
            for (auto error : {MS::kFailure, MS::kInvalidParameter}) {
                resetStage(); output = MMatrix(); output[3][0] = 123;
                failOperation = int(i); injectedApiStatus = error;
                assert(dommy::light::buildTransform(3,mode,position,direction,scale,1,animation,target,output) == error);
                assert(output[3][0] == 123 && output[0][0] == 1);
            }
        }
        for (std::size_t i=0; i<events.size(); ++i) for (int kind : {0,1}) {
            resetStage(); output = MMatrix(); output[3][0] = 123;
            throwOperation = int(i); exceptionKind = kind;
            bool threw = false;
            try { (void)dommy::light::buildTransform(3,mode,position,direction,scale,1,animation,target,output); }
            catch (const std::exception&) { threw = true; }
            assert(threw && output[3][0] == 123 && output[0][0] == 1);
        }
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    resetStage(); MMatrix output;
    // Mode-local admission: unused inputs are not evaluated by the builder.
    assert(dommy::light::buildTransform(3,DP_MAT_FULL_MAT_IGNORE_ALL,
        MVector(nan,0,0),MVector(nan,0,0),MVector(nan,0,0),nan,animation,MVector(nan,0,0),output));
    assert(operations.empty() && output[3][0] == 10);
    assert(dommy::light::buildTransform(3,DP_MAT_FULL_MAT_IGNORE_DIR,
        position,MVector(nan,0,0),scale,1,animation,MVector(nan,0,0),output));
    MMatrix unused; unused[0][0] = nan;
    assert(dommy::light::buildTransform(3,DP_MAT_APPLY_NONE,
        position,direction,scale,1,unused,MVector(nan,0,0),output));
    // The viewport keeps Double range; only the renderer sink narrows to Float.
    MMatrix wide; wide[0][0] = 1e100;
    assert(dommy::light::buildTransform(3,DP_MAT_FULL_MAT_IGNORE_ALL,
        position,direction,scale,1,wide,target,output) && output[0][0] == 1e100);
    for (int condition=0; condition<7; ++condition) {
        resetStage(); output = MMatrix(); output[3][0] = 123;
        const MStatus status = dommy::light::buildTransform(
            condition == 0 ? -1 : condition == 1 ? 7 : 3,
            condition == 2 ? -1 : condition == 3 ? DP_MAT_APPLY_TYPE_COUNT : DP_MAT_PRE_TRANSLATE,
            condition == 4 ? MVector(nan,0,0) : position,
            condition == 5 ? MVector(0,0,0) : direction,
            condition == 6 ? MVector(1e300,1,1) : scale,
            condition == 6 ? 1e100 : 1,animation,target,output);
        assert(status == MS::kInvalidParameter && output[3][0] == 123 && output[0][0] == 1);
    }
}

int main(int argc, char** argv) {
    const std::string phase = argc > 1 ? argv[1] : "all";
    if (phase == "all" || phase == "status") {
        sharedTransformTransactions();
        unsigned int failures = 0U;
        MMatrix animation; animation[3][0] = 10;
        for (int type=0; type<=6; ++type) for (int mode=0; mode<DP_MAT_APPLY_TYPE_COUNT; ++mode) {
            resetStage(); matrixStage(mode,type,2,1,animation); assert(accepted);
            const double expectedX = mode == DP_MAT_FULL_MAT_IGNORE_ALL ? 10 :
                mode == DP_MAT_APPLY_NONE ? 1 : 11;
            assert(submittedMatrix[3][0] == expectedX);
            assert(submittedMatrix[0][0] == (type > 2 && mode != DP_MAT_FULL_MAT_IGNORE_ALL ? 2 : 1));
            assert(submittedMatrix[1][1] == (type > 2 && mode != DP_MAT_FULL_MAT_IGNORE_ALL ? 3 : 1));
            assert(submittedMatrix[2][2] == (type > 2 && mode != DP_MAT_FULL_MAT_IGNORE_ALL ? 4 : 1));
            const auto events = operations;
            for (std::size_t i=0; i<events.size(); ++i) {
                if (events[i] != "transform.scale" && events[i] != "transform.rotation" &&
                    events[i] != "transform.translate" && events[i] != "transform.getTranslation") continue;
                for (auto error : {MS::kFailure, MS::kInvalidParameter}) {
                    resetStage(); failOperation = int(i); injectedApiStatus = error;
                    matrixStage(mode,type,2,1,animation);
                    assert(!accepted && reports == 1 && submittedMatrix[3][0] == 123);
                    assert(operations.size() == i+1U); ++failures;
                }
                resetStage(); matrixStage(mode,type,2,1,animation); assert(accepted);
            }
        }
        assert(failures > 100U);
    }
    if (phase == "all" || phase == "ignore") {
        resetStage(); matrixStage(DP_MAT_FULL_MAT_IGNORE_ALL,3,1e300,1e20f,MMatrix());
        assert(accepted && submittedMatrix[0][0] == 1);
        assert(operations.empty());
    }
    if (phase == "all" || phase == "link") {
        for (auto stage : {slidemapStage, colorStage}) {
            resetStage(); failLink = true; stage();
            assert(!accepted && reports == 1 && retireCalls == 0U && liveNodes == 0);
            resetStage(); failLink = false; stage();
            assert(accepted && reports == 0 && retireCalls == 1U && liveNodes == 0);
        }
    }
}
#endif
