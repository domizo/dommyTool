//
//  DPMatrix.cpp
//  dommyTool
//
//  Created by yuzo on 2016/03/05.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPMatrix.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaValueValidation.h"

#include "dpoint_ids.h"

#include "DMayaHeaders.h"

#include "DRandomGenerator.h"
#include "dommyMath.h"

#include <cmath>

const MTypeId DPMatrix::typeId(ID_DP_MATRIX_ARRAY);

// Attribute

MObject DPMatrix::inMatrix;

MObject DPMatrix::dataSize;

MObject DPMatrix::seed;
MObject DPMatrix::translateNoise;
MObject DPMatrix::rotateNoise;
MObject DPMatrix::scaleNoise;

MObject DPMatrix::outMatrixArray;

void* DPMatrix::creator() {
    return ::dommy::maya::createObject("DPMatrix", [&]() {
        return new DPMatrix();
    });
}

MStatus DPMatrix::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outMatrixArray) {

        // get Attribute
        const MDataHandle sizeHandle = data.inputValue(dataSize, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawDataSize = sizeHandle.asInt();

        std::size_t matrixCount = 0U;
        if (!dommy::generation::checkedCount(
                rawDataSize, dommy::generation::kMaxGeneratedTransformCount, matrixCount)) {
            return MS::kInvalidParameter;
        }

        const MDataHandle seedHandle = data.inputValue(seed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int seedVal = seedHandle.asInt();

        MMatrixArray newMatArray;
        DOMMY_RETURN_IF_FAILURE(newMatArray.setLength(static_cast<unsigned int>(matrixCount)));
        if (newMatArray.length() != matrixCount)
            return MS::kFailure;

        if (matrixCount > 0U) {
            const MDataHandle matrixHandle = data.inputValue(inMatrix, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MMatrix baseMatrix = matrixHandle.asMatrix();
            if (!dommy::maya::isFiniteMatrix(baseMatrix)) {
                return MS::kInvalidParameter;
            }
            const MTransformationMatrix transMat(baseMatrix);

            const MDataHandle translateNoiseHandle = data.inputValue(translateNoise, &stat);
            if (!stat)
                return stat;
            const MDataHandle rotateNoiseHandle = data.inputValue(rotateNoise, &stat);
            if (!stat)
                return stat;
            const MDataHandle scaleNoiseHandle = data.inputValue(scaleNoise, &stat);
            if (!stat)
                return stat;

            const double3& translateNoiseValue = translateNoiseHandle.asDouble3();
            const double3& rotateNoiseValue = rotateNoiseHandle.asDouble3();
            const double3& scaleNoiseValue = scaleNoiseHandle.asDouble3();

            double translateAmplitude[3];
            double rotateAmplitude[3];
            double scaleAmplitude[3];
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!std::isfinite(translateNoiseValue[axis]) ||
                    !std::isfinite(rotateNoiseValue[axis]) ||
                    !std::isfinite(scaleNoiseValue[axis])) {
                    return MS::kInvalidParameter;
                }
                translateAmplitude[axis] = std::fabs(translateNoiseValue[axis]);
                rotateAmplitude[axis] = std::fabs(rotateNoiseValue[axis]);
                scaleAmplitude[axis] = std::fabs(scaleNoiseValue[axis]);
            }

            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));

            for (unsigned int i = 0U; i < static_cast<unsigned int>(matrixCount); ++i) {
                double randX = 0, randY = 0, randZ = 0;
                if (translateAmplitude[0] != 0.0) {
                    randX = randomGenerator.uniform(-translateAmplitude[0], translateAmplitude[0]);
                }
                if (translateAmplitude[1] != 0.0) {
                    randY = randomGenerator.uniform(-translateAmplitude[1], translateAmplitude[1]);
                }
                if (translateAmplitude[2] != 0.0) {
                    randZ = randomGenerator.uniform(-translateAmplitude[2], translateAmplitude[2]);
                }

                double randRX = 0, randRY = 0, randRZ = 0;
                if (rotateAmplitude[0] != 0.0) {
                    randRX = randomGenerator.uniform(-rotateAmplitude[0], rotateAmplitude[0]);
                }
                if (rotateAmplitude[1] != 0.0) {
                    randRY = randomGenerator.uniform(-rotateAmplitude[1], rotateAmplitude[1]);
                }
                if (rotateAmplitude[2] != 0.0) {
                    randRZ = randomGenerator.uniform(-rotateAmplitude[2], rotateAmplitude[2]);
                }

                double randSX = 0, randSY = 0, randSZ = 0;
                if (scaleAmplitude[0] != 0.0) {
                    randSX = randomGenerator.uniform(-scaleAmplitude[0], scaleAmplitude[0]);
                }
                if (scaleAmplitude[1] != 0.0) {
                    randSY = randomGenerator.uniform(-scaleAmplitude[1], scaleAmplitude[1]);
                }
                if (scaleAmplitude[2] != 0.0) {
                    randSZ = randomGenerator.uniform(-scaleAmplitude[2], scaleAmplitude[2]);
                }

                MPoint offsetPt = MPoint(randX, randY, randZ);

                MTransformationMatrix tMat;
                double3 scaleVal;
                stat = transMat.getScale(scaleVal, MSpace::kObject);
                if (!stat)
                    return stat;

                scaleVal[0] += randSX;
                scaleVal[1] += randSY;
                scaleVal[2] += randSZ;
                if (!std::isfinite(scaleVal[0]) || !std::isfinite(scaleVal[1]) ||
                    !std::isfinite(scaleVal[2])) {
                    return MS::kInvalidParameter;
                }

                stat = tMat.setScale(scaleVal, MSpace::kObject);
                if (!stat)
                    return stat;

                double3 rotVal;
                MTransformationMatrix::RotationOrder order;
                stat = transMat.getRotation(rotVal, order, MSpace::kObject);
                if (!stat)
                    return stat;

                rotVal[0] += dommy::math::degreesToRadians(randRX);
                rotVal[1] += dommy::math::degreesToRadians(randRY);
                rotVal[2] += dommy::math::degreesToRadians(randRZ);
                if (!std::isfinite(rotVal[0]) || !std::isfinite(rotVal[1]) ||
                    !std::isfinite(rotVal[2])) {
                    return MS::kInvalidParameter;
                }

                stat = tMat.setRotation(rotVal, order, MSpace::kObject);
                if (!stat)
                    return stat;

                const MVector translation =
                    MVector(offsetPt) + transMat.getTranslation(MSpace::kWorld, &stat);
                if (!stat)
                    return stat;
                if (!dommy::maya::isFiniteVector(translation)) {
                    return MS::kInvalidParameter;
                }
                stat = tMat.setTranslation(translation, MSpace::kWorld);
                if (!stat)
                    return stat;

                const MMatrix generatedMatrix = tMat.asMatrix();
                if (!dommy::maya::isFiniteMatrix(generatedMatrix)) {
                    return MS::kInvalidParameter;
                }
                newMatArray[i] = generatedMatrix;
            }
        }

#pragma mark Create output point array
        MDataHandle outputData = data.outputValue(outMatrixArray, &stat);
        if (!stat)
            return stat;

        MFnMatrixArrayData matData;
        MObject outplug = matData.create(newMatArray, &stat);
        if (!stat)
            return stat;

        stat = outputData.set(outplug);
        if (!stat)
            return stat;

        outputData.setClean();
        return data.setClean(plug);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPMatrix")

MStatus DPMatrix::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnMatrixAttribute matAttr;
    MFnGenericAttribute outAttr;

    inMatrix = matAttr.create("inMatrix", "ima", MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMatrix));

    dataSize = nAttr.create("dataSize", "ds", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedTransformCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dataSize));

    seed = nAttr.create("seed", "se", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));

    translateNoise = nAttr.create("translateNoise", "tn", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(translateNoise));

    rotateNoise = nAttr.create("rotateNoise", "rn", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rotateNoise));

    scaleNoise = nAttr.create("scaleNoise", "sn", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scaleNoise));

    outMatrixArray = outAttr.create("outMatrixArray", "omat", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kMatrixArray));
    DOMMY_RETURN_IF_FAILURE(outAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(outAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMatrixArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMatrix, outMatrixArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(dataSize, outMatrixArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seed, outMatrixArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(translateNoise, outMatrixArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(rotateNoise, outMatrixArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(scaleNoise, outMatrixArray));
    return MS::kSuccess;
}
