//
//  DCameraFrustum.cpp
//  dommyTool
//
//  Created by yuzo on 4/27/16.
//

#include "DCameraFrustum.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaCheckedArray.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DViewportCallbackGuard.h"

#include <maya/M3dView.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnCamera.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>

#include <algorithm>
#include <cmath>
#include <memory>

MTypeId DCameraFrustum::typeId(0x2900A);
const MString DCameraFrustum::typeName("DCameraFrustum");
MString DCameraFrustum::classification("drawdb/geometry/DCameraFrustum");
MString DCameraFrustum::registrantId("DCameraFrustumPlugin");

MObject DCameraFrustum::aCameraName;
MObject DCameraFrustum::aVisibleMode;
MObject DCameraFrustum::aCustumNearPlane;
MObject DCameraFrustum::aCustumFarPlane;
MObject DCameraFrustum::extendNearArea;
MObject DCameraFrustum::extendFarArea;
MObject DCameraFrustum::aOutMesh;
MObject DCameraFrustum::aTime;

namespace {

void createPerspectivePlanes(float* frustum,
                             float fieldOfView,
                             float aspect,
                             float nearClip,
                             float farClip)
{
    const float angle = fieldOfView * (3.141592653589f / 180.0f);
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    frustum[0] = 0.0f; frustum[1] = -cosine; frustum[2] = sine; frustum[3] = 0.0f;
    frustum[4] = 0.0f; frustum[5] = cosine; frustum[6] = sine; frustum[7] = 0.0f;
    frustum[8] = cosine; frustum[9] = 0.0f; frustum[10] = sine * aspect; frustum[11] = 0.0f;
    frustum[12] = -cosine; frustum[13] = 0.0f; frustum[14] = sine * aspect; frustum[15] = 0.0f;
    frustum[16] = 0.0f; frustum[17] = 0.0f; frustum[18] = 1.0f; frustum[19] = farClip;
    frustum[20] = 0.0f; frustum[21] = 0.0f; frustum[22] = -1.0f; frustum[23] = -nearClip;
}

template <typename Frustum>
MStatus populateCameraFrustumData(const MObject& node, Frustum& output)
{
    MStatus status;
    const MString cameraName =
        MPlug(node, DCameraFrustum::aCameraName).asString(&status);
    if (!status || cameraName.length() == 0U) {
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    MSelectionList selection;
    status = selection.add(cameraName);
    if (!status || selection.length() == 0U) return status;

    MObject cameraNode;
    status = selection.getDependNode(0U, cameraNode);
    if (!status || cameraNode.isNull() || !cameraNode.hasFn(MFn::kCamera)) {
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    MFnCamera camera(cameraNode, &status);
    if (!status) return status;
    const int visibleMode =
        MPlug(node, DCameraFrustum::aVisibleMode).asInt(&status);
    if (!status) return status;
    if (visibleMode < 0 || visibleMode > 1) return MS::kInvalidParameter;

    output.aspect = camera.aspectRatio(&status);
    if (!status) return status;
    output.focalLength = camera.focalLength(&status);
    if (!status) return status;
    if (visibleMode == 0) {
        output.nearClipping = camera.nearClippingPlane(&status);
        if (!status) return status;
        output.farClipping = camera.farClippingPlane(&status);
        if (!status) return status;
    } else {
        output.nearClipping =
            MPlug(node, DCameraFrustum::aCustumNearPlane).asFloat(&status);
        if (!status) return status;
        output.farClipping =
            MPlug(node, DCameraFrustum::aCustumFarPlane).asFloat(&status);
        if (!status) return status;
    }
    output.extendNearSize =
        MPlug(node, DCameraFrustum::extendNearArea).asFloat(&status);
    if (!status) return status;
    output.extendFarSize =
        MPlug(node, DCameraFrustum::extendFarArea).asFloat(&status);
    if (!status) return status;

    if (!std::isfinite(output.aspect) || output.aspect <= 0.0 ||
        !std::isfinite(output.focalLength) || output.focalLength <= 0.0 ||
        !std::isfinite(output.nearClipping) || output.nearClipping <= 0.0 ||
        !std::isfinite(output.farClipping) ||
        output.farClipping <= output.nearClipping ||
        !std::isfinite(output.extendNearSize) || output.extendNearSize < 0.0 ||
        !std::isfinite(output.extendFarSize) || output.extendFarSize < 0.0) {
        return MS::kInvalidParameter;
    }

    status = camera.getFilmFrustum(
        output.nearClipping, output.nearHSize, output.nearVSize,
        output.nearHOffset, output.nearVOffset);
    if (!status) return status;
    status = camera.getFilmFrustum(
        output.farClipping, output.farHSize, output.farVSize,
        output.farHOffset, output.farVOffset);
    if (!status) return status;

    const double filmValues[] = {
        output.nearHSize, output.nearVSize,
        output.nearHOffset, output.nearVOffset,
        output.farHSize, output.farVSize,
        output.farHOffset, output.farVOffset};
    for (unsigned int index = 0U; index < 8U; ++index) {
        if (!std::isfinite(filmValues[index])) return MS::kInvalidParameter;
    }
    if (output.nearHSize <= 0.0 || output.nearVSize <= 0.0 ||
        output.farHSize <= 0.0 || output.farVSize <= 0.0) {
        return MS::kInvalidParameter;
    }

    float focalLength = 0.0f;
    float aspect = 0.0f;
    float nearClip = 0.0f;
    float farClip = 0.0f;
    if (!dommy::numeric::checkedFloatCast(output.focalLength, focalLength) ||
        !dommy::numeric::checkedFloatCast(output.aspect, aspect) ||
        !dommy::numeric::checkedFloatCast(output.nearClipping, nearClip) ||
        !dommy::numeric::checkedFloatCast(output.farClipping, farClip)) {
        return MS::kInvalidParameter;
    }
    createPerspectivePlanes(
        output.frustumMat, focalLength, aspect, nearClip, farClip);
    return MS::kSuccess;
}

template <typename Frustum>
MStatus frustumCorners(const Frustum& frustum,
                       bool includeExtension,
                       MPointArray& corners)
{
    const double nearExtension =
        includeExtension ? frustum.extendNearSize * 0.5 : 0.0;
    const double farExtension =
        includeExtension ? frustum.extendFarSize * 0.5 : 0.0;
    const double nearHorizontal = frustum.nearHSize * 0.5 + nearExtension;
    const double nearVertical = frustum.nearVSize * 0.5 + nearExtension;
    const double farHorizontal = frustum.farHSize * 0.5 + farExtension;
    const double farVertical = frustum.farVSize * 0.5 + farExtension;

    MPointArray candidate;
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.nearHOffset - nearHorizontal,
        frustum.nearVOffset - nearVertical, -frustum.nearClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.nearHOffset - nearHorizontal,
        frustum.nearVOffset + nearVertical, -frustum.nearClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.nearHOffset + nearHorizontal,
        frustum.nearVOffset + nearVertical, -frustum.nearClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.nearHOffset + nearHorizontal,
        frustum.nearVOffset - nearVertical, -frustum.nearClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.farHOffset - farHorizontal,
        frustum.farVOffset - farVertical, -frustum.farClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.farHOffset - farHorizontal,
        frustum.farVOffset + farVertical, -frustum.farClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.farHOffset + farHorizontal,
        frustum.farVOffset + farVertical, -frustum.farClipping)));
    DOMMY_RETURN_IF_FAILURE(candidate.append(MPoint(
        frustum.farHOffset + farHorizontal,
        frustum.farVOffset - farVertical, -frustum.farClipping)));
    return dommy::maya::copyCheckedArray(candidate, corners);
}

MStatus frustumLines(const MPointArray& corners, MPointArray& lines)
{
    if (corners.length() != 8U) return MS::kInvalidParameter;
    const unsigned int pairs[][2] = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
        {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
        {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}};
    MPointArray candidate;
    for (unsigned int index = 0U; index < 12U; ++index) {
        DOMMY_RETURN_IF_FAILURE(candidate.append(corners[pairs[index][0]]));
        DOMMY_RETURN_IF_FAILURE(candidate.append(corners[pairs[index][1]]));
    }
    return dommy::maya::copyCheckedArray(candidate, lines);
}

template <typename Frustum>
MBoundingBox frustumBounds(const Frustum& frustum)
{
    MPointArray corners;
    if (!frustumCorners(frustum, true, corners)) {
        return MBoundingBox();
    }
    MBoundingBox bounds;
    for (unsigned int index = 0U; index < corners.length(); ++index) {
        bounds.expand(corners[index]);
    }
    return bounds;
}

void drawFrustumLegacy(const MPointArray& corners)
{
    MPointArray lines;
    if (!frustumLines(corners, lines)) return;
    glBegin(GL_LINES);
    for (unsigned int index = 0U; index < lines.length(); ++index) {
        glVertex3d(lines[index].x, lines[index].y, lines[index].z);
    }
    glEnd();
}

} // namespace

DCameraFrustum::DCameraFrustum()
    : mBBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0))
{
}

DCameraFrustum::~DCameraFrustum() = default;

void* DCameraFrustum::creator()
{
    return ::dommy::maya::createObject("DCameraFrustum", [&]() {
        return new DCameraFrustum();
    });
}

bool DCameraFrustum::isBounded() const
{
    return true;
}

MBoundingBox DCameraFrustum::boundingBox() const try
{
    FrustumData frustum;
    return getCamData(frustum) ? frustumBounds(frustum) : mBBox;
} catch (...) {
    dommy::maya::reportViewportCallbackException(
        "DCameraFrustum", "boundingBox", std::current_exception());
    return mBBox;
}

MStatus DCameraFrustum::getCamData(FrustumData& frustum) const
{
    return populateCameraFrustumData(thisMObject(), frustum);
}

MStatus DCameraFrustum::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != aOutMesh) return MS::kUnknownParameter;

    MStatus status;
    data.inputValue(aTime, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    FrustumData frustum;
    DOMMY_RETURN_IF_FAILURE(getCamData(frustum));

    MPointArray vertices;
    DOMMY_RETURN_IF_FAILURE(frustumCorners(frustum, true, vertices));
    MIntArray faceCounts(6U, 4);
    MIntArray faceIndices;
    const int indices[] = {
        3, 2, 1, 0, 4, 5, 6, 7,
        1, 5, 4, 0, 2, 3, 7, 6,
        6, 5, 1, 2, 4, 7, 3, 0};
    for (unsigned int index = 0U; index < 24U; ++index) {
        DOMMY_RETURN_IF_FAILURE(faceIndices.append(indices[index]));
    }

    MFnMeshData meshDataCreator;
    MObject meshData = meshDataCreator.create(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (meshData.isNull()) return MS::kFailure;

    MFnMesh mesh;
    const MObject meshObject = mesh.create(
        vertices.length(), faceCounts.length(), vertices,
        faceCounts, faceIndices, meshData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (meshObject.isNull()) return MS::kFailure;

    const int edgeCount = mesh.numEdges(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (edgeCount < 0) return MS::kFailure;
    for (int edge = 0; edge < edgeCount; ++edge) {
        DOMMY_RETURN_IF_FAILURE(mesh.setEdgeSmoothing(edge, false));
    }
    DOMMY_RETURN_IF_FAILURE(mesh.cleanupEdgeSmoothing());

    MDataHandle output = data.outputValue(aOutMesh, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(output.set(meshData));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCameraFrustum")

void DCameraFrustum::draw(M3dView& view,
                          const MDagPath&,
                          M3dView::DisplayStyle style,
                          M3dView::DisplayStatus) try
{
    if (style != M3dView::kFlatShaded &&
        style != M3dView::kGouraudShaded &&
        style != M3dView::kWireFrame) {
        return;
    }

    FrustumData frustum;
    if (!getCamData(frustum)) return;

    dommy::maya::ScopedLegacyGL drawing(
        view, GL_CURRENT_BIT | GL_LINE_BIT);
    glLineWidth(1.0f);
    view.setDrawColor(MColor(0.0f, 1.0f, 1.0f));
    MPointArray corners;
    if (frustumCorners(frustum, false, corners)) {
        drawFrustumLegacy(corners);
    }
    if (frustum.extendNearSize > 0.0 || frustum.extendFarSize > 0.0) {
        view.setDrawColor(MColor(1.0f, 0.5f, 0.0f));
        if (frustumCorners(frustum, true, corners)) {
            drawFrustumLegacy(corners);
        }
    }
}
DOMMY_DRAW_CATCH("DCameraFrustum")

MStatus DCameraFrustum::initialize()
try {
    MStatus status;
    MFnTypedAttribute typedAttribute;
    MFnNumericAttribute numericAttribute;
    MFnStringData stringData;

    const MObject defaultCamera = stringData.create("perspShape", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aCameraName = typedAttribute.create(
        "CameraName", "cam", MFnData::kString, defaultCamera, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCameraName));

    MFnEnumAttribute visibleModeAttribute;
    aVisibleMode = visibleModeAttribute.create(
        "VisibleMode", "vism", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(visibleModeAttribute.addField("Camera Clip Plane", 0));
    DOMMY_RETURN_IF_FAILURE(visibleModeAttribute.addField("User Clip Plane", 1));
    DOMMY_RETURN_IF_FAILURE(visibleModeAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVisibleMode));

    aCustumNearPlane = numericAttribute.create(
        "CustumNearPlane", "cnear", MFnNumericData::kFloat, 1.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1.0e-6f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(5000.0f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCustumNearPlane));

    aCustumFarPlane = numericAttribute.create(
        "CustumFarPlane", "cfar", MFnNumericData::kFloat, 120.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1.0e-6f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(5000.0f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCustumFarPlane));

    extendNearArea = numericAttribute.create(
        "extendNearArea", "ena", MFnNumericData::kFloat, 0.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(50.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(extendNearArea));

    extendFarArea = numericAttribute.create(
        "extendFarArea", "efa", MFnNumericData::kFloat, 0.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(50.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(extendFarArea));

    aOutMesh = typedAttribute.create(
        "aOutMesh", "opm", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutMesh));

    MFnUnitAttribute unitAttribute;
    aTime = unitAttribute.create(
        "CurrentTime", "time", MFnUnitAttribute::kTime, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTime));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCameraName, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVisibleMode, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCustumNearPlane, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCustumFarPlane, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(extendNearArea, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(extendFarArea, aOutMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTime, aOutMesh));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCameraFrustum", "initialize", std::current_exception());
}

DCameraFrustumOverride::DCameraFrustumOverride(const MObject& object)
    : MHWRender::MPxDrawOverride(object, DCameraFrustumOverride::draw)
{
}

DCameraFrustumOverride::~DCameraFrustumOverride() = default;

MHWRender::DrawAPI DCameraFrustumOverride::supportedDrawAPIs() const
{
    return MHWRender::kOpenGL | MHWRender::kDirectX11 |
           MHWRender::kOpenGLCoreProfile;
}

bool DCameraFrustumOverride::isBounded(
    const MDagPath&, const MDagPath&) const
{
    return true;
}

MBoundingBox DCameraFrustumOverride::boundingBox(
    const MDagPath& objectPath, const MDagPath&) const try
{
    DCameraFrustumData frustum;
    return populateCameraFrustumData(objectPath.node(), frustum)
               ? frustumBounds(frustum)
               : MBoundingBox(
                     MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
} catch (...) {
    dommy::maya::reportViewportCallbackException(
        "DCameraFrustumOverride", "boundingBox", std::current_exception());
    return MBoundingBox(
        MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
}

MUserData* DCameraFrustumOverride::prepareForDraw(
    const MDagPath& objectPath,
    const MDagPath&,
    const MHWRender::MFrameContext&,
    MUserData* oldData) try
{
    DCameraFrustumData* frustum =
        dynamic_cast<DCameraFrustumData*>(oldData);
    std::unique_ptr<DCameraFrustumData> newFrustum;
    if (!frustum) {
        newFrustum.reset(new DCameraFrustumData());
        frustum = newFrustum.get();
    }
    const MStatus status =
        populateCameraFrustumData(objectPath.node(), *frustum);
    if (!status) {
        MGlobal::displayWarning("DCameraFrustum: failed to read camera data.");
        return NULL;
    }
    newFrustum.release();
    return frustum;
}
DOMMY_VIEWPORT_POINTER_CATCH("DCameraFrustumOverride", "prepareForDraw")

void DCameraFrustumOverride::addUIDrawables(
    const MDagPath&,
    MHWRender::MUIDrawManager& drawManager,
    const MHWRender::MFrameContext&,
    const MUserData* userData) try
{
    const DCameraFrustumData* frustum =
        dynamic_cast<const DCameraFrustumData*>(userData);
    if (!frustum) return;

    MPointArray corners;
    MPointArray lines;
    if (!frustumCorners(*frustum, false, corners) || !frustumLines(corners, lines)) return;

    // Preflight both shapes before submitting any draw calls. Failed extended
    // geometry must never reuse the successfully generated normal shape.
    const bool drawExtension = frustum->extendNearSize > 0.0 || frustum->extendFarSize > 0.0;
    MPointArray extendedCorners;
    MPointArray extendedLines;
    if (drawExtension && (!frustumCorners(*frustum, true, extendedCorners) ||
                          !frustumLines(extendedCorners, extendedLines))) return;

    dommy::maya::ScopedUIDrawable drawable(drawManager);
    drawManager.setColor(MColor(0.0f, 1.0f, 1.0f));
    drawManager.setLineWidth(1.0f);
    drawManager.setDepthPriority(5);
    drawManager.lineList(lines, false);

    if (drawExtension) {
        drawManager.setColor(MColor(1.0f, 0.5f, 0.0f));
        drawManager.lineList(extendedLines, false);
    }
}
DOMMY_VIEWPORT_VOID_CATCH("DCameraFrustumOverride", "addUIDrawables")
