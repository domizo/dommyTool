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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBData.cc

//
//  DOpenVDBData.cpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBData.h"

#include <openvdb/io/Stream.h>

#include <maya/MIOStream.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>

#include "dvdb_ids.h"
#include "../Common/DOpenVDBLimits.h"
#include "common/utility/DCheckedSize.h"

#include <stdexcept>

namespace {
template <typename GridVector>
bool hasValidGridBudget(const GridVector& grids)
{
	if (grids.size() > dommy::vdb::kMaxVdbGridCount) return false;
	std::size_t totalBytes = 0U;
	for (typename GridVector::const_iterator it = grids.begin();
		 it != grids.end(); ++it) {
		if (!*it) return false;
		std::size_t updatedBytes = 0U;
		if (!dommy::memory::checkedAdd(
				totalBytes, static_cast<std::size_t>((*it)->memUsage()),
				updatedBytes) ||
			updatedBytes > dommy::vdb::kMaxVdbPayloadBytes) {
			return false;
		}
		totalBytes = updatedBytes;
	}
	return true;
}
}


const MTypeId DOpenVDBData::id(ID_VDB_DOPENVDB_DATA);
const MString DOpenVDBData::typeName("DOpenVDBData");

bool DOpenVDBData::hasValidData() const
{
	return hasValidGridBudget(mGrids);
}

void* DOpenVDBData::creator()
{
    return ::dommy::maya::createObject("DOpenVDBData", [&]() {
        return new DOpenVDBData();
    });
}


DOpenVDBData::DOpenVDBData(): MPxData()
{
}

DOpenVDBData::~DOpenVDBData() = default;



void DOpenVDBData::copy(const MPxData& other)
{
    const DOpenVDBData* source = dynamic_cast<const DOpenVDBData*>(&other);
    if (!source || other.typeId() != DOpenVDBData::id || !copyFrom(*source)) {
        MGlobal::displayError(
            "DOpenVDBData::copy failed; destination was left unchanged.");
    }
}

MStatus DOpenVDBData::copyFrom(const DOpenVDBData& source) try
{
    if (!hasValidGridBudget(source.mGrids)) return MS::kInvalidParameter;
    if (this == &source) return MS::kSuccess;
    openvdb::GridCPtrVec copied(source.mGrids);
    mGrids.swap(copied);
    return MS::kSuccess;
} catch (...) {
    return MS::kFailure;
}

MTypeId DOpenVDBData::typeId() const
{
	return DOpenVDBData::id;
}


MString DOpenVDBData::name() const
{
	return DOpenVDBData::typeName;
}


MStatus DOpenVDBData::readASCII(const MArgList&, unsigned&)
{
	return MS::kFailure;
}


MStatus DOpenVDBData::writeASCII(ostream&)
{
	return MS::kFailure;
}

MStatus DOpenVDBData::readBinary(istream& in, unsigned length)
{
	try {
		if (length > dommy::vdb::kMaxVdbPayloadBytes)
			return MS::kFailure;
		const openvdb::GridPtrVecPtr grids = openvdb::io::Stream(in).getGrids();
		if (!grids || in.fail() || !hasValidGridBudget(*grids))
			return MS::kFailure;

		openvdb::GridCPtrVec decoded;
		decoded.reserve(grids->size());
		for (openvdb::GridPtrVec::const_iterator it = grids->begin();
		     it != grids->end(); ++it) {
			decoded.push_back((*it)->copyGrid());
		}
		if (!hasValidGridBudget(decoded))
			return MS::kFailure;
		mGrids.swap(decoded);
		return MS::kSuccess;
	} catch (...) {
		return MS::kFailure;
	}
}


MStatus DOpenVDBData::writeBinary(ostream& out)
{
	try {
		if (!hasValidGridBudget(mGrids))
			return MS::kFailure;
		openvdb::io::Stream(out).write(mGrids);
		return out.fail() ? MS::kFailure : MS::kSuccess;
	} catch (...) {
		return MS::kFailure;
	}
}


size_t DOpenVDBData::numberOfGrids() const
{
	return mGrids.size();
}


const openvdb::GridBase& DOpenVDBData::grid(size_t index) const
{
	return *(mGrids.at(index));
}


openvdb::GridBase::ConstPtr DOpenVDBData::gridPtr(size_t index) const
{
	return mGrids.at(index);
	
}


void DOpenVDBData::duplicate(const DOpenVDBData& rhs)
{
	if (!hasValidGridBudget(rhs.mGrids)) {
		throw std::length_error("DOpenVDBData::duplicate grid count exceeds the safety limit");
	}
	openvdb::GridCPtrVec duplicated;
	duplicated.reserve(rhs.mGrids.size());
	for (openvdb::GridCPtrVec::const_iterator it = rhs.mGrids.begin();
	     it != rhs.mGrids.end(); ++it) {
		duplicated.push_back((*it)->copyGrid());
	}
	if (!hasValidGridBudget(duplicated)) {
		throw std::length_error("DOpenVDBData::duplicate payload exceeds the safety limit");
	}
	mGrids.swap(duplicated);
}

void DOpenVDBData::insert(const openvdb::GridBase::ConstPtr& grid)
{
	if (!grid) {
		throw std::invalid_argument("DOpenVDBData::insert received a null grid");
	}
	openvdb::GridCPtrVec updated(mGrids);
	updated.push_back(grid);
	if (!hasValidGridBudget(updated)) {
		throw std::length_error("DOpenVDBData::insert payload exceeds the safety limit");
	}
	mGrids.swap(updated);
}

void DOpenVDBData::insert(const openvdb::GridBase& grid)
{
	openvdb::GridCPtrVec updated(mGrids);
	updated.push_back(grid.copyGrid());
	if (!hasValidGridBudget(updated)) {
		throw std::length_error("DOpenVDBData::insert payload exceeds the safety limit");
	}
	mGrids.swap(updated);
}


void DOpenVDBData::insert(const openvdb::GridPtrVec& rhs)
{
	std::size_t combinedSize = 0;
	if (!dommy::memory::checkedAdd(mGrids.size(), rhs.size(), combinedSize)) {
		throw std::length_error("DOpenVDBData::insert grid count overflow");
	}
	if (combinedSize > dommy::vdb::kMaxVdbGridCount) {
		throw std::length_error("DOpenVDBData::insert grid count exceeds the safety limit");
	}
	for (openvdb::GridPtrVec::const_iterator it = rhs.begin(); it != rhs.end(); ++it) {
		if (!*it) {
			throw std::invalid_argument("DOpenVDBData::insert received a null grid");
		}
	}
	openvdb::GridCPtrVec updated(mGrids);
	updated.reserve(combinedSize);
	for (openvdb::GridPtrVec::const_iterator it = rhs.begin(); it != rhs.end(); ++it) {
		updated.push_back((*it)->copyGrid());
	}
	if (!hasValidGridBudget(updated)) {
		throw std::length_error("DOpenVDBData::insert payload exceeds the safety limit");
	}
	mGrids.swap(updated);
}


void DOpenVDBData::insert(const openvdb::GridCPtrVec& rhs)
{
	std::size_t combinedSize = 0;
	if (!dommy::memory::checkedAdd(mGrids.size(), rhs.size(), combinedSize)) {
		throw std::length_error("DOpenVDBData::insert grid count overflow");
	}
	if (combinedSize > dommy::vdb::kMaxVdbGridCount) {
		throw std::length_error("DOpenVDBData::insert grid count exceeds the safety limit");
	}
	for (openvdb::GridCPtrVec::const_iterator it = rhs.begin(); it != rhs.end(); ++it) {
		if (!*it) {
			throw std::invalid_argument("DOpenVDBData::insert received a null grid");
		}
	}
	openvdb::GridCPtrVec updated(mGrids);
	updated.reserve(combinedSize);
	for (openvdb::GridCPtrVec::const_iterator it = rhs.begin(); it != rhs.end(); ++it) {
		updated.push_back((*it)->copyGrid());
	}
	if (!hasValidGridBudget(updated)) {
		throw std::length_error("DOpenVDBData::insert payload exceeds the safety limit");
	}
	mGrids.swap(updated);
}





void DOpenVDBData::write(const openvdb::io::File& file,
				   const openvdb::MetaMap& metadata) const
{
	if (!hasValidGridBudget(mGrids)) {
		throw std::length_error("DOpenVDBData::write payload exceeds the safety limit");
	}
	file.write(mGrids, metadata);
}
