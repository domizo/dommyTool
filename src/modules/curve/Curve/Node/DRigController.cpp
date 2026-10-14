//
//  DRigController.cpp
//  dommyTool
//
//  Created by yuzo on 2/22/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DRigController.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DProceduralMeshPlans.h"

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MVector.h>
#include <maya/MDoubleArray.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MEulerRotation.h>

#include "dommyMath.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

MTypeId DRigController::typeId(0x0050A);

// Attributes
MObject DRigController::shapeType;
MObject DRigController::localScale;
MObject DRigController::division;

MObject DRigController::offset;
MObject DRigController::offsetX;
MObject DRigController::offsetY;
MObject DRigController::offsetZ;

MObject DRigController::localRotate;
MObject DRigController::localRotateX;
MObject DRigController::localRotateY;
MObject DRigController::localRotateZ;

MObject DRigController::keepCenter;

MObject DRigController::ratio;
MObject DRigController::dirAxis;
MObject DRigController::taperAxis;
MObject DRigController::taperAmount;
MObject DRigController::bendAmount;
MObject DRigController::bendAxis;

MObject DRigController::arrowReverse;
MObject DRigController::arrowRatio;
MObject DRigController::arrowWidth;

MObject DRigController::outputCurve;

DRigController::DRigController() {}
DRigController::~DRigController() {}

void* DRigController::creator() {
    return ::dommy::maya::createObject("DRigController", [&]() {
        return new DRigController();
    });
}

void DRigController::addDivision(MPointArray& cvs, MVector sp, MVector ep, long div) {
    MVector np = ep - sp;

    for (long i = 1; i < div; ++i) {
        MVector midVec = sp + np * ((1.0 / (double)div) * i);
        if (cvs.append(MPoint(midVec.x, midVec.y, midVec.z)) != MS::kSuccess) {
            throw std::runtime_error("DRigController point allocation failed");
        }
    }
}

bool DRigController::taperDeform(MPointArray& cvs, double highBound, double lowBound,
                                 double curvature, D_AXIS_TYPE axis) {
    const double boundWidth = highBound - lowBound;
    if (!std::isfinite(highBound) || !std::isfinite(lowBound) || !std::isfinite(curvature) ||
        std::fabs(boundWidth) <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    for (long i = 0; i < cvs.length(); i++) {
        if (!dommy::maya::isFinitePoint(cvs[i]))
            return false;
        double x, y, z;

        x = cvs[i].x;
        y = cvs[i].y;
        z = cvs[i].z;

        double limited;

        switch (axis) {
        case D_X_AXIS:
            limited = x;
            if (x > highBound) {
                limited = highBound;
            } else if (x < lowBound) {
                limited = lowBound;
            }
            break;
        case D_Y_AXIS:
            limited = y;
            if (y > highBound) {
                limited = highBound;
            } else if (y < lowBound) {
                limited = lowBound;
            }
            break;
        case D_Z_AXIS:
        default:
            limited = z;
            if (z > highBound) {
                limited = highBound;
            } else if (z < lowBound) {
                limited = lowBound;
            }
            break;
        }

        double r = ((limited) / boundWidth) * curvature;
        if (!std::isfinite(r))
            return false;

        switch (axis) {
        case D_X_AXIS: {
            double px = x;
            double py = y + y * r;
            double pz = z + z * r;

            const MPoint result(px, py, pz);
            if (!dommy::maya::isFinitePoint(result))
                return false;
            cvs[i] = result;
        } break;
        case D_Y_AXIS: {
            double px = x + x * r;
            double py = y;
            double pz = z + z * r;

            const MPoint result(px, py, pz);
            if (!dommy::maya::isFinitePoint(result))
                return false;
            cvs[i] = result;
        } break;
        case D_Z_AXIS:
        default: {
            double px = x + x * r;
            double py = y + y * r;
            double pz = z;

            const MPoint result(px, py, pz);
            if (!dommy::maya::isFinitePoint(result))
                return false;
            cvs[i] = result;
        } break;
        }
    }
    return true;
}

bool DRigController::bendDeform(MPointArray& cvs, double highBound, double lowBound,
                                double curvature, D_AXIS_TYPE axis) {
    if (!std::isfinite(highBound) || !std::isfinite(lowBound) || !std::isfinite(curvature)) {
        return false;
    }
    if (std::fabs(curvature) <= std::numeric_limits<double>::epsilon()) {
        return true;
    }
    const double r = 1.0 / curvature;
    if (!std::isfinite(r))
        return false;

    for (long i = 0; i < cvs.length(); i++) {
        if (!dommy::maya::isFinitePoint(cvs[i]))
            return false;
        double x, y;
        switch (axis) {
        case D_X_AXIS:
            x = cvs[i].x;
            y = cvs[i].y;
            break;
        default:
            x = cvs[i].z;
            y = cvs[i].y;
            break;
        }

        // yr = y / r   # bend at point y
        double y_limited = y;
        if (y > highBound) {
            y_limited = highBound;
        } else if (y < lowBound) {
            y_limited = lowBound;
        }

        double yr = y_limited * curvature; // yr = y /r

        double c = cos(M_PI - yr);
        double s = sin(M_PI - yr);
        double px = (r * c) + r - (x * c);
        double py = (r * s) - (x * s);

        if (y > highBound) {
            py += -c * (y - highBound);
            px += s * (y - highBound);
        } else if (y < lowBound) {
            py += -c * (y - lowBound);
            px += s * (y - lowBound);
        }

        switch (axis) {
        case D_X_AXIS: {
            const MPoint result(px, py, cvs[i].z);
            if (!dommy::maya::isFinitePoint(result))
                return false;
            cvs[i] = result;
        } break;
        default: {
            const MPoint result(cvs[i].x, py, px);
            if (!dommy::maya::isFinitePoint(result))
                return false;
            cvs[i] = result;
        } break;
        }
    }
    return true;
}

bool DRigController::crossBendDeform(MPointArray& cvs, double highBound, double lowBound,
                                     double curvature) {
    if (!std::isfinite(curvature))
        return false;
    if (std::fabs(curvature) <= std::numeric_limits<double>::epsilon()) {
        return true;
    }

    MPointArray tmpArray;
    if (dommy::maya::copyCheckedArray(cvs, tmpArray) != MS::kSuccess) {
        return false;
    }

    if (!bendDeform(tmpArray, highBound, lowBound, curvature, D_Z_AXIS)) {
        return false;
    }
    const double r = 1.0 / curvature;
    if (!std::isfinite(r))
        return false;

    for (long i = 0; i < cvs.length(); i++) {
        double x, y;
        x = cvs[i].x;
        y = cvs[i].z;

        // yr = y / r   # bend at point y
        double x_limited = x;
        if (x > highBound) {
            x_limited = highBound;
        } else if (x < lowBound) {
            x_limited = lowBound;
        }

        double xr = x_limited * curvature; // yr = y /r

        double c = cos(M_PI - xr);
        double s = sin(M_PI - xr);
        double py = (r * c) + r - (y * c);
        double px = (r * s) - (y * s);

        if (x > highBound) {
            px += -c * (x - highBound);
            py += s * (x - highBound);
        } else if (x < lowBound) {
            px += -c * (x - lowBound);
            py += s * (x - lowBound);
        }

        const MPoint result(px + (tmpArray[i].x - cvs[i].x), cvs[i].y + (tmpArray[i].y - cvs[i].y),
                            py + (tmpArray[i].z - cvs[i].z));
        if (!dommy::maya::isFinitePoint(result))
            return false;
        cvs[i] = result;
    }
    return true;
}

MStatus DRigController::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outputCurve) {
        const MDataHandle shapeTypeHandle = block.inputValue(shapeType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        D_CONTROLLER_SHAPE_TYPE shapeTypeVal = shapeTypeHandle.asInt();
        const MDataHandle localScaleHandle = block.inputValue(localScale, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double localScaleVal = localScaleHandle.asDouble();
        double localSizeVal = 1.0;
        const MDataHandle divisionHandle = block.inputValue(division, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        long divisionVal = divisionHandle.asInt();
        const MDataHandle offsetHandle = block.inputValue(offset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double3& offsetVal = offsetHandle.asDouble3();
        const MDataHandle localRotateHandle = block.inputValue(localRotate, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double3& localRotateVal = localRotateHandle.asDouble3();

        const MDataHandle keepCenterHandle = block.inputValue(keepCenter, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        bool keepCenterVal = keepCenterHandle.asBool();

        const MDataHandle ratioHandle = block.inputValue(ratio, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double ratioVal = ratioHandle.asDouble();
        const MDataHandle dirAxisHandle = block.inputValue(dirAxis, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        D_AXIS_TYPE dirAxisVal = dirAxisHandle.asInt();
        const MDataHandle taperAxisHandle = block.inputValue(taperAxis, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        D_AXIS_TYPE taperAxisVal = taperAxisHandle.asInt();
        const MDataHandle taperAmountHandle = block.inputValue(taperAmount, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double taperAmountVal = taperAmountHandle.asDouble();
        const MDataHandle bendAmountHandle = block.inputValue(bendAmount, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double bendAmountVal = bendAmountHandle.asDouble();
        const MDataHandle bendAxisHandle = block.inputValue(bendAxis, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        D_AXIS_TYPE bendAxisVal = bendAxisHandle.asInt();

        const MDataHandle arrowReverseHandle = block.inputValue(arrowReverse, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        bool arrowReverseVal = arrowReverseHandle.asBool();
        const MDataHandle arrowRatioHandle = block.inputValue(arrowRatio, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double arrowRatioVal = arrowRatioHandle.asDouble();
        const MDataHandle arrowWidthHandle = block.inputValue(arrowWidth, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        double arrowWidthVal = arrowWidthHandle.asDouble();

        const double finiteValues[] = {
            localScaleVal,     offsetVal[0],      offsetVal[1],      offsetVal[2],
            localRotateVal[0], localRotateVal[1], localRotateVal[2], ratioVal,
            taperAmountVal,    bendAmountVal,     arrowRatioVal,     arrowWidthVal,
        };
        dommy::generation::RigControllerPlan generationPlan;
        if (!dommy::generation::buildRigControllerPlan(
                shapeTypeVal, divisionVal, finiteValues,
                sizeof(finiteValues) / sizeof(finiteValues[0]), generationPlan) ||
            ratioVal < 0.0 || ratioVal > 2.0 || arrowRatioVal < 0.0 || arrowRatioVal > 1.0 ||
            arrowWidthVal < 0.0 || arrowWidthVal > 1.0 || dirAxisVal < D_X_AXIS ||
            dirAxisVal >= D_AXIS_TYPE_COUNT || taperAxisVal < D_X_AXIS ||
            taperAxisVal >= D_AXIS_TYPE_COUNT || bendAxisVal < D_X_AXIS || bendAxisVal > D_Y_AXIS) {
            MGlobal::displayError(
                "DRigController: inputs are invalid or exceed the curve generation budget.");
            return MS::kInvalidParameter;
        }

        // Point data
        dommy::maya::CheckedArray<MPointArray> cvs;
        dommy::maya::CheckedArray<MDoubleArray> kv;

        // new curve data
        MFnNurbsCurveData dataCreator;
        MObject newCurveData = dataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        double baseSize = localSizeVal * .5;

        // create base Y-axis
        switch (shapeTypeVal) {
        case D_CTRL_SQUARE: {
            float xRatio = 1.0;
            float zRatio = 1.0;
            if (ratioVal > 1.0) {
                zRatio = 2.0 - ratioVal;
            } else {
                xRatio = ratioVal;
            }

            cvs.append(MPoint(-baseSize * xRatio, 0.0, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, 0.0, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, 0.0, baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, 0.0, baseSize * zRatio));

        } break;
        case D_CTRL_CIRCLE: {
            float xRatio = 1.0;
            float zRatio = 1.0;
            if (ratioVal > 1.0) {
                zRatio = 2.0 - ratioVal;
            } else {
                xRatio = ratioVal;
            }

            float stepDegree = 360.0 / (double)divisionVal;

            for (unsigned int i = 0; i < divisionVal; i++) {

                MVector baseVec(0.0, 0.0, -baseSize);

                baseVec = baseVec.rotateBy(MVector::kYaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * i)));

                cvs.append(MPoint(baseVec.x * xRatio, 0.0, baseVec.z * zRatio));
            }

        } break;
        case D_CTRL_CUBE: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            float zRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
                zRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            // top
            cvs.append(MPoint(-baseSize * xRatio, baseSize * yRatio, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, baseSize * yRatio, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, baseSize * yRatio, baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, baseSize * yRatio, baseSize * zRatio));

            // bottom
            cvs.append(MPoint(-baseSize * xRatio, baseSize * yRatio, -baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, -baseSize * yRatio, -baseSize * zRatio));

            cvs.append(MPoint(baseSize * xRatio, -baseSize * yRatio, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, baseSize * yRatio, -baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, -baseSize * yRatio, -baseSize * zRatio));

            cvs.append(MPoint(baseSize * xRatio, -baseSize * yRatio, baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, baseSize * yRatio, baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, -baseSize * yRatio, baseSize * zRatio));

            cvs.append(MPoint(-baseSize * xRatio, -baseSize * yRatio, baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, baseSize * yRatio, baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, -baseSize * yRatio, baseSize * zRatio));

            cvs.append(MPoint(-baseSize * xRatio, -baseSize * yRatio, -baseSize * zRatio));
        } break;
        case D_CTRL_SPHERE: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            float zRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
                zRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            float stepDegree = 360.0 / (double)divisionVal;

            cvs.append(MPoint(0.0, 0.0, 0.0));

            // XZ
            for (unsigned int i = 0; i < divisionVal; i++) {

                MVector baseVec(0.0, 0.0, -baseSize);

                baseVec = baseVec.rotateBy(MVector::kYaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * i)));

                cvs.append(MPoint(baseVec.x * xRatio, 0.0, baseVec.z * zRatio));
            }

            cvs.append(MPoint(0.0, 0.0, -baseSize * zRatio));

            // YZ
            for (unsigned int i = 0; i < divisionVal; i++) {

                MVector baseVec(0.0, 0.0, -baseSize);

                baseVec = baseVec.rotateBy(MVector::kXaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * i)));

                cvs.append(MPoint(0.0, baseVec.y * yRatio, baseVec.z * zRatio));
            }

            cvs.append(MPoint(0.0, 0.0, -baseSize * zRatio));

            cvs.append(MPoint(0.0, 0.0, baseSize * zRatio));
            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(baseSize * xRatio, 0.0, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, 0.0, 0.0));
            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(0.0, -baseSize * yRatio, 0.0));
            cvs.append(MPoint(0.0, baseSize * yRatio, 0.0));
            // XY
            for (unsigned int i = 0; i < divisionVal; i++) {

                MVector baseVec(0.0, baseSize, 0.0);

                baseVec = baseVec.rotateBy(MVector::kZaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * i)));

                cvs.append(MPoint(baseVec.x * xRatio, baseVec.y * yRatio, 0.0));
            }

            cvs.append(MPoint(0.0, baseSize * yRatio, 0.0));

        } break;
        case D_CTRL_PYRAMID: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            float zRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
                zRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            float baseY = localSizeVal * yRatio;
            // bottom square
            cvs.append(MPoint(0.0, baseY, -baseSize * zRatio));
            cvs.append(MPoint(-baseSize * xRatio, baseY, 0.0));
            cvs.append(MPoint(0.0, baseY, baseSize * zRatio));
            cvs.append(MPoint(baseSize * xRatio, baseY, 0.0));
            cvs.append(MPoint(0.0, baseY, -baseSize * zRatio));

            // top
            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(0.0, baseY, baseSize * zRatio));
            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, baseY, 0.0));
            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(baseSize * xRatio, baseY, 0.0));
        } break;
        case D_CTRL_S_ARROW: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            float arrowBarWidth = baseSize * (1.0 - arrowWidthVal);

            if (!arrowReverseVal) { // dir up
                // start top 左回り
                cvs.append(MPoint(0.0, localSizeVal * yRatio, 0.0));
                cvs.append(MPoint(-baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));
                cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                                  localSizeVal * arrowRatioVal * yRatio, 0.0));

                addDivision(cvs,
                            MVector((-baseSize + arrowBarWidth) * xRatio,
                                    localSizeVal * arrowRatioVal * yRatio, 0.0),
                            MVector((-baseSize + arrowBarWidth) * xRatio, 0.0, 0.0), divisionVal);

                cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio, 0.0, 0.0));
                cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio, 0.0, 0.0));

                addDivision(cvs, MVector((baseSize - arrowBarWidth) * xRatio, 0.0, 0.0),
                            MVector((baseSize - arrowBarWidth) * xRatio,
                                    localSizeVal * arrowRatioVal * yRatio, 0.0),
                            divisionVal);

                cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                                  localSizeVal * arrowRatioVal * yRatio, 0.0));
                cvs.append(MPoint(baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));

            } else { // dir origin
                // start bottom 右回り
                arrowRatioVal = 1.0 - arrowRatioVal;

                cvs.append(MPoint(0.0, 0.0, 0.0));
                cvs.append(MPoint(-baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));
                cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                                  localSizeVal * arrowRatioVal * yRatio, 0.0));

                addDivision(
                    cvs,
                    MVector((-baseSize + arrowBarWidth) * xRatio,
                            localSizeVal * arrowRatioVal * yRatio, 0.0),
                    MVector((-baseSize + arrowBarWidth) * xRatio, localSizeVal * yRatio, 0.0),
                    divisionVal);

                cvs.append(
                    MPoint((-baseSize + arrowBarWidth) * xRatio, localSizeVal * yRatio, 0.0));
                cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio, localSizeVal * yRatio, 0.0));

                addDivision(
                    cvs, MVector((baseSize - arrowBarWidth) * xRatio, localSizeVal * yRatio, 0.0),
                    MVector((baseSize - arrowBarWidth) * xRatio,
                            localSizeVal * arrowRatioVal * yRatio, 0.0),
                    divisionVal);

                cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                                  localSizeVal * arrowRatioVal * yRatio, 0.0));
                cvs.append(MPoint(baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));
            }

            // deform
            if (bendAmountVal != 0.0) {
                if (!bendDeform(cvs, localSizeVal, 0.0,
                                dommy::math::degreesToRadians(bendAmountVal), bendAxisVal)) {
                    return MS::kFailure;
                }
            }

        } break;
        case D_CTRL_TWIN_ARROW: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            float arrowBarWidth = baseSize * (1.0 - arrowWidthVal);

            // top
            cvs.append(MPoint(0.0, localSizeVal * yRatio, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              localSizeVal * arrowRatioVal * yRatio, 0.0));

            addDivision(cvs,
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                localSizeVal * arrowRatioVal * yRatio, 0.0),
                        MVector((-baseSize + arrowBarWidth) * xRatio, 0.0, 0.0), divisionVal);

            // half
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio, 0.0, 0.0));

            addDivision(cvs, MVector((-baseSize + arrowBarWidth) * xRatio, 0.0, 0.0),
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                -localSizeVal * arrowRatioVal * yRatio, 0.0),
                        divisionVal);

            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              -localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, -localSizeVal * arrowRatioVal * yRatio, 0.0));
            // bottom
            cvs.append(MPoint(0.0, -localSizeVal * yRatio, 0.0));
            cvs.append(MPoint(baseSize * xRatio, -localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              -localSizeVal * arrowRatioVal * yRatio, 0.0));

            addDivision(cvs,
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                -localSizeVal * arrowRatioVal * yRatio, 0.0),
                        MVector((baseSize - arrowBarWidth) * xRatio, 0.0, 0.0), divisionVal);

            // half
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio, 0.0, 0.0));

            addDivision(cvs, MVector((baseSize - arrowBarWidth) * xRatio, 0.0, 0.0),
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                localSizeVal * arrowRatioVal * yRatio, 0.0),
                        divisionVal);

            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint(baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));

            // deform
            if (bendAmountVal != 0.0) {
                if (!bendDeform(cvs, localSizeVal, -localSizeVal,
                                dommy::math::degreesToRadians(bendAmountVal), bendAxisVal)) {
                    return MS::kFailure;
                }
            }
        } break;
        case D_CTRL_CROSS_ARROW: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            if (ratioVal > 1.0) {
                xRatio = 2.0 - ratioVal;
            } else {
                yRatio = ratioVal;
            }

            float arrowBarWidth = baseSize * (1.0 - arrowWidthVal);

            // top
            cvs.append(MPoint(0.0, localSizeVal * yRatio, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              localSizeVal * arrowRatioVal * yRatio, 0.0));

            addDivision(cvs,
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                localSizeVal * arrowRatioVal * yRatio, 0.0),
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // center
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              (baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector(-localSizeVal * arrowRatioVal * yRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // left
            cvs.append(MPoint(-localSizeVal * arrowRatioVal * yRatio,
                              (baseSize - arrowBarWidth) * xRatio, 0.0));
            cvs.append(MPoint(-localSizeVal * arrowRatioVal * yRatio, baseSize * xRatio, 0.0));
            cvs.append(MPoint(-localSizeVal * yRatio, 0.0, 0.0));
            cvs.append(MPoint(-localSizeVal * arrowRatioVal * yRatio, -baseSize * xRatio, 0.0));
            cvs.append(MPoint(-localSizeVal * arrowRatioVal * yRatio,
                              -(baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector(-localSizeVal * arrowRatioVal * yRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // center
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              -(baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector((-baseSize + arrowBarWidth) * xRatio,
                                -localSizeVal * arrowRatioVal * yRatio, 0.0),
                        divisionVal);

            // bottom
            cvs.append(MPoint((-baseSize + arrowBarWidth) * xRatio,
                              -localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint(-baseSize * xRatio, -localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint(0.0, -localSizeVal * yRatio, 0.0));
            cvs.append(MPoint(baseSize * xRatio, -localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              -localSizeVal * arrowRatioVal * yRatio, 0.0));

            addDivision(cvs,
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                -localSizeVal * arrowRatioVal * yRatio, 0.0),
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // center
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              -(baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector(localSizeVal * arrowRatioVal * yRatio,
                                -(baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // right
            cvs.append(MPoint(localSizeVal * arrowRatioVal * yRatio,
                              -(baseSize - arrowBarWidth) * xRatio, 0.0));
            cvs.append(MPoint(localSizeVal * arrowRatioVal * yRatio, -baseSize * xRatio, 0.0));
            cvs.append(MPoint(localSizeVal * yRatio, 0.0, 0.0));
            cvs.append(MPoint(localSizeVal * arrowRatioVal * yRatio, baseSize * xRatio, 0.0));
            cvs.append(MPoint(localSizeVal * arrowRatioVal * yRatio,
                              (baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector(localSizeVal * arrowRatioVal * yRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        divisionVal);

            // center
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              (baseSize - arrowBarWidth) * xRatio, 0.0));

            addDivision(cvs,
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                (baseSize - arrowBarWidth) * xRatio, 0.0),
                        MVector((baseSize - arrowBarWidth) * xRatio,
                                localSizeVal * arrowRatioVal * yRatio, 0.0),
                        divisionVal);

            // top
            cvs.append(MPoint((baseSize - arrowBarWidth) * xRatio,
                              localSizeVal * arrowRatioVal * yRatio, 0.0));
            cvs.append(MPoint(baseSize * xRatio, localSizeVal * arrowRatioVal * yRatio, 0.0));

            // deform
            if (bendAmountVal != 0.0) {
                if (!crossBendDeform(cvs, localSizeVal, -localSizeVal,
                                     dommy::math::degreesToRadians(bendAmountVal))) {
                    return MS::kFailure;
                }
            }
        }

        break;
        case D_CTRL_PIN: {
            float xRatio = 1.0;
            float yRatio = 1.0;
            if (ratioVal > 1.0) {
                yRatio = 2.0 - ratioVal;
            } else {
                xRatio = ratioVal;
            }

            float stepDegree = 360.0 / (double)divisionVal;
            float radius = (localSizeVal * arrowRatioVal) * .5;

            cvs.append(MPoint(0.0, 0.0, 0.0));
            cvs.append(MPoint(0.0, localSizeVal * yRatio, 0.0));

            for (unsigned int i = 0; i < divisionVal; i++) {

                MVector baseVec(0.0, radius, 0.0);

                baseVec = baseVec.rotateBy(MVector::kZaxis,
                                           dommy::math::degreesToRadians(-(stepDegree * i)));

                cvs.append(MPoint(baseVec.x * xRatio,
                                  (baseVec.y + (localSizeVal - radius)) * yRatio, 0.0));
            }

            cvs.append(MPoint(0.0, localSizeVal * yRatio, 0.0));
        } break;
        default:
            MGlobal::displayError("DRigController: unknown shape type.");
            return MS::kFailure;
        }

        if (cvs.length() == 0U ||
            static_cast<std::size_t>(cvs.length()) > generationPlan.maximumPointCount) {
            MGlobal::displayError("DRigController: generated curve exceeds the point budget.");
            return MS::kFailure;
        }

        // deform
        if (taperAmountVal != 0.0) {
            if (!taperDeform(cvs, localSizeVal, -localSizeVal, taperAmountVal, taperAxisVal)) {
                return MS::kFailure;
            }
        }

        MTransformationMatrix dirTransMat = MTransformationMatrix();

        if (dirAxisVal == D_X_AXIS) {
            double lrot[3] = {dommy::math::degreesToRadians(0), dommy::math::degreesToRadians(0),
                              dommy::math::degreesToRadians(90)};
            DOMMY_RETURN_IF_FAILURE(dirTransMat.setRotation(lrot, MTransformationMatrix::kXYZ));
        } else if (dirAxisVal == D_Z_AXIS) {
            double lrot[3] = {dommy::math::degreesToRadians(90), dommy::math::degreesToRadians(0),
                              dommy::math::degreesToRadians(0)};
            DOMMY_RETURN_IF_FAILURE(dirTransMat.setRotation(lrot, MTransformationMatrix::kXYZ));
        } else if (dirAxisVal == D_Y_AXIS_MINUS) {
            double lrot[3] = {dommy::math::degreesToRadians(180), dommy::math::degreesToRadians(0),
                              dommy::math::degreesToRadians(0)};
            DOMMY_RETURN_IF_FAILURE(dirTransMat.setRotation(lrot, MTransformationMatrix::kXYZ));
        } else if (dirAxisVal == D_X_AXIS_MINUS) {
            double lrot[3] = {dommy::math::degreesToRadians(0), dommy::math::degreesToRadians(0),
                              dommy::math::degreesToRadians(-90)};
            DOMMY_RETURN_IF_FAILURE(dirTransMat.setRotation(lrot, MTransformationMatrix::kXYZ));
        } else if (dirAxisVal == D_Z_AXIS_MINUS) {
            double lrot[3] = {dommy::math::degreesToRadians(-90), dommy::math::degreesToRadians(0),
                              dommy::math::degreesToRadians(0)};
            DOMMY_RETURN_IF_FAILURE(dirTransMat.setRotation(lrot, MTransformationMatrix::kXYZ));
        }

        MTransformationMatrix rotMat = MTransformationMatrix();
        MTransformationMatrix scaleMat = MTransformationMatrix();
        double s[3] = {localScaleVal, localScaleVal, localScaleVal};
        DOMMY_RETURN_IF_FAILURE(scaleMat.setScale(s, MSpace::kWorld));

        double lrot[3] = {dommy::math::degreesToRadians(localRotateVal[0]),
                          dommy::math::degreesToRadians(localRotateVal[1]),
                          dommy::math::degreesToRadians(localRotateVal[2])};
        DOMMY_RETURN_IF_FAILURE(rotMat.setRotation(lrot, MTransformationMatrix::kXYZ));

        MVector firstPoint(cvs[0]);
        firstPoint *= dirTransMat.asMatrix();
        firstPoint *= rotMat.asMatrix();
        if (!dommy::maya::isFiniteVector(firstPoint))
            return MS::kFailure;

        double minX = firstPoint.x;
        double maxX = firstPoint.x;
        double minY = firstPoint.y;
        double maxY = firstPoint.y;
        double minZ = firstPoint.z;
        double maxZ = firstPoint.z;
        cvs[0] = MPoint(firstPoint.x, firstPoint.y, firstPoint.z);

        for (unsigned i = 1U; i < cvs.length(); ++i) {
            MVector newVec(cvs[i]);
            newVec *= dirTransMat.asMatrix();
            newVec *= rotMat.asMatrix();
            if (!dommy::maya::isFiniteVector(newVec))
                return MS::kFailure;

            minX = std::min(minX, newVec.x);
            maxX = std::max(maxX, newVec.x);
            minY = std::min(minY, newVec.y);
            maxY = std::max(maxY, newVec.y);
            minZ = std::min(minZ, newVec.z);
            maxZ = std::max(maxZ, newVec.z);
            cvs[i] = MPoint(newVec.x, newVec.y, newVec.z);
        }

        double midX = 0, midY = 0, midZ = 0;

        if (keepCenterVal) {
            midX = minX + (maxX - minX) * .5;
            midY = minY + (maxY - minY) * .5;
            midZ = minZ + (maxZ - minZ) * .5;
            midX = -midX;
            midY = -midY;
            midZ = -midZ;
        }

        for (long i = 0; i < cvs.length(); i++) {
            MVector newVec;

            if (keepCenterVal) {
                newVec = MVector(cvs[i].x + midX, cvs[i].y + midY, cvs[i].z + midZ);
            } else {
                newVec = MVector(cvs[i].x, cvs[i].y, cvs[i].z);
            }

            newVec *= scaleMat.asMatrix();
            if (!dommy::maya::isFiniteVector(newVec))
                return MS::kFailure;

            const MPoint transformed(newVec.x + offsetVal[0], newVec.y + offsetVal[1],
                                     newVec.z + offsetVal[2]);
            if (!dommy::maya::isFinitePoint(transformed))
                return MS::kFailure;
            cvs[i] = transformed;
        }

        const unsigned deg = 1;
        const unsigned ncvs = cvs.length();
        const unsigned spans = ncvs - deg;
        const unsigned nknots = spans + 2 * deg - 1;
        for (unsigned j = 0U; j < nknots; ++j)
            kv.append(static_cast<double>(j));

        MFnNurbsCurve curveFn;
        curveFn.create(cvs, kv, deg, MFnNurbsCurve::kClosed, false, false, newCurveData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        MDataHandle curveHandle = block.outputValue(DRigController::outputCurve, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(curveHandle.set(newCurveData));

        stat = block.setClean(plug);
        if (stat != MS::kSuccess) {
            MGlobal::displayError(MString("DRigController: failed to mark outputCurve clean: ") +
                                  stat.errorString());
            return stat;
        }

        return MS::kSuccess;
    } else {
        return MS::kUnknownParameter;
    }

    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DRigController")

MStatus DRigController::initialize() {
    MFnNumericAttribute nAttr;
    MFnTypedAttribute typedAttr;
    MStatus stat;

    MFnEnumAttribute enumFn;
    shapeType = enumFn.create("shapeType", "st", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Square", D_CTRL_SQUARE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Circle", D_CTRL_CIRCLE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Cube", D_CTRL_CUBE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Sphere", D_CTRL_SPHERE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Pyramid", D_CTRL_PYRAMID));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Simple Arrow", D_CTRL_S_ARROW));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Slide Arrow", D_CTRL_TWIN_ARROW));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Cross Arrow", D_CTRL_CROSS_ARROW));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Pin", D_CTRL_PIN));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(shapeType));

    localScale = nAttr.create("localScale", "lsc", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(localScale));

    division = nAttr.create("division", "div", MFnNumericData::kLong, 8, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(dommy::generation::kMaxRigControllerDivisionCount));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(division));

    offsetX = nAttr.create("offsetX", "ofx", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetY = nAttr.create("offsetY", "ofy", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetZ = nAttr.create("offsetZ", "ofz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    offset = nAttr.create("offset", "of", offsetX, offsetY, offsetZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));

    localRotateX = nAttr.create("localRotateX", "lrx", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    localRotateY = nAttr.create("localRotateY", "lry", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    localRotateZ = nAttr.create("localRotateZ", "lrz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    localRotate =
        nAttr.create("localRotate", "lr", localRotateX, localRotateY, localRotateZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(localRotate));

    keepCenter = nAttr.create("keepCenter", "kc", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(keepCenter));

    ratio = nAttr.create("ratio", "ra", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ratio));

    dirAxis = enumFn.create("dirAxis", "dax", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+X", D_X_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+Y", D_Y_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+Z", D_Z_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-X", D_X_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-Y", D_Y_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-Z", D_Z_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dirAxis));

    taperAmount = nAttr.create("taperAmount", "tam", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(taperAmount));

    taperAxis = enumFn.create("taperAxis", "tax", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+X", D_X_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+Y", D_Y_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("+Z", D_Z_AXIS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-X", D_X_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-Y", D_Y_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("-Z", D_Z_AXIS_MINUS));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(taperAxis));

    bendAmount = nAttr.create("bendAmount", "bamt", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(bendAmount));

    MFnEnumAttribute bendEnumFn;
    bendAxis = bendEnumFn.create("bendAxis", "baxs", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(bendEnumFn.addField("X", D_X_AXIS));
    DOMMY_RETURN_IF_FAILURE(bendEnumFn.addField("Z", D_Y_AXIS));
    DOMMY_RETURN_IF_FAILURE(bendEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(bendEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(bendAxis));

    arrowReverse = nAttr.create("arrowReverse", "arev", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(arrowReverse));

    arrowRatio = nAttr.create("arrowRatio", "arra", MFnNumericData::kDouble, 0.75, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(arrowRatio));

    arrowWidth = nAttr.create("arrowWidth", "arwi", MFnNumericData::kDouble, 0.25, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(arrowWidth));

    outputCurve = typedAttr.create("outputCurve", "out", MFnNurbsCurveData::kNurbsCurve,
                                   MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(shapeType, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(localScale, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(division, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(localRotate, outputCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(keepCenter, outputCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(ratio, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(dirAxis, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(taperAxis, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(taperAmount, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(bendAmount, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(bendAxis, outputCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(arrowReverse, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(arrowRatio, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(arrowWidth, outputCurve));

    return MS::kSuccess;
}
