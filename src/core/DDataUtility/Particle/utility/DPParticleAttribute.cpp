//
//  DPParticleAttribute.cpp
//  dommyTool
//
//  Created by yuzo on 5/17/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPParticleAttribute.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include <cstring>


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"





const MTypeId DPParticleAttribute::typeId( ID_DP_PARTICLE_ATTRIBUTE );


MObject DPParticleAttribute::inDP;

MObject DPParticleAttribute::aIsVelocity;
MObject DPParticleAttribute::aVelocityType;
MObject DPParticleAttribute::aVelocityAttribute;
MObject DPParticleAttribute::aVelocityVector;
MObject DPParticleAttribute::aVelocityTargetAttribute;

MObject DPParticleAttribute::aIsRadius;
MObject DPParticleAttribute::aRadiusType;
MObject DPParticleAttribute::aRadiusAttribute;
MObject DPParticleAttribute::aRadius;
MObject DPParticleAttribute::aRadiusTargetAttribute;

MObject DPParticleAttribute::aIsMass;
MObject DPParticleAttribute::aMassType;
MObject DPParticleAttribute::aMassAttribute;
MObject DPParticleAttribute::aMass;
MObject DPParticleAttribute::aMassTargetAttribute;

MObject DPParticleAttribute::outDP;



void *DPParticleAttribute::creator() {
    return ::dommy::maya::createObject("DPParticleAttribute", [&]() {
        return new DPParticleAttribute();
    });
}




namespace {

// Names are stored keys (spaces are significant), not space-separated lists.
MStatus readParticleAttributeName(MDataBlock& data, const MObject& attribute,
                                 std::string& name)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString value = handle.asString();
    if (value.length() > dommy::generation::kMaxGeneratedStringByteCount ||
        std::strlen(value.asChar()) > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;
    name = value.asChar();
    return dommy::attribute::validAttributeName(name)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

float particleConstant(const MDataHandle& handle, float*)
{
    return handle.asFloat();
}

Imath::V3f particleConstant(const MDataHandle& handle, Imath::V3f*)
{
    const float3& value = handle.asFloat3();
    return Imath::V3f(value[0], value[1], value[2]);
}

bool particleSourceValues(const DPAttributeData::ConstPtr& source,
                          std::size_t count, std::vector<float>& values)
{
    return dommy::attribute::convertScalarAttribute(source, count, values);
}

bool particleSourceValues(const DPAttributeData::ConstPtr& source,
                          std::size_t count, std::vector<Imath::V3f>& values)
{
    return dommy::attribute::convertVector3Attribute(source, count, values);
}

// Work only on the unpublished output. Resolve aliases before replacing the
// field, and keep velocity -> radius -> mass ordering for chained requests.
template <typename Value>
MStatus applyParticleAttribute(DPData& output, MDataBlock& data,
                               const MObject& enabledAttribute, const MObject& modeAttribute,
                               const MObject& nameAttribute, const MObject& valueAttribute,
                               const MObject& sourceAttribute)
{
    MStatus status;
    const MDataHandle enabledHandle = data.inputValue(enabledAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!enabledHandle.asBool()) return MS::kSuccess;

    const MDataHandle modeHandle = data.inputValue(modeAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short mode = modeHandle.asShort();
    if (mode != 0 && mode != 1) return MS::kInvalidParameter;
    std::string name;
    DOMMY_RETURN_IF_FAILURE(readParticleAttributeName(data, nameAttribute, name));
    const std::size_t count = output.numberOfPoints();
    if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Value)))
        return MS::kInvalidParameter;
    std::vector<Value> values;
    if (mode == 0) {
        const MDataHandle valueHandle = data.inputValue(valueAttribute, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const Value value = particleConstant(valueHandle, static_cast<Value*>(nullptr));
        if (!dommy::attribute::isValidValue(value)) return MS::kInvalidParameter;
        values.assign(count, value);
    } else {
        std::string sourceName;
        DOMMY_RETURN_IF_FAILURE(readParticleAttributeName(data, sourceAttribute, sourceName));
        if (!particleSourceValues(output.getAttr(sourceName), count, values))
            return MS::kInvalidParameter;
    }
    DPAttributeData::Ptr replacement =
        std::make_shared<TypedDPAttributeData<Value>>(values);
    return dommy::attribute::commitStagedAttribute(output, name, replacement, count)
        ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

} // namespace

MStatus DPParticleAttribute::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [&](DPData& output) -> MStatus {
            output.setDPData(*input);
            if (output.numberOfPoints() == 0U) return MS::kSuccess;
            DOMMY_RETURN_IF_FAILURE(applyParticleAttribute<Imath::V3f>(
                output, data, aIsVelocity, aVelocityType, aVelocityAttribute,
                aVelocityVector, aVelocityTargetAttribute));
            DOMMY_RETURN_IF_FAILURE(applyParticleAttribute<float>(
                output, data, aIsRadius, aRadiusType, aRadiusAttribute,
                aRadius, aRadiusTargetAttribute));
            return applyParticleAttribute<float>(
                output, data, aIsMass, aMassType, aMassAttribute,
                aMass, aMassTargetAttribute);
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPParticleAttribute")


		
		
		
		



MStatus DPParticleAttribute::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;

    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aIsVelocity = nAttr.create( "IsVelocity", "isvel", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVelocity));

    const MObject defaultString1 = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityAttribute = tAttr.create("VelocityAttribute", "vattr", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttribute));

    MFnEnumAttribute enumFn;
    aVelocityType = enumFn.create( "VelocityType", "velty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Vector", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityType));

    aVelocityVector = nAttr.create( "VelocityVector", "velvec", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0f,1.0f,0.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityVector));

    const MObject defaultString2 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityTargetAttribute = tAttr.create("VelocityTargetAttribute", "vtattr", MFnData::kString, defaultString2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityTargetAttribute));

    aIsRadius = nAttr.create( "IsRadius", "israd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRadius));

    MFnEnumAttribute radEnumFn;
    aRadiusType = radEnumFn.create( "RadiusType", "radty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(radEnumFn.addField( "Radius", 0 ));
    DOMMY_RETURN_IF_FAILURE(radEnumFn.addField( "From Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(radEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusType));

    const MObject defaultString3 = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusAttribute = tAttr.create("RadiusAttribute", "rattr", MFnData::kString, defaultString3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttribute));

    aRadius = nAttr.create( "Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.1f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 5.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    const MObject defaultString4 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRadiusTargetAttribute = tAttr.create("RadiusTargetAttribute", "rtattr", MFnData::kString, defaultString4, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusTargetAttribute));

    aIsMass = nAttr.create( "IsMass", "isma", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMass));

    MFnEnumAttribute masEnumFn;
    aMassType = masEnumFn.create( "MassType", "maty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(masEnumFn.addField( "Mass", 0 ));
    DOMMY_RETURN_IF_FAILURE(masEnumFn.addField( "From Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(masEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassType));

    const MObject defaultString5 = fnStringData.create("mass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMassAttribute = tAttr.create("MassAttribute", "mattr", MFnData::kString, defaultString5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassAttribute));

    aMass = nAttr.create( "Mass", "ma", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 10.0f ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMass));

    const MObject defaultString6 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMassTargetAttribute = tAttr.create("MassTargetAttribute", "mtattr", MFnData::kString, defaultString6, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassTargetAttribute));

    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsVelocity, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityVector, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVelocityTargetAttribute, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsRadius, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadius, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusTargetAttribute, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsMass, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMassType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMassAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMass, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMassTargetAttribute, outDP ));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPParticleAttribute", "initialize", std::current_exception());
}
