//
//  DMatToVec.cpp
//  dommyTool
//
//  Created by yuzo on 2018/05/20.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMatToVec.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"

#include "general_ids.h"

const MTypeId DMatToVec::typeId(ID_UTILITY_MAT_TO_VEC);

MObject DMatToVec::inMatrix;
MObject DMatToVec::aType;
MObject DMatToVec::aSpace;
MObject DMatToVec::outVector;

void* DMatToVec::creator() {
    return ::dommy::maya::createObject("DMatToVec", [&]() {
        return new DMatToVec();
    });
}

MStatus DMatToVec::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;
    if (plug != outVector)
        return MS::kUnknownParameter;

    const MDataHandle typeHandle = data.inputValue(aType, &stat);
    if (!stat)
        return stat;
    const short type = typeHandle.asShort();
    if (type < 0 || type > 2)
        return MS::kInvalidParameter;

    const MDataHandle spaceHandle = data.inputValue(aSpace, &stat);
    if (!stat)
        return stat;
    const short spaceValue = spaceHandle.asShort();
    if (spaceValue < 0 || spaceValue > 1)
        return MS::kInvalidParameter;
    const MSpace::Space space = spaceValue == 0 ? MSpace::kWorld : MSpace::kObject;

    const MDataHandle matrixHandle = data.inputValue(inMatrix, &stat);
    if (!stat)
        return stat;
    const MMatrix matrix = matrixHandle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(matrix))
        return MS::kInvalidParameter;

    const MTransformationMatrix transform(matrix);
    MVector output(0.0, 0.0, 0.0);
    if (type == 0) {
        output = transform.getTranslation(space, &stat);
    } else if (type == 1) {
        double rotation[3] = {0.0, 0.0, 0.0};
        MTransformationMatrix::RotationOrder order = MTransformationMatrix::kXYZ;
        stat = transform.getRotation(rotation, order);
        output = MVector(rotation[0], rotation[1], rotation[2]);
    } else {
        double scale[3] = {1.0, 1.0, 1.0};
        stat = transform.getScale(scale, space);
        output = MVector(scale[0], scale[1], scale[2]);
    }
    if (!stat)
        return stat;
    if (!dommy::maya::isFiniteVector(output))
        return MS::kFailure;

    MDataHandle outputHandle = data.outputValue(outVector, &stat);
    if (!stat)
        return stat;
    outputHandle.set3Double(output.x, output.y, output.z);
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMatToVec")

MStatus DMatToVec::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnMatrixAttribute matAttr;
    MFnEnumAttribute eAttr;

    inMatrix = matAttr.create("inMatrix", "ima", MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMatrix));

    aType = eAttr.create("Type", "ty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Translate", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Rotate", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Scale", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));

    aSpace = eAttr.create("Space", "sp", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("World", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Object", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpace));

    outVector = nAttr.create("outVector", "ov", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outVector));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMatrix, outVector));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aType, outVector));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpace, outVector));

    return MS::kSuccess;
}
