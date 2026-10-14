//
//  DMMakePiece.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMMakePiece.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshMergePlan.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"

#include "dommyMath.h"
#include "DDataOp.h"
#include <algorithm>
#include <utility>


const MTypeId DMMakePiece::typeId( ID_DM_MAKE_PIECE );


MObject DMMakePiece::inDM;

MObject DMMakePiece::aPieceGroupName;
MObject DMMakePiece::outDM;




void *DMMakePiece::creator()
{
    return ::dommy::maya::createObject("DMMakePiece", [&]() {
        return new DMMakePiece();
    });
}



MStatus DMMakePiece::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    MStatus status = getValidInputDM(inDM, data, input);
    if (!status) return status;

    const MDataHandle groupHandle = data.inputValue(aPieceGroupName, &status);
    if (!status) return status;
    const std::string pieceName = groupHandle.asString().asChar();

    DMeshData::DMeshList meshes;
    // Preserve the historical empty output for an empty group expression.
    if (!pieceName.empty() && input->numberOfMesh() != 0U) {
        std::vector<std::string> matchedNames;
        getDMeshMatchedGroups(*input, pieceName, matchedNames);
        std::vector<bool> contained(input->numberOfMesh(), false);
        std::vector<std::vector<const DMeshInternalData*>> batches;
        dommy::mesh::MeshMergePlan outputBudget;
        for (const std::string& name : matchedNames) {
            DPGroupData::ConstPtr group = input->getGrpPtr(name);
            if (!group) return MS::kInvalidParameter;
            DPIndexList indices;
            group->getActiveIndexList(indices);
            if (indices.empty()) continue;
            std::vector<const DMeshInternalData*> sources;
            sources.reserve(indices.size());
            for (const std::size_t index : indices) {
                if (index >= contained.size()) return MS::kInvalidParameter;
                ConstDMItem mesh = input->getMeshPtrConst(index);
                if (!mesh || !dommy::mesh::appendMeshMergePlan(
                        mesh->numberOfPoints(), static_cast<std::size_t>(mesh->numFace()),
                        mesh->numberOfConnections(), mesh->numberOfUVs(), outputBudget)) {
                    return MS::kInvalidParameter;
                }
                contained[index] = true;
                sources.push_back(mesh.get());
            }
            batches.push_back(std::move(sources));
        }
        for (std::size_t index = 0U; index < contained.size(); ++index) {
            if (contained[index]) continue;
            ConstDMItem mesh = input->getMeshPtrConst(index);
            if (!mesh || !dommy::mesh::appendMeshMergePlan(
                    mesh->numberOfPoints(), static_cast<std::size_t>(mesh->numFace()),
                    mesh->numberOfConnections(), mesh->numberOfUVs(), outputBudget)) {
                return MS::kInvalidParameter;
            }
        }

        // Groups may overlap: admit the complete retained output before any
        // merge/copy, not just each individually valid piece.
        if (!dommy::mesh::validMeshOperationStorage(outputBudget))
            return MS::kInvalidParameter;

        meshes.reserve(batches.size() +
            static_cast<std::size_t>(std::count(contained.begin(), contained.end(), false)));
        for (const auto& sources : batches) {
            DMItem merged;
            status = DMeshInternalData::createMerged(sources, merged);
            if (!status) return status;
            meshes.push_back(merged);
        }
        for (std::size_t index = 0U; index < contained.size(); ++index) {
            if (!contained[index]) {
                meshes.push_back(input->getMeshPtrConst(index)->copyNewMesh());
            }
        }
    }

    MObject outputObject;
    DMeshData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, output);
    if (!status) return status;
    if (!output || !output->insert(meshes)) return MS::kFailure;
    MDataHandle handle = data.outputValue(outDM, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(handle, outputObject, *output);
    if (!status) return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMMakePiece")
            





MStatus DMMakePiece::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    const MObject aPieceGroupNameDefault = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceGroupName = tAttr.create("PieceGroupName", "picgrpnm", MFnData::kString, aPieceGroupNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceGroupName));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPieceGroupName, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMMakePiece", "initialize", std::current_exception());
}
