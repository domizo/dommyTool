//
//  DComponentListGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/20.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DComponentListGen.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DIndexPattern.h"

#include "DMayaDebug.h"
#include "DMayaHeaders.h"
#include "general_ids.h"

#include <cstddef>
#include <limits>
#include <vector>

const MTypeId DComponentListGen::typeId(ID_UTILITY_COMPONENTLIST_GEN);

MObject DComponentListGen::aComponentType;
MObject DComponentListGen::aMode;
MObject DComponentListGen::aSize;
MObject DComponentListGen::aPattern;
MObject DComponentListGen::inputDataArray;
MObject DComponentListGen::outComponentList;

void* DComponentListGen::creator() {
    return ::dommy::maya::createObject("DComponentListGen", [&]() {
        return new DComponentListGen();
    });
}

MStatus DComponentListGen::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outComponentList)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle componentTypeHandle = data.inputValue(aComponentType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short componentType = componentTypeHandle.asShort();
    if (componentType < 0 || componentType > 3) {
        return MS::kInvalidParameter;
    }
    const MDataHandle modeHandle = data.inputValue(aMode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short mode = modeHandle.asShort();
    if (mode < 0 || mode > 2)
        return MS::kInvalidParameter;

    MIntArray elements;
    if (mode == 0) {
        const MDataHandle sizeHandle = data.inputValue(aSize, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int rawSize = sizeHandle.asInt();
        std::size_t size = 0U;
        if (!dommy::generation::checkedCount(rawSize, dommy::generation::kMaxGeneratedPointCount,
                                             size)) {
            return MS::kInvalidParameter;
        }
        for (std::size_t index = 0U; index < size; ++index) {
            DOMMY_RETURN_IF_FAILURE(elements.append(static_cast<int>(index)));
        }
    } else if (mode == 1) {
        const MDataHandle patternHandle = data.inputValue(aPattern, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MString pattern = patternHandle.asString();
        std::vector<std::size_t> parsed;
        bool reverse = false;
        const std::size_t indexLimit =
            static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1U;
        if (!dommy::component::parseIndexPattern(pattern.asChar(), indexLimit,
                                                 dommy::generation::kMaxGeneratedPointCount,
                                                 reverse, parsed) ||
            reverse) {
            EchoWarning(name().asChar(), "Pattern must contain non-negative indices and "
                                         "end-exclusive ranges; reverse is not supported here.");
            return MS::kInvalidParameter;
        }
        for (std::size_t index = 0U; index < parsed.size(); ++index) {
            DOMMY_RETURN_IF_FAILURE(elements.append(static_cast<int>(parsed[index])));
        }
    } else {
        const bool connected = MPlug(thisMObject(), inputDataArray).isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected) {
            EchoWarning(name().asChar(), "Not Connected inputDataArray plug!");
            return MS::kFailure;
        }
        MDataHandle inputHandle = data.inputValue(inputDataArray, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (inputHandle.type() != MFnData::kIntArray) {
            return MS::kInvalidParameter;
        }
        MFnIntArrayData arrayData(inputHandle.data(), &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MIntArray inputElements = arrayData.array(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (inputElements.length() > dommy::generation::kMaxGeneratedPointCount) {
            return MS::kInvalidParameter;
        }
        for (unsigned int index = 0U; index < inputElements.length(); ++index) {
            if (inputElements[index] < 0)
                return MS::kInvalidParameter;
        }
        elements = inputElements;
    }

    MFnComponentListData componentList;
    const MObject componentData = componentList.create(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (componentData.isNull())
        return MS::kFailure;

    const MFn::Type mayaComponentTypes[] = {MFn::kMeshVertComponent, MFn::kMeshEdgeComponent,
                                            MFn::kMeshPolygonComponent, MFn::kMeshMapComponent};
    MFnSingleIndexedComponent componentCreator;
    MObject component = componentCreator.create(mayaComponentTypes[componentType], &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (component.isNull())
        return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(componentCreator.addElements(elements));
    DOMMY_RETURN_IF_FAILURE(componentList.add(component));

    MDataHandle outputHandle = data.outputValue(outComponentList, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(outputHandle.set(componentData));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DComponentListGen")

MStatus DComponentListGen::initialize() {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    MFnEnumAttribute componentTypeAttribute;
    aComponentType = componentTypeAttribute.create("ComponentType", "compty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(componentTypeAttribute.addField("Point", 0));
    DOMMY_RETURN_IF_FAILURE(componentTypeAttribute.addField("Edge", 1));
    DOMMY_RETURN_IF_FAILURE(componentTypeAttribute.addField("Face", 2));
    DOMMY_RETURN_IF_FAILURE(componentTypeAttribute.addField("Map", 3));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aComponentType));

    MFnEnumAttribute modeAttribute;
    aMode = modeAttribute.create("Mode", "md", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField("Size", 0));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField("Pattern", 1));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField("Input Data", 2));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));

    aSize = numericAttribute.create("Size", "sz", MFnNumericData::kInt, 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSize));

    const MObject emptyString = stringData.create("", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aPattern = typedAttribute.create("Pattern", "patt", MFnData::kString, emptyString, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPattern));

    inputDataArray = genericAttribute.create("inputDataArray", "indataarray", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputDataArray));

    MFnComponentListData defaultDataCreator;
    const MObject defaultData = defaultDataCreator.create(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    outComponentList = typedAttribute.create("outComponentList", "outcompdata",
                                             MFnData::kComponentList, defaultData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outComponentList));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aComponentType, outComponentList));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSize, outComponentList));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMode, outComponentList));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPattern, outComponentList));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputDataArray, outComponentList));
    return MS::kSuccess;
}
