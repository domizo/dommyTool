//
//  DMNormal.cpp
//  dommyTool
//
//  Created by yuzo on 5/23/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMNormal.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaValueValidation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"




const MTypeId DMNormal::typeId( ID_DM_NORMAL );



MObject DMNormal::inDM;

MObject DMNormal::aIsAddAttribute;
MObject DMNormal::aAttributeName;
MObject DMNormal::aNormalType;

MObject DMNormal::aFaceGroup;
MObject DMNormal::aIsConcernCorner;
MObject DMNormal::aIsGroupEdgeOnly;

MObject DMNormal::aReverse;

MObject DMNormal::outDM;


void *DMNormal::creator() {
    return ::dommy::maya::createObject("DMNormal", [&]() {
        return new DMNormal();
    });
}



MStatus DMNormal::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* inputMeshes = nullptr;
    MStatus stat = getValidInputDM(inDM, data, inputMeshes);
    if (!stat) return stat;
    if (!inputMeshes) return MS::kFailure;
    MObject outputObject;
    DMeshData* outputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes));
    if (!outputMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
    if (outputMeshes->numberOfMesh() != 0U) {
        const MDataHandle addHandle = data.inputValue(aIsAddAttribute, &stat);
        if (!stat) return stat;
        const bool isAdd = addHandle.asBool();
        const MDataHandle nameHandle = data.inputValue(aAttributeName, &stat);
        if (!stat) return stat;
        const std::string attrName = nameHandle.asString().asChar();
        const MDataHandle typeHandle = data.inputValue(aNormalType, &stat);
        if (!stat) return stat;
        const short normalType = typeHandle.asShort();
        const MDataHandle groupHandle = data.inputValue(aFaceGroup, &stat);
        if (!stat) return stat;
        const std::string faceGrpName = groupHandle.asString().asChar();
        const MDataHandle cornerHandle = data.inputValue(aIsConcernCorner, &stat);
        if (!stat) return stat;
        const bool isCorner = cornerHandle.asBool();
        const MDataHandle edgeHandle = data.inputValue(aIsGroupEdgeOnly, &stat);
        if (!stat) return stat;
        const bool isGroupEdge = edgeHandle.asBool();
        const MDataHandle reverseHandle = data.inputValue(aReverse, &stat);
        if (!stat) return stat;
        const bool reverse = reverseHandle.asBool();
        if (normalType < 0 || normalType > 2) return MS::kInvalidParameter;

        if (reverse) {
            for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes(); ++mesh)
                if (!(*mesh)->reverseWinding()) return MS::kFailure;
        }
        if (isAdd && !attrName.empty()) {
            checkDMeshPointAttribute(attrName, *outputMeshes, true);
            for (auto mesh = outputMeshes->beginMeshes(); mesh != outputMeshes->endMeshes(); ++mesh) {
                MObject object;
                DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(**mesh, object));
                if (normalType == 1) {
                    // Face-vertex iteration order matches the mesh connectivity order.
                    const std::size_t count = (*mesh)->numberOfConnections();
                    DVec3DataList normals(count, DVec3Data(0, 0, 0));
                    MItMeshFaceVertex iterator(object, &stat);
                    if (!stat) return stat;
                    for (std::size_t i = 0U; i < count; ++i) {
                        const bool done = iterator.isDone(&stat);
                        if (!stat) return stat;
                        if (done) return MS::kFailure;
                        MVector normal;
                        DOMMY_RETURN_IF_FAILURE(iterator.getNormal(normal));
                        if (!dommy::maya::isFloatRepresentableVector(normal)) return MS::kFailure;
                        normals[i] = MVToDP(normal);
                        DOMMY_RETURN_IF_FAILURE(iterator.next());
                    }
                    const bool done = iterator.isDone(&stat);
                    if (!stat) return stat;
                    if (!done || !(*mesh)->addCompAttr(D_COMP_PER_VTX, attrName,
                            DPVec3fAttr(normals, DVec3Data(0, 0, 0)))) return MS::kFailure;
                } else if (normalType == 2 &&
                           (faceGrpName.empty() || !(*mesh)->isExistCompGrp(D_COMP_FACE, faceGrpName))) {
                    DVec3DataList normals((*mesh)->numFace(), DVec3Data(0, 0, 0));
                    MItMeshPolygon iterator(object, &stat);
                    if (!stat) return stat;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshPolygons(iterator, normals.size(),
                        [&](MItMeshPolygon& face) -> MStatus {
                            MStatus status;
                            const int id = face.index(&status);
                            if (!status) return status;
                            if (id < 0 || static_cast<std::size_t>(id) >= normals.size()) return MS::kFailure;
                            MVector normal;
                            DOMMY_RETURN_IF_FAILURE(face.getNormal(normal));
                            if (!dommy::maya::isFloatRepresentableVector(normal)) return MS::kFailure;
                            normals[static_cast<std::size_t>(id)] = MVToDP(normal);
                            return MS::kSuccess;
                        }));
                    if (!(*mesh)->addCompAttr(D_COMP_FACE, attrName,
                            DPVec3fAttr(normals, DVec3Data(0, 0, 0)))) return MS::kFailure;
                } else {
                    DVec3DataList normals((*mesh)->numberOfPoints(), DVec3Data(0, 0, 0));
                    DPGroupData::ConstPtr group;
                    if (normalType == 2) group = (*mesh)->getCompGrp(D_COMP_FACE, faceGrpName);
                    if (normalType == 2 && (!group || group->getGroupIds().size() !=
                            static_cast<std::size_t>((*mesh)->numFace()))) return MS::kFailure;
                    MItMeshPolygon faces(object, &stat);
                    if (!stat) return stat;
                    MItMeshVertex vertices(object, &stat);
                    if (!stat) return stat;
                    for (std::size_t i = 0U; i < normals.size(); ++i) {
                        const bool done = vertices.isDone(&stat);
                        if (!stat) return stat;
                        if (done) return MS::kFailure;
                        const int id = vertices.index(&stat);
                        if (!stat) return stat;
                        if (id < 0 || static_cast<std::size_t>(id) >= normals.size()) return MS::kFailure;
                        MVector normal;
                        if (!group) {
                            DOMMY_RETURN_IF_FAILURE(vertices.getNormal(normal));
                        } else {
                            MIntArray connected;
                            DOMMY_RETURN_IF_FAILURE(vertices.getConnectedFaces(connected));
                            unsigned int grouped = 0U;
                            for (unsigned int f = 0U; f < connected.length(); ++f) {
                                const int faceId = connected[f];
                                if (faceId < 0 || static_cast<std::size_t>(faceId) >= group->getGroupIds().size())
                                    return MS::kFailure;
                                if (group->getGroupIds()[static_cast<std::size_t>(faceId)]) {
                                    int previous = 0;
                                    DOMMY_RETURN_IF_FAILURE(faces.setIndex(faceId, previous));
                                    MVector faceNormal;
                                    DOMMY_RETURN_IF_FAILURE(faces.getNormal(faceNormal));
                                    if (!dommy::maya::isFloatRepresentableVector(faceNormal)) return MS::kFailure;
                                    normal += faceNormal;
                                    ++grouped;
                                }
                            }
                            if ((isCorner && connected.length() == 3U) ||
                                (isGroupEdge && grouped == connected.length())) normal = MVector(0, 0, 0);
                            else normal = normal.normal();
                        }
                        if (!dommy::maya::isFloatRepresentableVector(normal)) return MS::kFailure;
                        normals[static_cast<std::size_t>(id)] = MVToDP(normal);
                        DOMMY_RETURN_IF_FAILURE(vertices.next());
                    }
                    const bool done = vertices.isDone(&stat);
                    if (!stat) return stat;
                    if (!done) return MS::kFailure;
                    (*mesh)->addAttrWithValues(attrName, normals);
                }
            }
        }
    }
    MDataHandle output = data.outputValue(outDM, &stat);
    if (!stat) return stat;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMNormal")







MStatus DMNormal::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;
	
	// ----------------------- INPUTS --------------------------
	inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    
    aIsAddAttribute = nAttr.create( "IsAddAttribute", "isad", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAddAttribute));
    
    const MObject aAttributeNameDefault = fnStringData.create("N", &stat);
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    aAttributeName = tAttr.create("AttributeName", "attr", MFnData::kString, aAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
    
	
    MFnEnumAttribute normEnumFn;
    aNormalType = normEnumFn.create( "NormalType", "nrmty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(normEnumFn.addField( "Point Normal", 0 ));
    DOMMY_RETURN_IF_FAILURE(normEnumFn.addField( "Per Vertex Normal", 1 ));
    DOMMY_RETURN_IF_FAILURE(normEnumFn.addField( "Face Normal", 2 ));
    DOMMY_RETURN_IF_FAILURE(normEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalType));
	
	
    aIsConcernCorner = nAttr.create( "IsConcernCorner", "isconcor", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsConcernCorner));
    
    
    aIsGroupEdgeOnly = nAttr.create( "IsGroupEdgeOnly", "isgrpedg", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsGroupEdgeOnly));
    
    
    
	
    const MObject aFaceGroupDefault = fnStringData.create("outer", &stat);
    
    
    
	
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    
	
    aFaceGroup = tAttr.create("FaceGroup", "fgrp", MFnData::kString, aFaceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroup));
    
    
    aReverse = nAttr.create( "Reverse", "rev", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aReverse));
    
    
	//create custom data
	outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsAddAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalType, outDM ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceGroup, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsConcernCorner, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsGroupEdgeOnly, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aReverse, outDM ));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMNormal", "initialize", std::current_exception());
}
