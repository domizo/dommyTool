// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBTransformNode.cc

//
//  DOpenVDBTransform.cpp
//  dommyTool
//
//  Created by yuzo on 1/1/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBTransform.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointTransform.h"

#include <cmath>

#include <boost/math/constants/constants.hpp>

#include "DMayaHeaders.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"

const MTypeId DOpenVDBTransform::typeId(ID_VDB_DOPENVDB_TRANSFORM);

MObject DOpenVDBTransform::aVdbInput;

MObject DOpenVDBTransform::aVdbSelectedGridNames;
MObject DOpenVDBTransform::aTranslate;
MObject DOpenVDBTransform::aRotate;
MObject DOpenVDBTransform::aScale;
MObject DOpenVDBTransform::aPivot;
MObject DOpenVDBTransform::aUniformScale;
MObject DOpenVDBTransform::aInvert;

MObject DOpenVDBTransform::aVdbOutput;

void* DOpenVDBTransform::creator() {
    return ::dommy::maya::createObject("DOpenVDBTransform", [&]() {
        return new DOpenVDBTransform();
    });
}

namespace {
bool hasFiniteAffineMapCache(const openvdb::math::AffineMap& map) {
    // Finite matrix entries alone do not guarantee finite native cached
    // cofactors, determinant or squared voxel lengths at extreme scales.
    if (!std::isfinite(map.determinant()))
        return false;
    const openvdb::Vec3R voxelSize = map.voxelSize();
    const auto& inverseJacobian = map.getConstJacobianInv();
    for (int row = 0; row < 3; ++row) {
        if (!std::isfinite(voxelSize[row]) || voxelSize[row] <= 0.0)
            return false;
        for (int column = 0; column < 3; ++column)
            if (!std::isfinite(inverseJacobian[row][column]))
                return false;
    }
    return true;
}
} // namespace

MStatus DOpenVDBTransform::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    const DOpenVDBData* inputVdb = getInputVDB(aVdbInput, data);
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;

    MStatus status;
    const MDataHandle translateHandle = data.inputValue(aTranslate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MFloatVector t = translateHandle.asFloatVector();
    const MDataHandle rotateHandle = data.inputValue(aRotate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MFloatVector r = rotateHandle.asFloatVector();
    const MDataHandle pivotHandle = data.inputValue(aPivot, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MFloatVector p = pivotHandle.asFloatVector();
    const MDataHandle scaleHandle = data.inputValue(aScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MFloatVector scale = scaleHandle.asFloatVector();
    const MDataHandle uniformScaleHandle = data.inputValue(aUniformScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float uniformScale = uniformScaleHandle.asFloat();
    const MDataHandle invertHandle = data.inputValue(aInvert, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool invert = invertHandle.asBool();
    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string selection = selectionHandle.asString().asChar();

    if (!std::isfinite(uniformScale))
        return MS::kInvalidParameter;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!std::isfinite(t[axis]) || !std::isfinite(r[axis]) || !std::isfinite(p[axis]) ||
            !std::isfinite(scale[axis]))
            return MS::kInvalidParameter;
    }
    // Promote BEFORE multiplying: finite float controls can have a finite
    // double product outside the float range. Preserve signed/nonuniform scale.
    const openvdb::Vec3R s(static_cast<double>(scale[0]) * uniformScale,
                           static_cast<double>(scale[1]) * uniformScale,
                           static_cast<double>(scale[2]) * uniformScale);
    openvdb::Mat4R mat(openvdb::Mat4R::identity());
    mat.preTranslate(openvdb::Vec3R(p[0], p[1], p[2]));
    const double deg2rad = boost::math::constants::pi<double>() / 180.0;
    mat.preRotate(openvdb::math::X_AXIS, deg2rad * r[0]);
    mat.preRotate(openvdb::math::Y_AXIS, deg2rad * r[1]);
    mat.preRotate(openvdb::math::Z_AXIS, deg2rad * r[2]);
    mat.preScale(s);
    mat.preTranslate(openvdb::Vec3R(-p[0], -p[1], -p[2]));
    mat.preTranslate(openvdb::Vec3R(t[0], t[1], t[2]));
    if (!dommy::numeric::isFiniteAffineTransform(mat))
        return MS::kInvalidParameter;
    const openvdb::Mat4R inverse = mat.inverse();
    if (!dommy::numeric::isFiniteAffineTransform(inverse))
        return MS::kInvalidParameter;
    if (invert)
        mat = inverse;

    typedef openvdb::math::AffineMap AffineMap;
    typedef openvdb::math::Transform Transform;
    // Keep OpenVDB's own near-singular rejection policy.
    const AffineMap map(mat);
    if (!hasFiniteAffineMapCache(map))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    GridCPtrVec grids;
    // Preserve unselected-first ordering and shallow tree sharing.
    if (!getSelectedGrids(grids, selection, *inputVdb, *outputVdb))
        MGlobal::displayWarning("No grids are selected.");
    for (GridCPtrVecCIter it = grids.begin(); it != grids.end(); ++it) {
        const AffineMap::Ptr sourceMap = (*it)->transform().baseMap()->getAffineMap();
        if (!sourceMap || !dommy::numeric::isFiniteAffineTransform(sourceMap->getMat4()))
            return MS::kInvalidParameter;
        // Same source * operation order as AffineMap(first, second), validated
        // before a new map caches its inverse and other acceleration values.
        const openvdb::Mat4R compoundMatrix = sourceMap->getMat4() * map.getMat4();
        if (!dommy::numeric::isFiniteAffineTransform(compoundMatrix) ||
            !dommy::numeric::isFiniteAffineTransform(compoundMatrix.inverse()))
            return MS::kInvalidParameter;
        AffineMap::Ptr compound(new AffineMap(compoundMatrix));
        if (!hasFiniteAffineMapCache(*compound))
            return MS::kInvalidParameter;
        const GridCPtr copied = (*it)->copyGrid();
        if (!copied)
            return MS::kFailure;
        // The const copyGrid overload returns ConstPtr in OpenVDB 5.2. Only
        // the newly allocated header is made writable, never the input grid.
        GridPtr grid = openvdb::ConstPtrCast<openvdb::GridBase>(copied);
        grid->setTransform(Transform::Ptr(new Transform(openvdb::math::simplify(compound))));
        outputVdb->insert(grid);
    }
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBTransform")

MStatus DOpenVDBTransform::initialize() try {
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    stat = addAttribute(aVdbInput);
    DOMMY_RETURN_IF_FAILURE(stat);

    MFnStringData strData;
    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aVdbSelectedGridNames);
    DOMMY_RETURN_IF_FAILURE(stat);

    aTranslate = nAttr.createPoint("Translate", "t", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    stat = addAttribute(aTranslate);
    DOMMY_RETURN_IF_FAILURE(stat);

    aRotate = nAttr.createPoint("Rotate", "r", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    stat = addAttribute(aRotate);
    DOMMY_RETURN_IF_FAILURE(stat);

    aScale = nAttr.createPoint("Scale", "s", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    stat = addAttribute(aScale);
    DOMMY_RETURN_IF_FAILURE(stat);

    aPivot = nAttr.createPoint("Pivot", "p", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    stat = addAttribute(aPivot);
    DOMMY_RETURN_IF_FAILURE(stat);

    aUniformScale = nAttr.create("UniformScale", "us", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1e-7));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));

    stat = addAttribute(aUniformScale);
    DOMMY_RETURN_IF_FAILURE(stat);

    aInvert = nAttr.create("invert", "InvertTransformation", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    stat = addAttribute(aInvert);
    DOMMY_RETURN_IF_FAILURE(stat);

    // input / output

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    stat = addAttribute(aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTranslate, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotate, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPivot, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformScale, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInvert, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DVDBTransform", "initialize",
                                                  std::current_exception());
}
