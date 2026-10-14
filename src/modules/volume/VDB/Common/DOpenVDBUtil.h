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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb/viewer/RenderModules.h

//
//  DOpenVDBUtil.hpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#pragma once

#include "DVdbGridDispatch.h"

#include <stdio.h>


#include "DOpenVDBData.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportPointBudget.h"
#include "DVdbViewportMath.h"
#include "modules/volume/VDB/Edit/DOpenVDBFilterPolicy.h"

#include <openvdb/openvdb.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/points/PointDataGrid.h>

#include <openvdb/Types.h>
#include <openvdb/tree/LeafManager.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/util/Formats.h> // printBytes
#include <openvdb/util/NullInterrupter.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/math/Math.h>
#include <openvdb/tools/Clip.h> // for tools::clip()

#include <tbb/tick_count.h>

#include <maya/M3dView.h>
#include <maya/MString.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnPluginData.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>

#if defined(__APPLE__) || defined(MACOSX)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <iostream>
#include <sstream>
#include <limits>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/typeof/typeof.hpp>

#include <maya/MPoint.h>
#include <maya/MPointArray.h>


typedef openvdb::GridBase           Grid;
typedef openvdb::GridBase::Ptr      GridPtr;
typedef openvdb::GridBase::ConstPtr GridCPtr;
typedef openvdb::GridBase&          GridRef;
typedef const openvdb::GridBase&    GridCRef;

typedef openvdb::GridPtrVec         GridPtrVec;
typedef GridPtrVec::iterator        GridPtrVecIter;
typedef GridPtrVec::const_iterator  GridPtrVecCIter;

typedef openvdb::GridCPtrVec        GridCPtrVec;
typedef GridCPtrVec::iterator       GridCPtrVecIter;
typedef GridCPtrVec::const_iterator GridCPtrVecCIter;


/// @brief  returns a pointer to the input VDB data object or NULL if this fails.
const DOpenVDBData* getInputVDB(const MObject& vdb, MDataBlock& data);
const DOpenVDBData* getInputVDBfromHandle(MDataHandle handle);

// Checked status-preserving input view; never consumes a failed Maya handle.
MStatus getValidInputVDB(const MObject& attribute, MDataBlock& data,
                         const DOpenVDBData*& result);

/// @brief Create Maya-owned plug-in data for a VDB node output.
///
/// @note The returned pointer is owned by Maya through @p pluginData and must
///       never be placed in a smart pointer with a default deleter.
MStatus createOpenVDBOutputData(
    MFnPluginData& pluginData,
    DOpenVDBData*& outputVdb);

/// @brief Publish VDB plug-in data and mark the requested output plug clean.
MStatus publishOpenVDBOutput(
    MDataBlock& data,
    const MObject& outputAttribute,
    const MPlug& requestedPlug,
    const MObject& outputObject);


void getGrids(std::vector<openvdb::GridBase::ConstPtr>& grids,
			  const DOpenVDBData& vdb, const std::string& names);

std::string getGridNames(const DOpenVDBData& vdb);

// Space-separated selections accept indices, name globs, and @name=<glob>.
// The explicit name form never interprets a numeric grid name as an index.
bool containsGrid(const std::vector<std::string>& selectionList,
				  const std::string& gridName, size_t gridIndex);

/// @brief  Constructs a list of selected grids @c grids from
///         the @c inputVdb and passes through unselected grids
///         to the @c outputVdb.
///
/// @return @c false if no matching grids were found.
bool getSelectedGrids(GridCPtrVec& grids, const std::string& selection,
					  const DOpenVDBData& inputVdb, DOpenVDBData& outputVdb);


/// @brief  Constructs a list of selected grids @c grids from
///         the @c inputVdb.
///
/// @return @c false if no matching grids were found.
bool getSelectedGrids(GridCPtrVec& grids, const std::string& selection,
					  const DOpenVDBData& inputVdb);

/// @brief Return world-space bounds enclosing complete active voxel cells.
bool getActiveVoxelWorldBounds(
    const openvdb::GridBase& grid,
    openvdb::BBoxd& worldBounds);

// The viewport clip slider historically uses voxel centers plus its own
// fixed world-space padding, rather than adding half a voxel here.
bool getActiveVoxelWorldBounds(
    const openvdb::GridBase& grid,
    openvdb::BBoxd& worldBounds,
    bool includeVoxelExtent);

/// @brief   Replaces a sequence of pound signs (#) with the current
///          frame number.
///
/// @details The number of pound signs defines the zero padding width.
///          For example '###' for frame 5 would produce "name.005.type"
///
/// @note   Supports three numbering schemes:
///             0 = Frame.SubTick
///             1 = Fractional frame values
///             2 = Global ticks
bool insertFrameNumber(
    std::string& str, const MTime& time, int numberingScheme = 0);

////////////////////////////////////////

// Statistics and grid info


struct Timer
{
	Timer() : mStamp(tbb::tick_count::now()) { }
	
	void reset() { mStamp = tbb::tick_count::now(); }
	
	double seconds() const { return (tbb::tick_count::now() - mStamp).seconds(); }
	
	std::string elapsedTime() const {
		double sec = seconds();
		return sec < 1.0 ? boost::lexical_cast<std::string>(sec * 1000.0) + " ms" :
		boost::lexical_cast<std::string>(sec) + " s";
	}
	
private:
	tbb::tick_count mStamp;
};


void printGridInfo(std::ostream& os, const DOpenVDBData& vdb);

MStatus updateNodeInfo(std::stringstream& stream, MDataBlock& data, const MObject& strAttr);

////////////////////////////////////////
#pragma mark BBoxClipOp
struct BBoxClipOp
{
    BBoxClipOp(const openvdb::BBoxd& bbox_, bool inside_ = true):
    bbox(bbox_), inside(inside_)
    {}
    
    template<typename GridType>
    void operator()(const GridType& grid)
    {
        outputGrid = openvdb::tools::clip(grid, bbox, inside);
    }
    
    openvdb::BBoxd bbox;
    GridPtr outputGrid;
    bool inside = true;
};

////////////////////////////////////////

// OpenGL helper objects


class BufferObject
{
public:
	BufferObject();
	~BufferObject();
	
	void render() const;
	
	/// @note accepted @c primType: GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP,
	/// GL_LINES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES,
	/// GL_QUAD_STRIP, GL_QUADS and GL_POLYGON
	void genIndexBuffer(const std::vector<GLuint>&, GLenum primType);
	
	void genVertexBuffer(const std::vector<GLfloat>&);
	void genNormalBuffer(const std::vector<GLfloat>&);
	void genColorBuffer(const std::vector<GLfloat>&);
	
	void clear();
	
private:
	GLuint mVertexBuffer, mNormalBuffer, mIndexBuffer, mColorBuffer;
	GLenum mPrimType;
	GLsizei mPrimNum;
};


class BufferObjectVP2
{
public:
	BufferObjectVP2();
	~BufferObjectVP2();
	
	void clear();
	std::vector<MPoint> points;
    std::vector<MPoint> normals;
	std::vector<unsigned int> indexArray;
	
};

class lineBufferObjectVP2
{
public:
	lineBufferObjectVP2(){};
	~lineBufferObjectVP2(){clear();};
	
	void clear() {points.clear();indexArray.clear();colors.clear();};
	
	std::vector<float> points;
	std::vector<float> colors;
	std::vector<unsigned int> indexArray;
};



class valueBufferObjectVP2
{
public:
    valueBufferObjectVP2();
    ~valueBufferObjectVP2();
    
    void clear();
    void swap(valueBufferObjectVP2& other);
    

    std::vector<float> valueArray;
    std::vector<float> points;
	std::vector<float> normals;
	std::vector<float> colors;
    std::vector<unsigned int> indexArray;
    
private:
    
};



class ShaderProgram
{
public:
	ShaderProgram();
	~ShaderProgram();
	
	void setVertShader(const std::string&);
	void setFragShader(const std::string&);
	
	void build();
	void build(const std::vector<GLchar*>& attributes);
	
	void startShading() const;
	void stopShading() const;
	
	void clear();
	
private:
	GLuint mProgram, mVertShader, mFragShader;
};


////////////////////////////////////////


namespace util {
	
	template<class TreeType>
	class MinMaxVoxel
	{
	public:
		typedef openvdb::tree::LeafManager<TreeType> LeafArray;
		typedef typename TreeType::ValueType ValueType;
		
			MinMaxVoxel(LeafArray&);
		
		void runParallel();
		void runSerial();
		
		const ValueType& minVoxel() const { return mMin; }
		const ValueType& maxVoxel() const { return mMax; }
		
		inline MinMaxVoxel(const MinMaxVoxel<TreeType>&, tbb::split);
		inline void operator()(const tbb::blocked_range<size_t>&);
		inline void join(const MinMaxVoxel<TreeType>&);
		
	private:
		LeafArray& mLeafArray;
		ValueType mMin, mMax;
	};
	
	
	template <class TreeType>
	MinMaxVoxel<TreeType>::MinMaxVoxel(LeafArray& leafs)
	: mLeafArray(leafs)
	, mMin(std::numeric_limits<ValueType>::max())
	, mMax(-mMin)
	{
	}
	
	
	template <class TreeType>
	inline
	MinMaxVoxel<TreeType>::MinMaxVoxel(const MinMaxVoxel<TreeType>& rhs, tbb::split)
	: mLeafArray(rhs.mLeafArray)
	, mMin(std::numeric_limits<ValueType>::max())
	, mMax(-mMin)
	{
	}
	
	
	template <class TreeType>
	void
	MinMaxVoxel<TreeType>::runParallel()
	{
		tbb::parallel_reduce(mLeafArray.getRange(), *this);
	}
	
	
	template <class TreeType>
	void
	MinMaxVoxel<TreeType>::runSerial()
	{
		(*this)(mLeafArray.getRange());
	}
	
	
	template <class TreeType>
	inline void
	MinMaxVoxel<TreeType>::operator()(const tbb::blocked_range<size_t>& range)
	{
		typename TreeType::LeafNodeType::ValueOnCIter iter;
		
		for (size_t n = range.begin(); n < range.end(); ++n) {
			iter = mLeafArray.leaf(n).cbeginValueOn();
			for (; iter; ++iter) {
				const ValueType value = iter.getValue();
				if (!std::isfinite(static_cast<double>(value))) {
					throw std::invalid_argument("VDB viewport scalar values must be finite");
				}
				mMin = std::min(mMin, value);
				mMax = std::max(mMax, value);
			}
		}
	}
	
	
	template <class TreeType>
	inline void
	MinMaxVoxel<TreeType>::join(const MinMaxVoxel<TreeType>& rhs)
	{
		mMin = std::min(mMin, rhs.mMin);
		mMax = std::max(mMax, rhs.mMax);
	}
	
} // namespace util


////////////////////////////////////////

///@todo Move this into a graphics library.
// Should be shared with the stand alone viewer.

class WireBoxBuilder
{
public:
	WireBoxBuilder(const openvdb::math::Transform& xform,
				   std::vector<GLuint>& indices, std::vector<GLfloat>& points, std::vector<GLfloat>& colors);
	
	// Validates the complete box and output range before writing any components.
	// Empty/non-finite bounds and invalid storage throw without partial writes.
	void add(GLuint boxIndex, const openvdb::CoordBBox& bbox, const openvdb::Vec3s& color);
	
private:
	const openvdb::math::Transform *mXForm;
	std::vector<GLuint> *mIndices;
	std::vector<GLfloat> *mPoints;
	std::vector<GLfloat> *mColors;
}; // WireBoxBuilder


class BoundingBoxGeo
{
public:
	BoundingBoxGeo(BufferObject& buffer)
	: mBuffer(&buffer)
	, mMin(-1.0)
	, mMax(1.0)
	{
	}
	
	void operator()(openvdb::GridBase::ConstPtr grid)
	{
		const size_t N = 8 * 3;
		
		std::vector<GLuint> indices(N);
		std::vector<GLfloat> points(N);
		std::vector<GLfloat> colors(N);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		
		boxBuilder.add(0, grid->evalActiveVoxelBoundingBox(), openvdb::Vec3s(0.045, 0.045, 0.045));
		
		// store the sorted min/max points.
		mMin[0] = std::numeric_limits<float>::max();
		mMin[1] = mMin[0];
		mMin[2] = mMin[0];
		mMax[0] = -mMin[0];
		mMax[1] = -mMin[0];
		mMax[2] = -mMin[0];
		
		for (int i = 0; i < 8; ++i) {
			int p = i * 3;
			mMin[0] = std::min(mMin[0], points[p]);
			mMin[1] = std::min(mMin[1], points[p+1]);
			mMin[2] = std::min(mMin[2], points[p+2]);
			
			mMax[0] = std::max(mMax[0], points[p]);
			mMax[1] = std::max(mMax[1], points[p+1]);
			mMax[2] = std::max(mMax[2], points[p+2]);
		}
		
		// gen buffers and upload data to GPU (ignoring color array)
		mBuffer->genVertexBuffer(points);
		mBuffer->genIndexBuffer(indices, GL_LINES);
	}
	
	const openvdb::Vec3s& min() const { return mMin; }
	const openvdb::Vec3s& max() const { return mMax; }
	
private:
	BufferObject *mBuffer;
	openvdb::Vec3s mMin, mMax;
}; // BoundingBoxGeo


class BoundingBoxGeoVP2
{
public:
    explicit BoundingBoxGeoVP2(lineBufferObjectVP2& buffer)
        : mBuffer(&buffer), mMin(-1.0), mMax(1.0), mHasBounds(false) {}

    void operator()(openvdb::GridBase::ConstPtr grid)
    {
        mBuffer->clear();
        mMin = openvdb::Vec3s(-1.0);
        mMax = openvdb::Vec3s(1.0);
        mHasBounds = false;
        if (!grid) return;
        const openvdb::CoordBBox bounds = grid->evalActiveVoxelBoundingBox();
        if (bounds.empty()) return;

        std::vector<unsigned int> indices(24U);
        std::vector<float> points(24U), colors(24U);
        WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
        boxBuilder.add(0, bounds, openvdb::Vec3s(0.045, 0.045, 0.045));

        openvdb::Vec3s minimum(std::numeric_limits<float>::max());
        openvdb::Vec3s maximum(-std::numeric_limits<float>::max());
        for (size_t corner = 0; corner < 8U; ++corner) {
            for (int axis = 0; axis < 3; ++axis) {
                const float value = points[corner * 3U + static_cast<size_t>(axis)];
                minimum[axis] = std::min(minimum[axis], value);
                maximum[axis] = std::max(maximum[axis], value);
            }
        }
        mBuffer->points.swap(points);
        mBuffer->indexArray.swap(indices);
        mMin = minimum;
        mMax = maximum;
        mHasBounds = true;
    }

    bool hasBounds() const { return mHasBounds; }
    const openvdb::Vec3s& min() const { return mMin; }
    const openvdb::Vec3s& max() const { return mMax; }

private:
    lineBufferObjectVP2* mBuffer;
    openvdb::Vec3s mMin, mMax;
    bool mHasBounds;
}; // BoundingBoxGeoVP2


class InternalNodesGeo
{
public:
	InternalNodesGeo(BufferObject& buffer) : mBuffer(&buffer) {}
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		size_t nodeCount = grid->tree().nonLeafCount();
		const size_t N = nodeCount * 8 * 3;
		
		std::vector<GLuint> indices(N);
		std::vector<GLfloat> points(N);
		std::vector<GLfloat> colors(N);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		
		openvdb::CoordBBox bbox(openvdb::Coord(0), openvdb::Coord(10));
		size_t boxIndex = 0;
		
		typename GridType::TreeType::NodeCIter iter = grid->tree().cbeginNode();
		iter.setMaxDepth(GridType::TreeType::NodeCIter::LEAF_DEPTH - 1);
		
		const openvdb::Vec3s nodeColor[2] = {
			openvdb::Vec3s(0.0432, 0.33, 0.0411023), // first internal node level
			openvdb::Vec3s(0.871, 0.394, 0.01916) // intermediate internal node levels
		};
		
		for ( ; iter; ++iter) {
			iter.getBoundingBox(bbox);
			boxBuilder.add(boxIndex++, bbox, nodeColor[(iter.getLevel() == 1)]);
			
		} // end node iteration
		
		
		// gen buffers and upload data to GPU
		mBuffer->genVertexBuffer(points);
		mBuffer->genColorBuffer(colors);
		mBuffer->genIndexBuffer(indices, GL_LINES);
	}
	
private:
	BufferObject *mBuffer;
}; // InternalNodesGeo


class InternalNodesGeoVP2
{
public:
	InternalNodesGeoVP2(lineBufferObjectVP2& buffer, size_t maximumBoxCount)
	: mBuffer(&buffer), mMaximumBoxCount(maximumBoxCount), mGeneratedBoxCount(0) {}

	size_t generatedBoxCount() const { return mGeneratedBoxCount; }
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		const size_t nodeCount = std::min<size_t>(
			grid->tree().nonLeafCount(), mMaximumBoxCount);
		size_t vertexCount = 0, componentCount = 0, indexCount = 0;
		if (!dommy::viewport::checkedWireBoxLayout(
				nodeCount, vertexCount, componentCount, indexCount)) {
			mBuffer->clear();
			mGeneratedBoxCount = 0;
			return;
		}
		
		std::vector<unsigned int> indices(indexCount);
		std::vector<float> points(componentCount);
		std::vector<float> colors(componentCount);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		
		openvdb::CoordBBox bbox(openvdb::Coord(0), openvdb::Coord(10));
		size_t boxIndex = 0;
		
		typename GridType::TreeType::NodeCIter iter = grid->tree().cbeginNode();
		iter.setMaxDepth(GridType::TreeType::NodeCIter::LEAF_DEPTH - 1);
		
		const openvdb::Vec3s nodeColor[2] = {
			openvdb::Vec3s(0.0432, 0.33, 0.0411023), // first internal node level
			openvdb::Vec3s(0.871, 0.394, 0.01916) // intermediate internal node levels
		};
		
		for ( ; iter && boxIndex < nodeCount; ++iter) {
			iter.getBoundingBox(bbox);
			boxBuilder.add(boxIndex++, bbox, nodeColor[(iter.getLevel() == 1)]);
			
		} // end node iteration
		
		
		// gen buffers and upload data to GPU
		mBuffer->points.swap(points);
//		mBuffer->colors = colors;
		mBuffer->indexArray.swap(indices);
		mGeneratedBoxCount = boxIndex;
	}
	
private:
	lineBufferObjectVP2 *mBuffer;
	size_t mMaximumBoxCount;
	size_t mGeneratedBoxCount;
}; // InternalNodesGeo


class LeafNodesGeo
{
public:
	LeafNodesGeo(BufferObject& buffer) : mBuffer(&buffer) {}
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::TreeType TreeType;
		
		openvdb::tree::LeafManager<const TreeType> leafs(grid->tree());
		
		const size_t N = leafs.leafCount() * 8 * 3;
		std::vector<GLuint> indices(N);
		std::vector<GLfloat> points(N);
		std::vector<GLfloat> colors(N);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		const openvdb::Vec3s color(0.00608299, 0.279541, 0.625); // leaf node color
		
		tbb::parallel_for(leafs.getRange(), LeafOp<TreeType>(leafs, boxBuilder, color));
		
		// gen buffers and upload data to GPU
		mBuffer->genVertexBuffer(points);
		mBuffer->genColorBuffer(colors);
		mBuffer->genIndexBuffer(indices, GL_LINES);
	}
	
protected:
	template<typename TreeType>
	struct LeafOp
	{
		typedef openvdb::tree::LeafManager<const TreeType> LeafManagerType;
		
		LeafOp(const LeafManagerType& leafs, WireBoxBuilder& boxBuilder, const openvdb::Vec3s& color)
		: mLeafs(&leafs), mBoxBuilder(&boxBuilder), mColor(color) {}
		
		void operator()(const tbb::blocked_range<size_t>& range) const
		{
			openvdb::CoordBBox bbox;
			openvdb::Coord& min = bbox.min();
			openvdb::Coord& max = bbox.max();
			const int offset = int(TreeType::LeafNodeType::DIM) - 1;
			
			for (size_t n = range.begin(); n < range.end(); ++n) {
				min = mLeafs->leaf(n).origin();
				max[0] = min[0] + offset;
				max[1] = min[1] + offset;
				max[2] = min[2] + offset;
				mBoxBuilder->add(n, bbox, mColor);
			}
		}
		
	private:
		const LeafManagerType *mLeafs;
		WireBoxBuilder *mBoxBuilder;
		const openvdb::Vec3s mColor;
	}; // LeafOp
	
private:
	BufferObject *mBuffer;
}; // LeafNodesGeo



class LeafNodesGeoVP2
{
public:
	LeafNodesGeoVP2(lineBufferObjectVP2& buffer, size_t maximumBoxCount)
	: mBuffer(&buffer), mMaximumBoxCount(maximumBoxCount), mGeneratedBoxCount(0) {}

	size_t generatedBoxCount() const { return mGeneratedBoxCount; }
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::TreeType TreeType;
		
		openvdb::tree::LeafManager<const TreeType> leafs(grid->tree());
		
		const size_t leafCount = std::min<size_t>(leafs.leafCount(), mMaximumBoxCount);
		size_t vertexCount = 0, componentCount = 0, indexCount = 0;
		if (!dommy::viewport::checkedWireBoxLayout(
				leafCount, vertexCount, componentCount, indexCount)) {
			mBuffer->clear();
			mGeneratedBoxCount = 0;
			return;
		}
		std::vector<unsigned int> indices(indexCount);
		std::vector<float> points(componentCount);
		std::vector<float> colors(componentCount);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		const openvdb::Vec3s color(0.00608299, 0.279541, 0.625); // leaf node color
		
		tbb::parallel_for(
			tbb::blocked_range<size_t>(0, leafCount),
			LeafOp<TreeType>(leafs, boxBuilder, color));
		
			mBuffer->points.swap(points);
			mBuffer->indexArray.swap(indices);
		mGeneratedBoxCount = leafCount;
	}
	
protected:
	template<typename TreeType>
	struct LeafOp
	{
		typedef openvdb::tree::LeafManager<const TreeType> LeafManagerType;
		
		LeafOp(const LeafManagerType& leafs, WireBoxBuilder& boxBuilder, const openvdb::Vec3s& color)
		: mLeafs(&leafs), mBoxBuilder(&boxBuilder), mColor(color) {}
		
		void operator()(const tbb::blocked_range<size_t>& range) const
		{
			openvdb::CoordBBox bbox;
			openvdb::Coord& min = bbox.min();
			openvdb::Coord& max = bbox.max();
			const int offset = int(TreeType::LeafNodeType::DIM) - 1;
			
			for (size_t n = range.begin(); n < range.end(); ++n) {
				min = mLeafs->leaf(n).origin();
				max[0] = min[0] + offset;
				max[1] = min[1] + offset;
				max[2] = min[2] + offset;
				mBoxBuilder->add(n, bbox, mColor);
			}
		}
		
	private:
		const LeafManagerType *mLeafs;
		WireBoxBuilder *mBoxBuilder;
		const openvdb::Vec3s mColor;
	}; // LeafOp
	
private:
	lineBufferObjectVP2 *mBuffer;
	size_t mMaximumBoxCount;
	size_t mGeneratedBoxCount;
}; // LeafNodesGeo


class ActiveTileGeo
{
public:
	ActiveTileGeo(BufferObject& buffer) : mBuffer(&buffer) {}
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::TreeType TreeType;
		const openvdb::Index maxDepth = TreeType::ValueAllIter::LEAF_DEPTH - 1;
		size_t tileCount = 0;
		
		{
			typename TreeType::ValueOnCIter iter(grid->tree());
			iter.setMaxDepth(maxDepth);
			for ( ; iter; ++iter) { ++tileCount; }
		}
		
		const size_t N = tileCount * 8 * 3;
		
		std::vector<GLuint> indices(N);
		std::vector<GLfloat> points(N);
		std::vector<GLfloat> colors(N);
		
		WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
		
		const openvdb::Vec3s color(0.9, 0.3, 0.3);
		openvdb::CoordBBox bbox;
		size_t boxIndex = 0;
		
		
		
		typename TreeType::ValueOnCIter iter(grid->tree());
		iter.setMaxDepth(maxDepth);
		
		for ( ; iter; ++iter) {
			iter.getBoundingBox(bbox);
			boxBuilder.add(boxIndex++, bbox, color);
		} // end tile iteration
		
        
		
		// gen buffers and upload data to GPU
		mBuffer->genVertexBuffer(points);
		mBuffer->genColorBuffer(colors);
		mBuffer->genIndexBuffer(indices, GL_LINES);
	}
	
private:
	BufferObject *mBuffer;
}; // ActiveTileGeo

class ActiveTileGeoVP2
{
public:
    ActiveTileGeoVP2(lineBufferObjectVP2& buffer, size_t maximumBoxCount)
    : mBuffer(&buffer), mMaximumBoxCount(maximumBoxCount), mGeneratedBoxCount(0) {}

    size_t generatedBoxCount() const { return mGeneratedBoxCount; }
    
    template<typename GridType>
    void operator()(typename GridType::ConstPtr grid)
    {
        typedef typename GridType::TreeType TreeType;
        const openvdb::Index maxDepth = TreeType::ValueAllIter::LEAF_DEPTH - 1;
        size_t tileCount = 0;
        
        {
            typename TreeType::ValueOnCIter iter(grid->tree());
            iter.setMaxDepth(maxDepth);
            for ( ; iter && tileCount < mMaximumBoxCount; ++iter) { ++tileCount; }
        }
        
        size_t vertexCount = 0, componentCount = 0, indexCount = 0;
        if (!dommy::viewport::checkedWireBoxLayout(
                tileCount, vertexCount, componentCount, indexCount)) {
            mBuffer->clear();
            mGeneratedBoxCount = 0;
            return;
        }
        
        std::vector<unsigned int> indices(indexCount);
        std::vector<float> points(componentCount);
        std::vector<float> colors(componentCount);
        
        WireBoxBuilder boxBuilder(grid->constTransform(), indices, points, colors);
        
        const openvdb::Vec3s color(0.9, 0.3, 0.3);
        openvdb::CoordBBox bbox;
        size_t boxIndex = 0;
        
        
        
        typename TreeType::ValueOnCIter iter(grid->tree());
        iter.setMaxDepth(maxDepth);
        
        for ( ; iter && boxIndex < tileCount; ++iter) {
            iter.getBoundingBox(bbox);
            boxBuilder.add(boxIndex++, bbox, color);
        } // end tile iteration
	        mBuffer->points.swap(points);
	        mBuffer->indexArray.swap(indices);
        mGeneratedBoxCount = boxIndex;
    }
    
private:
    lineBufferObjectVP2 *mBuffer;
    size_t mMaximumBoxCount;
    size_t mGeneratedBoxCount;
}; // ActiveTileGeo



class SurfaceGeo
{
public:
	SurfaceGeo(BufferObject& buffer, float iso) : mBuffer(&buffer), mIso(iso) {}
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		openvdb::tools::VolumeToMesh mesher(mIso);
		mesher(*grid);
		
		// Copy points and generate point normals.
		std::vector<GLfloat> points(mesher.pointListSize() * 3);
		std::vector<GLfloat> normals(mesher.pointListSize() * 3);
		
		for (size_t n = 0, i = 0,  N = mesher.pointListSize(); n < N; ++n) {
			const openvdb::Vec3s& p = mesher.pointList()[n];
			points[i++] = p[0];
			points[i++] = p[1];
			points[i++] = p[2];
		}
		
		// Copy primitives
		openvdb::tools::PolygonPoolList& polygonPoolList = mesher.polygonPoolList();
		size_t numQuads = 0;
		for (size_t n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
			numQuads += polygonPoolList[n].numQuads();
		}
		
		std::vector<GLuint> indices;
		indices.reserve(numQuads * 4);
		openvdb::Vec3d normal, e1, e2;
		
		for (size_t n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
			const openvdb::tools::PolygonPool& polygons = polygonPoolList[n];
			for (size_t i = 0, I = polygons.numQuads(); i < I; ++i) {
				const openvdb::Vec4I& quad = polygons.quad(i);
				indices.push_back(quad[0]);
				indices.push_back(quad[1]);
				indices.push_back(quad[2]);
				indices.push_back(quad[3]);
				
				e1 = mesher.pointList()[quad[1]];
				e1 -= mesher.pointList()[quad[0]];
				e2 = mesher.pointList()[quad[2]];
				e2 -= mesher.pointList()[quad[1]];
				normal = e1.cross(e2);
				
				const double length = normal.length();
				if (length > 1.0e-7) normal *= (1.0 / length);
				
				for (size_t v = 0; v < 4; ++v) {
					normals[quad[v]*3]    = -normal[0];
					normals[quad[v]*3+1]  = -normal[1];
					normals[quad[v]*3+2]  = -normal[2];
				}
			}
		}
		
		// Construct and transfer GPU buffers.
		mBuffer->genVertexBuffer(points);
		mBuffer->genNormalBuffer(normals);
		mBuffer->genIndexBuffer(indices, GL_QUADS);
	}
	
private:
	BufferObject *mBuffer;
	float mIso;
}; // SurfaceGeo


class SurfaceGeoVP2
{
public:
	SurfaceGeoVP2(BufferObjectVP2& buffer,
				  float iso,
				  bool inv,
				  size_t maximumVertexCount,
				  size_t maximumIndexCount)
	: mBuffer(&buffer)
	, mIso(iso)
	, mInv(inv)
	, mMaximumVertexCount(maximumVertexCount)
	, mMaximumIndexCount(maximumIndexCount)
	, mGeneratedVertexCount(0)
	, mGeneratedIndexCount(0)
	, mSucceeded(false)
	{
	}

	size_t generatedVertexCount() const { return mGeneratedVertexCount; }
	size_t generatedIndexCount() const { return mGeneratedIndexCount; }
	bool succeeded() const { return mSucceeded; }

	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		mBuffer->clear();
		mGeneratedVertexCount = 0;
		mGeneratedIndexCount = 0;
		mSucceeded = false;
		if (!grid || !std::isfinite(mIso) || mMaximumVertexCount == 0 || mMaximumIndexCount == 0 ||
			grid->activeVoxelCount() >
				dommy::viewport::kMaxVdbViewportSurfaceWorkItemCount) {
			return;
		}

		openvdb::BBoxd worldBounds;
		if (!getActiveVoxelWorldBounds(*grid, worldBounds)) return;
		for (int axis = 0; axis < 3; ++axis) {
			if (worldBounds.min()[axis] > worldBounds.max()[axis] ||
				!dommy::numeric::canRepresentAsFloat(worldBounds.min()[axis]) ||
				!dommy::numeric::canRepresentAsFloat(worldBounds.max()[axis])) return;
		}

		openvdb::tools::VolumeToMesh mesher(mIso);
		mesher(*grid);

		const size_t pointCount = mesher.pointListSize();
		if (pointCount > mMaximumVertexCount ||
			pointCount > std::numeric_limits<unsigned int>::max()) {
			return;
		}

		std::vector<MPoint> points(pointCount);
		std::vector<MPoint> normals(pointCount, MPoint(0.0, 0.0, 0.0));
		for (size_t n = 0; n < pointCount; ++n) {
			const openvdb::Vec3s& point = mesher.pointList()[n];
			points[n].x = point[0];
			points[n].y = point[1];
			points[n].z = point[2];
			if (!dommy::maya::isFloatRepresentablePoint(points[n])) return;
		}

		openvdb::tools::PolygonPoolList& polygonPools = mesher.polygonPoolList();
		size_t quadCount = 0, triangleCount = 0;
		for (size_t pool = 0; pool < mesher.polygonPoolListSize(); ++pool) {
			if (!dommy::memory::checkedAdd(
					quadCount, polygonPools[pool].numQuads(), quadCount) ||
				!dommy::memory::checkedAdd(
					triangleCount, polygonPools[pool].numTriangles(), triangleCount)) {
				return;
			}
		}

		size_t indexCount = 0;
		if (!dommy::viewport::checkedSurfaceIndexCount(
				quadCount, triangleCount, mMaximumIndexCount, indexCount)) {
			return;
		}

		std::vector<unsigned int> indices;
		indices.reserve(indexCount);
		auto addTriangle = [&](int first, int second, int third) -> bool {
			if (first < 0 || second < 0 || third < 0 ||
				static_cast<size_t>(first) >= pointCount ||
				static_cast<size_t>(second) >= pointCount ||
				static_cast<size_t>(third) >= pointCount) {
				return false;
			}

			const unsigned int a = static_cast<unsigned int>(first);
			const unsigned int b = static_cast<unsigned int>(second);
			const unsigned int c = static_cast<unsigned int>(third);
			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);

			openvdb::Vec3d edge1 = mesher.pointList()[b];
			edge1 -= mesher.pointList()[a];
			openvdb::Vec3d edge2 = mesher.pointList()[c];
			edge2 -= mesher.pointList()[a];
			openvdb::Vec3d normal = edge1.cross(edge2);
			const double length = normal.length();
			if (!std::isfinite(length)) return false;
			if (length > 1.0e-12) {
				normal *= (mInv ? -1.0 : 1.0) / length;
				const unsigned int faceVertices[3] = {a, b, c};
				for (size_t vertex = 0; vertex < 3; ++vertex) {
					normals[faceVertices[vertex]].x += normal[0];
					normals[faceVertices[vertex]].y += normal[1];
					normals[faceVertices[vertex]].z += normal[2];
				}
			}
			return true;
		};

		for (size_t pool = 0; pool < mesher.polygonPoolListSize(); ++pool) {
			const openvdb::tools::PolygonPool& polygons = polygonPools[pool];
			for (size_t index = 0; index < polygons.numQuads(); ++index) {
				const openvdb::Vec4I& quad = polygons.quad(index);
				if (!addTriangle(quad[0], quad[1], quad[2]) ||
					!addTriangle(quad[2], quad[3], quad[0])) {
					return;
				}
			}
			for (size_t index = 0; index < polygons.numTriangles(); ++index) {
				const openvdb::Vec3I& triangle = polygons.triangle(index);
				if (!addTriangle(triangle[0], triangle[1], triangle[2])) {
					return;
				}
			}
		}

		for (size_t index = 0; index < normals.size(); ++index) {
			const double length = std::sqrt(
				normals[index].x * normals[index].x +
				normals[index].y * normals[index].y +
				normals[index].z * normals[index].z);
			if (!std::isfinite(length)) return;
			if (length > 1.0e-12) {
				normals[index].x /= length;
				normals[index].y /= length;
				normals[index].z /= length;
			}
			if (!dommy::maya::isFloatRepresentablePoint(normals[index])) return;
		}

		mBuffer->points.swap(points);
		mBuffer->normals.swap(normals);
		mBuffer->indexArray.swap(indices);
		mGeneratedVertexCount = pointCount;
		mGeneratedIndexCount = indexCount;
		mSucceeded = true;
	}

private:
	BufferObjectVP2 *mBuffer;
	float mIso;
	bool mInv;
	size_t mMaximumVertexCount;
	size_t mMaximumIndexCount;
	size_t mGeneratedVertexCount;
	size_t mGeneratedIndexCount;
	bool mSucceeded;
}; // SurfaceGeoVP2




template<typename TreeType>
class PointGenerator
{
public:
	typedef openvdb::tree::LeafManager<TreeType> LeafManagerType;
	
	PointGenerator(
				   std::vector<GLfloat>& points,
				   std::vector<GLuint>& indices,
				   std::vector<openvdb::Coord>& coordinates,
				   LeafManagerType& leafs,
				   const std::vector<size_t>& indexMap,
				   const std::vector<size_t>& sampleCounts,
				   const openvdb::math::Transform& transform)
	: mPoints(&points)
	, mIndices(&indices)
	, mCoordinates(&coordinates)
	, mLeafs(&leafs)
	, mIndexMap(&indexMap)
	, mSampleCounts(&sampleCounts)
	, mTransform(&transform)
	{
	}
	
	void runParallel()
	{
		size_t componentCount = 0, nextOffset = 0;
		if (mIndexMap->size() != mLeafs->leafCount() ||
			mSampleCounts->size() != mLeafs->leafCount() ||
			mCoordinates->size() != mIndices->size() ||
			mIndices->size() > std::numeric_limits<GLuint>::max() ||
			!dommy::memory::checkedMultiply(mIndices->size(), 3U, componentCount) ||
			mPoints->size() != componentCount) {
			throw std::length_error("VDB viewport sample buffers have inconsistent sizes");
		}
		for (size_t leaf = 0; leaf < mLeafs->leafCount(); ++leaf) {
			if ((*mIndexMap)[leaf] != nextOffset ||
				(*mSampleCounts)[leaf] > mLeafs->leaf(leaf).onVoxelCount() ||
				!dommy::memory::checkedAdd(nextOffset, (*mSampleCounts)[leaf], nextOffset) ||
				nextOffset > mIndices->size()) {
				throw std::length_error("VDB viewport sample ranges are inconsistent");
			}
		}
		if (nextOffset != mIndices->size()) {
			throw std::length_error("VDB viewport sample ranges do not cover the output");
		}
		tbb::parallel_for(mLeafs->getRange(), *this);
	}
	
	
	inline void operator()(const tbb::blocked_range<size_t>& range) const
	{
		typedef typename TreeType::LeafNodeType::ValueOnCIter ValueOnCIter;
		
		size_t index = 0;
		size_t activeVoxels = 0;
		
		for (size_t n = range.begin(); n < range.end(); ++n) {
			const size_t sampleCount = (*mSampleCounts)[n];
			if (sampleCount == 0) {
				continue;
			}

			index = (*mIndexMap)[n];
			ValueOnCIter it = mLeafs->leaf(n).cbeginValueOn();
			
			activeVoxels = mLeafs->leaf(n).onVoxelCount();
			size_t visitedCount = 0;
			
			if (activeVoxels <= sampleCount) {
				
				for ( ; it; ++it) {
					if (visitedCount++ >= activeVoxels) {
						throw std::length_error("VDB viewport leaf traversal exceeded its count");
					}
					insertPoint(it.getCoord(), index);
					++index;
				}
			} else {
				// Spread samples across the leaf without retaining a second copy
				// of all active coordinates.
				size_t accumulator = 0;
				for ( ; it; ++it) {
					if (visitedCount++ >= activeVoxels) {
						throw std::length_error("VDB viewport leaf traversal exceeded its count");
					}
					if (accumulator >= activeVoxels - sampleCount) {
						insertPoint(it.getCoord(), index);
						++index;
						accumulator -= activeVoxels - sampleCount;
					} else {
						accumulator += sampleCount;
					}
				}
			}
			if (visitedCount != activeVoxels || index - (*mIndexMap)[n] != sampleCount) {
				throw std::length_error("VDB viewport leaf traversal did not match its sample plan");
			}
		}
	}
	
private:
	void insertPoint(const openvdb::Coord& coordinate, size_t index) const
	{
		const openvdb::Vec3d position = mTransform->indexToWorld(coordinate);
		std::array<float, 3> point;
		GLuint vertexIndex = 0;
		if (index >= mIndices->size() ||
			!dommy::numeric::checkedNumericCast(index, vertexIndex)) {
			throw std::length_error("VDB viewport point index is out of range");
		}
		for (int axis = 0; axis < 3; ++axis) {
			if (!dommy::numeric::checkedFloatCast(position[axis], point[axis])) {
				throw std::invalid_argument("VDB viewport point is not float-representable");
			}
		}
		(*mCoordinates)[index] = coordinate;
		(*mIndices)[index] = vertexIndex;
		std::copy(point.begin(), point.end(), mPoints->begin() + index * 3U);
	}
	
	std::vector<GLfloat> *mPoints;
	std::vector<GLuint> *mIndices;
	std::vector<openvdb::Coord> *mCoordinates;
	LeafManagerType *mLeafs;
	const std::vector<size_t> *mIndexMap;
	const std::vector<size_t> *mSampleCounts;
	const openvdb::math::Transform *mTransform;
}; // PointGenerator

template<typename LeafManagerType>
dommy::viewport::PointSamplePlan buildLeafPointSamplePlan(
    const LeafManagerType& leafs,
    size_t maximumPointCount,
    bool onePointPerLeaf)
{
    std::vector<size_t> availableCounts(leafs.leafCount(), 0);
    for (size_t leafIndex = 0; leafIndex < leafs.leafCount(); ++leafIndex) {
        availableCounts[leafIndex] =
            static_cast<size_t>(leafs.leaf(leafIndex).onVoxelCount());
    }

    dommy::viewport::PointSamplePlan plan;
    const size_t safeMaximumPointCount = std::min<size_t>(
        maximumPointCount,
        static_cast<size_t>(std::numeric_limits<GLuint>::max()));
    if (!dommy::viewport::buildPointSamplePlan(
            availableCounts, safeMaximumPointCount, onePointPerLeaf, plan)) {
        throw std::length_error("Unable to construct a bounded VDB viewport sample plan");
    }
    return plan;
}

inline size_t checkedViewportComponentCount(size_t pointCount)
{
    size_t componentCount = 0;
    if (!dommy::memory::checkedMultiply(pointCount, 3U, componentCount)) {
        throw std::length_error("VDB viewport component count exceeds size_t capacity");
    }
    return componentCount;
}


template<typename GridType>
class PointAttributeGenerator
{
public:
	typedef typename GridType::ValueType ValueType;
	
	PointAttributeGenerator(
							const std::vector<openvdb::Coord>& coordinates,
							std::vector<GLfloat>& colors,
							const GridType& grid,
							ValueType minValue,
							ValueType maxValue,
							openvdb::Vec3s (&colorMap)[4],
							bool isLevelSet = false)
		: mCoordinates(&coordinates)
		, mColors(&colors)
		, mNormals(nullptr)
	, mGrid(&grid)
	, mMinValue(minValue)
	, mMaxValue(maxValue)
	, mColorMap(colorMap)
	, mIsLevelSet(isLevelSet)
	, mZeroValue(openvdb::zeroVal<ValueType>())
	{ }
	
	PointAttributeGenerator(
							const std::vector<openvdb::Coord>& coordinates,
							std::vector<GLfloat>& colors,
							std::vector<GLfloat>& normals,
							const GridType& grid,
							ValueType minValue,
							ValueType maxValue,
							openvdb::Vec3s (&colorMap)[4],
							bool isLevelSet = false)
	: PointAttributeGenerator(coordinates, colors, grid, minValue, maxValue, colorMap, isLevelSet)
	{ mNormals = &normals; }
	
	void runParallel()
	{
        size_t componentCount = 0;
        if (!dommy::memory::checkedMultiply(mCoordinates->size(), 3U, componentCount) ||
            mColors->size() != componentCount ||
            (mNormals && mNormals->size() != componentCount)) {
            throw std::length_error(
                "VDB point attribute buffers have inconsistent component counts");
        }
		tbb::parallel_for(tbb::blocked_range<size_t>(0, mCoordinates->size()), *this);
	}
	
	inline void operator()(const tbb::blocked_range<size_t>& range) const
	{
		// Each partition owns its accessor cache; coordinates remain integer
		// samples rather than a lossy world-position round trip.
		openvdb::tree::ValueAccessor<const typename GridType::TreeType> accessor(mGrid->tree());
		openvdb::Vec3d tmpNormal;
		openvdb::Vec3s color(0.9, 0.3, 0.3);
		float w = 0.0;
		
		size_t e1, e2, e3;
		for (size_t n = range.begin(); n < range.end(); ++n) {
			
			e1 = 3 * n;
			e2 = e1 + 1;
			e3 = e2 + 1;
			
			const openvdb::Coord& ijk = (*mCoordinates)[n];
			if (mNormals) {
				for (int axis = 0; axis < 3; ++axis) {
					if (!dommy::vdb::filterCoordinate(ijk[axis], 1)) {
						throw std::invalid_argument("VDB viewport gradient coordinate has no stencil headroom");
					}
				}
			}
			const ValueType value = accessor.getValue(ijk);
			
			bool isNegative = false;
			if (!dommy::vdb::normalizedSignedWeight(
					static_cast<double>(value),
					static_cast<double>(mMinValue),
					static_cast<double>(mMaxValue),
					static_cast<double>(mZeroValue),
					isNegative, w)) {
				throw std::invalid_argument("VDB viewport scalar range or value is invalid");
			}

			if (isNegative) {
				if (mIsLevelSet) {
					color = mColorMap[3];
				} else {
					color = (1.0f - w) * mColorMap[2] + w * mColorMap[3];
				}
			} else {
				if (mIsLevelSet) {
					color = mColorMap[1];
				} else {
					color = (1.0f - w) * mColorMap[0] + w * mColorMap[1];
				}
			}
			
			std::array<float, 3> normalComponents;
			if (mNormals) {
				openvdb::Vec3d normal(0.0, -1.0, 0.0);
				tmpNormal = openvdb::math::ISGradient<
					openvdb::math::CD_2ND>::result(accessor, ijk);
				
				const double length = tmpNormal.length();
				if (!std::isfinite(length)) {
					throw std::invalid_argument("VDB viewport gradient is nonfinite");
				}
				if (length > 1.0e-7) {
					tmpNormal *= 1.0 / length;
					normal = tmpNormal;
				}
				
				for (int axis = 0; axis < 3; ++axis) {
					if (!dommy::numeric::checkedFloatCast(normal[axis], normalComponents[axis])) {
						throw std::invalid_argument("VDB viewport normal is not float-representable");
					}
				}
			}
			for (int axis = 0; axis < 3; ++axis) {
				if (!std::isfinite(color[axis])) {
					throw std::invalid_argument("VDB viewport color is nonfinite");
				}
			}
			(*mColors)[e1] = color[0];
			(*mColors)[e2] = color[1];
			(*mColors)[e3] = color[2];
			if (mNormals) {
				std::copy(normalComponents.begin(), normalComponents.end(), mNormals->begin() + e1);
			}
		}
	}
	
private:
	
	const std::vector<openvdb::Coord> *mCoordinates;
	std::vector<GLfloat> *mColors;
	std::vector<GLfloat> *mNormals;
	
	const GridType *mGrid;
	
	ValueType mMinValue, mMaxValue;
	openvdb::Vec3s (&mColorMap)[4];
	const bool mIsLevelSet;
		
	ValueType mZeroValue;
}; // PointAttributeGenerator




template< typename valueType>
static inline void processVValue(valueType val,MVector &Colour )
{
	Colour = MVector( double(val),double(val),double(val));
}


template<>
 inline void processVValue( openvdb::Vec3i val,MVector &Colour )
{
	Colour  = MVector( double(val.x()),double(val.y()),double(val.z()));
}

template<>
 inline void processVValue( openvdb::Vec3d val,MVector &Colour )
{
	Colour  = MVector( double(val.x()),double(val.y()),double(val.z()));
}


template<>
 inline void processVValue( openvdb::Vec3f val,MVector &Colour )
{
	Colour  = MVector( double(val.x()),double(val.y()),double(val.z()));
}



template<typename GridType>
class VectorAttributeGenerator
{
public:
	typedef typename GridType::ValueType ValueType;
		
	VectorAttributeGenerator(const std::vector<openvdb::Coord>& coordinates,
							std::vector<GLfloat>& colors,
							const GridType& grid)
	: mCoordinates(&coordinates)
	, mColors(&colors)
	, mNormals(nullptr)
	, mGrid(&grid)
	{ }
		
	VectorAttributeGenerator(
							const std::vector<openvdb::Coord>& coordinates,
							std::vector<GLfloat>& colors,
							std::vector<GLfloat>& normals,
							const GridType& grid)
	: VectorAttributeGenerator(coordinates, colors, grid)
	{ mNormals = &normals; }
		
	void runParallel()
	{
		size_t componentCount = 0;
		if (!dommy::memory::checkedMultiply(mCoordinates->size(), 3U, componentCount) ||
			mColors->size() != componentCount ||
			(mNormals && mNormals->size() != componentCount)) {
			throw std::length_error(
				"VDB vector attribute buffers have inconsistent component counts");
		}
		tbb::parallel_for(tbb::blocked_range<size_t>(0, mCoordinates->size()), *this);
	}
		
	inline void operator()(const tbb::blocked_range<size_t>& range) const
	{
		openvdb::tree::ValueAccessor<const typename GridType::TreeType> accessor(mGrid->tree());
		for (size_t n = range.begin(); n < range.end(); ++n) {
			const size_t e1 = 3U * n;
			const size_t e2 = e1 + 1U;
			const size_t e3 = e2 + 1U;
			
			const openvdb::Coord& ijk = (*mCoordinates)[n];
			const ValueType value = accessor.getValue(ijk);
			
			MVector pColor;
			processVValue(value, pColor);
			
			std::array<float, 3> components;
			for (int axis = 0; axis < 3; ++axis) {
				if (!dommy::numeric::checkedFloatCast(pColor[axis], components[axis])) {
					throw std::invalid_argument("VDB viewport vector is not float-representable");
				}
			}
			(*mColors)[e1] = components[0];
			(*mColors)[e2] = components[1];
			(*mColors)[e3] = components[2];
			
			if (mNormals) {
				// The vector shader reads this buffer as the vector value, not
				// as a surface normal. Preserve every sample, including magnitude.
				std::copy(components.begin(), components.end(), mNormals->begin() + e1);
			}
		}
	}
		
private:
	const std::vector<openvdb::Coord> *mCoordinates;
	std::vector<GLfloat> *mColors;
	std::vector<GLfloat> *mNormals;
		
	const GridType *mGrid;
}; // VectorAttributeGenerator






class ActiveVoxelGeo
{
public:
	
	ActiveVoxelGeo(BufferObject& pointBuffer)
	: mPointBuffer(&pointBuffer)
	, mColorMinPosValue(0.3, 0.9, 0.3) // green
	, mColorMaxPosValue(0.9, 0.3, 0.3) // red
	, mColorMinNegValue(0.9, 0.9, 0.3) // yellow
	, mColorMaxNegValue(0.3, 0.3, 0.9) // blue
	{ }
	
	void setColorMinPosValue(const openvdb::Vec3s& c) { mColorMinPosValue = c; }
	void setColorMaxPosValue(const openvdb::Vec3s& c) { mColorMaxPosValue = c; }
	void setColorMinNegValue(const openvdb::Vec3s& c) { mColorMinNegValue = c; }
	void setColorMaxNegValue(const openvdb::Vec3s& c) { mColorMaxNegValue = c; }
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		if (!grid || grid->empty()) {
			mPointBuffer->clear();
			return;
		}
		openvdb::Vec3s colorMap[4];
		colorMap[0] = mColorMinPosValue;
		colorMap[1] = mColorMaxPosValue;
		colorMap[2] = mColorMinNegValue;
		colorMap[3] = mColorMaxNegValue;
		
		//////////
		
		typedef typename GridType::ValueType ValueType;
		typedef typename GridType::TreeType TreeType;
		
		const TreeType& tree = grid->tree();
		const bool isLevelSetGrid = grid->getGridClass() == openvdb::GRID_LEVEL_SET;
		
		ValueType minValue, maxValue;
		openvdb::tree::LeafManager<const TreeType> leafs(tree);
		
		{
			util::MinMaxVoxel<const TreeType> minmax(leafs);
			minmax.runParallel();
			minValue = minmax.minVoxel();
			maxValue = minmax.maxVoxel();
		}
		
		const dommy::viewport::PointSamplePlan samplePlan = buildLeafPointSamplePlan(
			leafs, dommy::viewport::kMaxVdbViewportPointCount, false);
		const size_t voxelCount = samplePlan.totalSampleCount;
		const size_t componentCount = checkedViewportComponentCount(voxelCount);
		
		std::vector<GLfloat>
		points(componentCount),
		colors(componentCount),
		normals(componentCount);
		std::vector<GLuint> indices(voxelCount);
		std::vector<openvdb::Coord> coordinates(voxelCount);
		
		PointGenerator<const TreeType> pointGen(
			points, indices, coordinates, leafs, samplePlan.offsets,
			samplePlan.sampleCounts, grid->transform());
		pointGen.runParallel();
		
		PointAttributeGenerator<GridType> attributeGen(
			coordinates, colors, normals, *grid, minValue, maxValue, colorMap, isLevelSetGrid);
		attributeGen.runParallel();
		
		mPointBuffer->genVertexBuffer(points);
		mPointBuffer->genColorBuffer(colors);
		mPointBuffer->genNormalBuffer(normals);
		mPointBuffer->genIndexBuffer(indices, GL_POINTS);
	}
	
private:
	BufferObject *mPointBuffer;
	openvdb::Vec3s mColorMinPosValue, mColorMaxPosValue, mColorMinNegValue, mColorMaxNegValue;
}; // ActiveVoxelGeo




class ActiveVoxelGeoVP2
{
public:
	
	ActiveVoxelGeoVP2(valueBufferObjectVP2& pointBuffer,
					  bool isLowDetail,
					  const short colorType,
					  size_t maximumPointCount)
	: mPointBuffer(&pointBuffer)
		, mColorMinPosValue(0.3, 0.9, 0.3) // green
		, mColorMaxPosValue(0.9, 0.3, 0.3) // red
		, mColorMinNegValue(0.9, 0.9, 0.3) // yellow
		, mColorMaxNegValue(0.3, 0.3, 0.9) // blue
	, mIsLowDetail(isLowDetail)
		, mColorType(colorType)
		, mMaximumPointCount(maximumPointCount)
		, mGeneratedPointCount(0)
		{ }

	size_t generatedPointCount() const { return mGeneratedPointCount; }
	
	void setColorMinPosValue(const openvdb::Vec3s& c) { mColorMinPosValue = c; }
	void setColorMaxPosValue(const openvdb::Vec3s& c) { mColorMaxPosValue = c; }
	void setColorMinNegValue(const openvdb::Vec3s& c) { mColorMinNegValue = c; }
	void setColorMaxNegValue(const openvdb::Vec3s& c) { mColorMaxNegValue = c; }
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		mGeneratedPointCount = 0;
		mPointBuffer->clear();
		if (!grid || grid->empty() || mMaximumPointCount == 0) {
			valueBufferObjectVP2 emptyBuffer;
			mPointBuffer->swap(emptyBuffer);
			return;
		}
		openvdb::Vec3s colorMap[4];
		colorMap[0] = mColorMinPosValue;
		colorMap[1] = mColorMaxPosValue;
		colorMap[2] = mColorMinNegValue;
		colorMap[3] = mColorMaxNegValue;
		
		//////////
		
		typedef typename GridType::ValueType ValueType;
		typedef typename GridType::TreeType TreeType;
		
		
		
		const TreeType& tree = grid->tree();
		const bool isLevelSetGrid = grid->getGridClass() == openvdb::GRID_LEVEL_SET;
		
		
		ValueType minValue, maxValue;
		openvdb::tree::LeafManager<const TreeType> leafs(tree);
		
		{
			util::MinMaxVoxel<const TreeType> minmax(leafs);
			minmax.runParallel();
			minValue = minmax.minVoxel();
			maxValue = minmax.maxVoxel();
		}
			const dommy::viewport::PointSamplePlan samplePlan = buildLeafPointSamplePlan(
				leafs, mMaximumPointCount, mIsLowDetail);
		const size_t voxelCount = samplePlan.totalSampleCount;
		const size_t componentCount = checkedViewportComponentCount(voxelCount);
			valueBufferObjectVP2 generatedBuffer;
		generatedBuffer.points.resize(componentCount);
		generatedBuffer.colors.resize(componentCount);
		generatedBuffer.normals.resize(componentCount);
		generatedBuffer.indexArray.resize(voxelCount);
		std::vector<openvdb::Coord> coordinates(voxelCount);
		
		PointGenerator<const TreeType> pointGen(
			generatedBuffer.points, generatedBuffer.indexArray, coordinates, leafs,
			samplePlan.offsets, samplePlan.sampleCounts, grid->transform());
		pointGen.runParallel();
		
		PointAttributeGenerator<GridType> attributeGen(
			coordinates, generatedBuffer.colors, generatedBuffer.normals,
			*grid, minValue, maxValue, colorMap, isLevelSetGrid);
		attributeGen.runParallel();
			if (mColorType != 0) {
			generatedBuffer.colors = generatedBuffer.normals;
		}
		mPointBuffer->swap(generatedBuffer);
		mGeneratedPointCount = voxelCount;
	}
	
private:
	valueBufferObjectVP2 *mPointBuffer;
	openvdb::Vec3s mColorMinPosValue, mColorMaxPosValue, mColorMinNegValue, mColorMaxNegValue;
    bool mIsLowDetail;
	const short mColorType;
	const size_t mMaximumPointCount;
	size_t mGeneratedPointCount;
}; // ActiveVoxelGeo


class ActiveVoxelVecVP2
{
public:
	
	ActiveVoxelVecVP2(valueBufferObjectVP2& pointBuffer,
					  bool isLowDetail,
					  size_t maximumPointCount)
	: mPointBuffer(&pointBuffer)
    ,mIsLowDetail(isLowDetail)
	,mMaximumPointCount(maximumPointCount)
	,mGeneratedPointCount(0)
	{ }

	size_t generatedPointCount() const { return mGeneratedPointCount; }
	
	template<typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		mGeneratedPointCount = 0;
		mPointBuffer->clear();
		if (!grid || grid->empty() || mMaximumPointCount == 0) {
			valueBufferObjectVP2 emptyBuffer;
			mPointBuffer->swap(emptyBuffer);
			return;
		}
		//////////
		
			typedef typename GridType::TreeType TreeType;
			
			const TreeType& tree = grid->tree();
			
			openvdb::tree::LeafManager<const TreeType> leafs(tree);
		
		const dommy::viewport::PointSamplePlan samplePlan = buildLeafPointSamplePlan(
			leafs, mMaximumPointCount, mIsLowDetail);
		const size_t voxelCount = samplePlan.totalSampleCount;
		const size_t componentCount = checkedViewportComponentCount(voxelCount);
			valueBufferObjectVP2 generatedBuffer;
		generatedBuffer.points.resize(componentCount);
		generatedBuffer.colors.resize(componentCount);
		generatedBuffer.normals.resize(componentCount);
		generatedBuffer.indexArray.resize(voxelCount);
		std::vector<openvdb::Coord> coordinates(voxelCount);
		
		PointGenerator<const TreeType> pointGen(
			generatedBuffer.points, generatedBuffer.indexArray, coordinates, leafs,
			samplePlan.offsets, samplePlan.sampleCounts, grid->transform());
		pointGen.runParallel();
		
			VectorAttributeGenerator<GridType> attributeGen(
				coordinates, generatedBuffer.colors, generatedBuffer.normals,
				*grid);
		attributeGen.runParallel();
			mPointBuffer->swap(generatedBuffer);
		mGeneratedPointCount = voxelCount;
	}
	
private:
	valueBufferObjectVP2 *mPointBuffer;
    bool mIsLowDetail;
	const size_t mMaximumPointCount;
	size_t mGeneratedPointCount;
}; // ActiveVoxelVecVP2



////////////////////////////////////////


/// Helper class used internally by processTypedGrid()
template<typename GridType, typename OpType, bool IsConst/*=false*/>
struct GridProcessor {
	static inline void call(OpType& op, openvdb::GridBase::Ptr grid) {
#ifdef _MSC_VER
		op.operator()<GridType>(openvdb::gridPtrCast<GridType>(grid));
#else
		op.template operator()<GridType>(openvdb::gridPtrCast<GridType>(grid));
#endif
	}
};


/// Helper class used internally by processTypedGrid()
template<typename GridType, typename OpType>
struct GridProcessor<GridType, OpType, /*IsConst=*/true> {
	static inline void call(OpType& op, openvdb::GridBase::ConstPtr grid) {
#ifdef _MSC_VER
		op.operator()<GridType>(openvdb::gridConstPtrCast<GridType>(grid));
#else
		op.template operator()<GridType>(openvdb::gridConstPtrCast<GridType>(grid));
#endif
	}
};





/// Helper function used internally by processTypedGrid()
template<typename GridType, typename OpType, typename GridPtrType>
inline void
doProcessTypedGrid(GridPtrType grid, OpType& op)
{
    GridProcessor<GridType, OpType,
    boost::is_const<typename GridPtrType::element_type>::value>::call(op, grid);
}
////////////////////////////////////////


/// @brief Utility function that, given a generic grid pointer,
/// calls a functor on the fully-resolved grid
///
/// Usage:
/// @code
/// struct PruneOp {
///     template<typename GridT>
///     void operator()(typename GridT::Ptr grid) const { grid->tree()->prune(); }
/// };
///
/// processTypedGrid(myGridPtr, PruneOp());
/// @endcode
///
/// @return @c false if the grid type is unknown or unhandled.
template<typename GridPtrType, typename OpType>
bool
processTypedGrid(GridPtrType grid, OpType& op)
{
	using namespace openvdb;
	if (grid->template isType<BoolGrid>())        doProcessTypedGrid<BoolGrid>(grid, op);
	else if (grid->template isType<FloatGrid>())  doProcessTypedGrid<FloatGrid>(grid, op);
	else if (grid->template isType<DoubleGrid>()) doProcessTypedGrid<DoubleGrid>(grid, op);
	else if (grid->template isType<Int32Grid>())  doProcessTypedGrid<Int32Grid>(grid, op);
	else if (grid->template isType<Int64Grid>())  doProcessTypedGrid<Int64Grid>(grid, op);
	else if (grid->template isType<Vec3IGrid>())  doProcessTypedGrid<Vec3IGrid>(grid, op);
	else if (grid->template isType<Vec3SGrid>())  doProcessTypedGrid<Vec3SGrid>(grid, op);
	else if (grid->template isType<Vec3DGrid>())  doProcessTypedGrid<Vec3DGrid>(grid, op);
	else return false;
	return true;
}


/// @brief Utility function that, given a generic grid pointer, calls
/// a functor on the fully-resolved grid, provided that the grid's
/// voxel values are scalars
///
/// Usage:
/// @code
/// struct PruneOp {
///     template<typename GridT>
///     void operator()(typename GridT::Ptr grid) const { grid->tree()->prune(); }
/// };
///
/// processTypedScalarGrid(myGridPtr, PruneOp());
/// @endcode
///
/// @return @c false if the grid type is unknown or non-scalar.
template<typename GridPtrType, typename OpType>
bool
processTypedScalarGrid(GridPtrType grid, OpType& op)
{
	using namespace openvdb;
	if (grid->template isType<FloatGrid>())       doProcessTypedGrid<FloatGrid>(grid, op);
	else if (grid->template isType<DoubleGrid>()) doProcessTypedGrid<DoubleGrid>(grid, op);
	else if (grid->template isType<Int32Grid>())  doProcessTypedGrid<Int32Grid>(grid, op);
	else if (grid->template isType<Int64Grid>())  doProcessTypedGrid<Int64Grid>(grid, op);
	else return false;
	return true;
}


/// @brief Utility function that, given a generic grid pointer, calls
/// a functor on the fully-resolved grid, provided that the grid's
/// voxel values are vectors
template<typename GridPtrType, typename OpType>
bool
processTypedVectorGrid(GridPtrType grid, OpType& op)
{
	using namespace openvdb;
	if (grid->template isType<Vec3IGrid>())       doProcessTypedGrid<Vec3IGrid>(grid, op);
	else if (grid->template isType<Vec3SGrid>()) doProcessTypedGrid<Vec3SGrid>(grid, op);
	else if (grid->template isType<Vec3DGrid>())  doProcessTypedGrid<Vec3DGrid>(grid, op);
	else return false;
	return true;
}






template<typename Sampler>
class GridTransformOp
{
public:
	/// @param outGrid  a generic pointer to an output grid of the same type
	///     as the grid to be resampled
	/// @param t        a @c GridTransformer that defines an affine transform
	/// @note GridTransformOp makes an internal copy of the @c GridTransformer
	/// and supplies the copy with a default Interrupter that replaces any
	/// existing interrupter.
	GridTransformOp(GridPtr& outGrid, const openvdb::tools::GridTransformer& t):
	mOutGrid(outGrid), mTransformer(t) {}
	
	template<typename GridType>
	void operator()(const GridType& inGrid)
	{
		typename GridType::Ptr outGrid = openvdb::gridPtrCast<GridType>(mOutGrid);
		
		openvdb::util::NullInterrupter interrupter;
		mTransformer.setInterrupter(interrupter);
		
		mTransformer.transformGrid<Sampler, GridType>(inGrid, *outGrid);
	}
	
private:
	GridPtr mOutGrid;
	openvdb::tools::GridTransformer mTransformer;
};


template<typename Sampler>
class GridResampleToMatchOp
{
public:
	GridResampleToMatchOp(GridPtr outGrid): mOutGrid(outGrid) {}
	
	template<typename GridType>
	void operator()(const GridType& inGrid)
	{
		typename GridType::Ptr outGrid = openvdb::gridPtrCast<GridType>(mOutGrid);
		openvdb::util::NullInterrupter interrupter;
		openvdb::tools::resampleToMatch<Sampler>(inGrid, *outGrid, interrupter);
	}
	
private:
	GridPtr mOutGrid;
};
