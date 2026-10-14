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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/AttributesWriter.cpp

//
//  MayaAttributeWriter.cpp
//  dommyTool
//
//  Created by yuzo on 3/18/17.
//
//

#include "MayaAttributeWriter.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMeshMergePlan.h"
#include "DNodeOperation.h"

#include <stdexcept>
#include <cstring>
#include <vector>
#include <utility>


namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;
namespace AbcA = Alembic::AbcCoreAbstract;

namespace {

    template<typename Property, typename... Arguments>
    Property createAttributeProperty(
        const std::shared_ptr<dommy::attribute::ExportAttributeBudget>& budget,
        const std::string& name, std::size_t ownerBytes, Arguments&&... arguments)
    {
        if (!budget) throw std::invalid_argument("Missing Alembic attribute budget");
        std::size_t bytes = 0U;
        if (!dommy::memory::checkedAdd(ownerBytes, sizeof(Property), bytes))
            throw std::length_error("Alembic native attribute record size overflows");
        budget->retainNativeProperty(name, bytes);
        return Property(std::forward<Arguments>(arguments)...);
    }

    dommy::generation::WorkBudget& attributeInspectionWork(const JobArgs& args)
    {
        if (!args.exportAttributeBudget)
            throw std::invalid_argument("Missing Alembic attribute budget");
        return args.exportAttributeBudget->inspectionWork();
    }

    void requireAttributeWork(dommy::generation::WorkBudget& work, std::size_t amount)
    {
        if (!work.consume(amount))
            throw std::length_error("Alembic attribute inspection exceeds its work budget");
    }

    void requireAttributeQuery(const MStatus& status)
    {
        if (!status) throw std::runtime_error("Unable to read an Alembic attribute");
    }

    template <typename Value>
    Value exportValue(const MPlug& plug)
    {
        Value value;
        requireAttributeQuery(readPlugValue(plug, value));
        return value;
    }

    template <>
    char exportValue<char>(const MPlug& plug)
    {
        MStatus status;
        const char value = plug.asChar(&status);
        requireAttributeQuery(status);
        return value;
    }

    template <typename Owner, typename Value>
    Value exportQuery(const Owner& owner, Value (Owner::*query)(MStatus*) const)
    {
        MStatus status;
        const Value value = (owner.*query)(&status);
        requireAttributeQuery(status);
        return value;
    }

    MPlug exportElement(const MPlug& plug, unsigned int index)
    {
        MStatus status;
        const MPlug element = plug.elementByPhysicalIndex(index, &status);
        requireAttributeQuery(status);
        return element;
    }

    MString exportName(const MPlug& plug)
    {
        MStatus status;
        const MString name = plug.partialName(false, false, false, false, false, true, &status);
        requireAttributeQuery(status);
        return name;
    }

    MPlug optionalExportPlug(const MFnDependencyNode& node, const MString& name)
    {
        MStatus status;
        const bool present = node.hasAttribute(name, &status);
        requireAttributeQuery(status);
        if (!present) return MPlug();
        const MPlug plug = node.findPlug(name, true, &status);
        requireAttributeQuery(status);
        return plug;
    }

    MFnNumericData::Type numericDataType(const MPlug& plug)
    {
        MStatus status;
        MFnNumericData data(exportValue<MObject>(plug), &status);
        requireAttributeQuery(status);
        const MFnNumericData::Type type = data.numericType(&status);
        requireAttributeQuery(status);
        return type;
    }

    void requireAttributeType(const AbcA::DataType& type,
                              Alembic::Util::PlainOldDataType pod,
                              unsigned int extent, std::size_t valueWidth)
    {
        std::size_t bytes = 0U;
        if (!dommy::memory::checkedMultiply(extent, valueWidth, bytes) ||
            type.getPod() != pod || type.getExtent() != extent ||
            type.getNumBytes() != bytes)
            throw std::runtime_error("Alembic attribute type or extent changed");
    }

    bool requireNumericAttributeType(MFnNumericData::Type numericType,
                                     const AbcA::DataType& type)
    {
        // The property schema is fixed at creation. Never label a newly read
        // buffer with an older, potentially wider schema (including empty arrays).
        switch (numericType) {
        case MFnNumericData::kBoolean:
            requireAttributeType(type, Alembic::Util::kBooleanPOD, 1U, sizeof(bool)); break;
        case MFnNumericData::kByte:
        case MFnNumericData::kChar:
            requireAttributeType(type, Alembic::Util::kInt8POD, 1U, sizeof(Alembic::Util::int8_t)); break;
        case MFnNumericData::kShort:
        case MFnNumericData::k2Short:
        case MFnNumericData::k3Short:
            requireAttributeType(type, Alembic::Util::kInt16POD,
                numericType == MFnNumericData::kShort ? 1U :
                (numericType == MFnNumericData::k2Short ? 2U : 3U), sizeof(Alembic::Util::int16_t)); break;
        case MFnNumericData::kInt:
        case MFnNumericData::k2Int:
        case MFnNumericData::k3Int:
            requireAttributeType(type, Alembic::Util::kInt32POD,
                numericType == MFnNumericData::kInt ? 1U :
                (numericType == MFnNumericData::k2Int ? 2U : 3U), sizeof(Alembic::Util::int32_t)); break;
        case MFnNumericData::kFloat:
        case MFnNumericData::k2Float:
        case MFnNumericData::k3Float:
            requireAttributeType(type, Alembic::Util::kFloat32POD,
                numericType == MFnNumericData::kFloat ? 1U :
                (numericType == MFnNumericData::k2Float ? 2U : 3U), sizeof(float)); break;
        case MFnNumericData::kDouble:
        case MFnNumericData::k2Double:
        case MFnNumericData::k3Double:
        case MFnNumericData::k4Double:
            requireAttributeType(type, Alembic::Util::kFloat64POD,
                numericType == MFnNumericData::kDouble ? 1U :
                (numericType == MFnNumericData::k2Double ? 2U :
                (numericType == MFnNumericData::k3Double ? 3U : 4U)), sizeof(double)); break;
        default:
            return false;
        }
        return true;
    }

    void requireAttributeStorage(std::size_t count, std::size_t width,
                                 std::size_t extra = 0U)
    {
        std::size_t bytes = 0U;
        const std::size_t maximum = dommy::mesh::kMaxMeshOperationBytes;
        // Retain room for the sample, native data and backend copies. This is
        // a logical payload policy, not an Alembic/Maya peak-memory guarantee.
        if (count > dommy::generation::kMaxGeneratedPointCount ||
            !dommy::generation::checkedProduct(count, width, maximum, bytes) ||
            !dommy::generation::checkedSum(bytes, extra, maximum, bytes) ||
            !dommy::generation::checkedProduct(bytes, 4U, maximum, bytes))
            throw std::length_error("Alembic attribute exceeds its storage budget");
    }

    std::string exportString(const MPlug& plug)
    {
        const MString value = exportValue<MString>(plug);
        requireAttributeStorage(1U, sizeof(std::string), std::strlen(value.asChar()));
        return value.asChar();
    }

    std::vector<std::string> exportStrings(const MPlug& plug)
    {
        MStatus status;
        MFnStringArrayData data(exportValue<MObject>(plug), &status);
        requireAttributeQuery(status);
        const unsigned int count = exportQuery(data, &MFnStringArrayData::length);
        if (count > dommy::generation::kMaxGeneratedStringCount)
            throw std::length_error("Too many Alembic attribute strings");
        requireAttributeStorage(count, sizeof(std::string));
        std::size_t bytes = 0U;
        for (unsigned int i = 0U; i < count; ++i) {
            if (!dommy::generation::checkedSum(bytes, std::strlen(data[i].asChar()),
                    dommy::generation::kMaxGeneratedStringByteCount, bytes))
                throw std::length_error("Alembic attribute strings exceed their budget");
        }
        requireAttributeStorage(count, sizeof(std::string), bytes);
        std::vector<std::string> values(count);
        for (unsigned int i = 0U; i < count; ++i) values[i] = data[i].asChar();
        return values;
    }

	template<typename T>
	const void* arrayDataOrNull(const std::vector<T>& values)
	{
		return dommy::memory::dataOrNull(values);
	}

	template<typename T>
	bool resizeArrayValues(std::vector<T>& values,
						   std::size_t elementCount,
						   std::size_t extent)
	{
		std::size_t valueCount = 0;
		if (!dommy::memory::checkedMultiply(elementCount, extent, valueCount)) {
			return false;
		}
		requireAttributeStorage(elementCount, sizeof(T) * extent);
		values.resize(valueCount);
		return true;
	}

	bool readNumericData2Short(const MPlug& plug,
							   Alembic::Util::int16_t& first,
							   Alembic::Util::int16_t& second)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		short firstValue = 0;
		short secondValue = 0;
		if (status != MS::kSuccess ||
			data.getData2Short(firstValue, secondValue) != MS::kSuccess) {
			return false;
		}
		first = static_cast<Alembic::Util::int16_t>(firstValue);
		second = static_cast<Alembic::Util::int16_t>(secondValue);
		return true;
	}

	bool readNumericData3Short(const MPlug& plug,
							   Alembic::Util::int16_t& first,
							   Alembic::Util::int16_t& second,
							   Alembic::Util::int16_t& third)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		short firstValue = 0;
		short secondValue = 0;
		short thirdValue = 0;
		if (status != MS::kSuccess ||
			data.getData3Short(firstValue, secondValue, thirdValue) != MS::kSuccess) {
			return false;
		}
		first = static_cast<Alembic::Util::int16_t>(firstValue);
		second = static_cast<Alembic::Util::int16_t>(secondValue);
		third = static_cast<Alembic::Util::int16_t>(thirdValue);
		return true;
	}

	bool readNumericData2Int(const MPlug& plug, int& first, int& second)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData2Int(first, second) == MS::kSuccess;
	}

	bool readNumericData3Int(const MPlug& plug, int& first, int& second, int& third)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData3Int(first, second, third) == MS::kSuccess;
	}

	bool readNumericData2Float(const MPlug& plug, float& first, float& second)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData2Float(first, second) == MS::kSuccess;
	}

	bool readNumericData3Float(const MPlug& plug, float& first, float& second, float& third)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData3Float(first, second, third) == MS::kSuccess;
	}

	bool readNumericData2Double(const MPlug& plug, double& first, double& second)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData2Double(first, second) == MS::kSuccess;
	}

	bool readNumericData3Double(const MPlug& plug,
								double& first,
								double& second,
								double& third)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData3Double(first, second, third) == MS::kSuccess;
	}

	bool readNumericData4Double(const MPlug& plug,
								double& first,
								double& second,
								double& third,
								double& fourth)
	{
		MStatus status;
		MFnNumericData data(exportValue<MObject>(plug), &status);
		return status == MS::kSuccess &&
			data.getData4Double(first, second, third, fourth) == MS::kSuccess;
	}
	
	static const char * cAttrScope = "_AbcGeomScope";
	static const char * cAttrType  = "_AbcType";
	
	// returns true if a plug is of a simple numeric data type
	bool isDataAttr(const MPlug & iParent)
	{
		MObject obj = exportValue<MObject>(iParent);
		switch (obj.apiType())
		{
			case MFn::kData2Double:
			case MFn::kData3Double:
			case MFn::kData4Double:
			case MFn::kData2Float:
			case MFn::kData3Float:
			case MFn::kData2Int:
			case MFn::kData3Int:
			case MFn::kData2Short:
			case MFn::kData3Short:
			case MFn::kStringData:
			case MFn::kStringArrayData:
			case MFn::kFloatVectorArrayData:
			case MFn::kVectorArrayData:
			case MFn::kFloatArrayData:
			case MFn::kDoubleArrayData:
			case MFn::kIntArrayData:
			case MFn::kPointArrayData:
			case MFn::kUInt64ArrayData:
				return true;
				
			default:
				return false;
		}
		return false;
	}
	
	AbcGeom::GeometryScope strToScope(MString iStr)
	{
		iStr.toLowerCase();
		if (iStr == "vtx")
		{
			return AbcGeom::kVertexScope;
		}
		else if (iStr == "fvr")
		{
			return AbcGeom::kFacevaryingScope;
		}
		else if (iStr == "uni")
		{
			return AbcGeom::kUniformScope;
		}
		else if (iStr == "var")
		{
			return AbcGeom::kVaryingScope;
		}
		
		return AbcGeom::kConstantScope;
	}
	
	// returns true if the string ends with _AbcGeomScope or _AbcType
	bool endsWithArbAttr(const std::string & iStr)
	{
		size_t len = iStr.size();
		
		return (len >= 13 && iStr.compare(len - 13, 13, cAttrScope) == 0) ||
		(len >= 8 && iStr.compare(len - 8, 8, cAttrType) == 0);
	}
	
	void createUserPropertyFromNumeric(MFnNumericData::Type iType,
									   const MObject& iAttr,
									   const MPlug& iPlug,
									   Abc::OCompoundProperty & iParent,
									   Alembic::Util::uint32_t iTimeIndex,
									   AbcGeom::GeometryScope iScope,
									   std::vector < PlugAndObjScalar > & oScalars,
									   std::vector < PlugAndObjArray > & oArrays, const std::shared_ptr<dommy::attribute::ExportAttributeBudget>& budget)
	{
		std::string plugName = exportName(iPlug).asChar();
		switch (iType)
		{
			case MFnNumericData::kBoolean:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OBoolProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::kByte:
			case MFnNumericData::kChar:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OCharProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::kShort:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OInt16Property>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::kInt:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OInt32Property>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::kFloat:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OFloatProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::kDouble:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::ODoubleProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Short:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV2sProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Short:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV3sProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Int:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV2iProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Int:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV3iProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Float:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV2fProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Float:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV3fProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Double:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV2dProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Double:
			{
				PlugAndObjScalar p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto up = createAttributeProperty<Abc::OV3dProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
				p.prop = up;
				oScalars.push_back(p);
			}
				break;
				
			case MFnNumericData::k4Double:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				AbcA::DataType dtype(Alembic::Util::kFloat64POD, 4);
				auto up = createAttributeProperty<Abc::OArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, dtype, iTimeIndex);
				p.prop = up;
				oArrays.push_back(p);
			}
				break;
				
			default:
				break;
		}
	}
	
	void createGeomPropertyFromNumeric(MFnNumericData::Type iType, const MObject& iAttr,
									   const MPlug& iPlug, Abc::OCompoundProperty & iParent,
									   Alembic::Util::uint32_t iTimeIndex,
									   AbcGeom::GeometryScope iScope,
									   std::vector < PlugAndObjArray > & oArrayVec, const std::shared_ptr<dommy::attribute::ExportAttributeBudget>& budget)
	{
		std::string plugName = exportName(iPlug).asChar();
		switch (iType)
		{
			case MFnNumericData::kBoolean:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OBoolGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										   iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::kByte:
			case MFnNumericData::kChar:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OCharGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										   iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::kShort:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OInt16GeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::kInt:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OInt32GeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::kFloat:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OFloatGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::kDouble:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::ODoubleGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											 iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Short:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV2sGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Short:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV3sGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Int:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV2iGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Int:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV3iGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Float:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV2fGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Float:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				
				MStatus status;
				MFnAttribute mfnAttr(iAttr, &status);
                requireAttributeQuery(status);
				if (exportQuery(mfnAttr, &MFnAttribute::isUsedAsColor))
				{
					auto gp = createAttributeProperty<AbcGeom::OC3fGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											  iTimeIndex);
					p.prop = gp.getValueProperty();
				}
				else
				{
					auto gp = createAttributeProperty<AbcGeom::OV3fGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											  iTimeIndex);
					p.prop = gp.getValueProperty();
				}
				
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k2Double:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV2dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k3Double:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				auto gp = createAttributeProperty<AbcGeom::OV3dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										  iTimeIndex);
				p.prop = gp.getValueProperty();
				oArrayVec.push_back(p);
			}
				break;
				
			case MFnNumericData::k4Double:
			{
				PlugAndObjArray p;
				p.plug = iPlug;
				p.obj = iAttr;
				AbcA::DataType dtype(Alembic::Util::kFloat64POD, 4);
				p.prop = createAttributeProperty<Abc::OArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, dtype, iTimeIndex);
				oArrayVec.push_back(p);
			}
				break;
				
			default:
				break;
		}
	}
	
	bool MFnNumericDataToSample(MFnNumericData::Type iType,
								const MPlug& iPlug,
								Abc::OScalarProperty & oProp)
	{
        if (!requireNumericAttributeType(iType, oProp.getDataType())) return false;
		int    ival;
		float  fval;
		double dval;
		bool   bval;
		Alembic::Util::int16_t sval;
		Alembic::Util::int8_t  cval;
		
		Alembic::Util::int16_t v2sVal[2];
		Alembic::Util::int16_t v3sVal[3];
		
		Alembic::Util::int32_t v2iVal[2];
		Alembic::Util::int32_t v3iVal[3];
		
		float v2fVal[2];
		float v3fVal[3];
		
		double v2dVal[2];
		double v3dVal[3];
		
		switch (iType)
		{
			case MFnNumericData::kBoolean:
			{
				bval = exportValue<bool>(iPlug);
				oProp.set(&bval);
			}
				break;
				
			case MFnNumericData::kByte:
			case MFnNumericData::kChar:
			{
				cval = exportValue<char>(iPlug);
				oProp.set(&cval);
			}
				break;
				
			case MFnNumericData::kShort:
			{
				sval = exportValue<short>(iPlug);
				oProp.set(&sval);
			}
				break;
				
			case MFnNumericData::kInt:
			{
				ival = exportValue<int>(iPlug);
				oProp.set(&ival);
			}
				break;
				
			case MFnNumericData::kFloat:
			{
				fval = exportValue<float>(iPlug);
				oProp.set(&fval);
			}
				break;
				
			case MFnNumericData::kDouble:
			{
				dval = exportValue<double>(iPlug);
				oProp.set(&dval);
			}
				break;
				
			case MFnNumericData::k2Short:
			{
				if (!readNumericData2Short(iPlug, v2sVal[0], v2sVal[1])) {
					return false;
				}
				
				oProp.set(&v2sVal[0]);
			}
				break;
				
			case MFnNumericData::k3Short:
			{
				if (!readNumericData3Short(iPlug, v3sVal[0], v3sVal[1], v3sVal[2])) {
					return false;
				}
				
				oProp.set(&v3sVal[0]);
			}
				break;
				
			case MFnNumericData::k2Int:
			{
				int val0, val1;
				if (!readNumericData2Int(iPlug, val0, val1)) {
					return false;
				}
				v2iVal[0] = Alembic::Util::int32_t(val0);
				v2iVal[1] = Alembic::Util::int32_t(val1);
				
				oProp.set(&v2iVal[0]);
			}
				break;
				
			case MFnNumericData::k3Int:
			{
				int val0, val1, val2;
				if (!readNumericData3Int(iPlug, val0, val1, val2)) {
					return false;
				}
				v3iVal[0] = Alembic::Util::int32_t(val0);
				v3iVal[1] = Alembic::Util::int32_t(val1);
				v3iVal[2] = Alembic::Util::int32_t(val2);
				
				oProp.set(&v3iVal[0]);
			}
				break;
				
			case MFnNumericData::k2Float:
			{
				if (!readNumericData2Float(iPlug, v2fVal[0], v2fVal[1])) {
					return false;
				}
				
				oProp.set(&v2fVal[0]);
			}
				break;
				
			case MFnNumericData::k3Float:
			{
				if (!readNumericData3Float(iPlug, v3fVal[0], v3fVal[1], v3fVal[2])) {
					return false;
				}
				
				oProp.set(&v3fVal[0]);
			}
				break;
				
			case MFnNumericData::k2Double:
			{
				if (!readNumericData2Double(iPlug, v2dVal[0], v2dVal[1])) {
					return false;
				}
				
				oProp.set(&v2dVal[0]);
			}
				break;
				
			case MFnNumericData::k3Double:
			{
				if (!readNumericData3Double(iPlug, v3dVal[0], v3dVal[1], v3dVal[2])) {
					return false;
				}
				
				oProp.set(&v3dVal[0]);
			}
				break;
				
			default:
				return false;
				break;
		}
		
		return true;
	}
	
	bool MFnNumericDataToSample(MFnNumericData::Type iType,
								const MPlug& iPlug,
								Abc::OArrayProperty & oProp)
	{
        if (!requireNumericAttributeType(iType, oProp.getDataType())) return false;
		const bool isArray = exportQuery(iPlug, &MPlug::isArray);
		const unsigned int numElements = isArray ? exportQuery(iPlug, &MPlug::numElements) : 0U;
		
		unsigned int dimSize = numElements;
		if (!isArray)
			dimSize = 1;
		
		switch (iType)
		{
			case MFnNumericData::kBoolean:
			{
				requireAttributeStorage(dimSize, sizeof(Alembic::Util::bool_t));
				std::vector< Alembic::Util::bool_t > val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<bool>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<bool>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::kByte:
			case MFnNumericData::kChar:
			{
				requireAttributeStorage(dimSize, sizeof(Alembic::Util::int8_t));
				std::vector< Alembic::Util::int8_t > val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<char>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<char>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::kShort:
			{
				requireAttributeStorage(dimSize, sizeof(Alembic::Util::int16_t));
				std::vector< Alembic::Util::int16_t > val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<short>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<short>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::kInt:
			{
				requireAttributeStorage(dimSize, sizeof(Alembic::Util::int32_t));
				std::vector< Alembic::Util::int32_t > val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<int>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<int>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::kFloat:
			{
				requireAttributeStorage(dimSize, sizeof(float));
				std::vector<float> val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<float>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<float>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::kDouble:
			{
				requireAttributeStorage(dimSize, sizeof(double));
				std::vector<double> val(dimSize);
				if (!isArray)
				{
					val[0] = exportValue<double>(iPlug);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						val[i] = exportValue<double>(exportElement(iPlug, i));
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k2Short:
			{
					std::vector< Alembic::Util::int16_t > val;
					if (!resizeArrayValues(val, dimSize, 2)) return false;
				if (!isArray)
				{
					if (!readNumericData2Short(iPlug, val[0], val[1])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData2Short(exportElement(iPlug, i), val[2*i], val[2*i+1])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k3Short:
			{
					std::vector< Alembic::Util::int16_t > val;
					if (!resizeArrayValues(val, dimSize, 3)) return false;
				if (!isArray)
				{
					if (!readNumericData3Short(iPlug, val[0], val[1], val[2])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData3Short(
								exportElement(iPlug, i), val[3*i], val[3*i+1], val[3*i+2])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k2Int:
			{
					std::vector< Alembic::Util::int32_t > val;
					if (!resizeArrayValues(val, dimSize, 2)) return false;
				if (!isArray)
				{
					int val0, val1;
					if (!readNumericData2Int(iPlug, val0, val1)) return false;
					val[0] = Alembic::Util::int32_t(val0);
					val[1] = Alembic::Util::int32_t(val1);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						int val0, val1;
						if (!readNumericData2Int(exportElement(iPlug, i), val0, val1)) return false;
						val[2*i] = Alembic::Util::int32_t(val0);
						val[2*i+1] = Alembic::Util::int32_t(val1);
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k3Int:
			{
					std::vector< Alembic::Util::int32_t > val;
					if (!resizeArrayValues(val, dimSize, 3)) return false;
				if (!isArray)
				{
					int val0, val1, val2;
					if (!readNumericData3Int(iPlug, val0, val1, val2)) return false;
					val[0] = Alembic::Util::int32_t(val0);
					val[1] = Alembic::Util::int32_t(val1);
					val[2] = Alembic::Util::int32_t(val2);
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						int val0, val1, val2;
						if (!readNumericData3Int(exportElement(iPlug, i), val0, val1, val2)) return false;
						val[3*i] = Alembic::Util::int32_t(val0);
						val[3*i+1] = Alembic::Util::int32_t(val1);
						val[3*i+2] = Alembic::Util::int32_t(val2);
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k2Float:
			{
					std::vector<float> val;
					if (!resizeArrayValues(val, dimSize, 2)) return false;
				if (!isArray)
				{
					if (!readNumericData2Float(iPlug, val[0], val[1])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData2Float(exportElement(iPlug, i), val[2*i], val[2*i+1])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k3Float:
			{
					std::vector<float> val;
					if (!resizeArrayValues(val, dimSize, 3)) return false;
				if (!isArray)
				{
					if (!readNumericData3Float(iPlug, val[0], val[1], val[2])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData3Float(
								exportElement(iPlug, i), val[3*i], val[3*i+1], val[3*i+2])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k2Double:
			{
					std::vector<double> val;
					if (!resizeArrayValues(val, dimSize, 2)) return false;
				if (!isArray)
				{
					if (!readNumericData2Double(iPlug, val[0], val[1])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData2Double(exportElement(iPlug, i), val[2*i], val[2*i+1])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k3Double:
			{
					std::vector<double> val;
					if (!resizeArrayValues(val, dimSize, 3)) return false;
				if (!isArray)
				{
					if (!readNumericData3Double(iPlug, val[0], val[1], val[2])) return false;
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData3Double(
								exportElement(iPlug, i), val[3*i], val[3*i+1], val[3*i+2])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			case MFnNumericData::k4Double:
			{
					std::vector<double> val;
					if (!resizeArrayValues(val, dimSize, 4)) return false;
				if (!isArray)
				{
					if (!readNumericData4Double(iPlug, val[0], val[1], val[2], val[3])) {
						return false;
					}
				}
				else
				{
					for (unsigned int i = 0; i < numElements; ++i)
					{
						if (!readNumericData4Double(
								exportElement(iPlug, i), val[4*i], val[4*i+1], val[4*i+2], val[4*i+3])) {
							return false;
						}
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(dimSize));
				oProp.set(samp);
			}
				break;
				
			default:
				return false;
				break;
		}
		
		return true;
	}
	
	bool MFnTypedDataToSample(MFnData::Type iType,
							  const MPlug& iPlug,
							  Abc::OScalarProperty & oProp)
	{
		
		switch (iType)
		{
			case MFnData::kNumeric:
			{
				return MFnNumericDataToSample(numericDataType(iPlug), iPlug,
											  oProp);
			}
				break;
				
			case MFnData::kString:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kStringPOD, 1U, sizeof(std::string));
				Abc::OStringProperty strProp( oProp.getPtr(), Abc::kWrapExisting );
				std::string val = exportString(iPlug);
				strProp.set(val);
			}
				break;
				
			case MFnData::kMatrix:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, 16U, sizeof(double));
				MStatus status;
				MFnMatrixData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				MMatrix mat = exportQuery(arr, &MFnMatrixData::matrix);
				double val[16];
				
				unsigned int i = 0;
				for (unsigned int r = 0; r < 4; r++)
				{
					for (unsigned int c = 0; c < 4; c++, i++)
					{
						val[i] = mat[r][c];
					}
				}
				
				oProp.set(&val);
			}
				break;
				
			default:
				return false;
				break;
		}
		
		return true;
	}
	
	bool MFnTypedDataToSample(MFnData::Type iType,
							  const MPlug& iPlug,
							  Abc::OArrayProperty & oProp)
	{
		
		switch (iType)
		{
			case MFnData::kNumeric:
			{
				return MFnNumericDataToSample(numericDataType(iPlug), iPlug,
											  oProp);
			}
				break;
				
			case MFnData::kString:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kStringPOD, 1U, sizeof(std::string));
				std::vector< std::string > val(1);
				val[0] = exportString(iPlug);
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(1));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kStringArray:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kStringPOD, 1U, sizeof(std::string));
				const std::vector<std::string> val = exportStrings(iPlug);
				const size_t length = val.size();
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(length));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kDoubleArray:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, 1U, sizeof(double));
				MStatus status;
				MFnDoubleArrayData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				
				unsigned int length = exportQuery(arr, &MFnDoubleArrayData::length);
				requireAttributeStorage(length, sizeof(double));
				std::vector< double > val(length);
				for (unsigned int i = 0; i < length; i++)
				{
					val[i] = arr[i];
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(length));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kIntArray:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kInt32POD, 1U, sizeof(Alembic::Util::int32_t));
				MStatus status;
				MFnIntArrayData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				
				unsigned int length = exportQuery(arr, &MFnIntArrayData::length);
				requireAttributeStorage(length, sizeof(Alembic::Util::int32_t));
				std::vector< Alembic::Util::int32_t > val(length);
				for (unsigned int i = 0; i < length; i++)
				{
					val[i] = arr[i];
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(length));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kPointArray:
			{
				MStatus status;
				MFnPointArrayData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				
					unsigned int length = exportQuery(arr, &MFnPointArrayData::length);
					unsigned int extent = oProp.getDataType().getExtent();
					if (extent < 2 || extent > 4) return false;
					requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, extent, sizeof(double));
					std::vector< double > val;
					if (!resizeArrayValues(val, length, extent)) return false;
				for (unsigned int i = 0; i < length; i++)
				{
					MPoint pt(arr[i]);
					val[extent*i] = pt.x;
					val[extent*i+1] = pt.y;
					
					if (extent > 2)
						val[extent*i+2] = pt.z;
					
					if (extent > 3)
						val[extent*i+3] = pt.w;
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(length));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kVectorArray:
			{
				MStatus status;
				MFnVectorArrayData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				
					unsigned int length = exportQuery(arr, &MFnVectorArrayData::length);
					unsigned int extent = oProp.getDataType().getExtent();
					if (extent < 2 || extent > 3) return false;
					requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, extent, sizeof(double));
					std::vector< double > val;
					if (!resizeArrayValues(val, length, extent)) return false;
				for (unsigned int i = 0; i < length; i++)
				{
					MVector v(arr[i]);
					val[extent*i] = v.x;
					val[extent*i+1] = v.y;
					
					if (extent > 2)
						val[extent*i+2] = v.z;
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(length));
				oProp.set(samp);
			}
				break;
				
			case MFnData::kMatrix:
			{
				requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, 16U, sizeof(double));
				MStatus status;
				MFnMatrixData arr(exportValue<MObject>(iPlug), &status);
				requireAttributeQuery(status);
				MMatrix mat = exportQuery(arr, &MFnMatrixData::matrix);
				std::vector<double> val(16);
				
				unsigned int i = 0;
				for (unsigned int r = 0; r < 4; r++)
				{
					for (unsigned int c = 0; c < 4; c++, i++)
					{
						val[i] = mat[r][c];
					}
				}
					AbcA::ArraySample samp(arrayDataOrNull(val), oProp.getDataType(),
									   Alembic::Util::Dimensions(1));
				oProp.set(samp);
			}
				break;
				
			default:
				return false;
				break;
		}
		
		return true;
	}
	
	bool attributeToScalarPropertyPair(const MObject& iAttr,
									   const MPlug& iPlug,
									   Abc::OScalarProperty & oProp)
	{
		if (iAttr.hasFn(MFn::kTypedAttribute))
		{
			MStatus status;
			MFnTypedAttribute typedAttr(iAttr, &status);
			requireAttributeQuery(status);
			return MFnTypedDataToSample(exportQuery(typedAttr, &MFnTypedAttribute::attrType), iPlug, oProp);
		}
		else if (iAttr.hasFn(MFn::kNumericAttribute))
		{
			MStatus status;
			MFnNumericAttribute numAttr(iAttr, &status);
			requireAttributeQuery(status);
			return MFnNumericDataToSample(exportQuery(numAttr, &MFnNumericAttribute::unitType), iPlug, oProp);
		}
		else if (iAttr.hasFn(MFn::kUnitAttribute))
		{
			requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, 1U, sizeof(double));
			double dval = exportValue<double>(iPlug);
			oProp.set(&dval);
			return true;
		}
		else if (iAttr.hasFn(MFn::kEnumAttribute))
		{
			requireAttributeType(oProp.getDataType(), Alembic::Util::kInt16POD, 1U, sizeof(Alembic::Util::int16_t));
			Alembic::Util::int16_t val = exportValue<short>(iPlug);
			oProp.set(&val);
			return true;
		}
		
		return false;
	}
	
	
	bool attributeToArrayPropertyPair(const MObject& iAttr,
									  const MPlug& iPlug,
									  Abc::OArrayProperty & oProp)
	{
		if (iAttr.hasFn(MFn::kTypedAttribute))
		{
			MStatus status;
			MFnTypedAttribute typedAttr(iAttr, &status);
			requireAttributeQuery(status);
			return MFnTypedDataToSample(exportQuery(typedAttr, &MFnTypedAttribute::attrType), iPlug,
										oProp);
		}
		else if (iAttr.hasFn(MFn::kNumericAttribute))
		{
			MStatus status;
			MFnNumericAttribute numAttr(iAttr, &status);
			requireAttributeQuery(status);
			return MFnNumericDataToSample(exportQuery(numAttr, &MFnNumericAttribute::unitType), iPlug,
            oProp);
		}
		else if (iAttr.hasFn(MFn::kUnitAttribute))
		{
			requireAttributeType(oProp.getDataType(), Alembic::Util::kFloat64POD, 1U, sizeof(double));
			double val = exportValue<double>(iPlug);
			AbcA::ArraySample samp(&val, oProp.getDataType(),
								   Alembic::Util::Dimensions(1));
			oProp.set(samp);
			return true;
		}
		else if (iAttr.hasFn(MFn::kEnumAttribute))
		{
			requireAttributeType(oProp.getDataType(), Alembic::Util::kInt16POD, 1U, sizeof(Alembic::Util::int16_t));
			Alembic::Util::int16_t val = exportValue<short>(iPlug);
			AbcA::ArraySample samp(&val, oProp.getDataType(),
								   Alembic::Util::Dimensions(1));
			oProp.set(samp);
			return true;
		}
		
		return false;
	}
	
	void createUserPropertyFromMFnAttr(const MObject& iAttr,
									   const MPlug& iPlug,
									   Abc::OCompoundProperty & iParent,
									   Alembic::Util::uint32_t iTimeIndex,
									   AbcGeom::GeometryScope iScope,
									   const MString & iTypeStr,
									   std::vector < PlugAndObjScalar > & oScalars,
									   std::vector < PlugAndObjArray > & oArrays, const std::shared_ptr<dommy::attribute::ExportAttributeBudget>& budget)
	{
		MStatus stat;
		std::string plugName = exportName(iPlug).asChar();
		
		if (iAttr.hasFn(MFn::kNumericAttribute))
		{
			MFnNumericAttribute numFn(iAttr, &stat);
			
			requireAttributeQuery(stat);
			
			createUserPropertyFromNumeric(exportQuery(numFn, &MFnNumericAttribute::unitType), iAttr, iPlug,
										  iParent, iTimeIndex, iScope,
										  oScalars, oArrays, budget);
		}
		else if (iAttr.hasFn(MFn::kTypedAttribute))
		{
			MFnTypedAttribute typeFn(iAttr, &stat);
			
			requireAttributeQuery(stat);
			
			switch (exportQuery(typeFn, &MFnTypedAttribute::attrType))
			{
				case MFnData::kString:
				{
					PlugAndObjScalar p;
					p.plug = iPlug;
					p.obj = iAttr;
					p.prop = createAttributeProperty<Abc::OStringProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
					oScalars.push_back(p);
				}
					break;
					
				case MFnData::kStringArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					p.prop = createAttributeProperty<Abc::OStringArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					oArrays.push_back(p);
				}
					break;
					
				case MFnData::kDoubleArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					p.prop = createAttributeProperty<Abc::ODoubleArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					oArrays.push_back(p);
				}
					break;
					
				case MFnData::kIntArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					p.prop = createAttributeProperty<Abc::OInt32ArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					oArrays.push_back(p);
				}
					break;
					
				case MFnData::kPointArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					if (iTypeStr  == "point2")
						p.prop = createAttributeProperty<Abc::OP2dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					else
						p.prop = createAttributeProperty<Abc::OP3dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					
					oArrays.push_back(p);
				}
					break;
					
				case MFnData::kVectorArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					if (iTypeStr == "vector2")
						p.prop = createAttributeProperty<Abc::OV2dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					else if (iTypeStr == "normal2")
						p.prop = createAttributeProperty<Abc::ON2dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					else if (iTypeStr == "normal3")
						p.prop = createAttributeProperty<Abc::ON3dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					else
						p.prop = createAttributeProperty<Abc::OV3dArrayProperty>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, iTimeIndex);
					
					oArrays.push_back(p);
				}
					break;
					
				case MFnData::kMatrix:
				{
					PlugAndObjScalar p;
					p.plug = iPlug;
					p.obj = iAttr;
					p.prop = createAttributeProperty<Abc::OM44dProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
					oScalars.push_back(p);
				}
					break;
					
				case MFnData::kNumeric:
				{
					const MFnNumericData::Type numericType = numericDataType(iPlug);
					createUserPropertyFromNumeric(numericType, iAttr,
												  iPlug, iParent, iTimeIndex, iScope, oScalars, oArrays, budget);
				}
					break;
					
				default:
				{
					// get the full property name for the warning
					MString msg = "WARNING: Couldn't convert ";
					msg += iPlug.partialName(1, 0, 0, 0, 1, 1);
					msg += " to a property, so skipping.";
					MGlobal::displayWarning(msg);
				}
					break;
			}
		}
		else if (iAttr.hasFn(MFn::kUnitAttribute))
		{
			PlugAndObjScalar p;
			p.plug = iPlug;
			p.obj = iAttr;
			p.prop = createAttributeProperty<Abc::ODoubleProperty>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
			oScalars.push_back(p);
		}
		else if (iAttr.hasFn(MFn::kEnumAttribute))
		{
			PlugAndObjScalar p;
			p.plug = iPlug;
			p.obj = iAttr;
			p.prop = createAttributeProperty<Abc::OInt16Property>(budget, plugName, sizeof(PlugAndObjScalar), iParent, plugName, iTimeIndex);
			oScalars.push_back(p);
		}
	}
	
	void createGeomPropertyFromMFnAttr(const MObject& iAttr,
									   const MPlug& iPlug,
									   Abc::OCompoundProperty & iParent,
									   Alembic::Util::uint32_t iTimeIndex,
									   AbcGeom::GeometryScope iScope,
									   const MString & iTypeStr,
									   std::vector < PlugAndObjArray > & oArrayVec, const std::shared_ptr<dommy::attribute::ExportAttributeBudget>& budget)
	{
		// for some reason we have just 1 of the elements of an array, bail
		if (exportQuery(iPlug, &MPlug::isElement))
			return;
		
		MStatus stat;
		
		std::string plugName = exportName(iPlug).asChar();
		
		if (iAttr.hasFn(MFn::kNumericAttribute))
		{
			MFnNumericAttribute numFn(iAttr, &stat);
			
			requireAttributeQuery(stat);
			
			createGeomPropertyFromNumeric(exportQuery(numFn, &MFnNumericAttribute::unitType), iAttr, iPlug, iParent,
            iTimeIndex, iScope, oArrayVec, budget);
		}
		else if (iAttr.hasFn(MFn::kTypedAttribute))
		{
			MFnTypedAttribute typeFn(iAttr, &stat);
			requireAttributeQuery(stat);
			
			Alembic::AbcCoreAbstract::MetaData md;
			md.set("isArray", "1");
			
			switch (exportQuery(typeFn, &MFnTypedAttribute::attrType))
			{
				case MFnData::kString:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					auto gp = createAttributeProperty<AbcGeom::OStringGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												 1, iTimeIndex);
					p.prop = gp.getValueProperty();
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kStringArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					auto gp = createAttributeProperty<AbcGeom::OStringGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												 1, iTimeIndex, md);
					p.prop = gp.getValueProperty();
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kDoubleArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					auto gp = createAttributeProperty<AbcGeom::ODoubleGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												 1, iTimeIndex, md);
					p.prop = gp.getValueProperty();
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kIntArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					auto gp = createAttributeProperty<AbcGeom::OInt32GeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												1, iTimeIndex, md);
					p.prop = gp.getValueProperty();
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kPointArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					if (iTypeStr  == "point2")
					{
						auto gp = createAttributeProperty<AbcGeom::OP2dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex, md);
						p.prop = gp.getValueProperty();
					}
					else
					{
						auto gp = createAttributeProperty<AbcGeom::OP3dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex);
						p.prop = gp.getValueProperty();
					}
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kVectorArray:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					if (iTypeStr == "vector2")
					{
						auto gp = createAttributeProperty<AbcGeom::OV2dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex, md);
						p.prop = gp.getValueProperty();
					}
					else if (iTypeStr == "normal2")
					{
						auto gp = createAttributeProperty<AbcGeom::ON2dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex, md);
						p.prop = gp.getValueProperty();
					}
					else if (iTypeStr == "normal3")
					{
						auto gp = createAttributeProperty<AbcGeom::ON3dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex, md);
						p.prop = gp.getValueProperty();
					}
					else
					{
						auto gp = createAttributeProperty<AbcGeom::OV3dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope,
												  1, iTimeIndex, md);
						p.prop = gp.getValueProperty();
					}
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kMatrix:
				{
					PlugAndObjArray p;
					p.plug = iPlug;
					p.obj = iAttr;
					auto gp = createAttributeProperty<AbcGeom::OM44dGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
											   iTimeIndex);
					p.prop = gp.getValueProperty();
					oArrayVec.push_back(p);
				}
					break;
					
				case MFnData::kNumeric:
				{
					const MFnNumericData::Type numericType = numericDataType(iPlug);
					createGeomPropertyFromNumeric(numericType, iAttr,
												  iPlug, iParent, iTimeIndex, iScope, oArrayVec, budget);
				}
					break;
					
				default:
				{
					// get the full property name for the warning
					MString msg = "WARNING: Couldn't convert ";
					msg += iPlug.partialName(1, 0, 0, 0, 1, 1);
					msg += " to a property, so skipping.";
					MGlobal::displayWarning(msg);
				}
					break;
			}
		}
		else if (iAttr.hasFn(MFn::kUnitAttribute))
		{
			PlugAndObjArray p;
			p.plug = iPlug;
			p.obj = iAttr;
			auto gp = createAttributeProperty<AbcGeom::ODoubleGeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										 iTimeIndex);
			p.prop = gp.getValueProperty();
			oArrayVec.push_back(p);
		}
		else if (iAttr.hasFn(MFn::kEnumAttribute))
		{
			PlugAndObjArray p;
			p.plug = iPlug;
			p.obj = iAttr;
			auto gp = createAttributeProperty<AbcGeom::OInt16GeomParam>(budget, plugName, sizeof(PlugAndObjArray), iParent, plugName, false, iScope, 1,
										iTimeIndex);
			p.prop = gp.getValueProperty();
			oArrayVec.push_back(p);
		}
	}
	
}

MayaAttributeWriter::MayaAttributeWriter(Alembic::Abc::OCompoundProperty & iArbGeom,
                                         Alembic::Abc::OCompoundProperty & iUserProps,
                                         Alembic::Abc::OObject & iParentObj,
                                         const MFnDependencyNode & iNode,
                                         Alembic::Util::uint32_t iTimeIndex,
                                         const JobArgs & iArgs)
{
    auto& work = attributeInspectionWork(iArgs);
	PlugAndObjScalar visPlug;
	
	MStatus status;
    const unsigned int attrCount = iNode.attributeCount(&status);
    requireAttributeQuery(status);
    if (attrCount > dommy::generation::kMaxGeneratedStringCount) {
        throw std::length_error("Too many Maya attributes for Alembic export");
    }
	unsigned int i;
	
	std::vector< PlugAndObjArray > staticPlugObjArrayVec;
	std::vector< PlugAndObjScalar > staticPlugObjScalarVec;
	
	for (i = 0; i < attrCount; i++)
	{
        requireAttributeWork(work, 1U);
		MObject attr = iNode.attribute(i, &status);
        requireAttributeQuery(status);
		MFnAttribute mfnAttr(attr, &status);
        requireAttributeQuery(status);
		MPlug plug = iNode.findPlug(attr, true, &status);
        requireAttributeQuery(status);
		
		// if it is not readable, then bail without any more checking
		if (!exportQuery(mfnAttr, &MFnAttribute::isReadable) || plug.isNull())
			continue;
		
		MString propName = exportName(plug);
		
		std::string propStr = propName.asChar();
		
		// we handle visibility in a special way
//		if (propStr == "visibility")
//		{
//			if (iArgs.writeVisibility)
//			{
//				visPlug.plug = plug;
//				visPlug.obj = attr;
//			}
//			continue;
//		}
		
		bool userAttr = false;
		if (!matchFilterOrAttribs(plug, iArgs, userAttr))
			continue;
		
		if (userAttr && !iUserProps.valid())
			continue;
		if (!userAttr && !iArbGeom.valid())
			continue;
		
        int sampType = ddata_export_util::getSampledType(plug, work);
		
		MPlug scopePlug = optionalExportPlug(iNode, propName + cAttrScope);
		AbcGeom::GeometryScope scope = AbcGeom::kUnknownScope;
		
		if (!scopePlug.isNull())
		{
			scope = strToScope(exportValue<MString>(scopePlug));
		}
		
		MString typeStr;
		MPlug typePlug = optionalExportPlug(iNode, propName + cAttrType);
		if (!typePlug.isNull())
		{
			typeStr= exportValue<MString>(typePlug);
		}
		
		if (userAttr)
		{
			switch (sampType)
			{
					// static
				case 0:
				{
					//
					// Fills in the static plug to OScalarProperty OR
					// OArrayProperty correspondence, used for the writing
					// below.
					//
					createUserPropertyFromMFnAttr(attr, plug, iUserProps, 0,
												  scope, typeStr, staticPlugObjScalarVec,
												  staticPlugObjArrayVec, iArgs.exportAttributeBudget);
				}
					break;
					
					// sampled
				case 1:
					// curve treat like sampled
				case 2:
				{
					//
					// Fill in the mPlugUserPropertyVec, used for the writing
					// below as well as in the write() method for animated
					// values.
					//
					createUserPropertyFromMFnAttr(attr, plug, iUserProps,
												  iTimeIndex, scope, typeStr, mPlugObjScalarVec,
												  mPlugObjArrayVec, iArgs.exportAttributeBudget);
				}
					break;
			}
		}
		else
		{
			switch (sampType)
			{
					// static
				case 0:
				{
					//
					// Fills in the plug to OArrayProperty correspondence,
					// used for the writing below.
					//
					createGeomPropertyFromMFnAttr(attr, plug, iArbGeom, 0,
												  scope, typeStr, staticPlugObjArrayVec, iArgs.exportAttributeBudget);
				}
					break;
					
					// sampled
				case 1:
					// curve treat like sampled
				case 2:
				{
					//
					// mPlugObjArrayVec
					//
					// member variable used by isAnimated and when sampling
					// the animated data.
					//
					createGeomPropertyFromMFnAttr(attr, plug, iArbGeom,
												  iTimeIndex, scope, typeStr, mPlugObjArrayVec, iArgs.exportAttributeBudget);
				}
					break;
			}
		}
	}
	
	// write the static scalar props
	std::vector< PlugAndObjScalar >::iterator k =
	staticPlugObjScalarVec.begin();
	std::vector< PlugAndObjScalar >::iterator kend =
	staticPlugObjScalarVec.end();
	
	for (; k != kend; k++)
	{
		MString propName = exportName(k->plug);
		
		//
		// attributeTo[Scalar|Array]PropertyPair does the writing.
		bool filledProp = attributeToScalarPropertyPair(k->obj, k->plug,
														k->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	// write the static array props
	std::vector< PlugAndObjArray >::iterator j =
	staticPlugObjArrayVec.begin();
	std::vector< PlugAndObjArray >::iterator jend =
	staticPlugObjArrayVec.end();
	
	for (; j != jend; j++)
	{
		MString propName = exportName(j->plug);
		bool filledProp = attributeToArrayPropertyPair(j->obj, j->plug,
													   j->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	// write the animated userProperties
	k = mPlugObjScalarVec.begin();
	kend = mPlugObjScalarVec.end();
	
	for (; k != kend; ++k)
	{
		MString propName = exportName(k->plug);
		
		bool filledProp = attributeToScalarPropertyPair(k->obj, k->plug,
														k->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	// write the animated arbGeomProps
	j = mPlugObjArrayVec.begin();
	jend = mPlugObjArrayVec.end();
	for (; j != jend; j++)
	{
		MString propName = exportName(j->plug);
		bool filledProp = attributeToArrayPropertyPair(j->obj, j->plug,j->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	//
	// Rest of this is specific to visibility
	if (!visPlug.plug.isNull())
	{
        int retVis = ddata_export_util::getVisibilityType(visPlug.plug);
        if (retVis == 1 || retVis == 2 || (retVis == 3 && iTimeIndex != 0)) {
            iArgs.exportAttributeBudget->retainNativeProperty(
                "visibility", sizeof(PlugAndObjScalar) + sizeof(Abc::OCharProperty));
        }
		
		// visible will go on the top most compound
		Abc::OCompoundProperty parent = iParentObj.getProperties();
		
		switch (retVis)
		{
				// static visibility 0 case
			case 1:
			{
				Alembic::Util::int8_t visVal =
				Alembic::AbcGeom::kVisibilityHidden;
				
				Abc::OCharProperty bp =
				Alembic::AbcGeom::CreateVisibilityProperty(
														   iParentObj, 0);
				bp.set(visVal);
			}
				break;
				
				// animated visibility 0 case
			case 2:
			{
				Alembic::Util::int8_t visVal =
				Alembic::AbcGeom::kVisibilityHidden;
				
				Abc::OCharProperty bp =
				Alembic::AbcGeom::CreateVisibilityProperty(
														   iParentObj, iTimeIndex);
				
				bp.set(visVal);
				visPlug.prop = bp;
				mAnimVisibility = visPlug;
			}
				break;
				
				// animated visibility 1 case
			case 3:
			{
				// dont add if we are forcing static (no frame range specified)
				if (iTimeIndex == 0)
				{
					break;
				}
				
				mAnimVisibility = visPlug;
				
				Alembic::Util::int8_t visVal =
				Alembic::AbcGeom::kVisibilityDeferred;
				
				Abc::OCharProperty bp =
				Alembic::AbcGeom::CreateVisibilityProperty(
														   iParentObj, iTimeIndex);
				
				bp.set(visVal);
				visPlug.prop = bp;
				mAnimVisibility = visPlug;
				
			}
				break;
				
				// dont write any visibility
			default:
				break;
		}
	}
}

//
// Returns true if the attribute is:
//
// * Included by name via a -a argument
// * Matches by name via a -atp or -uatp argument
//
// If it's matched via -uatp, userAttrOut is set to true.
// These are intended to go in the .userProperties bucket on the
// object.
//
bool MayaAttributeWriter::matchFilterOrAttribs(const MPlug & iPlug,
											const JobArgs & iArgs,
											bool& userAttrOut)
{
	
	MString propName = exportName(iPlug);
	std::string name = propName.asChar();
    auto& work = attributeInspectionWork(iArgs);
    requireAttributeWork(work, name.size());
	
	if (name.find("[") != std::string::npos)
	{
		return false;
	}
	
	// For .arbGeomParam bucket
	std::vector<std::string>::const_iterator f;
	std::vector<std::string>::const_iterator fEnd =
	iArgs.prefixFilters.end();
	for (f = iArgs.prefixFilters.begin(); f != fEnd; ++f)
	{
        requireAttributeWork(work, 1U);
        requireAttributeWork(work, f->size());
		// check the prefilter and ignore those that match but end with
		// arb attr
		if (f->length() > 0 &&
			name.compare(0, f->length(), *f) == 0 &&
			!endsWithArbAttr(name) &&
			( !exportQuery(iPlug, &MPlug::isChild) || !isDataAttr(exportQuery(iPlug, &MPlug::parent)) ))
		{
			userAttrOut = false;
			return true;
		}
	}
	
	//
	// For .userProperties bucket
	std::vector<std::string>::const_iterator it;
	std::vector<std::string>::const_iterator itEnd =
	iArgs.userPrefixFilters.end();
	for (it = iArgs.userPrefixFilters.begin(); it != itEnd; ++it)
	{
        requireAttributeWork(work, 1U);
        requireAttributeWork(work, it->size());
		// check the userprefilter and ignore those that match but end with
		// arb attr
		if (it->length() > 0 &&
			name.compare(0, it->length(), *it) == 0 &&
			!endsWithArbAttr(name) &&
			( !exportQuery(iPlug, &MPlug::isChild) || !isDataAttr(exportQuery(iPlug, &MPlug::parent)) ))
		{
			userAttrOut = true;
			return true;
		}
	}
	
	// check our specific list of attributes
	if (iArgs.attribs.find(name) != iArgs.attribs.end())
	{
		userAttrOut = false;
		return true;
	}
	
//	if (iArgs.userAttribs.find(name) != iArgs.userAttribs.end())
//	{
//		userAttrOut = true;
//		return true;
//	}
	
	return false;
}

bool MayaAttributeWriter::hasAnyAttr(const MFnDependencyNode & iNode,
								  const JobArgs & iArgs)
{
    auto& work = attributeInspectionWork(iArgs);
	MStatus status;
    const unsigned int attrCount = iNode.attributeCount(&status);
    requireAttributeQuery(status);
    if (attrCount > dommy::generation::kMaxGeneratedStringCount) {
        throw std::length_error("Too many Maya attributes for Alembic export");
    }
	unsigned int i;
	
	std::vector< PlugAndObjArray > staticPlugObjArrayVec;
	
	bool userAttr;
	for (i = 0; i < attrCount; i++)
	{
        requireAttributeWork(work, 1U);
		MObject attr = iNode.attribute(i, &status);
        requireAttributeQuery(status);
		MFnAttribute mfnAttr(attr, &status);
        requireAttributeQuery(status);
		MPlug plug = iNode.findPlug(attr, true, &status);
        requireAttributeQuery(status);
		
		// if it is not readable, then bail without any more checking
		if (!exportQuery(mfnAttr, &MFnAttribute::isReadable) || plug.isNull())
		{
			continue;
		}
		
		if (matchFilterOrAttribs(plug, iArgs, userAttr))
		{
			return true;
		}
	}
	
	return false;
}

MayaAttributeWriter::~MayaAttributeWriter()
{
}

bool MayaAttributeWriter::isAnimated()
{
	return !mPlugObjArrayVec.empty() || !mAnimVisibility.plug.isNull() ||
	!mPlugObjScalarVec.empty();
}

void MayaAttributeWriter::write()
{
	
	std::vector< PlugAndObjArray >::iterator j =
	mPlugObjArrayVec.begin();
	std::vector< PlugAndObjArray >::iterator jend =
	mPlugObjArrayVec.end();
	
	for (; j != jend; j++)
	{
		MString propName = exportName(j->plug);
		bool filledProp = attributeToArrayPropertyPair(j->obj, j->plug, j->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	std::vector< PlugAndObjScalar >::iterator k =
	mPlugObjScalarVec.begin();
	std::vector< PlugAndObjScalar >::iterator kend =
	mPlugObjScalarVec.end();
	
	for (; k != kend; ++k)
	{
		MString propName = exportName(k->plug);
		
		bool filledProp = attributeToScalarPropertyPair(k->obj, k->plug,
														k->prop);
		
		if (!filledProp)
		{
            throw std::runtime_error("Unable to serialize a selected Alembic attribute");
		}
	}
	
	if (!mAnimVisibility.plug.isNull())
	{
		Alembic::Util::int8_t visVal = -1;
		if (!exportValue<bool>(mAnimVisibility.plug))
		{
			visVal = 0;
		}
		
		mAnimVisibility.prop.set(&visVal);
	}
	
}
