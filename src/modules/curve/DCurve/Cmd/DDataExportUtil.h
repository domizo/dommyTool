// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaUtility.h

//
//  DCExportUtil.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/16.
//
//

#pragma once


#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "common/utility/DAttributeExport.h"
#include "common/utility/DExportAttributeState.h"
#include "common/utility/DWorkBudget.h"
#include "DNodeOperation.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DAbcRootAdmission.h"


#include <Alembic/AbcGeom/All.h>
#include <Alembic/Abc/OArrayProperty.h>
#include <Alembic/Abc/OScalarProperty.h>
#include <set>
#include <vector>
#include <string>
#include <array>
#include <stdexcept>
#include <list>
#include <locale>
#include <cstring>

#define DCABCEXPORT_VERSION "1.0"

namespace ddata_export_util
{
    // Built-in properties are sampled with geometry, but their existence and
    // width metadata cannot change. Missing initial optional data stays absent.
    // Publish this state only after the main geometry schema succeeds.
    class BuiltinSampleState
    {
    public:
        void require(bool widths, Alembic::AbcGeom::GeometryScope scope, bool velocities) const
        {
            if (widths && scope != Alembic::AbcGeom::kConstantScope &&
                scope != Alembic::AbcGeom::kUniformScope && scope != Alembic::AbcGeom::kVertexScope)
                throw std::invalid_argument("Invalid Alembic built-in width scope");
            if (initialized_ && (widths != widths_ || velocities != velocities_ ||
                (widths && scope != widthScope_)))
                throw std::invalid_argument("Alembic built-in width/velocity schema changed between frames");
        }

        void commit(bool widths, Alembic::AbcGeom::GeometryScope scope, bool velocities) noexcept
        {
            widths_ = widths;
            velocities_ = velocities;
            widthScope_ = widths ? scope : Alembic::AbcGeom::kUnknownScope;
            initialized_ = true;
        }

        bool initialized() const { return initialized_; }

    private:
        bool initialized_ = false, widths_ = false, velocities_ = false;
        Alembic::AbcGeom::GeometryScope widthScope_ = Alembic::AbcGeom::kUnknownScope;
    };

    // Shared exception boundary for archive writers: a failed Maya query must
    // reach the pending-output owner rather than becoming an exported default.
    void requireExportQuery(const MStatus& status, const char* message);

    template <typename Value>
    inline Value readExportValue(const MPlug& plug)
    {
        Value value;
        requireExportQuery(readPlugValue(plug, value), "Unable to read an Alembic transform input");
        return value;
    }

    inline MPlug requiredExportPlug(const MFnDependencyNode& node, const MString& name)
    {
        MStatus status;
        const MPlug plug = node.findPlug(name, true, &status);
        requireExportQuery(status, "Unable to find a required Alembic transform input");
        if (plug.isNull()) throw std::runtime_error("Missing required Alembic transform input");
        return plug;
    }

    inline MString exportNodeName(const MFnDependencyNode& node)
    {
        MStatus status;
        const MString name = node.name(&status);
        requireExportQuery(status, "Unable to read an Alembic transform name");
        return name;
    }

    inline MObject exportDagNode(const MDagPath& path)
    {
        MStatus status;
        const MObject object = path.node(&status);
        requireExportQuery(status, "Unable to read an Alembic DAG node");
        return object;
    }

    inline float checkedExportFloat(double value)
    {
        float result = 0.0f;
        if (!dommy::numeric::checkedFloatCast(value, result))
            throw std::invalid_argument("Alembic value exceeds finite float storage");
        return result;
    }

    template <typename Points>
    MBoundingBox exportSampleBounds(const Points& points)
    {
        if (points.empty() || points.size() > dommy::generation::kMaxGeneratedPointCount)
            throw std::invalid_argument("Invalid Alembic sample bounds point count");
        MBoundingBox bounds;
        for (const auto& point : points) {
            const MPoint value(point[0], point[1], point[2]);
            if (!dommy::maya::isFinitePoint(value))
                throw std::invalid_argument("Non-finite Alembic sample bounds");
            bounds.expand(value);
        }
        if (!dommy::maya::isFiniteBoundingBox(bounds))
            throw std::invalid_argument("Invalid Alembic sample bounds");
        return bounds;
    }

    // Keep the previous Boost is_space()/token-compress grammar, but admit
    // all tokens/bytes before allocating the list. Missing fields remain an
    // omission policy of the attribute writer, not a failed plug read.
    inline std::list<std::string> readExportNameList(const MPlug& plug, std::size_t repetitions,
                                                   dommy::maya::NameListBudget& budget)
    {
        const MString value = readExportValue<MString>(plug);
        if (value.length() > dommy::generation::kMaxGeneratedStringByteCount)
            throw std::length_error("Alembic attribute-name list is too long");
        const char* text = value.asChar();
        const std::size_t bytes = std::strlen(text);
        auto candidate = budget;
        if (!dommy::generation::checkedSum(candidate.bytes, bytes,
                dommy::generation::kMaxGeneratedStringByteCount, candidate.bytes))
            throw std::length_error("Alembic attribute names exceed the byte budget");
        const std::locale locale;
        std::size_t count = 0U;
        bool inName = false;
        for (std::size_t index = 0U; index < bytes; ++index) {
            const bool space = std::isspace(text[index], locale);
            if (!space && !inName) ++count;
            inName = !space;
        }
        std::size_t work = 0U, nodes = 0U, storage = 0U;
        if (!dommy::generation::checkedSum(candidate.names, count,
                dommy::generation::kMaxGeneratedStringCount, candidate.names) ||
            !dommy::generation::checkedProduct(count, repetitions,
                dommy::generation::kMaxWorkItemCount, work) ||
            !dommy::generation::checkedSum(candidate.workItems, work,
                dommy::generation::kMaxWorkItemCount, candidate.workItems) ||
            !dommy::generation::checkedProduct(candidate.names, sizeof(std::string) + 2U * sizeof(void*),
                dommy::generation::kMaxGeneratedStringByteCount, nodes) ||
            !dommy::generation::checkedSum(candidate.bytes, nodes,
                dommy::generation::kMaxGeneratedStringByteCount, storage) ||
            !dommy::attribute::validArrayConversionBudget(1U, storage))
            throw std::length_error("Alembic attribute names exceed the work/storage budget");
        std::list<std::string> names;
        std::size_t position = 0U;
        while (position < bytes) {
            while (position < bytes && std::isspace(text[position], locale)) ++position;
            const std::size_t first = position;
            while (position < bytes && !std::isspace(text[position], locale)) ++position;
            if (position != first) names.emplace_back(text + first, position - first);
        }
        budget = candidate;
        return names;
    }

    inline bool isShapeExportType(DP_ATTR_DATA_TYPE type)
    {
        return type >= DP_ATTR_BOOL && type <= DP_ATTR_VEC3D;
    }

    template <typename Owner>
    void addPrimitiveExportFields(const Owner& owner, std::size_t count,
                                  const std::list<std::string>& names,
                                  dommy::attribute::ExportAttributeFrame& frame)
    {
        dommy::attribute::ValidationBudget budget;
        for (const auto& name : names) {
            if (!owner.isExistAttr(name)) continue;
            const auto attribute = owner.getAttr(name);
            if (!attribute || attribute->attrSize() != count)
                throw std::invalid_argument("Malformed Alembic primitive attribute: " + name);
            const auto type = attribute->getAttrType();
            if (!isShapeExportType(type)) continue;
            if (!dommy::attribute::validateAttribute(attribute, count, budget))
                throw std::invalid_argument("Invalid Alembic primitive attribute values: " + name);
            frame.add(name, dommy::attribute::ExportAttributeDomain::Primitive, type, false);
        }
    }

    template <typename Owner>
    void addPointExportFields(const Owner& owner, const std::list<std::string>& names,
                              bool sampled, dommy::attribute::ExportAttributeFrame& frame)
    {
        for (const auto& name : names) {
            if (!owner.isExistPointAttr(name)) continue;
            const auto type = owner.getPointAttributeType(name);
            if (isShapeExportType(type))
                frame.add(name, dommy::attribute::ExportAttributeDomain::Point, type, sampled);
        }
    }

    inline dommy::attribute::ExportAttributeFrame planPointExportAttributes(
        const DPData& points, const std::list<std::string>& names,
        const dommy::attribute::ExportAttributeState& state)
    {
        auto frame = state.beginFrame();
        for (const auto& name : names) {
            if (!points.isExistAttr(name)) continue;
            const auto attribute = points.getAttr(name);
            if (!attribute || attribute->attrSize() != points.numberOfPoints())
                throw std::invalid_argument("Malformed Alembic point attribute: " + name);
            if (isShapeExportType(attribute->getAttrType()))
                frame.add(name, dommy::attribute::ExportAttributeDomain::Point,
                          attribute->getAttrType(), false);
        }
        if (frame.hasStatic(dommy::attribute::ExportAttributeDomain::Point))
            frame.setLayout(dommy::attribute::ExportAttributeDomain::Point, 1U,
                            [&points](std::size_t) { return points.numberOfPoints(); });
        return frame;
    }

    inline dommy::attribute::ExportAttributeFrame planCurveExportAttributes(
        const DCurveData& curves, const std::list<std::string>& primitiveNames,
        const std::list<std::string>& pointNames,
        const dommy::attribute::ExportAttributeState& state)
    {
        auto frame = state.beginFrame();
        addPrimitiveExportFields(curves, curves.numberOfCurves(), primitiveNames, frame);
        addPointExportFields(curves, pointNames, false, frame);
        if (frame.hasStatic(dommy::attribute::ExportAttributeDomain::Primitive))
            frame.setLayout(dommy::attribute::ExportAttributeDomain::Primitive, 1U,
                            [&curves](std::size_t) { return curves.numberOfCurves(); });
        if (frame.hasStatic(dommy::attribute::ExportAttributeDomain::Point)) {
            const auto* owners = curves.getCurveListConst();
            if (!owners) throw std::invalid_argument("Missing Alembic curve attribute owners");
            frame.setLayout(dommy::attribute::ExportAttributeDomain::Point, owners->size(),
                [owners](std::size_t i) {
                    if (!(*owners)[i]) throw std::invalid_argument("Null Alembic curve attribute owner");
                    return (*owners)[i]->numberOfPoints();
                });
        }
        return frame;
    }

    inline dommy::attribute::ExportAttributeFrame planMeshExportAttributes(
        const DMeshData& meshes, const std::list<std::string>& primitiveNames,
        const std::list<std::string>& pointNames,
        const dommy::attribute::ExportAttributeState& state)
    {
        auto frame = state.beginFrame();
        addPrimitiveExportFields(meshes, meshes.numberOfMesh(), primitiveNames, frame);
        addPointExportFields(meshes, pointNames, true, frame);
        if (frame.hasStatic(dommy::attribute::ExportAttributeDomain::Primitive)) {
            const auto* owners = meshes.getMeshListConst();
            if (!owners) throw std::invalid_argument("Missing Alembic mesh attribute owners");
            frame.setLayout(dommy::attribute::ExportAttributeDomain::Primitive, owners->size(),
                [owners](std::size_t i) {
                    if (!(*owners)[i] || (*owners)[i]->numFace() < 0)
                        throw std::invalid_argument("Invalid Alembic mesh face layout");
                    return static_cast<std::size_t>((*owners)[i]->numFace());
                });
        }
        return frame;
    }

    std::string exportDagPathName(const MDagPath& path);
    unsigned int exportDagDepth(const MDagPath& path);
    // False means a checked non-DAG omission; query failures throw. The caller's
    // path is assigned only after successful acquisition.
    bool readExportDagSelectionItem(const MSelectionList& selection,
                                    unsigned int index, MDagPath& path);
    // Keep root preflight and object writers on one emitted-name policy.
    MString exportObjectName(const MFnDependencyNode& node, unsigned int stripDepth);

	struct cmpDag
	{
		bool operator()( const MDagPath& lhs, const MDagPath& rhs ) const
		{
			const std::string nameA = exportDagPathName(lhs);
			const std::string nameB = exportDagPathName(rhs);
			return (nameA.compare(nameB) < 0);
		}
	};
	typedef std::set< MDagPath, cmpDag > ShapeSet;
    void insertExportRoot(ShapeSet& roots, const MDagPath& path,
                          dommy::exporting::RootAdmission& admission);
    void prepareExportSelection(MSelectionList& expanded, ShapeSet& roots,
                                const MSelectionList& active,
                                dommy::exporting::RootAdmission& admission);
    void requireUniqueExportRootNames(const ShapeSet& roots, unsigned int stripDepth,
                                      dommy::exporting::RootAdmission& admission);
	
	
	
	
	// safely inverse a scale component
	inline double inverseScale(double scale)
	{
		const double kScaleEpsilon = 1.0e-12;
		
		if (scale < kScaleEpsilon && scale >= 0.0)
			return 1.0 / kScaleEpsilon;
		else if (scale > -kScaleEpsilon && scale < 0.0)
			return 1.0 / -kScaleEpsilon;
		else
			return 1.0 / scale;
	}
	
	
	
	
    // seconds per frame
    double spf();
    
    bool isAncestorDescendentRelationship(const MDagPath & path1,
                                          const MDagPath & path2);
	
	// Returns 0 if static, 1 if sampled, and 2 if a curve; API failure throws
	// to the export transaction instead of being classified as static data.
	int getSampledType(const MPlug& iPlug);
    int getSampledType(const MPlug& root, dommy::generation::WorkBudget& work);
    // 0 omit, 1 static hidden, 2 animated hidden, 3 animated deferred.
    int getVisibilityType(const MPlug & iPlug);
	
	// determines what order we do the rotation in, returns false if iOrder is
	// kInvalid or kLast
	bool getRotOrder(MTransformationMatrix::RotationOrder iOrder,
					 unsigned int & oXAxis, unsigned int & oYAxis, unsigned int & oZAxis);
	
	// determine if a Maya Object is animated or not
	// copy from mayapit code (MayaPit.h .cpp)
	bool isAnimated(MObject & object, bool checkParent = false);
	
	// determine if a joint is driven by FBIK.
	// The joint is animated but has no input connections.
	bool isDrivenByFBIK(const MFnIkJoint & iJoint);
	
	// determine if a joint is driven by a spline ik.
	// The joint's is animated but has no input connections.
	bool isDrivenBySplineIK(const MFnIkJoint & iJoint);
	
	
	// determine if a Maya Object is intermediate
	bool isIntermediate(const MObject & object);
	
	
	MString stripNamespaces(const MString & iNodeName, unsigned int iDepth);
	
	MString getHelpText();
	
	
}//ddata_export_util

typedef std::map<std::string, Alembic::Abc::OArrayProperty> DAbcPointAttrList;


template <typename T, typename U>
inline void DGetAbcDPScalarAttribute(const DPData* points, DP_ATTR_DATA_TYPE type,
                                    const std::string& name, std::vector<U>& values)
{
    const std::array<const DPData*, 1> owners = {{points}};
    if (!dommy::attribute::buildExportAttributeValues<T, U>(
            owners.begin(), owners.end(), type, name, values))
        throw std::invalid_argument("Invalid Alembic point attribute: " + name);
}

template <typename T, typename U>
inline void DGetAbcDCPScalarAttribute(const DCurveData* curves, DP_ATTR_DATA_TYPE type,
                                     const std::string& name, std::vector<U>& values)
{
    if (!curves || !dommy::attribute::buildExportAttributeValues<T, U>(
            curves->beginCurves(), curves->endCurves(), type, name, values))
        throw std::invalid_argument("Invalid Alembic curve attribute: " + name);
}

template <typename T, typename U>
inline void DGetAbcDMPScalarAttribute(const DMeshData& meshes, DP_ATTR_DATA_TYPE type,
                                     const std::string& name, std::vector<U>& values)
{
    const auto* owners = meshes.getMeshListConst();
    if (!owners || !dommy::attribute::buildExportAttributeValues<T, U>(
            owners->begin(), owners->end(), type, name, values))
        throw std::invalid_argument("Invalid Alembic mesh attribute: " + name);
}

template <typename T, typename U>
inline void DGetAbcDMPFaceAttribute(const DMeshData& meshes,
                                  const DPAttributeData::ConstPtr& attribute,
                                  std::vector<U>& values)
{
    const auto* owners = meshes.getMeshListConst();
    if (!owners || !dommy::attribute::buildPrimitiveFaceAttributeValues<T, U>(
            attribute, owners->begin(), owners->end(), values))
        throw std::invalid_argument("Invalid Alembic mesh primitive-to-face attribute");
}



inline void DSetAbcAttributeValues(std::string attrName,
                                   DAbcPointAttrList &pointAttributeList,
                                   const Alembic::Abc::ArraySample val,
                                   Alembic::Util::uint32_t iTimeIndex)
{
	auto iter = pointAttributeList.find(attrName);
	
	if(iter != pointAttributeList.end()) {
        iter->second.setTimeSampling(iTimeIndex);
		iter->second.set(val);
	} else {
        throw std::logic_error("Missing selected Alembic attribute property: " + attrName);
	}
}

template <typename GeomParam>
inline void publishShapeExportAttribute(dommy::attribute::ExportAttributeState& state,
    Alembic::Abc::OCompoundProperty& parent, DAbcPointAttrList& properties,
    const std::string& name, const typename GeomParam::Sample& sample,
    Alembic::AbcGeom::GeometryScope scope, DP_ATTR_DATA_TYPE type,
    Alembic::Util::uint32_t timeIndex, bool sampled)
{
    if ((scope != Alembic::AbcGeom::kUniformScope && scope != Alembic::AbcGeom::kVertexScope) ||
        sample.getScope() != scope)
        throw std::logic_error("Invalid or mismatched Alembic attribute Scope: " + name);
    const auto domain = scope == Alembic::AbcGeom::kUniformScope
        ? dommy::attribute::ExportAttributeDomain::Primitive
        : dommy::attribute::ExportAttributeDomain::Point;
    state.requireCurrent(name, domain, type, sampled);
    const auto existing = properties.find(name);
    if (!state.initialized()) {
        if (existing != properties.end() || parent.getPropertyHeader(name))
            throw std::logic_error("Unexpected existing Alembic attribute schema: " + name);
        GeomParam parameter(parent, name, false, scope, 1U, timeIndex);
        parameter.set(sample);
        properties.emplace(name, parameter.getValueProperty());
    } else {
        if (existing == properties.end() || !parent.getPropertyHeader(name) ||
            existing->second.getNumSamples() != (sampled ? state.samples() : 1U))
            throw std::logic_error("Alembic attribute sample count/registry mismatch: " + name);
        if (sampled) DSetAbcAttributeValues(name, properties, sample.getVals(), timeIndex);
    }
    state.markWritten(name);
}



struct PlugAndObjScalar
{
	MPlug plug;
	MObject obj;
	Alembic::Abc::OScalarProperty prop;
};

struct PlugAndObjArray
{
	MPlug plug;
	MObject obj;
	Alembic::Abc::OArrayProperty prop;
};







struct JobArgs
{
    std::shared_ptr<dommy::exporting::RootAdmission> exportRootAdmission =
        std::make_shared<dommy::exporting::RootAdmission>();
    std::shared_ptr<dommy::attribute::ExportAttributeBudget> exportAttributeBudget =
        std::make_shared<dommy::attribute::ExportAttributeBudget>();
	JobArgs()
	{
		excludeInvisible = false;
		filterEulerRotations = false;
		stripNamespace = 0;
		useSelectionList = false;
		worldSpace = false;
		
	}
	
	bool excludeInvisible;
	bool filterEulerRotations;
	unsigned int stripNamespace;
	bool useSelectionList;
	bool worldSpace;
	
	
	std::string melPerFrameCallback;
	std::string melPostCallback;
	std::string pythonPerFrameCallback;
	std::string pythonPostCallback;
	
	// to put into .arbGeomParam
	std::vector< std::string > prefixFilters;
	std::set< std::string > attribs;
	
	// to put into .userProperties
	std::vector< std::string > userPrefixFilters;
//	std::set< std::string > userAttribs;
	
    std::set< std::string > primAttribs;
    std::set< std::string > pointAttribs;
    
    ddata_export_util::ShapeSet dagPaths;
};
