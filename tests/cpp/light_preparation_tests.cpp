#include "light_preparation_host.h"
#include "light_preparation_methods.inc"

void resetPreparation()
{
    operations.clear(); failOperation = throwOperation = failAttribute = -1;
    exceptionKind = reports = 0; connected = validInput = true;
}

void configurePreparation(int type=3, int mode=DP_MAT_APPLY_NONE)
{
    resetPreparation(); preparationControls.values.clear();
    preparationPoints = DPData();
    auto& values = preparationControls.values;
    values[DPLightGen::aPointSize.id].scalar = 3;
    values[DPLightGen::aLightType.id].scalar = type;
    values[DPLightGen::aLightVisType.id].scalar = 2;
    values[DPLightGen::aIntensityNormalize.id].scalar = 1;
    values[DPLightGen::aIntensity.id].scalar = 0.5;
    values[DPLightGen::aConeAngle.id].scalar = 45;
    values[DPLightGen::aBaseDirection.id].components[2] = -1;
    for (uint axis=0; axis<3; ++axis) {
        values[DPLightGen::aColor.id].components[axis] = 1;
        values[DPLightGen::aGlobalScale.id].components[axis] = 1;
    }
    values[DPLightGen::aApplyAnimMatType.id].scalar = mode;
    values[DPLightGen::aAnimMatrixAttribute.id].string = "matrix";
    MMatrix matrix; matrix[3][0] = 10;
    preparationPoints.field<MMatrix>("matrix","matrix",{matrix});
    operations.clear();
}

MUserData* prepare(MUserData* old=nullptr)
{
    DPLightGenOverride callback; MDagPath path; MHWRender::MFrameContext frame;
    return callback.prepareForDraw(path,path,frame,old);
}

void assertClean()
{
    assert(livePlugHandles == 0);
}

void ordinaryAndFailurePaths()
{
    for (int type=0; type<=6; ++type) for (int mode=0; mode<DP_MAT_APPLY_TYPE_COUNT; ++mode) {
        configurePreparation(type,mode);
        MUserData* value = prepare(); assert(value && liveUserData == 1);
        const auto& data = *dynamic_cast<DPLightGenData*>(value);
        assert(validLightDrawData(data) && data.matrixArray.length() == 1);
        const double expectedX = mode == DP_MAT_FULL_MAT_IGNORE_ALL ? 10 :
            mode == DP_MAT_APPLY_NONE ? 1 : 11;
        assert(data.matrixArray[0][3][0] == expectedX);
        const auto calls = operations;
        delete value; assert(liveUserData == 0); assertClean();
        for (std::size_t i=0; i<calls.size(); ++i) {
            for (int kind=0; kind<2; ++kind) {
                resetPreparation(); throwOperation = int(i); exceptionKind = kind;
                assert(prepare() == nullptr && reports == 1 && liveUserData == 0);
                assertClean();
            }
            if (calls[i] == "path.node" || calls[i] == "plug.handle" ||
                calls[i] == "point.handle" || calls[i] == "plug.read" ||
                calls[i] == "array.resize" || calls[i] == "array.copy" ||
                (calls[i].find("transform.") == 0 && calls[i] != "transform.matrix")) {
                resetPreparation(); failOperation = int(i);
                assert(prepare() == nullptr && liveUserData == 0);
                assertClean();
            }
        }
    }
    configurePreparation(); MUserData* cached = prepare(); assert(cached);
    resetPreparation(); assert(prepare(cached) == cached);
    const auto reusedCalls = operations;
    for (std::size_t i=0; i<reusedCalls.size(); ++i) {
        for (int kind=0; kind<2; ++kind) {
            resetPreparation(); throwOperation = int(i); exceptionKind = kind;
            assert(!prepare(cached) && liveUserData == 1 && reports == 1);
            assertClean();
            resetPreparation(); assert(prepare(cached) == cached);
        }
        if (reusedCalls[i] == "array.copy") {
            resetPreparation(); failOperation = int(i);
            assert(!prepare(cached) && liveUserData == 1);
            assertClean();
        }
        resetPreparation(); assert(prepare(cached) == cached);
        assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length() == 1);
    }
    resetPreparation(); connected = false;
    assert(prepare(cached) == cached && dynamic_cast<DPLightGenData*>(cached)->matrixArray.length() == 0);
    delete cached; assert(liveUserData == 0);
    configurePreparation(); cached=prepare(); assert(cached);
    resetPreparation(); assert(prepare(cached)==cached); const auto connectionCalls=operations;
    const auto query=std::find(connectionCalls.begin(),connectionCalls.end(),"plug.connected");
    assert(query!=connectionCalls.end());
    for(bool connectionValue:{false,true}) {
        resetPreparation();connected=connectionValue;failOperation=int(query-connectionCalls.begin());
        assert(!prepare(cached) && liveUserData==1);
        assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length()==1);
        assertClean();resetPreparation();assert(prepare(cached)==cached);
        for(int kind:{0,1}) {
            resetPreparation();failOperation=int(query-connectionCalls.begin());
            throwOperation=failOperation+1;exceptionKind=kind;
            assert(!prepare(cached) && liveUserData==1 && reports==1);
            assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length()==1);
            assertClean();
        }
    }
    delete cached;assert(liveUserData==0);
    configurePreparation(); validInput = false;
    assert(!prepare() && liveUserData == 0);
    for (MObject attr : lightPreparationAttributes) {
        configurePreparation(2); failAttribute = attr.id;
        assert(!prepare() && liveUserData == 0); assertClean();
    }
    MUserData wrongType(false);
    configurePreparation(); failAttribute = DPLightGen::aColor.id;
    assert(!prepare(&wrongType) && liveUserData == 1);
    resetPreparation(); MUserData* replacement = prepare(&wrongType);
    assert(replacement && replacement != &wrongType && liveUserData == 2);
    delete replacement;
}

void displayModesAndEmptyInput()
{
    for (int mode=0; mode<3; ++mode) {
        configurePreparation();
        preparationControls.values[DPLightGen::aLightVisType.id].scalar = mode;
        MUserData* cached = prepare(); assert(cached);
        assert(dynamic_cast<DPLightGenData*>(cached)->lightVisType == mode);
        preparationPoints.points.clear(); preparationPoints.attrs.clear();
        resetPreparation();
        assert(prepare(cached) == cached);
        assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length() == 0);
        configurePreparation();
        assert(prepare(cached) == cached);
        assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length() == 1);
        delete cached;
    }
    assert(liveUserData == 0); assertClean();
}

void invalidControlsAndRecovery()
{
    const double bad[] = {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (double value : bad) {
        for (int mode=0; mode<DP_MAT_APPLY_TYPE_COUNT; ++mode) {
            configurePreparation(3,mode);
            preparationPoints.points[0] = DPoint(value,2,3);
            if (mode == DP_MAT_FULL_MAT_IGNORE_ALL) {
                MUserData* ignored = prepare(); assert(ignored); delete ignored;
            } else {
                assert(!prepare());
                assert(std::find(operations.begin(),operations.end(),"transform.translate")==operations.end());
            }
        }
        configurePreparation(); preparationControls.values[DPLightGen::aPointSize.id].scalar = value;
        assert(!prepare());
        configurePreparation(3,DP_MAT_FULL_MAT_IGNORE_ALL);
        MMatrix matrix; matrix[1][2] = value;
        preparationPoints.field<MMatrix>("matrix","matrix",{matrix}); assert(!prepare());
        configurePreparation(); preparationControls.values[DPLightGen::aBaseDirection.id].components[0] = value;
        assert(!prepare());
    }
    configurePreparation(); preparationControls.values[DPLightGen::aGlobalScale.id].components[0] = 1e308;
    preparationControls.values[DPLightGen::aScaleAttribute.id].string = "scale";
    preparationPoints.field<double>("scale","double",{1e308}); assert(!prepare());
    configurePreparation(3,DP_MAT_LOOKAT);
    auto& target = preparationControls.values[DPLightGen::aAnimLookPos.id];
    target.components[0] = 11; target.components[1] = 2; target.components[2] = 3;
    assert(!prepare());
    configurePreparation(); preparationControls.values[DPLightGen::aBaseDirection.id].components[2] = 0;
    assert(!prepare());
    configurePreparation(); preparationControls.values[DPLightGen::aColorVis.id].scalar = 1;
    preparationControls.values[DPLightGen::aColor.id].components[0] = 1e300;
    assert(!prepare());
    configurePreparation(); preparationControls.values[DPLightGen::aColorVis.id].scalar = 0;
    preparationControls.values[DPLightGen::aColor.id].components[0] = 1e300;
    MUserData* value = prepare(); assert(value); delete value; // Unused color does not gate geometry.
    configurePreparation(); value = prepare(); assert(value); delete value;
    assert(liveUserData == 0); assertClean();
}

void inheritedFieldTypes()
{
    for (MObject control : {DPLightGen::aIntensityAttribute,DPLightGen::aColorAttribute,
             DPLightGen::aSpotAngleAttribute,DPLightGen::aScaleAttribute,
             DPLightGen::aAnimMatrixAttribute,DPLightGen::aDirectionAttribute}) {
        configurePreparation(2);
        MUserData* cached=prepare(); assert(cached);
        preparationControls.values[control.id].string="wrong";
        preparationControls.values[DPLightGen::aDirectionType.id].scalar=1;
        preparationPoints.field<std::string>("wrong","string",{"not numeric"});
        resetPreparation(); assert(!prepare(cached) && liveUserData==1);
        assert(dynamic_cast<DPLightGenData*>(cached)->matrixArray.length()==1);
        preparationPoints.attrs.erase("wrong"); resetPreparation();
        assert(prepare(cached)==cached); // Missing fields retain normal defaults.
        delete cached;
    }
    configurePreparation(3);
    preparationControls.values[DPLightGen::aSpotAngleAttribute.id].string="unused";
    preparationPoints.field<std::string>("unused","string",{"ignored"});
    MUserData* data=prepare(); assert(data); delete data;
}

int main()
{
    ordinaryAndFailurePaths();
    displayModesAndEmptyInput();
    invalidControlsAndRecovery();
    inheritedFieldTypes();
    assert(liveUserData == 0); assertClean();
}
