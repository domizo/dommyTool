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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb/viewer/RenderModules.cc

//
//  DOpenVDBUtil.cpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#include "DOpenVDBUtil.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/file/DSequencePath.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"

#include <openvdb/math/Math.h>

#include <stdexcept>

#include <maya/MGlobal.h>


MStatus getValidInputVDB(const MObject& attribute, MDataBlock& data,
                         const DOpenVDBData*& result) {
    result = nullptr;
    MStatus status;
    const auto handle = data.inputValue(attribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DOpenVDBData::id, result));
    return result && result->hasValidData() ? MS::kSuccess : MS::kInvalidParameter;
}

const DOpenVDBData* getInputVDB(const MObject& vdb, MDataBlock& data)
{
	MStatus status;
	MDataHandle inputVdbHandle = data.inputValue(vdb, &status);
	if (!status) return NULL;

	const DOpenVDBData* inputData = NULL;
	status = dommy::maya::getPluginDataFromHandle(
		inputVdbHandle, DOpenVDBData::id, inputData);
	return status ? inputData : NULL;
}

const DOpenVDBData* getInputVDBfromHandle(MDataHandle handle)
{
    const DOpenVDBData* inputData = NULL;
    const MStatus status = dommy::maya::getPluginDataFromHandle(
        handle, DOpenVDBData::id, inputData);
    return status ? inputData : NULL;
}

MStatus createOpenVDBOutputData(
    MFnPluginData& pluginData,
    DOpenVDBData*& outputVdb)
{
    outputVdb = nullptr;

    MStatus status;
    pluginData.create(DOpenVDBData::id, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    return dommy::maya::getPluginData(pluginData, outputVdb);
}

MStatus publishOpenVDBOutput(
    MDataBlock& data,
    const MObject& outputAttribute,
    const MPlug& requestedPlug,
    const MObject& outputObject)
{
    if (outputObject.isNull()) {
        return MS::kFailure;
    }

    MStatus status;
    MDataHandle outputHandle = data.outputValue(outputAttribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    status = dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
        outputHandle, outputObject, DOpenVDBData::id);
    if (status != MS::kSuccess) {
        return status;
    }
    return data.setClean(requestedPlug);
}

void getGrids(std::vector<openvdb::GridBase::ConstPtr>& grids,  const DOpenVDBData& vdb, const std::string& names)
{
    grids.clear();
    
    if (names.empty() || names == "*") {
        for (size_t n = 0, N = vdb.numberOfGrids(); n < N; ++n) {
            grids.push_back(vdb.gridPtr(n));
        }
    } else {
        for (size_t n = 0, N = vdb.numberOfGrids(); n < N; ++n) {
            if (vdb.grid(n).getName() == names) grids.push_back(vdb.gridPtr(n));
        }
    }
    
}


std::string getGridNames(const DOpenVDBData& vdb)
{
	std::vector<std::string> names;
	for (size_t n = 0, N = vdb.numberOfGrids(); n < N; ++n) {
		names.push_back(vdb.grid(n).getName());
	}
	
	return boost::algorithm::join(names, " ");
}


namespace {

bool matchesGridName(const std::string& pattern, const std::string& gridName)
{
	size_t patternIndex = 0;
	size_t nameIndex = 0;
	size_t starIndex = std::string::npos;
	size_t retryNameIndex = 0;

	while (nameIndex < gridName.size()) {
		if (patternIndex < pattern.size() &&
			pattern[patternIndex] == gridName[nameIndex]) {
			++patternIndex;
			++nameIndex;
		} else if (patternIndex < pattern.size() &&
			pattern[patternIndex] == '*') {
			starIndex = patternIndex++;
			retryNameIndex = nameIndex;
		} else if (starIndex != std::string::npos) {
			patternIndex = starIndex + 1;
			nameIndex = ++retryNameIndex;
		} else {
			return false;
		}
	}

	while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
		++patternIndex;
	}
	return patternIndex == pattern.size();
}

} // namespace

bool containsGrid(const std::vector<std::string>& selectionList, const std::string& gridName, size_t gridIndex)
{
	bool hasSelector = false;
	for (size_t n = 0, N = selectionList.size(); n < N; ++n) {
		
		const std::string& word = selectionList[n];
		if (word.empty()) continue;
		hasSelector = true;
		// Legacy vector-merge defaults use an explicit name selector. Keep it
		// distinct from numeric index tokens (e.g. @name=12 names a grid).
		if (word.compare(0, 6, "@name=") == 0) {
			if (matchesGridName(word.substr(6), gridName)) return true;
			continue;
		}
		
		try {
			if (boost::lexical_cast<size_t>(word) == gridIndex) return true;
			continue;
			
		} catch (const boost::bad_lexical_cast&) {
			if (matchesGridName(word, gridName)) return true;
		}
	}
	
	// Empty or whitespace-only selection strings conventionally mean "all
	// grids" in these nodes. boost::split() represents an empty string as one
	// empty token, so checking selectionList.empty() alone is insufficient.
	return !hasSelector;
}


bool
getSelectedGrids(GridCPtrVec& grids, const std::string& selection, const DOpenVDBData& inputVdb, DOpenVDBData& outputVdb)
{
	grids.clear();
	
	std::vector<std::string> selectionList;
	boost::split(selectionList, selection, boost::is_any_of(" "));
	
	for (size_t n = 0, N = inputVdb.numberOfGrids(); n < N; ++n) {
		
		GridCRef grid = inputVdb.grid(n);
		
		if (containsGrid(selectionList, grid.getName(), n)) {
			grids.push_back(inputVdb.gridPtr(n));
		} else {
			outputVdb.insert(grid);
		}
	}
	
	return !grids.empty();
}


bool getSelectedGrids(GridCPtrVec& grids, const std::string& selection, const DOpenVDBData& inputVdb)
{
	grids.clear();
	
	std::vector<std::string> selectionList;
	boost::split(selectionList, selection, boost::is_any_of(" "));
	
	for (size_t n = 0, N = inputVdb.numberOfGrids(); n < N; ++n) {
		
		GridCRef grid = inputVdb.grid(n);
		
		if (containsGrid(selectionList, grid.getName(), n)) {
			grids.push_back(inputVdb.gridPtr(n));
		}
	}
	
	return !grids.empty();
}

bool getActiveVoxelWorldBounds(
    const openvdb::GridBase& grid,
    openvdb::BBoxd& worldBounds)
{
    return getActiveVoxelWorldBounds(grid, worldBounds, true);
}

bool getActiveVoxelWorldBounds(
    const openvdb::GridBase& grid,
    openvdb::BBoxd& worldBounds,
    bool includeVoxelExtent)
{
    const openvdb::CoordBBox activeBounds = grid.evalActiveVoxelBoundingBox();
    if (activeBounds.empty()) {
        worldBounds = openvdb::BBoxd();
        return false;
    }

    const openvdb::Vec3d halfVoxel(includeVoxelExtent ? 0.5 : 0.0);
    const openvdb::BBoxd indexBounds(
        activeBounds.min().asVec3d() - halfVoxel,
        activeBounds.max().asVec3d() + halfVoxel);
    worldBounds = grid.transform().indexToWorld(indexBounds);
    return !worldBounds.empty();
}


////////////////////////////////////////


void printGridInfo(std::ostream& os, const DOpenVDBData& vdb)
{
	os << "\nOutput " << vdb.numberOfGrids() << " VDB(s)\n";
	
	size_t memUsage = 0, idx = 0;
	for (size_t n = 0, N = vdb.numberOfGrids(); n < N; ++n) {
		
		const openvdb::GridBase& grid = vdb.grid(n);
		
		memUsage += grid.memUsage();
		openvdb::Coord dim = grid.evalActiveVoxelDim();
		
		os << "[" << idx++ << "]";
		
		if (!grid.getName().empty()) os << " '" << grid.getName() << "'";
		
		os << " voxel size: " << grid.voxelSize()[0] << ", type: "
		<< grid.valueType() << ", dim: "
		<< dim[0] << "x" << dim[1] << "x" << dim[2] <<"\n";
	}
	
	openvdb::util::printBytes(os, memUsage, "\nApproximate Memory Usage:");
}


MStatus updateNodeInfo(std::stringstream& stream, MDataBlock& data, const MObject& strAttr)
{
    if (!stream) return MS::kFailure;
    const MString str = stream.str().c_str();
    MStatus status;
    MDataHandle strHandle = data.outputValue(strAttr, &status);
    if (!status) return status;
    strHandle.set(str);
    return data.setClean(strAttr);
}

bool insertFrameNumber(std::string& str, const MTime& time, int numberingScheme)
{
	const double frame = time.as(MTime::uiUnit());
	const MTime oneSecond(1.0, MTime::kSeconds);
	const double framesPerSecond = oneSecond.as(MTime::uiUnit());
	std::string resolved;
	if (!dommy::file::replaceHashFrameToken(
			str, frame, framesPerSecond, numberingScheme, resolved)) {
		return false;
	}
	str.swap(resolved);
	return true;
}


////////////////////////////////////////


BufferObject::BufferObject():
mVertexBuffer(0),
mNormalBuffer(0),
mIndexBuffer(0),
mColorBuffer(0),
mPrimType(GL_POINTS),
mPrimNum(0)
{
}

BufferObject::~BufferObject() { clear(); }

void BufferObject::render() const
{
	if (mPrimNum == 0 || !glIsBuffer(mIndexBuffer) || !glIsBuffer(mVertexBuffer)) {
		OPENVDB_LOG_DEBUG_RUNTIME("request to render empty or uninitialized buffer");
		return;
	}
	
	const bool usesColorBuffer = glIsBuffer(mColorBuffer);
	const bool usesNormalBuffer = glIsBuffer(mNormalBuffer);
	
	glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	
	if (usesColorBuffer) {
		glBindBuffer(GL_ARRAY_BUFFER, mColorBuffer);
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(3, GL_FLOAT, 0, 0);
	}
	
	if (usesNormalBuffer) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, mNormalBuffer);
		glNormalPointer(GL_FLOAT, 0, 0);
	}
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
	glDrawElements(mPrimType, mPrimNum, GL_UNSIGNED_INT, 0);
	
	// disable client-side capabilities
	if (usesColorBuffer) glDisableClientState(GL_COLOR_ARRAY);
	if (usesNormalBuffer) glDisableClientState(GL_NORMAL_ARRAY);
	
	// release vbo's
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void BufferObject::genIndexBuffer(const std::vector<GLuint>& v, GLenum primType)
{
	// clear old buffer
	if (glIsBuffer(mIndexBuffer) == GL_TRUE) glDeleteBuffers(1, &mIndexBuffer);
	
	// gen new buffer
	glGenBuffers(1, &mIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
	if (glIsBuffer(mIndexBuffer) == GL_FALSE) throw std::runtime_error("Unable to create index buffer");
	
	// upload data
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
				 sizeof(GLuint) * v.size(), v.data(), GL_STATIC_DRAW); // upload data
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to upload index buffer data");
	
	// release buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	mPrimNum = v.size();
	mPrimType = primType;
}

void BufferObject::genVertexBuffer(const std::vector<GLfloat>& v)
{
	if (glIsBuffer(mVertexBuffer) == GL_TRUE) glDeleteBuffers(1, &mVertexBuffer);
	
	glGenBuffers(1, &mVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
	if (glIsBuffer(mVertexBuffer) == GL_FALSE) throw std::runtime_error("Unable to create vertex buffer");
	
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * v.size(), v.data(), GL_STATIC_DRAW);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to upload vertex buffer data");
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BufferObject::genNormalBuffer(const std::vector<GLfloat>& v)
{
	if (glIsBuffer(mNormalBuffer) == GL_TRUE) glDeleteBuffers(1, &mNormalBuffer);
	
	glGenBuffers(1, &mNormalBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mNormalBuffer);
	if (glIsBuffer(mNormalBuffer) == GL_FALSE) throw std::runtime_error("Unable to create normal buffer");
	
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * v.size(), v.data(), GL_STATIC_DRAW);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to upload normal buffer data");
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BufferObject::genColorBuffer(const std::vector<GLfloat>& v)
{
	if (glIsBuffer(mColorBuffer) == GL_TRUE) glDeleteBuffers(1, &mColorBuffer);
	
	glGenBuffers(1, &mColorBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mColorBuffer);
	if (glIsBuffer(mColorBuffer) == GL_FALSE) throw std::runtime_error("Unable to create color buffer");
	
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * v.size(), v.data(), GL_STATIC_DRAW);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to upload color buffer data");
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BufferObject::clear()
{
	if (glIsBuffer(mIndexBuffer) == GL_TRUE) glDeleteBuffers(1, &mIndexBuffer);
	if (glIsBuffer(mVertexBuffer) == GL_TRUE) glDeleteBuffers(1, &mVertexBuffer);
	if (glIsBuffer(mColorBuffer) == GL_TRUE) glDeleteBuffers(1, &mColorBuffer);
	if (glIsBuffer(mNormalBuffer) == GL_TRUE) glDeleteBuffers(1, &mNormalBuffer);
	
	mPrimType = GL_POINTS;
	mPrimNum = 0;
}

////////////////////////////////////////

#pragma mark BufferObjectVP2

BufferObjectVP2::BufferObjectVP2()
{
	
}

BufferObjectVP2::~BufferObjectVP2()
{
	clear();
}



void BufferObjectVP2::clear() {
	points.clear();
    normals.clear();
	indexArray.clear();
}

#pragma mark valueBufferObjectVP2

valueBufferObjectVP2::valueBufferObjectVP2()
{
    
}

valueBufferObjectVP2::~valueBufferObjectVP2()
{
    clear();
}



void valueBufferObjectVP2::clear() {
    valueArray.clear();
    points.clear();
	normals.clear();
	colors.clear();
    indexArray.clear();
    
}

void valueBufferObjectVP2::swap(valueBufferObjectVP2& other) {
    valueArray.swap(other.valueArray);
    points.swap(other.points);
    normals.swap(other.normals);
    colors.swap(other.colors);
    indexArray.swap(other.indexArray);
}


////////////////////////////////////////

ShaderProgram::ShaderProgram():
mProgram(0),
mVertShader(0),
mFragShader(0)
{
}

ShaderProgram::~ShaderProgram() { clear(); }

void ShaderProgram::setVertShader(const std::string& s)
{
	mVertShader = glCreateShader(GL_VERTEX_SHADER);
	if (glIsShader(mVertShader) == GL_FALSE) throw std::runtime_error("Unable to create shader program");
	
	GLint length = s.length();
	const char *str = s.c_str();
	glShaderSource(mVertShader, 1, &str, &length);
	
	glCompileShader(mVertShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to compile vertex shader");
}

void ShaderProgram::setFragShader(const std::string& s)
{
	mFragShader = glCreateShader(GL_FRAGMENT_SHADER);
	if (glIsShader(mFragShader) == GL_FALSE) throw std::runtime_error("Unable to create shader program");
	
	GLint length = s.length();
	const char *str = s.c_str();
	glShaderSource(mFragShader, 1, &str, &length);
	
	glCompileShader(mFragShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to compile fragment shader");
}

void ShaderProgram::build()
{
	mProgram = glCreateProgram();
	if (glIsProgram(mProgram) == GL_FALSE) throw std::runtime_error("Unable to create shader program");
	
	if (glIsShader(mVertShader) == GL_TRUE) glAttachShader(mProgram, mVertShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to attach vertex shader");
	
	if (glIsShader(mFragShader) == GL_TRUE) glAttachShader(mProgram, mFragShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to attach fragment shader");
	
	
	glLinkProgram(mProgram);
	
	GLint linked;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
	
	if (!linked) throw std::runtime_error("Unable to link shader program");
}

void ShaderProgram::build(const std::vector<GLchar*>& attributes)
{
	mProgram = glCreateProgram();
	if (glIsProgram(mProgram) == GL_FALSE) throw std::runtime_error("Unable to create shader program");
	
	
	for (GLuint n = 0, N = attributes.size(); n < N; ++n) {
		glBindAttribLocation(mProgram, n, attributes[n]);
	}
	
	if (glIsShader(mVertShader) == GL_TRUE) glAttachShader(mProgram, mVertShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to attach vertex shader");
	
	if (glIsShader(mFragShader) == GL_TRUE) glAttachShader(mProgram, mFragShader);
	if (GL_NO_ERROR != glGetError()) throw std::runtime_error("Unable to attach fragment shader");
	
	
	glLinkProgram(mProgram);
	
	GLint linked;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
	
	if (!linked) throw std::runtime_error("Unable to link shader program");
}

void ShaderProgram::startShading() const
{
	if (glIsProgram(mProgram) == GL_FALSE)
		throw std::runtime_error("startShading() called on an uncompiled shader program");
	
	glUseProgram(mProgram);
}

void ShaderProgram::stopShading() const
{
	glUseProgram(0);
}

void ShaderProgram::clear()
{
	GLsizei numShaders;
	GLuint shaders[2];
	
	glGetAttachedShaders(mProgram, 2, &numShaders, shaders);
	
	// detach and remove shaders
	for (GLsizei n = 0; n < numShaders; ++n) {
		
		glDetachShader(mProgram, shaders[n]);
		
		if (glIsShader(shaders[n]) == GL_TRUE) glDeleteShader(shaders[n]);
	}
	
	// remove program
	if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
}


////////////////////////////////////////

WireBoxBuilder::WireBoxBuilder(
							   const openvdb::math::Transform& xform,
							   std::vector<GLuint>& indices,
							   std::vector<GLfloat>& points,
							   std::vector<GLfloat>& colors)
: mXForm(&xform)
, mIndices(&indices)
, mPoints(&points)
, mColors(&colors)
{
}

void WireBoxBuilder::add(GLuint boxIndex, const openvdb::CoordBBox& bbox, const openvdb::Vec3s& color)
{
    if (bbox.empty()) {
        throw std::invalid_argument("WireBoxBuilder received empty bounds");
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(color[axis])) {
            throw std::invalid_argument("WireBoxBuilder received non-finite color");
        }
    }

    size_t vertexBase = 0, lastVertex = 0, componentBase = 0, componentEnd = 0;
    if (!dommy::memory::checkedMultiply(static_cast<size_t>(boxIndex), 8U, vertexBase) ||
        !dommy::memory::checkedAdd(vertexBase, 7U, lastVertex) ||
        lastVertex > std::numeric_limits<GLuint>::max() ||
        !dommy::memory::checkedMultiply(static_cast<size_t>(boxIndex), 24U, componentBase) ||
        !dommy::memory::checkedAdd(componentBase, 24U, componentEnd) ||
        componentEnd > mPoints->size() || componentEnd > mColors->size() ||
        componentEnd > mIndices->size()) {
        throw std::length_error("WireBoxBuilder output range is invalid");
    }

    // Keep the historical oriented-corner/edge order and half-cell padding.
    const openvdb::Vec3d minimum(bbox.min().x() - 0.5, bbox.min().y() - 0.5,
                                 bbox.min().z() - 0.5);
    const openvdb::Vec3d maximum(bbox.max().x() + 0.5, bbox.max().y() + 0.5,
                                 bbox.max().z() + 0.5);
    static const unsigned int corners[8][3] = {
        {0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0},
        {0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}
    };
    static const unsigned int edges[24] = {
        0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
        6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7
    };
    std::array<GLfloat, 24> points, colors;
    std::array<GLuint, 24> indices;
    for (size_t corner = 0; corner < 8U; ++corner) {
        openvdb::Vec3d point;
        for (int axis = 0; axis < 3; ++axis) {
            point[axis] = corners[corner][axis] ? maximum[axis] : minimum[axis];
        }
        point = mXForm->indexToWorld(point);
        for (int axis = 0; axis < 3; ++axis) {
            const size_t component = corner * 3U + static_cast<size_t>(axis);
            if (!dommy::numeric::checkedFloatCast(point[axis], points[component])) {
                throw std::invalid_argument("WireBoxBuilder world coordinate is not float-representable");
            }
            colors[component] = color[axis];
        }
    }
    for (size_t edge = 0; edge < 24U; ++edge) {
        indices[edge] = static_cast<GLuint>(vertexBase + edges[edge]);
    }
    // Nothing is written until every transform/narrowing operation succeeds.
    std::copy(points.begin(), points.end(), mPoints->begin() + componentBase);
    std::copy(colors.begin(), colors.end(), mColors->begin() + componentBase);
    std::copy(indices.begin(), indices.end(), mIndices->begin() + componentBase);
}
