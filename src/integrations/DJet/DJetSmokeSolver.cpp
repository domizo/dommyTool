// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Walt Disney Studios
 
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
 
1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 
Written by: Nicola Candussi <nicola@fluidinteractive.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked

Modified by Francisco Gochez <fjgochez@gmail.com>
Nov 2011 - Dec 2011 : Added logic for soft bodies

Modified by Dongsoo Han <dongsoo.han@amd.com>
04/11/2012 : Added contactData attribute and contact information code to enable/disable updating contactCount, contactName and 
			 contactPosition attributes in rigidBodyNode.
*/

/*
The smoke-scene initialization in DJetSmokeSolver::initSolver is adapted from
runExample1 in fluid-engine-dev/src/examples/smoke_sim/main.cpp.
Copyright (c) 2016 Doyub Kim

The MIT License (MIT)

Copyright (c) 2017 Doyub Kim

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/dSolverNode.cpp
// Source comparison reference: https://github.com/doyubkim/fluid-engine-dev/blob/c667964eb8c86cc5956054f234822e11c0436d99/src/examples/smoke_sim/main.cpp
// Archive member: src/examples/smoke_sim/main.cpp

//
//  DJetSmokeSolver.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/24.
//

#include "DJetSmokeSolver.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFluidGridPlan.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DViewportPointBudget.h"

#if DOMMY_WITH_JET

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "dpoint_ids.h"

#include <cmath>


MTypeId DJetSmokeSolver::typeId( ID_DJET_SMOKE_SOLVER );
const MString DJetSmokeSolver::typeName("DJetSmokeSolver");


MObject DJetSmokeSolver::aEnabled;
MObject DJetSmokeSolver::aCurrentTime;
MObject DJetSmokeSolver::aStartTime;
MObject DJetSmokeSolver::aFrameRate;

MObject DJetSmokeSolver::aResolusion;
MObject DJetSmokeSolver::aGridSize;


MObject DJetSmokeSolver::aOutFluid;

DJetSmokeSolver::DJetSmokeSolver() = default;

DJetSmokeSolver::~DJetSmokeSolver() = default;

void *DJetSmokeSolver::creator() {
    return ::dommy::maya::createObject("DJetSmokeSolver", [&]() {
        return new DJetSmokeSolver();
    });
}

void DJetSmokeSolver::postConstructor()
{
    setExistWithoutOutConnections(true);
}

bool DJetSmokeSolver::setInternalValueInContext(
    const MPlug& /*plug*/,
    const MDataHandle& /*dataHandle*/,
    MDGContext& /*context*/)
{
    return false;
}

void DJetSmokeSolver::draw( M3dView & view, const MDagPath& /*path*/,
                        M3dView::DisplayStyle style,
                        M3dView::DisplayStatus status ) try
{
    if(!m_solver) return;
    dommy::maya::ScopedLegacyGL drawing(
        view, GL_CURRENT_BIT | GL_POINT_BIT);
    
    if ( ( style == M3dView::kFlatShaded ) ||
        ( style == M3dView::kGouraudShaded ) )
    {
        auto density = m_solver->smokeDensity();
        jet::Size3 res = density->resolution();
        auto pos = density->cellCenterPosition();
        
        if ( status == M3dView::kActive ) {
            view.setDrawColor( 13, M3dView::kActiveColors );
        } else {
            view.setDrawColor( 13, M3dView::kDormantColors );
        }
        
        glPointSize(3.0f);
        
        glBegin( GL_POINTS );

        std::size_t visitedCount = 0U;
        for (std::size_t x = 0U;
             x < res[0] &&
                 visitedCount < dommy::viewport::kMaxVdbViewportPointCount;
             ++x) {
            for (std::size_t y = 0U;
                 y < res[1] &&
                     visitedCount < dommy::viewport::kMaxVdbViewportPointCount;
                 ++y) {
                for (std::size_t z = 0U;
                     z < res[2] &&
                         visitedCount <
                             dommy::viewport::kMaxVdbViewportPointCount;
                     ++z, ++visitedCount) {

                    jet::Vector3D p = pos(x,y,z);
                    float d = static_cast<float>((*density)(x, y, z));
                    if(d>0.0) {
                        glColor4f(d, 0.0, 0.0, d);
                        glVertex3f(p[0],p[1],p[2]);
                    }
                }
            }
        }
        glEnd();
    }
}
DOMMY_DRAW_CATCH("DJetSmokeSolver")

#pragma mark solver method


MStatus DJetSmokeSolver::initSolver(MDataBlock& data) {
    MStatus status;
    MDataHandle resolutionHandle = data.inputValue(aResolusion, &status);
    if (status != MS::kSuccess) return status;
    const int3& res = resolutionHandle.asInt3();

    MDataHandle gridSizeHandle = data.inputValue(aGridSize, &status);
    if (status != MS::kSuccess) return status;
    const double3& gSize = gridSizeHandle.asDouble3();

    if (res[0] <= 0 || res[1] <= 0 || res[2] <= 0) {
        return MS::kInvalidParameter;
    }

    dommy::fluid::GridPlan gridPlan;
    if (!dommy::fluid::buildGridPlan(
            static_cast<unsigned int>(res[0]),
            static_cast<unsigned int>(res[1]),
            static_cast<unsigned int>(res[2]),
            gSize[0], gSize[1], gSize[2],
            0.0, 0.0, 0.0,
            2U, true,
            dommy::generation::kMaxWorkItemCount,
            gridPlan)) {
        return MS::kInvalidParameter;
    }
    
    //Smoke
    m_solver = jet::GridSmokeSolver3::builder()
        .withResolution({res[0], res[1], res[2]})
        .withDomainSizeX(gSize[0])
        .makeShared();
    
    m_solver->setAdvectionSolver(std::make_shared<jet::CubicSemiLagrangian3>());
    
    auto grids = m_solver->gridSystemData();
    jet::BoundingBox3D domain = grids->boundingBox();
    
    // Build emitter
    auto box = jet::Box3::builder()
    .withLowerCorner({0.45, -1, 0.45})
    .withUpperCorner({0.55, 0.05, 0.55})
    .makeShared();
    
    auto emitter = jet::VolumeGridEmitter3::builder()
    .withSourceRegion(box)
    .withIsOneShot(false)
    .makeShared();
    
    m_solver->setEmitter(emitter);
    emitter->addStepFunctionTarget(m_solver->smokeDensity(), 0, 1);
    emitter->addStepFunctionTarget(m_solver->temperature(), 0, 1);
    
    // Build collider
    auto sphere = jet::Sphere3::builder()
    .withCenter({0.5, 0.3, 0.5})
    .withRadius(0.075 * domain.width())
    .makeShared();
    
    auto collider = jet::RigidBodyCollider3::builder()
    .withSurface(sphere)
    .makeShared();
    
    m_solver->setCollider(collider);
    
    jet::Logging::mute();

    return m_solver ? MS::kSuccess : MS::kFailure;
}

void DJetSmokeSolver::clearSolver()  {
    if (!m_solver) {
        return;
    }
    
    DOMMY_MAYA_DEBUG("DJetSmokeSolver", "clearing solver");
    m_solver.reset();
}


#pragma mark main compute

MStatus DJetSmokeSolver::compute( const MPlug& plug, MDataBlock& data ) try
{
    DOMMY_MAYA_DEBUG(
        "DJetSmokeSolver", "compute plug: " << plug.name().asChar());
    if(plug == aOutFluid) {
        MStatus status;
        MDataHandle enabledHandle = data.inputValue(aEnabled, &status);
        if (status != MS::kSuccess) return status;
        const bool enabled = enabledHandle.asBool();
        
        if(!enabled) {
            MDataHandle output = data.outputValue(aOutFluid, &status);
            if (status != MS::kSuccess) return status;
            output.set(true);
            output.setClean();
            return data.setClean(plug);
        }
        
        MDataHandle timeHandle = data.inputValue(aCurrentTime, &status);
        if (status != MS::kSuccess) return status;
        const MTime time = timeHandle.asTime();

        MDataHandle startTimeHandle = data.inputValue(aStartTime, &status);
        if (status != MS::kSuccess) return status;
        const MTime startTime = startTimeHandle.asTime();

        MObject thisObject = thisMObject();
        MPlug plgRigidBodies(thisObject, aOutFluid);
        MPlugArray rbConnections;
        plgRigidBodies.connectedTo(rbConnections, true, true);
        
        DOMMY_MAYA_DEBUG(
            "DJetSmokeSolver", "time: " << time << ", start time: " << startTime);
        
        if(time == startTime) {
            DOMMY_MAYA_DEBUG(
                "DJetSmokeSolver", "initializing simulation at the start frame");
            
            //init solver
            clearSolver();

            status = initSolver(data);
            if (status != MS::kSuccess) {
                clearSolver();
                return status;
            }

            MDataHandle frameRateHandle = data.inputValue(aFrameRate, &status);
            if (status != MS::kSuccess) {
                clearSolver();
                return status;
            }
            const double framerate = frameRateHandle.asDouble();
            if (!std::isfinite(framerate) || framerate <= 0.0) {
                clearSolver();
                return MS::kInvalidParameter;
            }
            m_frame = jet::Frame(0, 1.0 / framerate);
            m_solver->update(m_frame);
            m_frame++;
            
            m_prevTime = time;
        } else {
            
            if(m_solver) {
                double delta_frames = (time - m_prevTime).value();
                bool playback = MConditionMessage::getConditionState("playingBack");
                
                if(time > m_prevTime && ((delta_frames <= 1.0) || (playback && delta_frames < 100))) {
                    m_solver->update(m_frame);
                    m_frame++;
                    jet::GridSystemData3Ptr grids = m_solver->gridSystemData();
                    
                    DOMMY_MAYA_DEBUG(
                        "DJetSmokeSolver",
                        "scalar grids: " << grids->numberOfScalarData()
                        << ", vector grids: " << grids->numberOfVectorData());
                    
                    if(grids->numberOfScalarData()>0) {
                        jet::ScalarGrid3Ptr sg = grids->scalarDataAt(0);
                        jet::Size3 ssz = sg->dataSize();
                        jet::Size3 res = sg->resolution();
                        DOMMY_MAYA_DEBUG(
                            "DJetSmokeSolver",
                            "scalar grid " << sg->typeName()
                            << ", data size: " << ssz[0] << " " << ssz[1]
                            << " " << ssz[2] << ", resolution: " << res[0]
                            << " " << res[1] << " " << res[2]);
                    }
                    jet::Size3 resolution = grids->resolution();
                    jet::BoundingBox3D domain = grids->boundingBox();
                    jet::Vector3D gridSpacing = grids->gridSpacing();
                    
                    
                    DOMMY_MAYA_DEBUG(
                        "DJetSmokeSolver",
                        "resolution: " << resolution.x << " x " << resolution.y
                        << " x " << resolution.z << ", domain: ["
                        << domain.lowerCorner.x << ", " << domain.lowerCorner.y
                        << ", " << domain.lowerCorner.z << "] x ["
                        << domain.upperCorner.x << ", " << domain.upperCorner.y
                        << ", " << domain.upperCorner.z << "], grid spacing: ["
                        << gridSpacing.x << ", " << gridSpacing.y << ", "
                        << gridSpacing.z << "]");
                    m_prevTime = time;
                }
                
            }//check solver
            
            
        }//check time
        
        
        MDataHandle output = data.outputValue(aOutFluid, &status);
        if (status != MS::kSuccess) return status;
        output.set(true);
        output.setClean();
        return data.setClean(plug);
        
        
    } else {
        return MStatus::kUnknownParameter;
    }
    
    return MStatus::kSuccess;
    
}
DOMMY_COMPUTE_CATCH("DJetSmokeSolver")






MStatus DJetSmokeSolver::initialize() try {
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnMatrixAttribute matAttr;
    
    MStatus stat;
    
    
    
    // ----------------------- INPUTS --------------------------
   
    
    MFnMessageAttribute fnMsgAttr;
    aOutFluid = fnMsgAttr.create("OutFluid", "outfld", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutFluid));
    
    
    
    
    
    aEnabled = nAttr.create("enabled", "enbl", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnabled));
    
    
    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create( "CurrentTime", "ctm" , MFnUnitAttribute::kTime,  0.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));
    
    
    aStartTime = uniAttr.create( "StartTime", "stm" , MFnUnitAttribute::kTime, 1.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartTime));
    
    
    aFrameRate  = nAttr.create("FrameRate", "frt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(24.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrameRate));
    
    
    aResolusion = nAttr.create("Resolusion", "res", MFnNumericData::k3Int, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(64,64,64));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aResolusion));
    
    aGridSize = nAttr.create("GridSize", "gsz", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGridSize));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, aOutFluid));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEnabled, aOutFluid));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartTime, aOutFluid));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrameRate, aOutFluid));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aResolusion, aOutFluid));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGridSize, aOutFluid));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DJetSmokeSolver", "initialize", std::current_exception());
}

#endif
