#include "draw_callback_host.h"
#include "draw_callback_methods.inc"

#include <cfenv>
#include <limits>

void resetDraw()
{
    resetOperations();
    submittedColors.clear();
    submittedPoints.clear();
    submittedLines = 0;
}

void configureLight(DPLightGenData& data, unsigned int count)
{
    data.lightType = 1;
    data.lightVisType = 0;
    data.useConeAngleVis = data.colorVis = data.intensityVis = false;
    data.pointSize = 3;
    data.intensityNormalizeVal = 1;
    data.matrixArray.clear(); data.spotAngleArray.clear();
    data.intensityArray.clear(); data.colorArray.clear();
    for (unsigned int i = 0; i < count; ++i) {
        MMatrix matrix;
        matrix[3][0] = 2+i;
        data.matrixArray.append(matrix);
        data.spotAngleArray.append(45);
        data.intensityArray.append(0.5);
        data.colorArray.append(MVector(2, 0.5, 0.25));
    }
    resetDraw();
}

void drawLight(const MUserData* data)
{
    DPLightGenOverride callback;
    MDagPath path; MHWRender::MFrameContext frame;
    MHWRender::MUIDrawManager manager;
    callback.addUIDrawables(path, manager, frame, data);
}

void cameraOwnership()
{
    DCameraFrustumOverride callback;
    MDagPath path; MHWRender::MFrameContext frame;
    resetOperations();
    MUserData* data = callback.prepareForDraw(path, path, frame, nullptr);
    assert(data && liveUserData == 1);
    const std::size_t successOperations = operations.size();
    delete data;
    assert(liveUserData == 0);
    for (std::size_t index = 0; index < successOperations; ++index) {
        for (int kind = 1; kind <= 2; ++kind) {
            resetOperations(); throwAt = static_cast<int>(index); exceptionKind = kind;
            assert(!callback.prepareForDraw(path, path, frame, nullptr));
            assert(reportedExceptions == 1 && liveUserData == 0);
        }
    }
    resetOperations(); cameraReadSucceeds = false;
    assert(!callback.prepareForDraw(path, path, frame, nullptr));
    assert(liveUserData == 0);
    const std::size_t failureOperations = operations.size();
    for (std::size_t index = 0; index < failureOperations; ++index) {
        resetOperations(); cameraReadSucceeds = false;
        throwAt = static_cast<int>(index); exceptionKind = 1;
        assert(!callback.prepareForDraw(path, path, frame, nullptr));
        assert(liveUserData == 0 && reportedExceptions == 1);
    }
    {
        resetOperations(); DCameraFrustumData cached;
        resetOperations();
        assert(callback.prepareForDraw(path, path, frame, &cached) == &cached);
        assert(liveUserData == 1);
        const std::size_t cachedOperations = operations.size();
        for (std::size_t index = 0; index < cachedOperations; ++index) {
            resetOperations(); throwAt = static_cast<int>(index); exceptionKind = 1;
            assert(!callback.prepareForDraw(path, path, frame, &cached));
            assert(liveUserData == 1 && reportedExceptions == 1);
        }
        resetOperations(); cameraReadSucceeds = false;
        assert(!callback.prepareForDraw(path, path, frame, &cached));
        assert(liveUserData == 1);
    }
    {
        resetOperations(); MUserData wrongType(false);
        resetOperations();
        MUserData* replacement = callback.prepareForDraw(path, path, frame, &wrongType);
        assert(replacement && replacement != &wrongType && liveUserData == 2);
        delete replacement;
        assert(liveUserData == 1);
        resetOperations(); cameraReadSucceeds = false;
        assert(!callback.prepareForDraw(path, path, frame, &wrongType));
        assert(liveUserData == 1);
    }
    assert(liveUserData == 0);
}

void cameraDrawFailures()
{
    DCameraFrustumData data;
    data.nearClipping=1;data.farClipping=2;
    data.nearHSize=data.nearVSize=2;data.farHSize=data.farVSize=4;
    DCameraFrustumOverride callback;MDagPath path;MHWRender::MFrameContext frame;
    MHWRender::MUIDrawManager manager;
    for(bool extension:{false,true}){
        data.extendNearSize=data.extendFarSize=extension?1:0;
        resetDraw();callback.addUIDrawables(path,manager,frame,&data);
        assert(submittedLines==(extension?2:1)&&beginAttempts==1&&endAttempts==1);
        const auto trace=operations;
        for(std::size_t i=0;i<trace.size();++i){
            if(trace[i]=="array.append"||trace[i]=="array.copy"){
                resetDraw();failAt=int(i);callback.addUIDrawables(path,manager,frame,&data);
                assert(submittedLines==0&&beginAttempts==0&&endAttempts==0);
            }
            for(int kind:{1,2}){
                resetDraw();throwAt=int(i);exceptionKind=kind;
                callback.addUIDrawables(path,manager,frame,&data);
                assert(reportedExceptions==(trace[i]=="draw.end"?0:1)&&endAttempts==beginAttempts);
                if(trace[i]=="array.append"||trace[i]=="array.copy")assert(submittedLines==0);
            }
        }
        resetDraw();callback.addUIDrawables(path,manager,frame,&data);
        assert(submittedLines==(extension?2:1));
    }
}

void emptyAndMalformedLights()
{
    resetDraw(); drawLight(nullptr); assert(operations.empty());
    MUserData wrongType(false);
    resetDraw(); drawLight(&wrongType); assert(operations.empty());
    DPLightGenData data;
    for (int lightType = 0; lightType <= 6; ++lightType) {
        for (int display = 0; display <= 2; ++display) {
            for (bool cone : {false, true}) {
                configureLight(data, 0); data.lightType = lightType;
                data.lightVisType = display; data.useConeAngleVis = cone;
                drawLight(&data); assert(operations.empty());
            }
        }
    }
    for (unsigned int count : {0U, 1U, 3U}) {
        configureLight(data, 2); data.colorVis = true;
        data.colorArray.values.resize(count);
        drawLight(&data); assert(operations.empty());
        configureLight(data, 2); data.intensityVis = true;
        data.intensityArray.values.resize(count);
        drawLight(&data); assert(operations.empty());
        for (bool cone : {false, true}) {
            configureLight(data, 2); data.lightType = 2; data.lightVisType = 2;
            data.useConeAngleVis = cone; data.spotAngleArray.values.resize(count);
            drawLight(&data); assert(operations.empty());
        }
    }
    // Disabled fields are not read solely to validate unrelated draw modes.
    configureLight(data, 1); data.colorArray.clear(); data.intensityArray.clear();
    data.spotAngleArray.clear();
    drawLight(&data); assert(submittedPoints.size() == 1 && reportedExceptions == 0);
    const double badValues[] = {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::nextafter(static_cast<double>(std::numeric_limits<float>::max()),
                       std::numeric_limits<double>::infinity())};
    for (double value : badValues) {
        for (unsigned int axis = 0; axis < 3; ++axis) {
            configureLight(data, 1); data.colorVis = true;
            MVector& color = data.colorArray.values[0];
            (axis == 0 ? color.x : axis == 1 ? color.y : color.z) = value;
            drawLight(&data);
            assert(beginAttempts == 0 && submittedColors.empty());
        }
        configureLight(data, 1); data.lightType = 2; data.lightVisType = 2;
        data.spotAngleArray.values[0] = value*4;
        drawLight(&data); assert(beginAttempts == 0 && submittedColors.empty());
    }
    for (float size : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()}) {
        configureLight(data, 1); data.pointSize = size;
        drawLight(&data); assert(operations.empty());
    }
}

void intensityPolicy()
{
    for (double normalization : {0.25, 1.0, 3.0, 100.0}) {
        for (double intensity : {-5.0, -0.25, 0.0, 0.25, 0.5, 1.0, 5.0, 100.0}) {
            float actual = -1;
            assert(lightIntensityAlpha(intensity, normalization, actual));
            const float base = 1.0-intensity/normalization;
            const float legacy = 0.01+(base < 0 ? 0.0 : base);
            assert(actual == legacy);
        }
    }
    const double invalid[] = {0.0, -0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    DPLightGenData data;
    for (double normalization : invalid) {
        float alpha = 0.75f;
        assert(!lightIntensityAlpha(0, normalization, alpha) && alpha == 0.75f);
        configureLight(data, 1); data.intensityVis = true;
        data.intensityArray.values[0] = 0;
        data.intensityNormalizeVal = normalization;
        std::feclearexcept(FE_ALL_EXCEPT);
        drawLight(&data);
        assert(submittedPoints.size() == 1 && submittedColors.back().a == 1);
        assert(std::fetestexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW) == 0);
    }
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
        float alpha = 0.75f;
        assert(!lightIntensityAlpha(value, 1, alpha) && alpha == 0.75f);
        configureLight(data, 1); data.intensityVis = data.colorVis = true;
        data.intensityArray.values[0] = value;
        drawLight(&data); assert(submittedColors.back().a == 1 && submittedPoints.size() == 1);
    }
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    std::feclearexcept(FE_ALL_EXCEPT);
    float alpha = 0.75f;
    assert(lightIntensityAlpha(maximum, tiny, alpha) && alpha == 0.01f);
    alpha = 0.75f;
    assert(!lightIntensityAlpha(-maximum, tiny, alpha) && alpha == 0.75f);
    assert(std::fetestexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW) == 0);
    configureLight(data, 2); data.colorVis = data.intensityVis = true;
    data.intensityArray.values[0] = 0.5; data.intensityArray.values[1] = 0.25;
    drawLight(&data);
    assert(submittedPoints.size() == 2 && submittedPoints[0].x == 2 && submittedPoints[1].x == 3);
    assert(submittedColors[1].r == 2 && submittedColors[1].a == 0.51f);
    assert(submittedColors[2].a == 0.76f);
    resetDraw();
    data.intensityArray.values[0] = -2;
    data.intensityArray.values[1] = std::numeric_limits<double>::quiet_NaN();
    drawLight(&data);
    assert(submittedPoints.size() == 2 && submittedColors[1].r == 2);
    assert(submittedColors[1].a == 3.01f && submittedColors[2].a == 1);
    // Fallback must reset a later light after an earlier valid alpha modulation.
    configureLight(data, 1); data.colorVis = true;
    const float floatMaximum = std::numeric_limits<float>::max();
    data.colorArray.values[0] = MVector(floatMaximum, -floatMaximum, 0);
    drawLight(&data);
    assert(submittedColors.back().r == floatMaximum &&
           submittedColors.back().g == -floatMaximum);
}

void matrixAndCoordinateAdmission()
{
    DPLightGenData data;
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
        for (unsigned int row = 0U; row < 4U; ++row) {
            for (unsigned int column = 0U; column < 4U; ++column) {
                configureLight(data, 2);
                data.matrixArray.values[1][row][column] = value;
                drawLight(&data);
                assert(beginAttempts == 0 && submittedPoints.empty());
            }
        }
    }
    for (int display = 0; display <= 2; ++display) {
        configureLight(data, 1); data.lightType = 4; data.lightVisType = display;
        data.matrixArray.values[0][3][0] = std::numeric_limits<double>::max();
        drawLight(&data);
        assert(submittedPoints.empty() && submittedLines == 0);
    }
    configureLight(data, 1); data.lightType = 4; data.lightVisType = 2;
    data.matrixArray.values[0][0][0] = std::numeric_limits<double>::max();
    data.matrixArray.values[0][1][0] = std::numeric_limits<double>::max();
    drawLight(&data); // Finite input matrices may still yield invalid coordinates.
    assert(submittedLines == 0);
    configureLight(data, 1); drawLight(&data);
    assert(submittedPoints.size() == 1U && reportedExceptions == 0);
}

void callbackFailuresAndRecovery()
{
    DPLightGenData data;
    for (int lightType = 0; lightType <= 6; ++lightType) {
        for (int display = 0; display <= 2; ++display) {
            for (bool cone : {false, true}) {
                configureLight(data, 2); data.lightType = lightType; data.lightVisType = display;
                data.useConeAngleVis = cone; data.colorVis = data.intensityVis = true;
                drawLight(&data);
                assert(beginAttempts == 1 && endAttempts == 1 && reportedExceptions == 0);
                assert(display != 0 || submittedPoints.size() == 2);
                const std::vector<std::string> successfulOperations = operations;
                bool submittedBefore = false;
                for (std::size_t index = 0; index < successfulOperations.size(); ++index) {
                    for (int kind = 1; kind <= 2; ++kind) {
                        resetDraw(); throwAt = static_cast<int>(index); exceptionKind = kind;
                        drawLight(&data);
                        assert(endAttempts == beginAttempts);
                        // endDrawable exceptions are contained by the existing RAII guard.
                        assert(reportedExceptions ==
                               (successfulOperations[index] == "draw.end" ? 0 : 1));
                    }
                    resetDraw(); failAt = static_cast<int>(index);
                    drawLight(&data);
                    assert(endAttempts == beginAttempts);
                    if (!submittedBefore && successfulOperations[index] == "array.append")
                        assert(submittedPoints.empty() && submittedLines == 0);
                    submittedBefore = submittedBefore ||
                        successfulOperations[index] == "draw.point" ||
                        successfulOperations[index] == "draw.lineList" ||
                        successfulOperations[index] == "draw.lineStrip";
                }
                resetDraw(); drawLight(&data);
                assert(beginAttempts == 1 && endAttempts == 1 && reportedExceptions == 0);
            }
        }
    }
}

int main()
{
    cameraOwnership();
    cameraDrawFailures();
    emptyAndMalformedLights();
    intensityPolicy();
    matrixAndCoordinateAdmission();
    callbackFailuresAndRecovery();
    assert(liveUserData == 0);
}
