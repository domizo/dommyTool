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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_From_Particles.cc

//
//  DOpenVDBCommon.hpp
//  dommyTool
//
//  Created by yuzo on 11/28/16.
//
//

#ifndef DOpenVDBCommon_hpp
#define DOpenVDBCommon_hpp

#include <cstddef>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <openvdb/Grid.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/ParticlesToLevelSet.h>
#include <openvdb/tools/TopologyToLevelSet.h>
#include <openvdb/tools/PointIndexGrid.h>

#include "DMayaHeaders.h"
#include "DOpenVDBData.h"


#include "DPData.h"
#include "DPAttributeData.h"


class ParticleList
{
public:
	// required typedef for bucketing
	typedef openvdb::Vec3R    PosType;
	
	ParticleList(const MPointArray& posArray,
				 const MDoubleArray& rArray,
				 const MVectorArray& velArray,
				 openvdb::Real radiusMult = 1,
				 openvdb::Real velocityMult = 1) :
	positions(posArray),
	radiusArray(rArray),
	velocityArray(velArray),
	mHasRadius(rArray.length() != 0U),
	mHasVelocity(velArray.length() != 0U),
	mRadiusMult(radiusMult),
	mVelocityMult(velocityMult)
	{
		const unsigned int count = positions.length();
		if (count > static_cast<unsigned int>(
				std::numeric_limits<openvdb::Int32>::max())) {
			throw std::length_error(
				"OpenVDB particle count exceeds the attribute index range");
		}
		if (mHasRadius && radiusArray.length() != count) {
			throw std::invalid_argument(
				"OpenVDB particle radii must match the position count");
		}
		if (mHasVelocity && velocityArray.length() != count) {
			throw std::invalid_argument(
				"OpenVDB particle velocities must match the position count");
		}
		if (!std::isfinite(mRadiusMult) || mRadiusMult < 0.0 ||
			!std::isfinite(mVelocityMult)) {
			throw std::invalid_argument(
				"OpenVDB particle multipliers must be finite");
		}

		if (!mHasRadius) {
			radiusArray = MDoubleArray(count, 1.0);
		}
		if (!mHasVelocity) {
			velocityArray =
				MVectorArray(count, MVector(0.0, 0.0, 0.0));
		}

		for (unsigned int index = 0U; index < count; ++index) {
			const MPoint& position = positions[index];
			const double radius = radiusArray[index];
			const MVector& velocity = velocityArray[index];
			if (!std::isfinite(position.x) ||
				!std::isfinite(position.y) ||
				!std::isfinite(position.z) ||
				!std::isfinite(radius) || radius < 0.0 ||
				!std::isfinite(radius * mRadiusMult) ||
				!std::isfinite(velocity.x) ||
				!std::isfinite(velocity.y) ||
				!std::isfinite(velocity.z) ||
				!std::isfinite(velocity.x * mVelocityMult) ||
				!std::isfinite(velocity.y * mVelocityMult) ||
				!std::isfinite(velocity.z * mVelocityMult)) {
				throw std::invalid_argument(
					"OpenVDB particle data must be finite and radii non-negative");
			}
		}
	}
	
	// Do the particles have non-constant radius
	bool hasRadius()   const { return mHasRadius;}
	
	// Do the particles have velocity
	bool hasVelocity() const { return mHasVelocity;}
	
	// Multiplier for the radius
	openvdb::Real radiusMult() const { return mRadiusMult; }

	bool scaleRadiusMultiplier(openvdb::Real factor)
	{
		if (!std::isfinite(factor) || factor < 0.0) {
			return false;
		}
		const openvdb::Real scaled = mRadiusMult * factor;
		if (!std::isfinite(scaled)) {
			return false;
		}
		for (unsigned int index = 0U;
			 index < radiusArray.length(); ++index) {
			if (!std::isfinite(radiusArray[index] * scaled)) {
				return false;
			}
		}
		mRadiusMult = scaled;
		return true;
	}
	
	// The public methods below are the only ones required
	// by tools::ParticlesToLevelSet
	size_t size() const { return positions.length(); }
	
	// Position of particle in world space
	// This is required by ParticlesToLevelSet::rasterizeSpheres(*this,radius)
	void getPos(size_t n, openvdb::Vec3R& xyz) const
	{
		const MPoint p = positions[n];
        xyz[0] = p[0]; xyz[1] = p[1]; xyz[2] = p[2];
	}
	// Position and radius of particle in world space
	// This is required by ParticlesToLevelSet::rasterizeSpheres(*this)
	void getPosRad(size_t n, openvdb::Vec3R& xyz, openvdb::Real& rad) const
	{
		const MPoint p = positions[n];
        xyz[0] = p[0]; xyz[1] = p[1]; xyz[2] = p[2];
		rad = mRadiusMult*radiusArray[n];
	}
	// Position, radius and velocity of particle in world space
	// This is required by ParticlesToLevelSet::rasterizeTrails
	void getPosRadVel(size_t n, openvdb::Vec3R& xyz,
					  openvdb::Real& rad, openvdb::Vec3R& vel) const
	{
		const MPoint p = positions[n];
        xyz[0] = p[0]; xyz[1] = p[1]; xyz[2] = p[2];
		rad = mHasRadius ? mRadiusMult*radiusArray[n] : mRadiusMult;
		const MVector v = velocityArray[n];
        vel[0] = mVelocityMult*v[0]; vel[1] = mVelocityMult*v[1]; vel[2] = mVelocityMult*v[2];
	}
	// Required for attribute transfer
	void getAtt(size_t n, openvdb::Int32& att) const { att = openvdb::Int32(n); }
	
protected:
	
	MPointArray positions;
	MDoubleArray radiusArray;
	MVectorArray velocityArray;
	
	const bool          mHasRadius, mHasVelocity;
	openvdb::Real       mRadiusMult; // multiplier for radius
	const openvdb::Real mVelocityMult; // multiplier for velocity
};// ParticleList

struct Settings
{
	Settings(): mRasterizeTrails(false), mDx(1.0) {}
	bool mRasterizeTrails;
	float mDx;
};




void convert(openvdb::FloatGrid::Ptr outputGrid,
			 ParticleList& paList,
			 const Settings& settings,
			 float Rmin,
			 float Rmax,
			 bool prune);


#endif /* DOpenVDBCommon_hpp */
