// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/PixarAnimationStudios/OpenSubdiv/blob/3b5c3d99d29ba078385013d15f8b2ebeac0daf45/tutorials/far/tutorial_1/far_tutorial_1.cpp
// Adapted portion: the Vertex interpolation adaptor; other routines are project-owned.

//
//  DMSubdivide.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/07.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMSubDivide.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DSubdivisionBudget.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DNumericValidation.h"


#include "DMeshDataHeaders.h"



#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>

#include <cstdio>
#include <cmath>
#include <cstddef>
#include <memory>



using namespace OpenSubdiv;

struct Vertex {
	
	// Minimal required interface ----------------------
	Vertex() : _position{0.0f, 0.0f, 0.0f} { }
	
	Vertex(Vertex const & src) {
		_position[0] = src._position[0];
		_position[1] = src._position[1];
		_position[2] = src._position[2];
	}
	
	void Clear( void * =0 ) {
		_position[0]=_position[1]=_position[2]=0.0f;
	}
	
	void AddWithWeight(Vertex const & src, float weight) {
		_position[0]+=weight*src._position[0];
		_position[1]+=weight*src._position[1];
		_position[2]+=weight*src._position[2];
	}
	
	// Public interface ------------------------------------
	void SetPosition(float x, float y, float z) {
		_position[0]=x;
		_position[1]=y;
		_position[2]=z;
	}
	
	const float * GetPosition() const {
		return _position;
	}
	
private:
	float _position[3];
};

namespace {

MStatus checkRefinedMeshCounts(MFnMesh& mesh, const dommy::mesh::SubdivisionBudget& budget)
{
    MStatus status;
    const int points = mesh.numVertices(&status);
    if (!status) return status;
    const int faces = mesh.numPolygons(&status);
    if (!status) return status;
    const int connections = mesh.numFaceVertices(&status);
    if (!status) return status;
    if (points <= 0 || faces <= 0 || connections <= 0 ||
        static_cast<std::size_t>(points) > budget.estimatedFinalVertexCount ||
        static_cast<std::size_t>(faces) > budget.estimatedFinalFaceCount ||
        static_cast<std::size_t>(connections) > budget.estimatedFinalFaceVertexCount)
        return MS::kFailure;
    return MS::kSuccess;
}

MStatus refineFaces(DMeshInternalData& mesh, MObject object,
                    const dommy::mesh::SubdivisionBudget& budget,
                    std::size_t triangleCount, int iteration, float creaseWeight,
                    Sdc::SchemeType type, const Sdc::Options& options)
{
    MStatus status;
    MFnMesh meshFn(object, &status);
    if (!status) return status;
    std::vector<int> faceCounts, faceConnects;
    if (type == Sdc::SCHEME_LOOP) {
        std::size_t connectionCount = 0U;
        if (!dommy::generation::checkedProduct(triangleCount, 3U,
                dommy::generation::kMaxWorkItemCount, connectionCount)) return MS::kInvalidParameter;
        MIntArray counts, ids;
        DOMMY_RETURN_IF_FAILURE(meshFn.getTriangles(counts, ids));
        if (counts.length() != static_cast<unsigned int>(mesh.numFace()) ||
            ids.length() != connectionCount) return MS::kFailure;
        std::size_t total = 0U;
        for (unsigned int i = 0U; i < counts.length(); ++i)
            if (counts[i] < 0 || !dommy::generation::checkedSum(total,
                    static_cast<std::size_t>(counts[i]), triangleCount, total)) return MS::kFailure;
        if (total != triangleCount || total == 0U) return MS::kFailure;
        faceCounts.assign(total, 3);
        faceConnects.reserve(connectionCount);
        for (unsigned int i = 0U; i < ids.length(); ++i) {
            if (ids[i] < 0 || static_cast<std::size_t>(ids[i]) >= mesh.numberOfPoints()) return MS::kFailure;
            faceConnects.push_back(ids[i]);
        }
    } else {
        // Take one checked snapshot, not a full array return inside each iteration.
        const MIntArray counts = mesh.getFaceList();
        const MIntArray ids = mesh.getConnectList();
        if (counts.length() != static_cast<unsigned int>(mesh.numFace()) ||
            ids.length() != mesh.numberOfConnections()) return MS::kFailure;
        faceCounts.reserve(counts.length());
        faceConnects.reserve(ids.length());
        for (unsigned int i = 0U; i < counts.length(); ++i) faceCounts.push_back(counts[i]);
        for (unsigned int i = 0U; i < ids.length(); ++i) faceConnects.push_back(ids[i]);
    }

    const int rawEdges = meshFn.numEdges(&status);
    if (!status) return status;
    std::size_t edgeCount = 0U, creaseIndexCount = 0U;
    if (!dommy::generation::checkedCount(rawEdges, mesh.numberOfConnections(), edgeCount) ||
        !dommy::generation::checkedProduct(edgeCount, 2U,
            dommy::generation::kMaxWorkItemCount, creaseIndexCount)) return MS::kInvalidParameter;
    std::vector<int> creasePairs(creaseIndexCount);
    std::vector<float> creaseWeights(edgeCount, creaseWeight);
    MItMeshEdge edges(object, &status);
    if (!status) return status;
    std::size_t offset = 0U;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(edges, edgeCount,
        [&](MItMeshEdge& edge) -> MStatus {
            for (int end = 0; end < 2; ++end) {
                MStatus stat;
                const int vertex = edge.index(end, &stat);
                if (!stat) return stat;
                if (vertex < 0 || static_cast<std::size_t>(vertex) >= mesh.numberOfPoints() ||
                    offset >= creasePairs.size()) return MS::kFailure;
                creasePairs[offset++] = vertex;
            }
            return MS::kSuccess;
        }));
    if (offset != creasePairs.size()) return MS::kFailure;

    typedef Far::TopologyDescriptor Descriptor;
    Descriptor descriptor;
    descriptor.numVertices = static_cast<int>(mesh.numberOfPoints());
    descriptor.numFaces = static_cast<int>(faceCounts.size());
    descriptor.numVertsPerFace = faceCounts.data();
    descriptor.vertIndicesPerFace = faceConnects.data();
    descriptor.numCreases = static_cast<int>(edgeCount);
    descriptor.creaseVertexIndexPairs = creasePairs.empty() ? nullptr : creasePairs.data();
    descriptor.creaseWeights = creaseWeights.empty() ? nullptr : creaseWeights.data();
    std::unique_ptr<Far::TopologyRefiner> refiner(
        Far::TopologyRefinerFactory<Descriptor>::Create(descriptor,
            Far::TopologyRefinerFactory<Descriptor>::Options(type, options)));
    if (!refiner) return MS::kFailure;
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(iteration));
    const int totalVertices = refiner->GetNumVerticesTotal();
    if (totalVertices <= 0 || static_cast<std::size_t>(totalVertices) < mesh.numberOfPoints() ||
        static_cast<std::size_t>(totalVertices) > budget.estimatedTotalWorkItems ||
        refiner->GetMaxLevel() != iteration) return MS::kFailure;
    std::vector<Vertex> vertices(static_cast<std::size_t>(totalVertices));
    const DPointList& positions = *mesh.getPositionPtrConst();
    for (std::size_t i = 0U; i < positions.size(); ++i)
        vertices[i].SetPosition(positions[i][0], positions[i][1], positions[i][2]);
    Far::PrimvarRefiner primvars(*refiner);
    std::size_t sourceOffset = 0U;
    for (int level = 1; level <= iteration; ++level) {
        const int sourceCount = refiner->GetLevel(level - 1).GetNumVertices();
        const int targetCount = refiner->GetLevel(level).GetNumVertices();
        std::size_t targetOffset = 0U, end = 0U;
        if (sourceCount <= 0 || targetCount <= 0 ||
            !dommy::generation::checkedSum(sourceOffset, static_cast<std::size_t>(sourceCount),
                vertices.size(), targetOffset) ||
            !dommy::generation::checkedSum(targetOffset, static_cast<std::size_t>(targetCount),
                vertices.size(), end)) return MS::kFailure;
        Vertex* source = vertices.data() + sourceOffset;
        Vertex* target = vertices.data() + targetOffset;
        primvars.Interpolate(level, source, target);
        sourceOffset = targetOffset;
    }
    const Far::TopologyLevel& last = refiner->GetLevel(iteration);
    const int pointCount = last.GetNumVertices(), faceCount = last.GetNumFaces();
    if (pointCount <= 0 || faceCount <= 0 ||
        static_cast<std::size_t>(pointCount) > budget.estimatedFinalVertexCount ||
        static_cast<std::size_t>(faceCount) > budget.estimatedFinalFaceCount ||
        static_cast<std::size_t>(pointCount) != vertices.size() - sourceOffset) return MS::kFailure;
    DPointList points(static_cast<std::size_t>(pointCount));
    for (std::size_t i = 0U; i < points.size(); ++i) {
        const float* position = vertices[sourceOffset + i].GetPosition();
        if (!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]))
            return MS::kFailure;
        points[i] = DPoint(position[0], position[1], position[2]);
    }
    MIntArray counts, ids;
    std::size_t connectionCount = 0U;
    for (int face = 0; face < faceCount; ++face) {
        const Far::ConstIndexArray corners = last.GetFaceVertices(face);
        if (corners.size() < 3 || !dommy::generation::checkedSum(connectionCount,
                static_cast<std::size_t>(corners.size()), budget.estimatedFinalFaceVertexCount,
                connectionCount)) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(counts.append(corners.size()));
        for (int corner = 0; corner < corners.size(); ++corner) {
            if (corners[corner] < 0 || corners[corner] >= pointCount) return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(ids.append(corners[corner]));
        }
    }
    if (counts.length() != static_cast<unsigned int>(faceCount) || ids.length() != connectionCount)
        return MS::kFailure;
    return mesh.setReplaceMeshData(points, counts, ids) ? MS::kSuccess : MS::kFailure;
}

MStatus refineEdges(DMeshInternalData& mesh, const dommy::mesh::SubdivisionBudget& budget,
                    short type, int iteration, int maximumIterations, double minimumLength,
                    const std::string& faceGroupName)
{
    DPGroupData::ConstPtr group;
    if (type == 1 && !faceGroupName.empty() && mesh.isExistCompGrp(D_COMP_FACE, faceGroupName)) {
        group = mesh.getCompGrp(D_COMP_FACE, faceGroupName);
        if (!group || group->getGroupIds().size() != static_cast<std::size_t>(mesh.numFace()))
            return MS::kFailure;
        if (!group->isExistActive()) return MS::kSuccess;
    }
    MObject object;
    DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmeshWithUV(mesh, object));
    MStatus status;
    MFnMesh meshFn(object, &status);
    if (!status) return status;
    const int steps = type == 0 ? 1 : maximumIterations;
    for (int step = 0; step < steps; ++step) {
        const int rawEdges = meshFn.numEdges(&status);
        if (!status) return status;
        std::size_t edgeCount = 0U;
        if (!dommy::generation::checkedCount(rawEdges, budget.estimatedFinalFaceVertexCount, edgeCount))
            return MS::kInvalidParameter;
        MIntArray selected;
        if (type == 0) {
            DOMMY_RETURN_IF_FAILURE(selected.setLength(static_cast<unsigned int>(edgeCount)));
            if (selected.length() != edgeCount) return MS::kFailure;
            for (unsigned int edge = 0U; edge < selected.length(); ++edge) selected[edge] = static_cast<int>(edge);
        } else {
            MItMeshEdge edges(object, &status);
            if (!status) return status;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(edges, edgeCount,
                [&](MItMeshEdge& edge) -> MStatus {
                    MStatus stat;
                    const int id = edge.index(&stat);
                    if (!stat) return stat;
                    if (id < 0 || static_cast<std::size_t>(id) >= edgeCount) return MS::kFailure;
                    if (group) {
                        MIntArray faces;
                        const int faceCount = edge.getConnectedFaces(faces, &stat);
                        if (!stat) return stat;
                        if (faceCount < 0 || faces.length() != static_cast<unsigned int>(faceCount))
                            return MS::kFailure;
                        bool inside = false, outside = false;
                        for (unsigned int f = 0U; f < faces.length(); ++f) {
                            if (faces[f] < 0 || static_cast<std::size_t>(faces[f]) >= group->getGroupIds().size())
                                return MS::kFailure;
                            if (group->getGroupIds()[static_cast<std::size_t>(faces[f])]) inside = true;
                            else outside = true;
                        }
                        const bool boundary = edge.onBoundary(&stat);
                        if (!stat) return stat;
                        if (!(inside && outside) && !(boundary && inside)) return MS::kSuccess;
                    }
                    double length = 0.0;
                    DOMMY_RETURN_IF_FAILURE(edge.getLength(length));
                    if (!std::isfinite(length) || length < 0.0) return MS::kFailure;
                    if (length >= minimumLength) DOMMY_RETURN_IF_FAILURE(selected.append(id));
                    return MS::kSuccess;
                }));
            if (selected.length() == 0U) break;
        }
        DOMMY_RETURN_IF_FAILURE(meshFn.subdivideEdges(selected, type == 0 ? iteration : 1));
        if (type == 1) DOMMY_RETURN_IF_FAILURE(meshFn.updateSurface());
        DOMMY_RETURN_IF_FAILURE(checkRefinedMeshCounts(meshFn, budget));
    }
    DOMMY_RETURN_IF_FAILURE(checkRefinedMeshCounts(meshFn, budget));
    return mesh.setReplaceMeshData(object) ? MS::kSuccess : MS::kFailure;
}

} // namespace

const MTypeId DMSubDivide::typeId( ID_DM_SUBDIVIDE );


MObject DMSubDivide::inDM;

MObject DMSubDivide::aSubdivClassType;
//MObject DMSubDivide::aMethodType;
MObject DMSubDivide::aIteration;
MObject DMSubDivide::aWeight;

MObject DMSubDivide::aSchemeType;
MObject DMSubDivide::aInterpolationType;
MObject DMSubDivide::aFVarLinearInterpolationType;
MObject DMSubDivide::aCreasingMethod;
MObject DMSubDivide::aTriangleSubdivision;

//edge option
MObject DMSubDivide::aEdgeSamplingType;
MObject DMSubDivide::aEdgeMaxIteration;
MObject DMSubDivide::aEdgeMinLength;
MObject DMSubDivide::aFaceGroup;

MObject DMSubDivide::outDM;



void *DMSubDivide::creator()
{
    return ::dommy::maya::createObject("DMSubDivide", [&]() {
        return new DMSubDivide();
    });
}





MStatus DMSubDivide::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (!input) return MS::kFailure;
    MStatus stat;
    const std::size_t meshCount = input->numberOfMesh();
    int iteration = 0, maxIteration = 1;
    short classtype = D_COMP_FACE, scheme = 1, edgeType = 0;
    double minLength = 1.0;
    float creaseWeight = 0.0f;
    std::string faceGroupName;
    Sdc::SchemeType type = Sdc::SCHEME_CATMARK;
    Sdc::Options options;
    bool active = false;
    if (meshCount != 0U) {
        const MDataHandle classtypeHandle = data.inputValue(aSubdivClassType, &stat);
        if (!stat) return stat;
        classtype = classtypeHandle.asShort();
        const MDataHandle iterationHandle = data.inputValue(aIteration, &stat);
        if (!stat) return stat;
        iteration = iterationHandle.asInt();
        const MDataHandle weightHandle = data.inputValue(aWeight, &stat);
        if (!stat) return stat;
        const float weight = weightHandle.asFloat();
        const MDataHandle schemeHandle = data.inputValue(aSchemeType, &stat);
        if (!stat) return stat;
        scheme = schemeHandle.asShort();
        const MDataHandle interpTypeHandle = data.inputValue(aInterpolationType, &stat);
        if (!stat) return stat;
        const short interpType = interpTypeHandle.asShort();
        const MDataHandle fVarLinearInterpolationTypeHandle = data.inputValue(aFVarLinearInterpolationType, &stat);
        if (!stat) return stat;
        const short fVarLinearInterpolationType = fVarLinearInterpolationTypeHandle.asShort();
        const MDataHandle creasingMethodHandle = data.inputValue(aCreasingMethod, &stat);
        if (!stat) return stat;
        const short creasingMethod = creasingMethodHandle.asShort();
        const MDataHandle triangleSubdivisionHandle = data.inputValue(aTriangleSubdivision, &stat);
        if (!stat) return stat;
        const short triangleSubdivision = triangleSubdivisionHandle.asShort();
        if ((classtype != D_COMP_FACE && classtype != D_COMP_EDGE) ||
            iteration < 0 || iteration > dommy::mesh::kMaxSubdivisionLevel ||
            !std::isfinite(weight) || weight < 0.0f) return MS::kInvalidParameter;
        if (classtype == D_COMP_FACE && iteration > 0) {
            if (!dommy::numeric::checkedNumericCast(static_cast<long double>(weight) * iteration, creaseWeight))
                return MS::kInvalidParameter;
            switch (scheme) {
                case 0:
                    type= OpenSubdiv::Sdc::SCHEME_BILINEAR;
                    break;
                case 1:
                    type= OpenSubdiv::Sdc::SCHEME_CATMARK;
                    break;
                case 2:
                    type= OpenSubdiv::Sdc::SCHEME_LOOP;
                    break;
                default:
                    return MS::kFailure;
            }
            switch (interpType) {
                case 0:
                    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_NONE);
                    break;
                case 1:
                    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
                    break;
                case 2:
                    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
                    break;
                default:
                    return MS::kFailure;
            }
            switch (fVarLinearInterpolationType) {
                case 0:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_NONE);
                    break;
                case 1:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
                    break;
                case 3:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_PLUS1);
                    break;
                case 4:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_PLUS2);
                    break;
                case 5:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_BOUNDARIES);
                    break;
                case 6:
                    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_ALL);
                    break;
                default:
                    return MS::kFailure;
            }
            switch (creasingMethod) {
                case 0:
                    options.SetCreasingMethod(Sdc::Options::CREASE_UNIFORM);
                    break;
                case 1:
                    options.SetCreasingMethod(Sdc::Options::CREASE_CHAIKIN);
                    break;
                default:
                    return MS::kFailure;
            }
            switch (triangleSubdivision) {
                case 0:
                    options.SetTriangleSubdivision(Sdc::Options::TRI_SUB_CATMARK);
                    break;
                case 1:
                    options.SetTriangleSubdivision(Sdc::Options::TRI_SUB_SMOOTH);
                    break;
                default:
                    return MS::kFailure;
            }
            active = true;
        } else if (classtype == D_COMP_EDGE) {
            const MDataHandle edgeTypeHandle = data.inputValue(aEdgeSamplingType, &stat);
            if (!stat) return stat;
            edgeType = edgeTypeHandle.asShort();
            const MDataHandle maxIterationHandle = data.inputValue(aEdgeMaxIteration, &stat);
            if (!stat) return stat;
            maxIteration = maxIterationHandle.asInt();
            const MDataHandle minLengthHandle = data.inputValue(aEdgeMinLength, &stat);
            if (!stat) return stat;
            minLength = minLengthHandle.asDouble();
            const MDataHandle groupHandle = data.inputValue(aFaceGroup, &stat);
            if (!stat) return stat;
            faceGroupName = groupHandle.asString().asChar();
            if ((edgeType != 0 && edgeType != 1) || maxIteration < 1 ||
                maxIteration > dommy::mesh::kMaxSubdivisionLevel ||
                !std::isfinite(minLength) || minLength <= 0.0) return MS::kInvalidParameter;
            active = edgeType == 1 || iteration > 0;
        }
    }

    std::vector<dommy::mesh::SubdivisionBudget> budgets(meshCount);
    std::vector<MObject> loopObjects(meshCount);
    std::vector<std::size_t> triangleCounts(meshCount, 0U);
    if (active) {
        // Bound all input staging before creating native meshes or acquiring triangles.
        dommy::mesh::MeshMergePlan inputBudget;
        for (auto mesh = input->beginMeshes(); mesh != input->endMeshes(); ++mesh)
            if (!dommy::mesh::appendMeshMergePlan((*mesh)->numberOfPoints(), (*mesh)->numFace(),
                    (*mesh)->numberOfConnections(), (*mesh)->numberOfUVs(), inputBudget) ||
                !dommy::mesh::validMeshOperationStorage(inputBudget)) return MS::kInvalidParameter;
        dommy::mesh::SubdivisionBudget total;
        std::size_t index = 0U;
        for (auto mesh = input->beginMeshes(); mesh != input->endMeshes(); ++mesh, ++index) {
            std::size_t faces = static_cast<std::size_t>((*mesh)->numFace());
            std::size_t connections = (*mesh)->numberOfConnections();
            if (classtype == D_COMP_FACE && scheme == 2) {
                DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(**mesh, loopObjects[index]));
                MFnMesh meshFn(loopObjects[index], &stat);
                if (!stat) return stat;
                std::size_t sourceFaces = 0U;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshTriangles(
                    meshFn, loopObjects[index], sourceFaces, triangleCounts[index]));
                if (sourceFaces != faces || triangleCounts[index] == 0U ||
                    !dommy::generation::checkedProduct(triangleCounts[index], 3U,
                        dommy::generation::kMaxWorkItemCount, connections)) return MS::kFailure;
                faces = triangleCounts[index];
            }
            const int levels = classtype == D_COMP_EDGE && edgeType == 1 ? maxIteration : iteration;
            if (!dommy::mesh::checkedSubdivisionBudget(levels, (*mesh)->numberOfPoints(), faces, connections,
                    dommy::generation::kMaxGeneratedPointCount, dommy::generation::kMaxGeneratedFaceCount,
                    dommy::generation::kMaxWorkItemCount, budgets[index]) ||
                !dommy::mesh::appendSubdivisionBudget(budgets[index], total)) return MS::kInvalidParameter;
        }
    }
    MObject outputObject;
    DMeshData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    if (active) {
        std::size_t index = 0U;
        for (auto mesh = output->beginMeshes(); mesh != output->endMeshes(); ++mesh, ++index) {
            if (index >= budgets.size()) return MS::kFailure;
            if (classtype == D_COMP_FACE) {
                MObject object = loopObjects[index];
                if (scheme != 2) DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(**mesh, object));
                DOMMY_RETURN_IF_FAILURE(refineFaces(**mesh, object, budgets[index], triangleCounts[index],
                    iteration, creaseWeight, type, options));
            } else {
                DOMMY_RETURN_IF_FAILURE(refineEdges(**mesh, budgets[index], edgeType,
                    iteration, maxIteration, minLength, faceGroupName));
            }
        }
        if (index != meshCount) return MS::kFailure;
        output->updateBBox();
    }
    if (!output->hasValidMeshTopology()) return MS::kFailure;
    MDataHandle handle = data.outputValue(outDM, &stat);
    if (!stat) return stat;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        handle, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMSubDivide")



MStatus DMSubDivide::initialize() try {
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

    MFnEnumAttribute subduvClassEnumFn;
    aSubdivClassType = subduvClassEnumFn.create( "SubdivClassType", "sbdvclty", D_COMP_FACE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(subduvClassEnumFn.addField( "Face", D_COMP_FACE ));
    DOMMY_RETURN_IF_FAILURE(subduvClassEnumFn.addField( "Edge", D_COMP_EDGE ));
    DOMMY_RETURN_IF_FAILURE(subduvClassEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubdivClassType));

    aIteration = nAttr.create( "Iteration", "iter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(dommy::mesh::kMaxSubdivisionLevel));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIteration));

    aWeight = nAttr.create( "Weight", "wei", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    MFnEnumAttribute SchemeEnumFn;
    aSchemeType = SchemeEnumFn.create( "SchemeType", "shmty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(SchemeEnumFn.addField( "OpenSubdiv Bilinear", 0 ));
    DOMMY_RETURN_IF_FAILURE(SchemeEnumFn.addField( "OpenSubdiv Catmull-Clark", 1 ));
    DOMMY_RETURN_IF_FAILURE(SchemeEnumFn.addField( "OpenSubdiv Loop", 2 ));
    DOMMY_RETURN_IF_FAILURE(SchemeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSchemeType));

    MFnEnumAttribute interpEnumFn;
    aInterpolationType = interpEnumFn.create( "InterpolationType", "itpty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(interpEnumFn.addField( "None", 0 ));
    DOMMY_RETURN_IF_FAILURE(interpEnumFn.addField( "Edge Only", 1 ));
    DOMMY_RETURN_IF_FAILURE(interpEnumFn.addField( "Edge And Corner", 2 ));
    DOMMY_RETURN_IF_FAILURE(interpEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInterpolationType));

    MFnEnumAttribute fVerInterpEnumFn;
    aFVarLinearInterpolationType = fVerInterpEnumFn.create( "FVarLinearInterpolationType", "fvitpty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "None", 0 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "Corners Only", 1 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "Corners Plus1", 3 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "Corners Plus2", 4 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "Boundaries", 5 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.addField( "All", 6 ));
    DOMMY_RETURN_IF_FAILURE(fVerInterpEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFVarLinearInterpolationType));

    MFnEnumAttribute CreasingEnumFn;
    aCreasingMethod = CreasingEnumFn.create( "CreasingMethod", "crmt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(CreasingEnumFn.addField( "Uniform", 0 ));
    DOMMY_RETURN_IF_FAILURE(CreasingEnumFn.addField( "Chaikin", 1 ));
    DOMMY_RETURN_IF_FAILURE(CreasingEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCreasingMethod));

    MFnEnumAttribute TriSubEnumFn;
    aTriangleSubdivision = TriSubEnumFn.create( "TriangleSubdivision", "trsbd", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(TriSubEnumFn.addField( "Catmark", 0 ));
    DOMMY_RETURN_IF_FAILURE(TriSubEnumFn.addField( "Smooth", 1 ));
    DOMMY_RETURN_IF_FAILURE(TriSubEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTriangleSubdivision));
    MFnEnumAttribute edgeEnumFn;
    aEdgeSamplingType = edgeEnumFn.create( "EdgeSamplingType", "edgsmpty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.addField( "Simple", 0 ));
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.addField( "Min Length Cutting", 1 ));
    DOMMY_RETURN_IF_FAILURE(edgeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeSamplingType));

    aEdgeMaxIteration = nAttr.create( "EdgeMaxIteration", "emaxitr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(dommy::mesh::kMaxSubdivisionLevel));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeMaxIteration));

    aEdgeMinLength = nAttr.create( "EdgeMinLength", "eminlen", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeMinLength));

    const MObject aFaceGroupDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFaceGroup = tAttr.create("FaceGroup", "fcattr", MFnData::kString, aFaceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroup));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSubdivClassType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIteration, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWeight, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSchemeType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInterpolationType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFVarLinearInterpolationType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCreasingMethod, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTriangleSubdivision, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeSamplingType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeMaxIteration, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeMinLength, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceGroup, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMSubDivide", "initialize", std::current_exception());
}
