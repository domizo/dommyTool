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
//  DOpenVDBCommon.cpp
//  dommyTool
//
//  Created by yuzo on 11/28/16.
//
//

#include "DOpenVDBCommon.h"


void convert(openvdb::FloatGrid::Ptr outputGrid,
			 ParticleList& paList,
			 const Settings& settings,
			 float Rmin,
			 float Rmax,
			 bool prune) {
	
	openvdb::tools::ParticlesToLevelSet<openvdb::FloatGrid, void> raster(*outputGrid);
	
	raster.setRmin(Rmin);
	raster.setRmax(Rmax);
	
	if (settings.mRasterizeTrails && paList.hasVelocity()) {
		raster.rasterizeTrails(paList, settings.mDx);
	} else if (paList.hasRadius()){
		raster.rasterizeSpheres(paList);
	} else {
		raster.rasterizeSpheres(paList, paList.radiusMult());
	}
	
	raster.finalize(prune);
	
	if (raster.ignoredParticles()) {
		std::ostringstream ostr;
		ostr << "Ignored " << raster.getMinCount() << " small and " << raster.getMaxCount()
		<< " large particles (hint: change Minimum Radius in Voxels)";
		MGlobal::displayError(ostr.str().c_str());
	}
}



