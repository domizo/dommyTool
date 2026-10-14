//
//  DCurveFromPfx.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/05.
//

#include "DCurveFromPfx.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DCurveGenerationStorage.h"

#include "DMayaHeaders.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include <cmath>
#include <cstddef>
#include <vector>
#include <algorithm>


namespace {

struct PfxExportOptions {
    int degree;
    bool width;
    bool twist;
    bool flatness;
    bool parameter;
    bool color;
    bool incandescence;
    bool transparency;

    std::size_t enabledAttributeCount() const
    {
        return static_cast<std::size_t>(width) +
               static_cast<std::size_t>(twist) +
               static_cast<std::size_t>(flatness) +
               static_cast<std::size_t>(parameter) +
               static_cast<std::size_t>(color) +
               static_cast<std::size_t>(incandescence) +
               static_cast<std::size_t>(transparency);
    }
};

struct PfxGenerationBudget {
    std::size_t curveCount;
    std::size_t pointCount;
    std::size_t workItemCount;
    std::size_t largestLine, metadataBytes, estimatedBytes;

    PfxGenerationBudget()
        : curveCount(0U), pointCount(0U), workItemCount(0U),
          largestLine(0U), metadataBytes(0U), estimatedBytes(0U)
    {
    }
};

MStatus reserveLineBudget(std::size_t linePointCount,
                          const PfxExportOptions& options,
                          PfxGenerationBudget& budget)
{
    std::size_t nextCurveCount = 0U;
    std::size_t nextPointCount = 0U;
    std::size_t perLineWork = 0U;
    std::size_t nextWorkItemCount = 0U;
    if (!dommy::generation::checkedSum(
            budget.curveCount, 1U,
            dommy::generation::kMaxGeneratedCurveCount,
            nextCurveCount) ||
        !dommy::generation::checkedSum(
            budget.pointCount, linePointCount,
            dommy::generation::kMaxGeneratedPointCount,
            nextPointCount) ||
        !dommy::generation::checkedProduct(
            linePointCount,
            options.enabledAttributeCount() + 1U,
            dommy::generation::kMaxWorkItemCount,
            perLineWork) ||
        !dommy::generation::checkedSum(
            budget.workItemCount, perLineWork,
            dommy::generation::kMaxWorkItemCount,
            nextWorkItemCount)) {
        return MS::kFailure;
    }

    const std::size_t largestLine = std::max(budget.largestLine, linePointCount);
    const std::size_t doubles = options.width + options.flatness + options.parameter;
    const std::size_t vectors = options.twist + options.color + options.incandescence + options.transparency;
    std::size_t bytes = 0U, metadata = budget.metadataBytes;
    // Four copies of values plus defaults, names and attribute-map objects.
    if (!dommy::generation::addCurveGenerationBytes(linePointCount, doubles * 32U + vectors * 48U, metadata) ||
        !dommy::generation::addCurveGenerationBytes(options.enabledAttributeCount(), 1024U, metadata) ||
        !dommy::generation::curveGenerationStorageBytes(nextCurveCount, nextPointCount, largestLine, 0U, bytes) ||
        !dommy::generation::addCurveGenerationBytes(1U, metadata, bytes)) return MS::kInvalidParameter;
    budget.largestLine = largestLine;
    budget.metadataBytes = metadata;
    budget.estimatedBytes = bytes;
    budget.curveCount = nextCurveCount;
    budget.pointCount = nextPointCount;
    budget.workItemCount = nextWorkItemCount;
    return MS::kSuccess;
}

MStatus preflightLines(const MRenderLineArray& lines, const PfxExportOptions& options,
                       PfxGenerationBudget& budget)
{
    std::size_t count = 0U;
    if (!dommy::generation::checkedCount(lines.length(),
            dommy::generation::kMaxGeneratedCurveCount, count)) return MS::kInvalidParameter;
    for (std::size_t i = 0U; i < count; ++i) {
        MStatus status;
        const MRenderLine line = lines.renderLine(static_cast<unsigned int>(i), &status);
        DOMMY_RETURN_IF_FAILURE(status);
        // Maya exposes the point count through this returned array. Its native
        // retrieval allocation cannot be admitted before getLine/getLineData.
        const std::size_t points = line.getLine().length();
        if (points >= 2U) DOMMY_RETURN_IF_FAILURE(reserveLineBudget(points, options, budget));
    }
    return MS::kSuccess;
}

MStatus addDoubleAttribute(const MDoubleArray& source,
                           std::size_t expectedCount,
                           const char* name,
                           const DCItem& curve)
{
    if (source.length() != expectedCount) return MS::kFailure;

    std::vector<double> values;
    values.reserve(expectedCount);
    for (unsigned int index = 0U; index < source.length(); ++index) {
        if (!std::isfinite(source[index])) return MS::kFailure;
        values.push_back(source[index]);
    }
    curve->addAttr(name, DPDoubleAttr(values, 0.0));
    return MS::kSuccess;
}

MStatus addVectorAttribute(const MVectorArray& source,
                           std::size_t expectedCount,
                           const char* name,
                           const DCItem& curve)
{
    if (source.length() != expectedCount) return MS::kFailure;

    std::vector<Imath::V3f> values;
    values.reserve(expectedCount);
    for (unsigned int index = 0U; index < source.length(); ++index) {
        if (!dommy::maya::isFloatRepresentableVector(source[index])) {
            return MS::kFailure;
        }
        values.push_back(Imath::V3f(
            static_cast<float>(source[index].x),
            static_cast<float>(source[index].y),
            static_cast<float>(source[index].z)));
    }
    curve->addAttr(
        name, DPVec3fAttr(values, Imath::V3f(0.0f, 0.0f, 0.0f)));
    return MS::kSuccess;
}

MStatus appendLines(const MRenderLineArray& lines,
                    const PfxExportOptions& options,
                    PfxGenerationBudget& budget,
                    DCurveData::DCurveList& generatedCurves)
{
    const int rawLineCount = lines.length();
    std::size_t lineCount = 0U;
    if (!dommy::generation::checkedCount(
            rawLineCount,
            dommy::generation::kMaxGeneratedCurveCount,
            lineCount)) {
        return MS::kFailure;
    }

    for (std::size_t lineIndex = 0U;
         lineIndex < lineCount; ++lineIndex) {
        MStatus status;
        const MRenderLine line = lines.renderLine(
            static_cast<unsigned int>(lineIndex), &status);
        if (!status) return status;

        const MVectorArray vertices = line.getLine();
        const std::size_t pointCount =
            static_cast<std::size_t>(vertices.length());
        if (pointCount < 2U) continue;

        status = reserveLineBudget(pointCount, options, budget);
        if (!status) return status;

        MPointArray controlVertices(
            static_cast<unsigned int>(pointCount));
        if (controlVertices.length() != pointCount) return MS::kFailure;
        for (unsigned int point = 0U;
             point < vertices.length(); ++point) {
            if (!dommy::maya::isFiniteVector(vertices[point])) {
                return MS::kFailure;
            }
            controlVertices[point] = MPoint(vertices[point]);
        }

        const int degree =
            options.degree == 3 && pointCount >= 4U ? 3 : 1;
        MDoubleArray knots;
        GenerateDCurveKnots(
            controlVertices.length(), degree, knots);

        DCItem curve(new DCurveInternalData(
            controlVertices, knots, degree));
        if (!curve || !curve->hasValidCurveTopology()) {
            return MS::kFailure;
        }

        if (options.width) {
            status = addDoubleAttribute(
                line.getWidth(), pointCount, "width", curve);
            if (!status) return status;
        }
        if (options.twist) {
            status = addVectorAttribute(
                line.getTwist(), pointCount, "twist", curve);
            if (!status) return status;
        }
        if (options.flatness) {
            status = addDoubleAttribute(
                line.getFlatness(), pointCount, "flatness", curve);
            if (!status) return status;
        }
        if (options.parameter) {
            status = addDoubleAttribute(
                line.getParameter(), pointCount, "param", curve);
            if (!status) return status;
        }
        if (options.color) {
            status = addVectorAttribute(
                line.getColor(), pointCount, "Cd", curve);
            if (!status) return status;
        }
        if (options.incandescence) {
            status = addVectorAttribute(
                line.getIncandescence(), pointCount,
                "Incandescence", curve);
            if (!status) return status;
        }
        if (options.transparency) {
            status = addVectorAttribute(
                line.getTransparency(), pointCount,
                "Transparency", curve);
            if (!status) return status;
        }

        generatedCurves.push_back(curve);
    }
    return MS::kSuccess;
}

MStatus readBoolInput(MDataBlock& data,
                      const MObject& attribute,
                      bool& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) return status;
    value = handle.asBool();
    return MS::kSuccess;
}

} // namespace


const MTypeId DCurveFromPfx::typeId(ID_DC_FROM_PFX);

MObject DCurveFromPfx::inPfx;
MObject DCurveFromPfx::aEvaluationTime;
MObject DCurveFromPfx::aDegree;

MObject DCurveFromPfx::aIsMainLines;
MObject DCurveFromPfx::aIsLeafLines;
MObject DCurveFromPfx::aIsFlowerLines;

MObject DCurveFromPfx::aIsExportTwist;
MObject DCurveFromPfx::aIsExportWidth;
MObject DCurveFromPfx::aIsExportFlat;
MObject DCurveFromPfx::aIsExportParameter;
MObject DCurveFromPfx::aIsExportColor;
MObject DCurveFromPfx::aIsExportIncandence;
MObject DCurveFromPfx::aIsExportTransparency;

MObject DCurveFromPfx::outDC;


void* DCurveFromPfx::creator()
{
    return ::dommy::maya::createObject("DCurveFromPfx", [&]() {
        return new DCurveFromPfx();
    });
}


MStatus DCurveFromPfx::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle evaluationTimeHandle =
        data.inputValue(aEvaluationTime, &status);
    if (!status) return status;
    (void)evaluationTimeHandle.asTime();

    const MDataHandle degreeHandle = data.inputValue(aDegree, &status);
    if (!status) return status;
    const short degreeType = degreeHandle.asShort();
    if (degreeType != 0 && degreeType != 1) {
        MGlobal::displayError(
            "DCurveFromPfx: unsupported degree type.");
        return MS::kInvalidParameter;
    }

    bool exportMainLines = false;
    bool exportLeafLines = false;
    bool exportFlowerLines = false;
    PfxExportOptions options;
    options.degree = degreeType == 0 ? 1 : 3;

    status = readBoolInput(data, aIsMainLines, exportMainLines);
    if (!status) return status;
    status = readBoolInput(data, aIsLeafLines, exportLeafLines);
    if (!status) return status;
    status = readBoolInput(data, aIsFlowerLines, exportFlowerLines);
    if (!status) return status;
    status = readBoolInput(data, aIsExportWidth, options.width);
    if (!status) return status;
    status = readBoolInput(data, aIsExportTwist, options.twist);
    if (!status) return status;
    status = readBoolInput(data, aIsExportFlat, options.flatness);
    if (!status) return status;
    status = readBoolInput(
        data, aIsExportParameter, options.parameter);
    if (!status) return status;
    status = readBoolInput(data, aIsExportColor, options.color);
    if (!status) return status;
    status = readBoolInput(
        data, aIsExportIncandence, options.incandescence);
    if (!status) return status;
    status = readBoolInput(
        data, aIsExportTransparency, options.transparency);
    if (!status) return status;

    MPlug messagePlug(thisMObject(), inPfx);
    const MPlug sourcePlug = messagePlug.source(&status);
    if (!status || sourcePlug.isNull()) {
        MGlobal::displayWarning(
            "DCurveFromPfx: inPfx is not connected.");
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    const MObject pfxNode = sourcePlug.node(&status);
    if (!status || pfxNode.isNull() ||
        !pfxNode.hasFn(MFn::kPfxGeometry)) {
        MGlobal::displayError(
            "DCurveFromPfx: inPfx must be connected to PfxGeometry.");
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    MFnPfxGeometry pfxGeometry(pfxNode, &status);
    if (!status) return status;

    MRenderLineArray mainLines;
    MRenderLineArray leafLines;
    MRenderLineArray flowerLines;
    status = pfxGeometry.getLineData(
        mainLines, leafLines, flowerLines,
        true,
        options.twist,
        options.width,
        options.flatness,
        options.parameter,
        options.color,
        options.incandescence,
        options.transparency,
        true);
    if (!status) return status;

    // Admit all selected lines together before creating any custom curves.
    PfxGenerationBudget plan;
    if (exportMainLines) DOMMY_RETURN_IF_FAILURE(preflightLines(mainLines, options, plan));
    if (exportLeafLines) DOMMY_RETURN_IF_FAILURE(preflightLines(leafLines, options, plan));
    if (exportFlowerLines) DOMMY_RETURN_IF_FAILURE(preflightLines(flowerLines, options, plan));

    MObject outputObject;
    DCurveData* outputCurves = NULL;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputObject, outputCurves);
    if (!status || !outputCurves) {
        return status ? MStatus(MS::kFailure) : status;
    }

    PfxGenerationBudget budget;
    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(plan.curveCount);
    if (exportMainLines) {
        status = appendLines(
            mainLines, options, budget, generatedCurves);
        if (!status) return status;
    }
    if (exportLeafLines) {
        status = appendLines(
            leafLines, options, budget, generatedCurves);
        if (!status) return status;
    }
    if (exportFlowerLines) {
        status = appendLines(
            flowerLines, options, budget, generatedCurves);
        if (!status) return status;
    }

    if (budget.curveCount != plan.curveCount || budget.pointCount != plan.pointCount ||
        budget.estimatedBytes != plan.estimatedBytes) return MS::kFailure;
    if (!generatedCurves.empty() && !outputCurves->insert(generatedCurves)) {
        return MS::kFailure;
    }
    generatedCurves.clear();
    std::size_t outputPointCount = 0U;
    if (!outputCurves->hasValidCurveTopology() ||
        outputCurves->numberOfCurves() != budget.curveCount ||
        !outputCurves->getCheckedTotalCvs(outputPointCount) ||
        outputPointCount != budget.pointCount) {
        MGlobal::displayError(
            "DCurveFromPfx: generated invalid DCurve topology.");
        return MS::kFailure;
    }
    if (outputCurves->numberOfCurves() > 0U) {
        outputCurves->updateBBox();
    }

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        outputHandle, outputObject, *outputCurves);
    if (!status) return status;
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveFromPfx")


MStatus DCurveFromPfx::initialize() try
{
    MStatus status;
    MFnMessageAttribute messageAttribute;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inPfx = messageAttribute.create("inPfx", "inpf", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(messageAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(messageAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inPfx));

    MFnUnitAttribute unitAttribute;
    aEvaluationTime = unitAttribute.create(
        "EvaluationTime", "evtime", MFnUnitAttribute::kTime,
        0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEvaluationTime));

    MFnEnumAttribute degreeAttribute;
    aDegree = degreeAttribute.create("Degree", "deg", 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Cubic", 1));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    aIsMainLines = numericAttribute.create(
        "IsMainLines", "mlines", MFnNumericData::kBoolean,
        true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMainLines));

    aIsLeafLines = numericAttribute.create(
        "IsLeafLines", "llines", MFnNumericData::kBoolean,
        true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsLeafLines));

    aIsFlowerLines = numericAttribute.create(
        "IsFlowerLines", "flines", MFnNumericData::kBoolean,
        true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFlowerLines));

    aIsExportWidth = numericAttribute.create(
        "IsExportWidth", "width", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportWidth));

    aIsExportTwist = numericAttribute.create(
        "IsExportTwist", "twist", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportTwist));

    aIsExportFlat = numericAttribute.create(
        "IsExportFlat", "flat", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportFlat));

    aIsExportParameter = numericAttribute.create(
        "IsExportParameter", "param", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportParameter));

    aIsExportColor = numericAttribute.create(
        "IsExportColor", "color", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportColor));

    aIsExportIncandence = numericAttribute.create(
        "IsExportIncandence", "incan", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportIncandence));

    aIsExportTransparency = numericAttribute.create(
        "IsExportTransparency", "trans", MFnNumericData::kBoolean,
        false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsExportTransparency));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inPfx, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aEvaluationTime, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsMainLines, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsLeafLines, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsFlowerLines, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportTwist, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportWidth, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportFlat, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportParameter, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportColor, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportIncandence, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsExportTransparency, outDC));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DCurveFromPfx", "initialize", std::current_exception());
}
