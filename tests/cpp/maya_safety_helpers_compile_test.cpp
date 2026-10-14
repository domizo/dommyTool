#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DGoalMath.h"
#include "common/utility/DParticleFieldMath.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DViewportRenderItem.h"

bool compileAffinePointTransform(const MPoint& point, const MMatrix& matrix,
                                 std::array<double, 3>& output)
{
    return dommy::numeric::checkedTransformPoint(point, matrix, output);
}

bool compileAffineDirectionTransform(const MVector& direction, const MMatrix& matrix,
                                     std::array<float, 3>& output)
{
    return dommy::numeric::checkedTransformDirection(direction, matrix, output);
}

bool compileWorldPivotTransform(const MMatrix& primitive, const MPoint& center,
                                const MMatrix& operation, MMatrix& output)
{
    return dommy::numeric::composeWorldPivotTransform(primitive, center, operation, output);
}

bool compileAffineNormalTransform(const MVector& normal, const MMatrix& matrix,
                                  std::array<float, 3>& output)
{
    return dommy::numeric::checkedTransformNormal(normal, matrix, output);
}

bool compileFiniteForceResponses(const MPoint& current, const MPoint& target,
                                 const MVector& velocity, const MVector& force,
                                 double inverseMass, double deltaTime, float& pointOutput,
                                 double& fieldOutput)
{
    return dommy::point::computeGoalVelocityComponent(current.x, target.x, velocity.x,
        1.0, 2.0, 0.0, 1.0, pointOutput) &&
        dommy::field::computeGoalFieldForceComponent(current.x, target.x, velocity.x,
            1.0, 2.0, 0.25, fieldOutput) &&
        dommy::field::integrateFieldVelocity(velocity.x, force.x, inverseMass, deltaTime, fieldOutput);
}

bool compileFiniteRangeNormalization(double value, double minimum, double maximum,
                                     double& output)
{
    return dommy::math::normalizeRange(value, minimum, maximum, output) &&
           dommy::numeric::normalizeRange(value, minimum, maximum, output);
}

void compileStockShaderAcquisition(bool& retry, MHWRender::MShaderInstance*& shader)
{
    dommy::viewport::StockShaderAcquisition acquisition(retry, "CompileTest");
    acquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, shader);
}

void compileStockShaderRelease(MHWRender::MShaderInstance*& shader) noexcept
{
    dommy::viewport::StockShaderRelease release("CompileTest");
    release.release(shader);
}

float* compileViewportBufferHelper(
    MHWRender::MVertexBuffer* buffer,
    const MHWRender::MVertexBufferDescriptor& descriptor,
    std::size_t scalarCount)
{
    return dommy::viewport::acquireFloatBuffer(
        buffer, descriptor, scalarCount, 3);
}

unsigned int* compileViewportIndexBufferHelper(
    MHWRender::MIndexBuffer* buffer, std::size_t indexCount)
{
    return dommy::viewport::acquireIndexBuffer(buffer, indexCount);
}

MHWRender::MIndexBuffer* compileViewportIndexCreationHelper(
    MHWRender::MGeometry& geometry,
    std::size_t indexCount,
    unsigned int*& indices)
{
    return dommy::viewport::createIndexBuffer(
        geometry, indexCount, indices);
}

MStatus compileMayaComputeGuard(bool fail) try
{
    if (fail) {
        throw 1;
    }
    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("compileMayaComputeGuard")

bool compileMayaValueValidation(
    const MMatrix& matrix, const MPoint& point, const MVector& vector)
{
    MVector normalized;
    return dommy::maya::isFiniteMatrix(matrix) &&
           dommy::maya::isFinitePoint(point) &&
           dommy::maya::isFloatRepresentablePoint(point) &&
           dommy::maya::isFiniteVector(vector) &&
           dommy::maya::isFloatRepresentableVector(vector) &&
           dommy::maya::normalizedVector(vector, normalized);
}

bool compileHistoricalSampleTime(const MTime& origin, MTime& sampleTime)
{
    return dommy::maya::historicalSampleTime(
        origin, 0.0, -1.0, 0.0, 1U, sampleTime);
}

MStatus compileHistoricalPlugSamples(
    const MPlug& plug,
    const MTime& sampleTime,
    MMatrix& matrix,
    MVector& vector)
{
    MStatus status =
        dommy::maya::sampleMatrixPlugAtTime(
            plug, sampleTime, matrix);
    if (!status) return status;
    return dommy::maya::sampleVectorPlugAtTime(
        plug, sampleTime, vector);
}

MStatus compileMeshClosestPointHelper(
    const MObject& mesh,
    const MMatrix& matrix,
    const MPoint& queryPoint,
    MPoint& closestPoint)
{
    dommy::maya::MeshClosestPointQuery query;
    MStatus status = query.create(mesh, matrix);
    if (!status) {
        return status;
    }
    return query.getClosestPoint(queryPoint, closestPoint);
}

MStatus compileMayaPluginDataAccess(MFnPluginData& pluginData)
{
    MPxData* outputData = nullptr;
    return dommy::maya::getPluginData(pluginData, outputData);
}

MStatus compileStagedMayaPluginData(
    const MTypeId& typeId,
    MObject& dataObject,
    MPxData*& outputData)
{
    return dommy::maya::createPluginDataObject<MPxData>(
        typeId, dataObject, outputData);
}

MPxData* compileLegacyMayaPluginDataAccess(
    MFnPluginData& pluginData, MStatus& status)
{
    return dommy::maya::pluginDataPointer<MPxData>(
        pluginData, &status);
}

MStatus compileMayaPluginDataHandleAccess(
    const MDataHandle& handle, const MTypeId& expectedTypeId)
{
    const MPxData* inputData = nullptr;
    return dommy::maya::getPluginDataFromHandle(
        handle, expectedTypeId, inputData);
}

MStatus compileMayaPluginDataPlugAccess(
    const MPlug& plug, const MTypeId& expectedTypeId, MObject& object)
{
    const MPxData* inputData = nullptr;
    return dommy::maya::getPluginDataFromPlug(
        plug, expectedTypeId, object, inputData);
}

MPxNode* compileCheckedUserNodeAccess(
    const MFnDependencyNode& functionSet, const MTypeId& expectedTypeId)
{
    return dommy::maya::checkedUserNode<MPxNode>(
        functionSet, expectedTypeId);
}

void compileLegacyDrawGuard(M3dView& view)
{
    dommy::maya::ScopedLegacyGL drawing(view, GL_CURRENT_BIT);
}

void compileUIDrawManagerGuard(MHWRender::MUIDrawManager& drawManager)
{
    dommy::maya::ScopedUIDrawable drawing(drawManager);
}

MBoundingBox compileBoundingBoxCallbackGuard() try
{
    return MBoundingBox();
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("CompileTest")
