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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/GeometryUtil.h

//
//  DOpenVDBGeoUtil.hpp
//  dommyTool
//
//  Created by yuzo on 12/17/16.
//
//


#pragma once

#include "DMayaHeaders.h"

#include <openvdb/openvdb.h>
#include <openvdb/tools/MeshToVolume.h> // for openvdb::tools::MeshToVoxelEdgeData
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tree/LeafManager.h>
#include <openvdb/util/Util.h>

#include <vector>
#include <limits>
#include <stdexcept>

#include "DNodeOperation.h"
#include "DOpenVDBUtil.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "../Edit/DOpenVDBMeshingPolicy.h"




#pragma mark convert polygon method

MStatus DOpenVDBMakePointList(MObject meshObj,
							  MDataBlock& data,
							  const openvdb::math::Transform& transform,
							  std::vector<openvdb::Vec3s> &pointList);

MStatus DOpenVDBMakePointList(const MObject& meshObject,
							  const openvdb::math::Transform& transform,
							  std::vector<openvdb::Vec3s>& pointList);

MStatus DOpenVDBMakePrimList(MObject meshObj,
							  MDataBlock& data,
							  std::vector<openvdb::Vec4I> &primList);

MStatus DOpenVDBMakePrimList(const MObject& meshObject,
							  std::vector<openvdb::Vec4I>& primList,
                              std::vector<int>* sourceFaces = nullptr);





namespace {
// Callers preflight the complete selected grid set before invoking this adapter.
class VDBToMayaMesh {
public:
    MObject mesh;
    MStatus status = MS::kFailure;
    VDBToMayaMesh(openvdb::tools::VolumeToMesh& mesher, dommy::vdb::MeshingBudget& budget)
        : mMesher(mesher), mBudget(budget) {}
    const std::vector<bool>& getSurfaceGroup() const { return surfaceGroup; }
    const std::vector<bool>& getInteriorGroup() const { return interiorGroup; }
    const std::vector<bool>& getSeamGroup() const { return seamGroup; }
    const std::vector<bool>& getSeamPointGroup() const { return seamPointGroup; }

    template<typename GridType> void operator()(typename GridType::ConstPtr grid) {
        mesh = MObject::kNullObj;
        status = convert<GridType>(grid);
    }
private:
    template<typename GridType> MStatus convert(typename GridType::ConstPtr grid) {
        if (!grid || grid->activeVoxelCount() > dommy::generation::kMaxGeneratedPointCount)
            return MS::kInvalidParameter;
        mMesher(*grid);
        const std::size_t pointCount = mMesher.pointListSize();
        const std::size_t poolCount = mMesher.polygonPoolListSize();
        if (pointCount > dommy::generation::kMaxGeneratedPointCount ||
            poolCount > dommy::generation::kMaxGeneratedFaceCount ||
            (pointCount && !mMesher.pointList().get()) ||
            (poolCount && !mMesher.polygonPoolList().get())) return MS::kFailure;
        const auto& flags = mMesher.pointFlags();
        if (!flags.empty() && flags.size() != pointCount) return MS::kFailure;
        std::size_t faces = 0, connects = 0;
        for (std::size_t pool = 0; pool < poolCount; ++pool) {
            const auto& polygons = mMesher.polygonPoolList()[pool];
            std::size_t count = 0, quads = 0, triangles = 0;
            if (!dommy::generation::checkedSum(polygons.numQuads(), polygons.numTriangles(),
                    dommy::generation::kMaxGeneratedFaceCount, count) ||
                !dommy::generation::checkedSum(faces, count, dommy::generation::kMaxGeneratedFaceCount, faces) ||
                !dommy::memory::checkedMultiply(polygons.numQuads(), 4U, quads) ||
                !dommy::memory::checkedMultiply(polygons.numTriangles(), 3U, triangles) ||
                !dommy::memory::checkedAdd(connects, quads, connects) ||
                !dommy::memory::checkedAdd(connects, triangles, connects)) return MS::kFailure;
        }
        if (!mBudget.addOutput(pointCount, faces, connects)) return MS::kInvalidParameter;
        MFloatPointArray points;
        MIntArray counts, indices;
        DOMMY_RETURN_IF_FAILURE(points.setLength(static_cast<unsigned int>(pointCount)));
        DOMMY_RETURN_IF_FAILURE(counts.setLength(static_cast<unsigned int>(faces)));
        DOMMY_RETURN_IF_FAILURE(indices.setLength(static_cast<unsigned int>(connects)));
        std::vector<bool> surface(faces, false), interior(faces, false), seam(faces, false), seamPoints(pointCount, false);
        for (std::size_t i = 0; i < pointCount; ++i) {
            const auto& p = mMesher.pointList()[i];
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(p[axis]) || std::fabs(p[axis]) > 1e12) return MS::kFailure;
            const auto index = grid->transform().worldToIndex(openvdb::Vec3d(p[0], p[1], p[2]));
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(index[axis]) || std::fabs(index[axis]) > 1e6) return MS::kFailure;
            points[static_cast<unsigned int>(i)] = MFloatPoint(p[0], p[1], p[2]);
            if (!flags.empty()) seamPoints[i] = flags[i] != 0;
        }
        std::size_t face = 0, connection = 0;
        for (std::size_t pool = 0; pool < poolCount; ++pool) {
            const auto& polygons = mMesher.polygonPoolList()[pool];
            for (int kind = 0; kind < 2; ++kind) {
                const std::size_t count = kind == 0 ? polygons.numQuads() : polygons.numTriangles();
                const int width = kind == 0 ? 4 : 3;
                for (std::size_t p = 0; p < count; ++p) {
                    counts[static_cast<unsigned int>(face)] = width;
                    // Preserve polygon-pool order and the historical reversed winding.
                    for (int vertex = width - 1; vertex >= 0; --vertex) {
                        const auto index = kind == 0 ? polygons.quad(p)[vertex] : polygons.triangle(p)[vertex];
                        if (!dommy::memory::indexInRange(index, pointCount)) return MS::kFailure;
                        indices[static_cast<unsigned int>(connection++)] = static_cast<int>(index);
                    }
                    const int flag = kind == 0 ? polygons.quadFlags(p) : polygons.triangleFlags(p);
                    surface[face] = (flag & openvdb::tools::POLYFLAG_EXTERIOR) != 0;
                    interior[face] = !surface[face];
                    seam[face] = (flag & openvdb::tools::POLYFLAG_FRACTURE_SEAM) != 0;
                    ++face;
                }
            }
        }
        if (face != faces || connection != connects) return MS::kFailure;
        // Do not retain native output buffers during Maya mesh creation or
        // reference post-processing. The copied arrays/groups now own the data.
        mMesher.polygonPoolList().reset();
        mMesher.pointList().reset();
        MStatus result;
        MObject candidate = MFnMeshData().create(&result);
        DOMMY_RETURN_IF_FAILURE(result);
        if (candidate.isNull()) return MS::kFailure;
        if (pointCount || faces) {
            MFnMesh meshFunction;
            const MObject created = meshFunction.create(static_cast<int>(pointCount), static_cast<int>(faces),
                points, counts, indices, candidate, &result);
            DOMMY_RETURN_IF_FAILURE(result);
            if (created.isNull()) return MS::kFailure;
        }
        surfaceGroup.swap(surface); interiorGroup.swap(interior);
        seamGroup.swap(seam); seamPointGroup.swap(seamPoints);
        mesh = candidate;
        return MS::kSuccess;
    }
    openvdb::tools::VolumeToMesh& mMesher;
    dommy::vdb::MeshingBudget& mBudget;
    std::vector<bool> surfaceGroup, interiorGroup, seamGroup, seamPointGroup;
};
} // namespace












/// TBB body object for threaded sharp feature construction
template<typename IndexTreeType, typename BoolTreeType>
class GenAdaptivityMaskOp
{
public:
	typedef openvdb::tree::LeafManager<BoolTreeType> BoolLeafManager;
	
	GenAdaptivityMaskOp(const MFloatVectorArray& faceNormals,
						const IndexTreeType& indexTree, BoolLeafManager&, float edgetolerance = 0.0);
	
	void run(bool threaded = true);
	
	void operator()(const tbb::blocked_range<size_t>&) const;
		
private:
	const MFloatVectorArray& mFaceNormals;
	const IndexTreeType& mIndexTree;
	BoolLeafManager& mLeafs;
	float mEdgeTolerance;
};


template<typename IndexTreeType, typename BoolTreeType>
GenAdaptivityMaskOp<IndexTreeType, BoolTreeType>::GenAdaptivityMaskOp(const MFloatVectorArray& faceNormals,
																	  const IndexTreeType& indexTree,
																		  BoolLeafManager& leafMgr,
																		  float edgetolerance)
: mFaceNormals(faceNormals)
, mIndexTree(indexTree)
, mLeafs(leafMgr)
, mEdgeTolerance(edgetolerance)
{
	mEdgeTolerance = std::max(0.0f, mEdgeTolerance);
	mEdgeTolerance = std::min(1.0f, mEdgeTolerance);
}


template<typename IndexTreeType, typename BoolTreeType>
void
GenAdaptivityMaskOp<IndexTreeType, BoolTreeType>::run(bool threaded)
{
	if (threaded) {
		tbb::parallel_for(mLeafs.getRange(), *this);
	} else {
		(*this)(mLeafs.getRange());
	}
}


template<typename IndexTreeType, typename BoolTreeType>
void
GenAdaptivityMaskOp<IndexTreeType, BoolTreeType>::operator()(
															 const tbb::blocked_range<size_t>& range) const
{
	typedef typename openvdb::tree::ValueAccessor<const IndexTreeType> IndexAccessorType;
	IndexAccessorType idxAcc(mIndexTree);
	MVector tmpN, normal;
	int tmpIdx;

	const std::size_t normalCount =
		static_cast<std::size_t>(mFaceNormals.length());
	openvdb::Coord ijk, nijk;
	typename BoolTreeType::LeafNodeType::ValueOnIter iter;
	
	for (size_t n = range.begin(); n < range.end(); ++n) {
		iter = mLeafs.leaf(n).beginValueOn();
		for (; iter; ++iter) {
			ijk = iter.getCoord();
			
			bool edgeVoxel = false;
			
			const int idx = idxAcc.getValue(ijk);
			if (idx < 0 || static_cast<std::size_t>(idx) >= normalCount) {
				iter.setValueOff();
				continue;
			}
			normal = mFaceNormals[static_cast<unsigned int>(idx)];
			
			for (size_t i = 0; i < 18; ++i) {
				nijk = ijk + openvdb::util::COORD_OFFSETS[i];
				if (idxAcc.probeValue(nijk, tmpIdx) && tmpIdx != idx) {
					if (tmpIdx < 0 ||
						static_cast<std::size_t>(tmpIdx) >= normalCount) {
						continue;
					}
					tmpN = mFaceNormals[static_cast<unsigned int>(tmpIdx)];
					
					if ((normal*tmpN) < mEdgeTolerance) {
						edgeVoxel = true;
						break;
					}
				}
			}
			
			if (!edgeVoxel) iter.setValueOff();
		}
	}
}
