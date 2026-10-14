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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBData.h

//
//  DOpenVDBData.hpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#pragma once

#include <cstddef>
#include <openvdb/Platform.h>
#include <openvdb/openvdb.h>
#include <openvdb/Grid.h>



#include <maya/MPxData.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>

#include <iosfwd>

#include "common/utility/DMayaPluginData.h"

class DOpenVDBData final : public MPxData
{
public:
	DOpenVDBData();
	~DOpenVDBData() override;
	
	size_t numberOfGrids() const;
	bool hasValidData() const;
	
	/// @brief  return a constant reference to the specified grid.
	const openvdb::GridBase& grid(size_t index) const;
	
	/// @brief  return a constant pointer to the specified grid.
	openvdb::GridBase::ConstPtr gridPtr(size_t index) const;
	
	/// @brief Clears this container and duplicates @c rhs.
	/// @throws std::exception on invalid input, allocation, or grid-copy failure.
	void duplicate(const DOpenVDBData& rhs);
	/// @brief Append the given grid with the strong exception guarantee.
	void insert(const openvdb::GridBase::ConstPtr&);
	/// @brief Append a shallow copy with the strong exception guarantee.
	void insert(const openvdb::GridBase&);
	/// @brief Append shallow copies with the strong exception guarantee.
	void insert(const openvdb::GridPtrVec&);
	/// @brief Append shallow copies with the strong exception guarantee.
	void insert(const openvdb::GridCPtrVec&);
	void write(const openvdb::io::File& file,
			   const openvdb::MetaMap& = openvdb::MetaMap()) const;
	
	
	/// @{
	/// required maya interface methods
	static void* creator();
	
	MStatus readASCII(const MArgList&, unsigned&) override;
	MStatus writeASCII(ostream&) override;
	
	MStatus readBinary(istream&, unsigned length) override;
	MStatus writeBinary(ostream&) override;
	
	void copy(const MPxData&) override;
    // Checked internal copy; retains shared const grids and preserves the
    // destination on failure. The void Maya adapter cannot propagate errors.
    MStatus copyFrom(const DOpenVDBData& source);
	MTypeId typeId() const override;
	MString name() const override;
	
	static const MString typeName;
	static const MTypeId id;
	/// @}
	
private:
	openvdb::GridCPtrVec mGrids;
};
