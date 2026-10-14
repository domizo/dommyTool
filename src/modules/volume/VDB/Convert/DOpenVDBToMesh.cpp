//
//  DOpenVDBToMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBToMesh.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMeshMergePlan.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include <openvdb/tools/VolumeToMesh.h>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MArrayDataBuilder.h>

#include <cmath>
#include <string>
#include <vector>

#include "DOpenVDBGeoUtil.h"

#include "DMeshDataHeaders.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"

MObject DOpenVDBToMesh::aVdbInput;
MObject DOpenVDBToMesh::aIsovalue;
MObject DOpenVDBToMesh::aAdaptivity;
MObject DOpenVDBToMesh::aVdbAllGridNames;
MObject DOpenVDBToMesh::aVdbSelectedGridNames;
MObject DOpenVDBToMesh::outMesh;
MObject DOpenVDBToMesh::outDM;

const MTypeId DOpenVDBToMesh::typeId(ID_VDB_DOPENVDB_TO_MESH);

namespace {

typedef std::vector<openvdb::GridBase::ConstPtr> GridList;

struct ConversionInputs {
    GridList grids;
    float isovalue;
    float adaptivity;
};

MStatus readInputVdb(MDataBlock& data, const DOpenVDBData*& inputVdb) {
    return getValidInputVDB(DOpenVDBToMesh::aVdbInput, data, inputVdb);
}

MStatus readConversionInputs(MDataBlock& data, const DOpenVDBData& inputVdb,
                             ConversionInputs& inputs) {
    MStatus status;
    const MDataHandle selectionHandle =
        data.inputValue(DOpenVDBToMesh::aVdbSelectedGridNames, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    const std::string selection = selectionHandle.asString().asChar();
    getGrids(inputs.grids, inputVdb, selection);

    const MDataHandle isovalueHandle = data.inputValue(DOpenVDBToMesh::aIsovalue, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    inputs.isovalue = isovalueHandle.asFloat();

    const MDataHandle adaptivityHandle = data.inputValue(DOpenVDBToMesh::aAdaptivity, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    inputs.adaptivity = adaptivityHandle.asFloat();
    if (!std::isfinite(inputs.isovalue) ||
        !std::isfinite(inputs.adaptivity) ||
        inputs.adaptivity < 0.0f || inputs.adaptivity > 1.0f) {
        MGlobal::displayError(
            "DVDBToMesh requires a finite isovalue and adaptivity in [0, 1].");
        return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

MStatus convertToMayaMeshes(const ConversionInputs& inputs, std::vector<MObject>& meshes) {
    meshes.clear();
    meshes.reserve(inputs.grids.size());

    try {
        dommy::vdb::MeshingBudget budget;
        if (!dommy::vdb::planMeshingGrids(inputs.grids, budget)) return MS::kInvalidParameter;
        openvdb::tools::VolumeToMesh mesher(inputs.isovalue, inputs.adaptivity);
        for (GridList::const_iterator it = inputs.grids.begin(); it != inputs.grids.end(); ++it) {
            if (!*it) {
                continue;
            }

            VDBToMayaMesh converter(mesher, budget);
            if (processTypedScalarGrid(*it, converter)) {
                DOMMY_RETURN_IF_FAILURE(converter.status);
                if (converter.mesh.isNull()) return MS::kFailure;
                meshes.push_back(converter.mesh);
            }
        }
    } catch (...) {
        // OpenVDB exceptions must not cross Maya's compute boundary.
        meshes.clear();
        return MS::kFailure;
    }

    return MS::kSuccess;
}

MStatus writeMeshArray(MDataBlock& data, const std::vector<MObject>& meshes) {
    MStatus status;
    MArrayDataHandle outputArray = data.outputArrayValue(DOpenVDBToMesh::outMesh, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MArrayDataBuilder builder(DOpenVDBToMesh::outMesh, static_cast<unsigned int>(meshes.size()),
                              &status);
    if (status != MS::kSuccess) {
        return status;
    }

    for (std::size_t index = 0; index < meshes.size(); ++index) {
        MDataHandle element = builder.addElement(static_cast<unsigned int>(index), &status);
        if (status != MS::kSuccess) {
            return status;
        }
        DOMMY_RETURN_IF_FAILURE(element.set(meshes[index]));
    }

    status = outputArray.set(builder);
    if (status != MS::kSuccess) {
        return status;
    }
    return outputArray.setAllClean();
}

MStatus writeDMeshData(MDataBlock& data, const MPlug& plug, const std::vector<MObject>& meshes) {
    MStatus status;
    MFnPluginData pluginData;
    pluginData.create(DMeshData::id, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    DMeshData* const outputData = dommy::maya::pluginDataPointer<DMeshData>(pluginData, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    if (outputData == nullptr) {
        return MS::kFailure;
    }

    if (meshes.size() > dommy::generation::kMaxGeneratedMeshCount) return MS::kInvalidParameter;
    DMeshData::DMeshList stagedMeshes;
    stagedMeshes.reserve(meshes.size());
    dommy::mesh::MeshMergePlan outputPlan;
    for (std::vector<MObject>::const_iterator it = meshes.begin();
         it != meshes.end(); ++it) {
        DMItem mesh;
        status = DMeshInternalData::createFromMayaObject(*it, mesh);
        if (status != MS::kSuccess) {
            return status;
        }
        if (!mesh || !dommy::mesh::appendMeshMergePlan(mesh->numberOfPoints(), mesh->numFace(),
                mesh->numberOfConnections(), mesh->numberOfUVs(), outputPlan) ||
            !dommy::mesh::validMeshOperationStorage(outputPlan)) {
            return MS::kFailure;
        }
        stagedMeshes.push_back(mesh);
    }
    if (!outputData->insert(stagedMeshes)) return MS::kFailure;
    outputData->updateBBox();

    MDataHandle output = data.outputValue(DOpenVDBToMesh::outDM, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            output, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}

} // namespace

void* DOpenVDBToMesh::creator() {
    return ::dommy::maya::createObject("DOpenVDBToMesh", [&]() {
        return new DOpenVDBToMesh();
    });
}

MStatus DOpenVDBToMesh::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbAllGridNames && plug != outMesh && plug != outDM) {
        return MS::kUnknownParameter;
    }

    const DOpenVDBData* inputVdb = nullptr;
    MStatus status = readInputVdb(data, inputVdb);
    if (status != MS::kSuccess) {
        return status;
    }

    if (plug == aVdbAllGridNames) {
        MDataHandle output = data.outputValue(aVdbAllGridNames, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        output.set(MString(getGridNames(*inputVdb).c_str()));
        return data.setClean(plug);
    }

    ConversionInputs inputs;
    status = readConversionInputs(data, *inputVdb, inputs);
    if (status != MS::kSuccess) {
        return status;
    }

    std::vector<MObject> meshes;
    status = convertToMayaMeshes(inputs, meshes);
    if (status != MS::kSuccess) {
        return status;
    }

    if (plug == outMesh) {
        return writeMeshArray(data, meshes);
    }
    return writeDMeshData(data, plug, meshes);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBToMesh")

MStatus DOpenVDBToMesh::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    // Setup input / output attributes

    aVdbInput = tAttr.create("vdbInput", "input", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    // Setup UI attributes
    aIsovalue = nAttr.create("isovalue", "iso", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsovalue));

    aAdaptivity = nAttr.create("adaptivity", "adapt", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAdaptivity));

    MFnStringData fnStringData;
    MObject defaultStringData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    aVdbAllGridNames =
        tAttr.create("vdbAllGridNames", "allgrids", MFnData::kString, defaultStringData, &stat);
    if (stat != MS::kSuccess)
        return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbAllGridNames));

    aVdbSelectedGridNames = tAttr.create("vdbSelectedGridNames", "selectedgrids", MFnData::kString,
                                         defaultStringData, &stat);
    if (stat != MS::kSuccess)
        return stat;
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    // Setup dependencies
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbAllGridNames));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsovalue, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdaptivity, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsovalue, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAdaptivity, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBToMesh", "initialize", std::current_exception());
}
