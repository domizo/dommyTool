// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2014,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/AbcWriteJob.cpp

//
//  DCAbcWriteJob.cpp
//  dommyTool
//
//  Created by yuzo on 3/16/17.
//
//

#include "DCAbcWriteJob.h"

#include <Alembic/AbcCoreOgawa/All.h>


#include "DPointDisplay.h"
#include "DCurveDisplay.h"
#include "DMeshDisplay.h"
#include "DMayaDebug.h"
#include "common/utility/DMayaPluginData.h"
#include "common/file/DSequencePath.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <limits>
#include <utility>
#include <locale>
#include <sstream>


namespace
{
	bool validFrameSet(const std::set<double>& frames)
	{
		if (frames.empty()) {
			return false;
		}
		for (std::set<double>::const_iterator frame = frames.begin();
			 frame != frames.end(); ++frame) {
			if (!std::isfinite(*frame)) {
				return false;
			}
		}
		return true;
	}

	
	void addToString(std::string & str,
					 const std::string & name, std::uint64_t value)
	{
		if (value > 0)
		{
			str += name + " " + std::to_string(value) + " ";
		}
	}
	
    bool prepareExportCallback(std::string& callback, bool mel, double frame,
                               const MBoundingBox& bounds)
    {
        const std::size_t maximumBytes = 4U * 1024U * 1024U;
        if (callback.size() > maximumBytes || callback.find('\0') != std::string::npos ||
            !std::isfinite(frame)) return false;
        std::string candidate(callback);
        if (candidate.find("#FRAME#") != std::string::npos) {
            std::ostringstream value;
            value.imbue(std::locale::classic());
            value.precision(std::numeric_limits<double>::max_digits10);
            value << frame;
            if (!value) return false;
            if (!dommy::file::replaceAll(candidate, "#FRAME#", value.str(), maximumBytes))
                return false;
        }
        if (candidate.find("#BOUNDS#") != std::string::npos ||
            candidate.find("#BOUNDSARRAY#") != std::string::npos) {
            const MPoint minimum = bounds.min(), maximum = bounds.max();
            const double values[] = {minimum.x, minimum.y, minimum.z,
                                     maximum.x, maximum.y, maximum.z};
            std::ostringstream scalar, array;
            scalar.imbue(std::locale::classic());
            array.imbue(std::locale::classic());
            scalar.precision(std::numeric_limits<double>::max_digits10);
            array.precision(std::numeric_limits<double>::max_digits10);
            array << (mel ? " {" : " [");
            for (std::size_t index = 0U; index < 6U; ++index) {
                if (!std::isfinite(values[index])) return false;
                scalar << ' ' << values[index];
                if (index != 0U) array << ',';
                array << values[index];
            }
            array << (mel ? "} " : "] ");
            if (!scalar || !array) return false;
            if (!dommy::file::replaceAll(candidate, "#BOUNDS#", scalar.str(), maximumBytes) ||
                !dommy::file::replaceAll(candidate, "#BOUNDSARRAY#", array.str(), maximumBytes))
                return false;
        }
        callback.swap(candidate);
        return true;
    }

    void processBestEffortCallback(std::string callback, bool mel, double frame,
                                   const MBoundingBox& bounds)
    {
        if (callback.empty()) return;
        // These flags intentionally run trusted caller-supplied code. Rejection
        // and returned execution failure remain visible, best-effort warnings.
        if (!prepareExportCallback(callback, mel, frame, bounds)) {
            MGlobal::displayWarning(
                "Alembic callback was rejected as invalid; export will continue.");
            return;
        }
        const MStatus status = mel ? MGlobal::executeCommand(callback.c_str(), true) :
                                    MGlobal::executePythonCommand(callback.c_str(), true);
        if (!status)
            MGlobal::displayWarning(
                "Alembic callback execution failed; export will continue.");
    }
}

DCAbcWriteJob::DCAbcWriteJob(const char * iFileName,
							 bool iAsOgawa,
							 const std::set<double> & iTransFrames,
							 Alembic::AbcCoreAbstract::TimeSamplingPtr iTransTime,
							 const std::set<double> & iShapeFrames,
							 Alembic::AbcCoreAbstract::TimeSamplingPtr iShapeTime,
							 Alembic::AbcCoreAbstract::TimeSamplingPtr iBoundsTime,
							 const JobArgs & iArgs)
{
	if (iFileName == NULL || iFileName[0] == '\0') {
		throw std::invalid_argument(
			"Alembic output file name must not be empty");
	}
	if (!validFrameSet(iTransFrames) || !validFrameSet(iShapeFrames)) {
		throw std::invalid_argument(
			"Alembic transform and shape frame sets must be non-empty and finite");
	}
	if (!iTransTime || !iShapeTime || !iBoundsTime) {
		throw std::invalid_argument(
			"Alembic transform and shape time samplings must be valid");
	}

	MStatus status;
	mFileName = iFileName;
	mAsOgawa = iAsOgawa;
    mBoundsTime = iBoundsTime;
    mBoundsTimeIndex = 0U;
    mBoundsSamples = 0U;
	mArgs = iArgs;
    if (!mArgs.exportRootAdmission)
        throw std::invalid_argument("Missing Alembic root admission");
	mShapeSamples = 1;
	mTransSamples = 1;
	
    
#pragma mark get active select node
	if (mArgs.useSelectionList)
	{
		
		// get the active selection 現在の選択アイテムを取得
		MSelectionList activeList;
		status = MGlobal::getActiveSelectionList(activeList);
		if (status != MS::kSuccess) {
			throw std::runtime_error(
				"Unable to read the active Maya selection");
		}
        ddata_export_util::prepareExportSelection(
            mSList, mArgs.dagPaths, activeList, *mArgs.exportRootAdmission);
	}
    mArgs.exportRootAdmission->validate();
	
	mTransFrames = iTransFrames;
	mShapeFrames = iShapeFrames;
	if (*mTransFrames.begin() != *mShapeFrames.begin()) {
		throw std::invalid_argument(
			"Alembic transform and shape sampling must share the setup frame");
	}
	
	// only needed during creation of the transforms and shapes
	mTransTime = iTransTime;
	mTransTimeIndex = 0;
	mShapeTime = iShapeTime;
	mShapeTimeIndex = 0;
	
	mFirstFrame = *mTransFrames.begin();
	std::set<double>::iterator last = mTransFrames.end();
	last--;
	mLastFrame = *last;
	last = mShapeFrames.end();
	last--;
	
	double lastShapeFrame = *last;
	if (lastShapeFrame > mLastFrame)
		mLastFrame = lastShapeFrame;
}

namespace {
const std::size_t kNoExportParent = std::numeric_limits<std::size_t>::max();
const std::size_t kMaxExportDagDepth = 256U;

void requireExportRecordStorage(std::size_t transforms, std::size_t shapes,
                                std::size_t transformBytes, std::size_t shapeBytes)
{
    std::size_t first = 0U, second = 0U, bytes = 0U;
    if (transforms > dommy::generation::kMaxGeneratedTransformCount ||
        shapes > dommy::generation::kMaxConnectedGeometryCount ||
        !dommy::memory::checkedMultiply(transforms, transformBytes + sizeof(MMatrix), first) ||
        !dommy::memory::checkedMultiply(shapes, shapeBytes, second) ||
        !dommy::memory::checkedAdd(first, second, bytes) ||
        !dommy::attribute::validArrayConversionBudget(1U, bytes))
        throw std::length_error("Alembic bounds records exceed the storage budget");
}
}

void DCAbcWriteJob::trackShapeBounds(std::function<MBoundingBox()> read,
                                   std::size_t parentIndex)
{
    if (!read || parentIndex >= mBoundsTransforms.size())
        throw std::logic_error("Invalid Alembic shape bounds parent");
    requireExportRecordStorage(mBoundsTransforms.size(), mBoundsShapes.size() + 1U,
                              sizeof(TransformBoundsEntry), sizeof(ShapeBoundsEntry));
    mBoundsShapes.push_back(ShapeBoundsEntry{std::move(read), parentIndex});
}

MBoundingBox DCAbcWriteJob::getBoundingBox() const
{
    requireExportRecordStorage(mBoundsTransforms.size(), mBoundsShapes.size(),
                              sizeof(TransformBoundsEntry), sizeof(ShapeBoundsEntry));
    dommy::generation::WorkBudget work;
    std::vector<MMatrix> world;
    world.reserve(mBoundsTransforms.size());
    for (const auto& entry : mBoundsTransforms) {
        if (!entry.writer || !work.consume(1U))
            throw std::length_error("Invalid or excessive Alembic bounds transforms");
        MMatrix matrix = entry.writer->getSampleMatrix();
        if (entry.parent != kNoExportParent) {
            if (entry.parent >= world.size())
                throw std::logic_error("Invalid Alembic bounds transform hierarchy");
            if (entry.writer->inheritsSampleTransform()) matrix = matrix * world[entry.parent];
        }
        if (!dommy::numeric::isFiniteAffineTransform(matrix))
            throw std::invalid_argument("Non-finite or non-affine Alembic bounds transform");
        world.push_back(matrix);
    }

    MBoundingBox bounds;
    for (const auto& entry : mBoundsShapes) {
        if (!entry.read || entry.parent >= world.size() || !work.consume(8U))
            throw std::logic_error("Invalid Alembic shape bounds record");
        const MBoundingBox local = entry.read();
        if (!dommy::maya::isFiniteBoundingBox(local))
            throw std::invalid_argument("Invalid Alembic shape sample bounds");
        const MPoint minimum = local.min(), maximum = local.max();
        // Transform all eight corners; using only min/max is wrong under
        // rotation, shear or reflection. Reuse the checked double point math.
        for (unsigned int corner = 0U; corner < 8U; ++corner) {
            const std::array<double, 3> point{{(corner & 1U) ? maximum.x : minimum.x,
                (corner & 2U) ? maximum.y : minimum.y, (corner & 4U) ? maximum.z : minimum.z}};
            std::array<double, 3> result;
            if (!dommy::numeric::checkedTransformPoint(point, world[entry.parent], result))
                throw std::invalid_argument("Unrepresentable Alembic archive bounds");
            bounds.expand(MPoint(result[0], result[1], result[2]));
        }
    }
    if (!mBoundsShapes.empty() && !dommy::maya::isFiniteBoundingBox(bounds))
        throw std::invalid_argument("Invalid Alembic archive bounds");
    return bounds;
}

void DCAbcWriteJob::setup(double iFrame, MayaTransformWriterPtr iParent,
                        std::size_t parentIndex, std::size_t depth,
                        dommy::generation::WorkBudget& work)
{

    
	MStatus status;

    if (depth > kMaxExportDagDepth || !work.consume(1U)) {
        throw std::length_error("Alembic DAG traversal exceeds its depth/visit budget");
    }
	
	// short-circuit if selection flag is on but this node isn't actively
	// selected
	if (mArgs.useSelectionList) {
        const bool selected = mSList.hasItem(mCurDag, MObject::kNullObj, &status);
        ddata_export_util::requireExportQuery(status, "Unable to inspect Alembic DAG selection");
        if (!selected) return;
    }
	
    
	MObject ob = ddata_export_util::exportDagNode(mCurDag);
	
    
	MFnDependencyNode nodeFn(ob, &status);
    ddata_export_util::requireExportQuery(status, "Unable to bind an Alembic DAG node");
    const MTypeId nodeType = nodeFn.typeId(&status);
    ddata_export_util::requireExportQuery(status, "Unable to classify an Alembic DAG node");
    
	if (ob.hasFn(MFn::kTransform))
	{
        
        
		MayaTransformWriterPtr trans;
		
		// parented to the root case
		if (iParent == NULL)
		{
			Alembic::Abc::OObject obj = mRoot.getTop();
			trans = MayaTransformWriterPtr(new MayaTransformWriter( obj, mCurDag, mTransTimeIndex, mArgs));
		}
		else
		{
			trans = MayaTransformWriterPtr(new MayaTransformWriter( *iParent, mCurDag, mTransTimeIndex, mArgs));
		}
		
        requireExportRecordStorage(mBoundsTransforms.size() + 1U, mBoundsShapes.size(),
                                  sizeof(TransformBoundsEntry), sizeof(ShapeBoundsEntry));
        const std::size_t transformIndex = mBoundsTransforms.size();
        mBoundsTransforms.push_back(TransformBoundsEntry{trans, parentIndex});
		if (trans->isAnimated() && mTransTimeIndex != 0)
		{
			mTransList.push_back(trans);
			mStats.add(mStats.mTransAnimNum, 1U);
		}
		else
			mStats.add(mStats.mTransStaticNum, 1U);
        
        
        
        
        
		MayaAttributeWriterPtr attrs = trans->getAttrs();
		if (mTransTimeIndex != 0 && attrs->isAnimated())
			mTransAttrList.push_back(attrs);
		
        
        
		// loop through the children, making sure to push and pop them
		// from the MDagPath
		const unsigned int numChild = mCurDag.childCount(&status);
        ddata_export_util::requireExportQuery(status, "Unable to count Alembic DAG children");
        if (numChild > work.remaining())
            throw std::length_error("Alembic DAG children exceed the visit budget");
        
		for (unsigned int i=0; i<numChild; ++i)
		{
			const MObject child = mCurDag.child(i, &status);
            ddata_export_util::requireExportQuery(status, "Unable to read an Alembic DAG child");
            if (mCurDag.push(child) != MS::kSuccess) {
				throw std::runtime_error(
					"Unable to descend the Alembic DAG traversal path");
			}
			
			setup(iFrame, trans, transformIndex, depth + 1U, work);
			if (mCurDag.pop() != MS::kSuccess) {
				throw std::runtime_error(
					"Unable to restore the Alembic DAG traversal path");
			}
		}
	}
	else if(nodeType == DPointDisplay::typeId) {
		
        DOMMY_MAYA_DEBUG("DCAbcWriteJob", "setting up DPointDisplay");
        
        MPlug dpPlug(ob, DPointDisplay::inDP);
        MStatus connectionStatus;
        const bool connected = !dpPlug.isNull() && dpPlug.isConnected(&connectionStatus);
        if (!connectionStatus) {
            throw std::runtime_error("Unable to query Alembic DPoint input connection");
        }
        if (connected) {
            MObject dataObject;
            const DPData* dpoint = nullptr;
            const MStatus dataStatus =
                dommy::maya::getPluginDataFromPlug<DPData>(
                    dpPlug, DPData::id, dataObject, dpoint);
            if (!dataStatus || !dpoint) {
                throw std::runtime_error(
                    "DCAbcWriteJob could not read valid DPoint input data");
            }
            
            if (dpoint && dpoint->numberOfPoints()) {
                
//                cout << "point number:" << dpoint->numberOfPoints() << endl;
                
                if (iParent != NULL)
                {
                    Alembic::Abc::OObject obj = iParent->getObject();
                    DPointWriterPtr dpwrite(new DPointWriter( mCurDag, obj, mShapeTimeIndex, mArgs));
                    trackShapeBounds([dpwrite] { return dpwrite->getSampleBounds(); }, parentIndex);
                    
                    if (dpwrite->isAnimated() && mShapeTimeIndex != 0)
                    {
                        mPointList.push_back(dpwrite);
                    }
                    else
                    {
                        
                    }
                    
//                    cout << "set dPoint:" << endl;
                    
                }
                else
                {
                    MString err = "DPointDisplay hasn't  transform ";
                    err += nodeFn.name() + " since it doesn't have a parent.";
                    MGlobal::displayError(err);
                }
                
                
                
            } else {
                MString err = "Failed get DPData data : ";
                err += nodeFn.name();
                MGlobal::displayError(err);
            }
            
            
            
        } else {
            MString err = "Error DPointDisplay : ";
            err += nodeFn.name() + " since it doesn't connected inDP.";
            MGlobal::displayError(err);
        }
		
		
	} else if(nodeType == DCurveDisplay::typeId) {
        
#pragma mark -
#pragma mark DCurve
        
		MPlug dcPlug(ob, DCurveDisplay::inDC);
        MStatus connectionStatus;
        const bool connected = !dcPlug.isNull() && dcPlug.isConnected(&connectionStatus);
        if (!connectionStatus) {
            throw std::runtime_error("Unable to query Alembic DCurve input connection");
        }
			if (connected) {
				MObject dataObject;
				const DCurveData* dCurves = nullptr;
				const MStatus dataStatus =
					dommy::maya::getPluginDataFromPlug<DCurveData>(
						dcPlug, DCurveData::id, dataObject, dCurves);
				if (!dataStatus || !dCurves) {
					throw std::runtime_error(
						"DCAbcWriteJob could not read valid DCurve input data");
				}
            
			if (dCurves && dCurves->numberOfCurves()) {
				
//                cout << "curve number:" << dCurves->numberOfCurves() << endl;
                
				
				if (iParent != NULL)
				{
					Alembic::Abc::OObject obj = iParent->getObject();
					DCurveWriterPtr nurbsCurve(new DCurveWriter( mCurDag, obj, mShapeTimeIndex, mArgs));
                    trackShapeBounds([nurbsCurve] { return nurbsCurve->getSampleBounds(); }, parentIndex);
					
					if (nurbsCurve->isAnimated() && mShapeTimeIndex != 0)
					{
						mCurveList.push_back(nurbsCurve);
//						mStats.add(mStats.mDCAnimNum, 1U);
//						mStats.add(mStats.mDCAnimCurves, 1U);
//						mStats.add(mStats.mDCAnimCVs, nurbsCurve->getNumCVs());
					}
					else
					{
//						mStats.add(mStats.mDCStaticNum, 1U);
//						mStats.add(mStats.mDCStaticCurves, 1U);
//						mStats.add(mStats.mDCStaticCVs, nurbsCurve->getNumCVs());
					}
					
//                    cout << "set dcurves:" << endl;
                    
//					MayaAttributeWriterPtr attrs = nurbsCurve->getAttrs();
//					if (mShapeTimeIndex != 0 && attrs->isAnimated())
//						mShapeAttrList.push_back(attrs);
//                    
//                    
				}
				else
				{
					MString err = "DCurve hasn't  transform ";
					err += nodeFn.name() + " since it doesn't have a parent.";
					MGlobal::displayError(err);
				}
				
				
				
			} else {
				MString err = "Failed get DCurve data : ";
				err += nodeFn.name();
				MGlobal::displayError(err);
			}
			
			
			
		} else {
			MString err = "Error DCurveDisplay : ";
			err += nodeFn.name() + " since it doesn't connected inDC.";
			MGlobal::displayError(err);
		}
		
		
		
	}
    else if(nodeType == DMeshDisplay::typeId)
    {
#pragma mark -
#pragma mark DMesh
        
        MPlug dmPlug(ob, DMeshDisplay::inDM);
        MStatus connectionStatus;
        const bool connected = !dmPlug.isNull() && dmPlug.isConnected(&connectionStatus);
        if (!connectionStatus) {
            throw std::runtime_error("Unable to query Alembic DMesh input connection");
        }
        if (connected) {
            MObject dataObject;
            const DMeshData* dmesh = nullptr;
            const MStatus dataStatus =
                dommy::maya::getPluginDataFromPlug<DMeshData>(
                    dmPlug, DMeshData::id, dataObject, dmesh);
            if (!dataStatus || !dmesh) {
                throw std::runtime_error(
                    "DCAbcWriteJob could not read valid DMesh input data");
            }
            
            if (dmesh && dmesh->numberOfMesh()) {
                
                DOMMY_MAYA_DEBUG(
                    "DCAbcWriteJob", "mesh count: " << dmesh->numberOfMesh());
                
                
                if (iParent != NULL)
                {
                    Alembic::Abc::OObject obj = iParent->getObject();
                    DMeshWriterPtr mesh(new DMeshWriter( mCurDag, obj, mShapeTimeIndex, mArgs));
                    trackShapeBounds([mesh] { return mesh->getSampleBounds(); }, parentIndex);
                    
                    if (mesh->isAnimated() && mShapeTimeIndex != 0)
                    {
                        mMeshList.push_back(mesh);
                        //                        mStats.add(mStats.mDCAnimNum, 1U);
                        //                        mStats.add(mStats.mDCAnimCurves, 1U);
                        //                        mStats.add(mStats.mDCAnimCVs, nurbsCurve->getNumCVs());
                    }
                    else
                    {
                        //                        mStats.add(mStats.mDCStaticNum, 1U);
                        //                        mStats.add(mStats.mDCStaticCurves, 1U);
                        //                        mStats.add(mStats.mDCStaticCVs, nurbsCurve->getNumCVs());
                    }
                    
//                    cout << "set dmeshs:" << endl;
                    
                    //                    MayaAttributeWriterPtr attrs = nurbsCurve->getAttrs();
                    //                    if (mShapeTimeIndex != 0 && attrs->isAnimated())
                    //                        mShapeAttrList.push_back(attrs);
                    //
                    //
                }
                else
                {
                    MString err = "DMesh hasn't transform ";
                    err += nodeFn.name() + " since it doesn't have a parent.";
                    MGlobal::displayError(err);
                }
                
                
                
            } else {
                MString err = "Failed get DMesh data : ";
                err += nodeFn.name();
                MGlobal::displayError(err);
            }
            
            
            
        } else {
            MString err = "Error DMeshDisplay : ";
            err += nodeFn.name() + " since it doesn't connected inDM.";
            MGlobal::displayError(err);
        }
        
        
    }
    else if (ob.hasFn(MFn::kLocator))
    {
        
        DOMMY_MAYA_DEBUG("DCAbcWriteJob", "setting up locator");
        
        MFnDependencyNode fnLocator(ob, & status);
        ddata_export_util::requireExportQuery(status, "Unable to bind an Alembic locator");
        
        if (iParent != NULL)
        {
            Alembic::Abc::OObject obj = iParent->getObject();
            MayaLocatorWriterPtr locator(new MayaLocatorWriter( mCurDag, obj, mShapeTimeIndex, mArgs));
            
            if (locator->isAnimated() && mShapeTimeIndex != 0)
            {
                mLocatorList.push_back(locator);
                mStats.add(mStats.mLocatorAnimNum, 1U);
            }
            else
            {
                mStats.add(mStats.mLocatorStaticNum, 1U);
            }
            
            MayaAttributeWriterPtr attrs = locator->getAttrs();
            if (mShapeTimeIndex != 0 && attrs->isAnimated())
                mShapeAttrList.push_back(attrs);
        }
        else
        {
            MString err = "Can't translate ";
            err += fnLocator.name() + " since it doesn't have a parent.";
            MGlobal::displayError(err);
        }
        
    }
	else
	{
        MString warning = "DCAbcWriteJob: unsupported object ";
        warning += mCurDag.fullPathName();
        warning += " (";
        warning += ob.apiTypeStr();
        warning += ").";
        MGlobal::displayWarning(warning);
	}
}


DCAbcWriteJob::~DCAbcWriteJob()
{
}

bool DCAbcWriteJob::eval(double iFrame)
{
	if (iFrame == mFirstFrame)
	{
#pragma mark initialize
        // Root paths share one archive parent. Check their emitted names before
        // creating the archive, using exactly the same naming rule as writers.
        ddata_export_util::requireUniqueExportRootNames(
            mArgs.dagPaths, mArgs.stripNamespace, *mArgs.exportRootAdmission);
		
		std::string appWriter = "Maya ";
		appWriter += MGlobal::mayaVersion().asChar();
		appWriter += " DAbcExportCmd v";
		appWriter += DCABCEXPORT_VERSION;
		
		std::string userInfo = "Exported from: ";
		userInfo += MFileIO::currentFile().asChar();
		
		// these symbols can't be in the meta data
		if (userInfo.find('=') != std::string::npos ||
			userInfo.find(';') != std::string::npos)
		{
			userInfo = "";
		}
		
        //現在Ogawaのみ対応
//        if (mAsOgawa)
//        {
			mRoot = CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive(),
										  mFileName, appWriter, userInfo,
										  Alembic::Abc::ErrorHandler::kThrowPolicy);
//        }
//        else
//        {
//            mRoot = CreateArchiveWithInfo(Alembic::AbcCoreHDF5::WriteArchive(),
//                                          mFileName, appWriter, userInfo,
//                                          Alembic::Abc::ErrorHandler::kThrowPolicy);
//        }
		if (!mRoot.valid())
		{
			std::string theError = "Unable to create abc file";
			throw std::runtime_error(theError);
		}

		mShapeTimeIndex = mRoot.addTimeSampling(*mShapeTime);
		mTransTimeIndex = mRoot.addTimeSampling(*mTransTime);
        mBoundsTimeIndex = mRoot.addTimeSampling(*mBoundsTime);
		
	        
		mBoxProp = Alembic::AbcGeom::CreateOArchiveBounds(mRoot, mBoundsTimeIndex);
		
        
        dommy::generation::WorkBudget traversal(dommy::generation::kMaxConnectedGeometryCount);
        for (auto it = mArgs.dagPaths.begin(),end = mArgs.dagPaths.end(); it!=end; ++it)
		{
			mCurDag = *it;
            setup(iFrame * ddata_export_util::spf(), MayaTransformWriterPtr(),
                  kNoExportParent, 0U, traversal);
		}
        
		perFrameCallback(iFrame);
	}
	else
	{
        
		std::set<double>::iterator checkFrame = mShapeFrames.find(iFrame);
		bool foundShapeFrame = false;
		if (checkFrame != mShapeFrames.end())
		{
			if (!mRoot.valid()) {
				throw std::logic_error(
					"Alembic shape sample evaluated before archive initialization");
			}
			foundShapeFrame = true;
			mShapeSamples ++;
//			double curTime = iFrame * dc_export_util::spf();
			
            //DPoint
            for (auto &pointItm:mPointList)
            {
                pointItm->write();
                mStats.add(mStats.mDPAnimPts, pointItm->getNumPoints());
            }
			
            //DCurve
            for (auto &curveIt:mCurveList)
			{
				curveIt->write();
				mStats.add(mStats.mDCAnimCVs, curveIt->getNumCVs());
			}
            
            //DMesh
            for (auto &mIt:mMeshList)
            {
                mIt->write();
                mStats.add(mStats.mDMAnimVtxs, mIt->getNumPoints());
            }
			
            //Locator
            for (auto &locIt:mLocatorList)
			{
				locIt->write();
			}
		
			//SHape
            for(auto &shape:mShapeAttrList)
			{
				shape->write();
			}
		}
		
		checkFrame = mTransFrames.find(iFrame);
		bool foundTransFrame = false;
		if (checkFrame != mTransFrames.end())
		{
			if (!mRoot.valid()) {
				throw std::logic_error(
					"Alembic transform sample evaluated before archive initialization");
			}
			foundTransFrame = true;
			mTransSamples ++;
			
			
            for (auto &trans:mTransList)
			{
				trans->write();
			}
			
            for(auto &tAttr:mTransAttrList)
			{
				tAttr->write();
			}
		}
		
		if (foundTransFrame || foundShapeFrame)
			perFrameCallback(iFrame);
	}
	
	if (iFrame == mLastFrame)
	{
		postCallback(iFrame);
		return true;
	}
	
	return false;
}

void DCAbcWriteJob::perFrameCallback(double iFrame)
{
    if (mBoundsSamples == std::numeric_limits<Alembic::Util::uint32_t>::max()) {
        throw std::overflow_error("Too many Alembic bounds samples");
    }
	const MBoundingBox bbox = getBoundingBox();
	
	Alembic::Abc::V3d min(bbox.min().x, bbox.min().y, bbox.min().z);
	Alembic::Abc::V3d max(bbox.max().x, bbox.max().y, bbox.max().z);
	Alembic::Abc::Box3d b(min, max);
	mBoxProp.set(b);
    ++mBoundsSamples;
	
	processBestEffortCallback(
		mArgs.melPerFrameCallback, true, iFrame, bbox);
	processBestEffortCallback(
		mArgs.pythonPerFrameCallback, false, iFrame, bbox);
}


// write the frame ranges and statistic string on the root
// Also call the post callbacks
void DCAbcWriteJob::postCallback(double iFrame)
{
	std::string statsStr = "";
	
	addToString(statsStr, "DCStaticNum", mStats.mDCStaticNum);
	addToString(statsStr, "DCStaticCurves", mStats.mDCStaticCurves);
	addToString(statsStr, "DCAnimNum", mStats.mDCAnimNum);
	addToString(statsStr, "DCAnimCurves", mStats.mDCAnimCurves);
	addToString(statsStr, "DCStaticCVs", mStats.mDCStaticCVs);
    
	addToString(statsStr, "DCAnimCVs", mStats.mDCAnimCVs);
	addToString(statsStr, "DPAnimPts", mStats.mDPAnimPts);
    addToString(statsStr, "DMAnimVtxs", mStats.mDMAnimVtxs);
    
	addToString(statsStr, "TransStaticNum", mStats.mTransStaticNum);
	addToString(statsStr, "TransAnimNum", mStats.mTransAnimNum);
	
	addToString(statsStr, "LocatorStaticNum", mStats.mLocatorStaticNum);
	addToString(statsStr, "LocatorAnimNum", mStats.mLocatorAnimNum);
	
	
	if (statsStr.length() > 0)
	{
		Alembic::Abc::OStringProperty stats(mRoot.getTop().getProperties(),
											"statistics");
		stats.set(statsStr);
	}
	
	if (mTransTimeIndex != 0)
	{
		MString propName;
		propName += static_cast<int>(mTransTimeIndex);
		propName += ".samples";
		Alembic::Abc::OUInt32Property samp(mRoot.getTop().getProperties(),
										   propName.asChar());
        Alembic::Util::uint32_t count = mTransSamples;
        if (mShapeTimeIndex == mTransTimeIndex) count = std::max(count, mShapeSamples);
        if (mBoundsTimeIndex == mTransTimeIndex) count = std::max(count, mBoundsSamples);
		samp.set(count);
	}
	
	if (mShapeTimeIndex != 0 && mShapeTimeIndex != mTransTimeIndex)
	{
		MString propName;
		propName += static_cast<int>(mShapeTimeIndex);
		propName += ".samples";
		Alembic::Abc::OUInt32Property samp(mRoot.getTop().getProperties(),
										   propName.asChar());
        const Alembic::Util::uint32_t count = mBoundsTimeIndex == mShapeTimeIndex
            ? std::max(mShapeSamples, mBoundsSamples) : mShapeSamples;
		samp.set(count);
	}

    if (mBoundsTimeIndex != 0 && mBoundsTimeIndex != mTransTimeIndex &&
        mBoundsTimeIndex != mShapeTimeIndex) {
        MString propName;
        propName += static_cast<int>(mBoundsTimeIndex);
        propName += ".samples";
        Alembic::Abc::OUInt32Property samp(mRoot.getTop().getProperties(), propName.asChar());
        samp.set(mBoundsSamples);
    }
	
	MBoundingBox bbox;
	
	if (mArgs.melPostCallback.find("#BOUNDS#") != std::string::npos ||
		mArgs.pythonPostCallback.find("#BOUNDS#") != std::string::npos ||
		mArgs.melPostCallback.find("#BOUNDSARRAY#") != std::string::npos ||
		mArgs.pythonPostCallback.find("#BOUNDSARRAY#") != std::string::npos)
	{
        bbox = getBoundingBox();
	}
	
	processBestEffortCallback(
		mArgs.melPostCallback, true, iFrame, bbox);
	processBestEffortCallback(
		mArgs.pythonPostCallback, false, iFrame, bbox);
}
