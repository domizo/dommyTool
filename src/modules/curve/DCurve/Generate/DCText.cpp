//
//  DCText.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/16.
//

#include "DCText.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DTextGenerationPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <codecvt>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <locale>
#include <string>
#include <vector>

namespace {

class FreeTypeLibrary final {
public:
    FreeTypeLibrary() : handle_(NULL) {}
    ~FreeTypeLibrary()
    {
        if (handle_ != NULL && FT_Done_FreeType(handle_) != 0) {
            MGlobal::displayWarning(
                "DCText: failed to release the FreeType library.");
        }
    }

    FT_Library* address() { return &handle_; }
    operator FT_Library() const { return handle_; }

private:
    FreeTypeLibrary(const FreeTypeLibrary&);
    FreeTypeLibrary& operator=(const FreeTypeLibrary&);

    FT_Library handle_;
};

class FreeTypeFace final {
public:
    FreeTypeFace() : handle_(NULL) {}
    ~FreeTypeFace()
    {
        if (handle_ != NULL && FT_Done_Face(handle_) != 0) {
            MGlobal::displayWarning(
                "DCText: failed to release the FreeType face.");
        }
    }

    FT_Face* address() { return &handle_; }
    FT_Face operator->() const { return handle_; }
    operator FT_Face() const { return handle_; }

private:
    FreeTypeFace(const FreeTypeFace&);
    FreeTypeFace& operator=(const FreeTypeFace&);

    FT_Face handle_;
};

bool isFinitePoint(const DPoint& point)
{
    return std::isfinite(point[0]) &&
           std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}

bool appendPointWithinBudget(DPointList& points,
                             const DPoint& point,
                             const dommy::curve::TextOutputBudget& budget)
{
    std::size_t candidatePointCount = 0U, bytes = 0U;
    if (!isFinitePoint(point) ||
        !dommy::generation::checkedSum(
            points.size(), 1U,
            dommy::generation::kMaxGeneratedPointCount,
            candidatePointCount) ||
        !dommy::curve::admitTextCurvePoints(budget, candidatePointCount, bytes)) {
        return false;
    }
    points.push_back(point);
    return true;
}

DPoint quadraticBezierPoint(double t,
                            const DPoint& start,
                            const DPoint& control,
                            const DPoint& end)
{
    const double oneMinusT = 1.0 - t;
    return DPoint(
        oneMinusT * oneMinusT * start[0] +
            2.0 * oneMinusT * t * control[0] +
            t * t * end[0],
        oneMinusT * oneMinusT * start[1] +
            2.0 * oneMinusT * t * control[1] +
            t * t * end[1],
        oneMinusT * oneMinusT * start[2] +
            2.0 * oneMinusT * t * control[2] +
            t * t * end[2]);
}

bool appendQuadraticSamples(
    DPointList& points,
    const DPoint& control,
    const DPoint& end,
    std::size_t sampling,
    const dommy::curve::TextOutputBudget& budget)
{
    if (points.empty()) return false;
    std::size_t candidateLocalCount = 0U;
    std::size_t bytes = 0U;
    if (!dommy::generation::checkedSum(
            points.size(), sampling,
            dommy::generation::kMaxGeneratedPointCount,
            candidateLocalCount) ||
        !dommy::curve::admitTextCurvePoints(budget, candidateLocalCount, bytes)) {
        return false;
    }

    const DPoint start = points.back();
    for (std::size_t sample = 1U; sample <= sampling; ++sample) {
        const double t =
            static_cast<double>(sample) /
            static_cast<double>(sampling);
        const DPoint point =
            quadraticBezierPoint(t, start, control, end);
        if (!isFinitePoint(point)) return false;
        points.push_back(point);
    }
    return points.size() == candidateLocalCount;
}

DPoint outlinePoint(double x,
                    double y,
                    double currentX,
                    double currentY)
{
    return DPoint(x + currentX, y + currentY, 0.0);
}

MStatus resolveFontPath(const std::string& requestedPath,
                        std::string& resolvedPath)
{
    MFileObject fileObject;
    DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName(requestedPath.c_str()));
    if (!fileObject.exists()) {
#if defined(__APPLE__)
        DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName("/System/Library/Fonts/SFNSText.ttf"));
#endif
    }
    const std::string candidate = fileObject.resolvedFullName().asChar();
    if (candidate.empty() || candidate.size() > dommy::curve::kMaxFontPathBytes ||
        !fileObject.exists()) {
        DOMMY_MAYA_ERROR(
            "DCText", "font file does not exist: " << requestedPath);
        return MS::kInvalidParameter;
    }
    resolvedPath = candidate;
    return MS::kSuccess;
}

MStatus generateTextCurves(
    DCurveData& curves,
    const std::string& text,
    const std::string& requestedFontPath,
    const dommy::curve::TextGenerationPlan& plan,
    bool createCharacterGroups,
    const std::string& groupName)
{
    std::string fontPath;
    MStatus status = resolveFontPath(
        requestedFontPath, fontPath);
    if (!status) return status;

    FreeTypeLibrary library;
    if (FT_Init_FreeType(library.address()) != 0) {
        MGlobal::displayError(
            "DCText: failed to initialize FreeType.");
        return MS::kFailure;
    }

    FreeTypeFace face;
    if (FT_New_Face(
            library, fontPath.c_str(), 0, face.address()) != 0) {
        DOMMY_MAYA_ERROR(
            "DCText", "failed to load font file: " << fontPath);
        return MS::kFailure;
    }

    const double pixelSizeValue =
        std::ceil(plan.fontSize * 1000.0);
    if (!std::isfinite(pixelSizeValue) ||
        pixelSizeValue < 1.0 ||
        pixelSizeValue >
            static_cast<double>(
                std::numeric_limits<FT_UInt>::max())) {
        return MS::kInvalidParameter;
    }
    const FT_UInt pixelSize =
        static_cast<FT_UInt>(pixelSizeValue);
    if (FT_Set_Pixel_Sizes(face, pixelSize, pixelSize) != 0 ||
        face->size == NULL) {
        MGlobal::displayError(
            "DCText: failed to configure the font size.");
        return MS::kFailure;
    }

    const std::u32string characters =
        std::wstring_convert<
            std::codecvt_utf8<char32_t>, char32_t>()
            .from_bytes(text);
    if (characters.size() >
        dommy::generation::kMaxGeneratedStringCount) {
        return MS::kInvalidParameter;
    }

    const double trackingX = plan.trackingX * 1000.0;
    const double trackingY = plan.trackingY * 1000.0;
    double currentX = 0.0;
    double currentY = 0.0;
    dommy::curve::TextOutputBudget budget;
    if (!dommy::curve::initializeTextOutputBudget(characters.size(), text.size(),
            groupName.size(), createCharacterGroups, budget)) return MS::kInvalidParameter;
    DCurveData::DCurveList generatedCurves;
    std::vector<DPIndexList> characterGroups;
    if (createCharacterGroups) characterGroups.reserve(characters.size());

    for (std::size_t characterIndex = 0U;
         characterIndex < characters.size(); ++characterIndex) {
        const char32_t character = characters[characterIndex];
        if (character == U'\r') continue;
        if (character == U'\n') {
            currentX = 0.0;
            currentY -=
                static_cast<double>(face->size->metrics.height);
            if (!std::isfinite(currentY)) {
                return MS::kInvalidParameter;
            }
            continue;
        }
        if (character == U'\t') {
            const double tabWidth =
                static_cast<double>(
                    face->size->metrics.max_advance) * 4.0;
            if (std::isfinite(tabWidth) &&
                std::fabs(tabWidth) >
                    std::numeric_limits<double>::epsilon()) {
                currentX =
                    std::ceil(currentX / tabWidth) * tabWidth;
            } else {
                currentX += trackingX * 4.0;
            }
            if (!std::isfinite(currentX)) {
                return MS::kInvalidParameter;
            }
            continue;
        }

        const FT_UInt glyphIndex =
            FT_Get_Char_Index(face, character);
        if (FT_Load_Glyph(
                face, glyphIndex, FT_LOAD_NO_BITMAP) != 0 ||
            face->glyph == NULL) {
            return MS::kFailure;
        }

        const FT_GlyphSlot slot = face->glyph;
        const FT_Outline& outline = slot->outline;
        if (outline.n_points < 0 || outline.n_contours < 0 ||
            static_cast<std::size_t>(outline.n_points) >
                dommy::generation::kMaxGeneratedPointCount ||
            static_cast<std::size_t>(outline.n_contours) >
                dommy::generation::kMaxGeneratedCurveCount) {
            return MS::kInvalidParameter;
        }

        DPIndexList characterCurveIds;
        if (outline.n_points > 0 && outline.n_contours > 0) {
            if (!dommy::curve::reserveTextGroup(budget)) return MS::kInvalidParameter;
            if (outline.points == NULL ||
                outline.tags == NULL ||
                outline.contours == NULL) {
                return MS::kFailure;
            }
            const std::size_t outlinePointCount =
                static_cast<std::size_t>(outline.n_points);
            const std::size_t contourCount =
                static_cast<std::size_t>(outline.n_contours);
            std::size_t contourStart = 0U;

            for (std::size_t contourIndex = 0U;
                 contourIndex < contourCount; ++contourIndex) {
                const short rawContourEnd =
                    outline.contours[contourIndex];
                if (rawContourEnd < 0) return MS::kFailure;
                const std::size_t contourEnd =
                    static_cast<std::size_t>(rawContourEnd);
                if (contourEnd < contourStart ||
                    contourEnd >= outlinePointCount) {
                    return MS::kFailure;
                }
                const std::size_t contourPointCount =
                    contourEnd - contourStart + 1U;
                if (contourPointCount == 0U) {
                    return MS::kFailure;
                }

                DPointList points;
                if (!appendPointWithinBudget(
                        points,
                        outlinePoint(
                            static_cast<double>(
                                outline.points[contourStart].x),
                            static_cast<double>(
                                outline.points[contourStart].y),
                            currentX, currentY),
                        budget)) {
                    return MS::kInvalidParameter;
                }

                for (std::size_t pointOffset = 0U;
                     pointOffset < contourPointCount;
                     ++pointOffset) {
                    const std::size_t current =
                        contourStart + pointOffset;
                    const std::size_t next =
                        contourStart +
                        ((pointOffset + 1U) % contourPointCount);
                    const std::size_t nextNext =
                        contourStart +
                        ((pointOffset + 2U) % contourPointCount);

                    double x =
                        static_cast<double>(outline.points[current].x);
                    double y =
                        static_cast<double>(outline.points[current].y);
                    const double nextX =
                        static_cast<double>(outline.points[next].x);
                    const double nextY =
                        static_cast<double>(outline.points[next].y);
                    double nextNextX =
                        static_cast<double>(
                            outline.points[nextNext].x);
                    double nextNextY =
                        static_cast<double>(
                            outline.points[nextNext].y);

                    bool currentIsControl =
                        (outline.tags[current] & 1) == 0;
                    const bool nextIsControl =
                        (outline.tags[next] & 1) == 0;
                    const bool nextNextIsControl =
                        (outline.tags[nextNext] & 1) == 0;

                    if (currentIsControl && nextIsControl) {
                        x = (x + nextX) * 0.5;
                        y = (y + nextY) * 0.5;
                        currentIsControl = false;
                        if (pointOffset == 0U &&
                            !appendPointWithinBudget(
                                points,
                                outlinePoint(
                                    x, y, currentX, currentY),
                                budget)) {
                            return MS::kInvalidParameter;
                        }
                    }

                    if (!currentIsControl && nextIsControl) {
                        if (nextNextIsControl) {
                            nextNextX =
                                (nextX + nextNextX) * 0.5;
                            nextNextY =
                                (nextY + nextNextY) * 0.5;
                        }
                        if (!appendQuadraticSamples(
                                points,
                                outlinePoint(
                                    nextX, nextY,
                                    currentX, currentY),
                                outlinePoint(
                                    nextNextX, nextNextY,
                                    currentX, currentY),
                                plan.sampling,
                                budget)) {
                            return MS::kInvalidParameter;
                        }
                    } else if (!currentIsControl &&
                               !nextIsControl) {
                        if (!appendPointWithinBudget(
                                points,
                                outlinePoint(
                                    nextX, nextY,
                                    currentX, currentY),
                                budget)) {
                            return MS::kInvalidParameter;
                        }
                    }
                }

                if (points.size() >= 2U) {
                    if (!dommy::curve::commitTextCurve(budget, points.size())) {
                        return MS::kInvalidParameter;
                    }

                    MDoubleArray knots;
                    GenerateDCurveKnots(points.size(), 1, knots);
                    DCItem curve(new DCurveInternalData(
                        points, knots, 1, false));
                    if (!curve || !curve->hasValidCurveTopology()) {
                        return MS::kFailure;
                    }
                    generatedCurves.push_back(curve);
                    if (createCharacterGroups)
                        characterCurveIds.push_back(generatedCurves.size() - 1U);
                }
                contourStart = contourEnd + 1U;
            }
            if (contourStart != outlinePointCount) {
                return MS::kFailure;
            }
            if (createCharacterGroups) {
                characterGroups.emplace_back();
                characterGroups.back().swap(characterCurveIds);
            }
        }

        currentX +=
            static_cast<double>(slot->advance.x) + trackingX;
        if (slot->advance.y > 0) {
            currentY +=
                static_cast<double>(slot->advance.y) + trackingY;
        } else {
            currentY += static_cast<double>(slot->advance.y);
        }
        if (!std::isfinite(currentX) ||
            !std::isfinite(currentY)) {
            return MS::kInvalidParameter;
        }
    }

    for (DCurveData::DCurveList::iterator curve = generatedCurves.begin();
         curve != generatedCurves.end(); ++curve) {
        if (!*curve) return MS::kFailure;
        DPointList points = (*curve)->getPositionConst();
        for (DPointIter point = points.begin();
             point != points.end(); ++point) {
            (*point)[0] *= 0.001f;
            (*point)[1] *= 0.001f;
            (*point)[2] *= 0.001f;
            if (!isFinitePoint(*point)) {
                return MS::kFailure;
            }
        }
        if (!(*curve)->replacePositions(points)) {
            return MS::kFailure;
        }
    }

    if (!curves.insert(generatedCurves)) return MS::kFailure;
    generatedCurves.clear();
    std::size_t actualPoints = 0U;
    if (curves.numberOfCurves() != budget.curves ||
        !curves.getCheckedTotalCvs(actualPoints) || actualPoints != budget.points ||
        (createCharacterGroups && characterGroups.size() != budget.groups)) return MS::kFailure;

    if (createCharacterGroups) {
        if (characterGroups.size() >
            dommy::generation::kMaxGeneratedStringCount) {
            return MS::kInvalidParameter;
        }
        const MString baseGroupName(groupName.c_str());
        for (std::size_t groupIndex = 0U;
             groupIndex < characterGroups.size(); ++groupIndex) {
            if (groupIndex >
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max())) {
                return MS::kFailure;
            }
            DPIdList groupIds(
                curves.numberOfCurves(), false);
            for (DPIndexList::const_iterator curveIndex =
                     characterGroups[groupIndex].begin();
                 curveIndex !=
                     characterGroups[groupIndex].end();
                 ++curveIndex) {
                if (*curveIndex >= curves.numberOfCurves()) {
                    return MS::kFailure;
                }
                groupIds[*curveIndex] = true;
            }
            const std::string generatedGroupName =
                (baseGroupName +
                 static_cast<int>(groupIndex)).asChar();
            curves.addGrp(generatedGroupName, groupIds);
        }
    }

    if (!curves.hasValidCurveTopology()) return MS::kFailure;
    curves.updateBBox();
    return MS::kSuccess;
}

} // namespace

const MTypeId DCText::typeId(ID_DC_TEXT);

MObject DCText::aText;
MObject DCText::aFontName;
MObject DCText::aSize;
MObject DCText::aSampling;
MObject DCText::aTracking;
MObject DCText::aGroupCharPiece;
MObject DCText::aGroupName;
MObject DCText::outDC;

void* DCText::creator()
{
    return ::dommy::maya::createObject("DCText", [&]() {
        return new DCText();
    });
}

MStatus DCText::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle control = data.inputValue(aText, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string text = control.asString().asChar();
    control = data.inputValue(aFontName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string fontPath = control.asString().asChar();
    control = data.inputValue(aSize, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double fontSize = static_cast<double>(control.asFloat());
    control = data.inputValue(aSampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int sampling = control.asInt();
    control = data.inputValue(aTracking, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float* trackingValues = control.asFloat2();
    if (!trackingValues) return MS::kFailure;
    const float tracking[2] = {trackingValues[0], trackingValues[1]};
    control = data.inputValue(aGroupCharPiece, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool createCharacterGroups = control.asBool();
    control = data.inputValue(aGroupName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string groupName = control.asString().asChar();

    dommy::curve::TextGenerationPlan plan;
    if (!dommy::curve::buildTextGenerationPlan(
            text.size(), fontPath.size(), groupName.size(),
            fontSize, sampling,
            static_cast<double>(tracking[0]),
            static_cast<double>(tracking[1]), plan)) {
        return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    DCurveData* output = NULL;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
        pluginData, DCurveData::id, output));
    if (output == NULL) return MS::kFailure;
    if (!text.empty()) {
        DOMMY_RETURN_IF_FAILURE(generateTextCurves(
            *output, text, fontPath, plan,
            createCharacterGroups, groupName));
    }
    if (!output->hasValidCurveTopology()) return MS::kFailure;

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DCurveData>(
            outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCText")

MStatus DCText::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    const MObject defaultText =
        stringData.create("Text", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aText = typedAttribute.create(
        "Text", "txt", MFnData::kString,
        defaultText, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aText));

    const MObject defaultFont =
        stringData.create("", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aFontName = typedAttribute.create(
        "FontName", "fntnm", MFnData::kString,
        defaultFont, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFontName));

    aSize = numericAttribute.create(
        "Size", "sz",
        MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(
        std::numeric_limits<float>::epsilon()));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(dommy::curve::kMaxTextSize));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSize));

    aSampling = numericAttribute.create(
        "Sampling", "smp",
        MFnNumericData::kInt, 4, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(dommy::curve::kMaxTextSampling));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aTracking = numericAttribute.create(
        "Tracking", "trk",
        MFnNumericData::k2Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTracking));

    aGroupCharPiece = numericAttribute.create(
        "GroupCharPiece", "grpcp",
        MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupCharPiece));

    const MObject defaultGroupName =
        stringData.create("piece", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aGroupName = typedAttribute.create(
        "GroupName", "grpn", MFnData::kString,
        defaultGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aText, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFontName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSize, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTracking, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroupCharPiece, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroupName, outDC));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DCText", "initialize", std::current_exception());
}
