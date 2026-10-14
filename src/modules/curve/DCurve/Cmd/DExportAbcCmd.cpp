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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/AbcExport.cpp

//
//  DExportAbcCmd.cpp
//  dommyTool
//
//  Created by yuzo on 2017/03/16.
//
//

#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "DExportAbcCmd.h"



#include "DMayaHeaders.h"

#include "DCAbcWriteJob.h"
#include "DDataExportUtil.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DMelString.h"
#include "common/utility/DAbcSamplingPlan.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DFrameSampling.h"
#include "common/utility/DMayaStateGuard.h"
#include "common/utility/DWorkBudget.h"



#include <utility>
#include <limits>

#include <set>
#include <vector>
#include <map>
#include <memory>
#include <list>
#include <algorithm>
#include <functional>

#include <stdexcept>
#include <exception>

#include <string>

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace AbcA = Alembic::AbcCoreAbstract;

namespace {

// An unsuccessful scene query must not turn into permission to replace a file.
MStatus checkAlembicDestinationUnused(const MFileObject& destination)
{
    MStatus status;
    MItDependencyNodes nodes(MFn::kPluginDependNode, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::generation::WorkBudget work(dommy::generation::kMaxConnectedGeometryCount);
    for (;;) {
        const bool done = nodes.isDone(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (done) return MS::kSuccess;
        if (!work.consume(1U)) return MS::kInvalidParameter;
        const MObject object = nodes.thisNode(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        MFnDependencyNode node(object, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MString type = node.typeName(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (type == "AlembicNode") {
            const MPlug plug = node.findPlug("abc_File", true, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (plug.isNull()) return MS::kFailure;
            MString path;
            DOMMY_RETURN_IF_FAILURE(readPlugValue(plug, path));
            MFileObject file;
            DOMMY_RETURN_IF_FAILURE(file.setRawFullName(path));
            if (file.exists() && file.resolvedFullName() == destination.resolvedFullName()) {
                MGlobal::displayError("Can't export to an Alembic file which is in use.");
                return MS::kFailure;
            }
        }
        DOMMY_RETURN_IF_FAILURE(nodes.next());
    }
}

class PendingAbcWriteJob final
{
public:
    PendingAbcWriteJob(const std::string& destination,
                       bool asOgawa,
                       const std::set<double>& transFrames,
                       AbcA::TimeSamplingPtr transTime,
                       const std::set<double>& shapeFrames,
                       AbcA::TimeSamplingPtr shapeTime,
                       AbcA::TimeSamplingPtr boundsTime,
                       const JobArgs& args)
        : output_(destination, dommy::file::PendingOutputFile::Access::TrustedPath)
        , job_(new DCAbcWriteJob(output_.trustedTemporaryPath().c_str(), asOgawa,
                                 transFrames, transTime, shapeFrames,
                                 shapeTime, boundsTime, args))
    {
    }

    bool eval(double frame)
    {
        if (!job_) {
            return true;
        }
        const bool complete = job_->eval(frame);
        if (complete) {
            // Alembic owns the archive through the job and its child writers.
            // Release all of them before moving the completed temporary file.
            job_.reset();
        }
        return complete;
    }

    void commit()
    {
        if (job_) {
            throw std::logic_error("Alembic job committed before completion");
        }
        if (!output_.commit()) {
            throw std::runtime_error(
                "Unable to replace the Alembic destination with the completed file");
        }
    }

private:
    PendingAbcWriteJob(const PendingAbcWriteJob&);
    PendingAbcWriteJob& operator=(const PendingAbcWriteJob&);

    // Declaration order is intentional: job_ must be destroyed before output_
    // removes an uncommitted temporary file.
    dommy::file::PendingOutputFile output_;
    DCAbcWriteJobPtr job_;
};

typedef std::shared_ptr<PendingAbcWriteJob> PendingAbcWriteJobPtr;

bool checkedInsertUnitFrames(double start,
                             double endExclusive,
                             std::set<double>& frames)
{
    if (!std::isfinite(start) || !std::isfinite(endExclusive)) return false;
    if (endExclusive <= start) return true;

    const long double span =
        static_cast<long double>(endExclusive) - static_cast<long double>(start);
    const long double countValue = std::ceil(span);
    if (!std::isfinite(countValue) || countValue < 0.0L ||
        countValue > static_cast<long double>(dommy::sampling::kMaxExportFrameSamples)) {
        return false;
    }

    const std::size_t count = static_cast<std::size_t>(countValue);
    if (frames.size() > dommy::sampling::kMaxExportFrameSamples - count) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        double frame = 0.0;
        if (!dommy::sampling::frameAt(start, 1.0, index, frame)) {
            return false;
        }
        if (frame < endExclusive) {
            frames.insert(frame);
        }
    }
    return frames.size() <= dommy::sampling::kMaxExportFrameSamples;
}

bool convertFrameToSeconds(double frame,
                           double secondsPerFrame,
                           double& seconds)
{
    if (!std::isfinite(frame) || !std::isfinite(secondsPerFrame) ||
        secondsPerFrame <= 0.0) {
        return false;
    }
    seconds = frame * secondsPerFrame;
    return std::isfinite(seconds);
}

bool convertFramesToSeconds(std::vector<double>& frames,
                            double secondsPerFrame)
{
    for (std::vector<double>::iterator frame = frames.begin();
         frame != frames.end(); ++frame) {
        double seconds = 0.0;
        if (!convertFrameToSeconds(*frame, secondsPerFrame, seconds)) {
            return false;
        }
        *frame = seconds;
    }
    return true;
}

bool createTimeSampling(
    const dommy::sampling::AbcTimeSamplingPlan& plan,
    double secondsPerFrame,
    AbcA::TimeSamplingPtr& output)
{
    if (plan.mode == dommy::sampling::kAbcDefaultTimeSampling) {
        output.reset(new AbcA::TimeSampling());
        return true;
    }
    if (plan.sampleTimes.empty()) {
        return false;
    }

    std::vector<double> samples(plan.sampleTimes);
    if (!convertFramesToSeconds(samples, secondsPerFrame)) {
        return false;
    }

    if (plan.mode == dommy::sampling::kAbcAcyclicTimeSampling) {
        output.reset(new AbcA::TimeSampling(
            AbcA::TimeSamplingType(AbcA::TimeSamplingType::kAcyclic),
            samples));
        return true;
    }

    double period = 0.0;
    if (!convertFrameToSeconds(plan.period, secondsPerFrame, period)) {
        return false;
    }
    if (plan.mode == dommy::sampling::kAbcUniformTimeSampling) {
        if (samples.size() != 1U) {
            return false;
        }
        output.reset(new AbcA::TimeSampling(period, samples.front()));
        return true;
    }
    if (plan.mode != dommy::sampling::kAbcCyclicTimeSampling ||
        samples.size() >
            static_cast<std::size_t>(
                std::numeric_limits<Alembic::Util::uint32_t>::max())) {
        return false;
    }

    output.reset(new AbcA::TimeSampling(
        AbcA::TimeSamplingType(
            static_cast<Alembic::Util::uint32_t>(samples.size()), period),
        samples));
    return true;
}

} // namespace

DExportAbcCmd::DExportAbcCmd()
{
}

DExportAbcCmd::~DExportAbcCmd()
{
}


void* DExportAbcCmd::creator()
{
    return ::dommy::maya::createObject("DExportAbcCmd", [&]() {
        return new DExportAbcCmd();
    });
}

MStatus DExportAbcCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag("-v",  "-verbose", MSyntax::kNoArg));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag("-h",  "-help", MSyntax::kNoArg));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag("-prs", "-preRollStartFrame", MSyntax::kDouble));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag("-duf", "-dontSkipUnwrittenFrames", MSyntax::kNoArg));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag("-j", "-jobArg", MSyntax::kString));
    
    DOMMY_RETURN_IF_FAILURE(syntax.makeFlagMultiUse("-j"));
    syntax.enableQuery(true);
    syntax.enableEdit(false);
    
    return MS::kSuccess;
}

MSyntax DExportAbcCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<DExportAbcCmd>("DExportAbcCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<DExportAbcCmd>(
        "DExportAbcCmd", std::current_exception());
    return MSyntax();
}




MStatus DExportAbcCmd::doIt(const MArgList & args)
{
    

    
	try
	{
		MStatus status;
		dommy::maya::ScopedCurrentTimeRestore timeRestore;
		
		
	#pragma mark syntax
		MSyntax commandSyntax;
        status = dommy::maya::buildCommandSyntax<DExportAbcCmd>(
            commandSyntax, "DExportAbcCmd");
        if (!status) return status;
        MArgParser argData(commandSyntax, args, &status);
		if (status != MS::kSuccess) {
			return status;
		}
		
#pragma mark help
		if (argData.isFlagSet("help"))
		{
			MGlobal::displayInfo(ddata_export_util::getHelpText());
			return MS::kSuccess;
		}
		
		bool verbose = argData.isFlagSet("verbose");
		
		// If skipFrame is true, when going through the playback range of the
		// scene, as much frames are skipped when possible.  This could cause
		// a problem for, time dependent solutions like
		// particle system / hair simulation
		bool skipFrame = true;
		if (argData.isFlagSet("dontSkipUnwrittenFrames"))
			skipFrame = false;
		
		double startEvaluationTime = DBL_MAX;
		if (argData.isFlagSet("preRollStartFrame"))
		{
			double startAt = 0.0;
			status = argData.getFlagArgument("preRollStartFrame", 0, startAt);
			if (status != MS::kSuccess || !std::isfinite(startAt)) {
				MGlobal::displayError("preRollStartFrame must be finite.");
				return MS::kFailure;
			}
			startEvaluationTime = startAt;
		}
		
		
#pragma mark job
		unsigned int jobSize = argData.numberOfFlagUses("jobArg");
		
		if (jobSize == 0) {
			MGlobal::displayError("At least one -jobArg is required.");
			return MS::kFailure;
		}
		if (static_cast<std::size_t>(jobSize) >
			dommy::sampling::kMaxExportJobs) {
			MGlobal::displayError("Too many Alembic export jobs were requested.");
			return MS::kFailure;
		}
		
		// the frame range we will be iterating over for all jobs,
		// includes frames which are not skipped and the startAt offset
		std::set<double> allFrameRange;
		
#pragma mark job args
		// this will eventually hold only the animated jobs.
		// its a list because we will be removing jobs from it
		std::list<PendingAbcWriteJobPtr> jobList;
		std::vector<PendingAbcWriteJobPtr> allJobs;
		allJobs.reserve(jobSize);
		std::set<std::string> outputPaths;
		std::size_t totalSampleEvents = 0U;
        auto rootWork = std::make_shared<dommy::generation::WorkBudget>();
		
		for (unsigned int jobIndex = 0; jobIndex < jobSize; jobIndex++)
		{
			JobArgs jobArgs;
            jobArgs.exportRootAdmission =
                std::make_shared<dommy::exporting::RootAdmission>(rootWork);
			MArgList jobArgList;
			status = argData.getFlagArgumentList("jobArg", jobIndex, jobArgList);
			if (status != MS::kSuccess || jobArgList.length() != 1U) {
				MGlobal::displayError("Unable to read an Alembic -jobArg value.");
				return MS::kFailure;
			}
			MString jobArgsStr;
			status = jobArgList.get(0, jobArgsStr);
			if (status != MS::kSuccess || jobArgsStr.length() == 0U ||
				static_cast<std::size_t>(jobArgsStr.numChars()) >
					dommy::sampling::kMaxExportArgumentCharacters) {
				MGlobal::displayError("Alembic -jobArg is empty or exceeds the safety limit.");
				return MS::kFailure;
			}
			MStringArray jobArgsArray;
			
			{
				// parse the job arguments
				// e.g. -perFrameCallbackMel "print \"something\"" will be splitted to
				//    [0] -perFrameCallbackMel
				//    [1] print "something"
				enum State {
					kArgument,               // parsing an argument (not quoted)
					kDoubleQuotedString,     // parsing a double quoted string
					kSingleQuotedString,     // parsing a single quoted string
				};
				
				State state = kArgument;
				MString stringBuffer;
				for (unsigned int charIdx = 0; charIdx < jobArgsStr.numChars();
					 charIdx++)
				{
					MString ch = jobArgsStr.substringW(charIdx, charIdx);
					switch (state)
					{
						case kArgument:
							if (ch == " ")
							{
								// space terminates the current argument
								if (stringBuffer.length() > 0) {
									jobArgsArray.append(stringBuffer);
									stringBuffer.clear();
								}
								// goto another argument
								state = kArgument;
							}
							else if (ch == "\"")
							{
								if (stringBuffer.length() > 0)
								{
									// double quote is part of the argument
									stringBuffer += ch;
								}
								else
								{
									// goto double quoted string
									state = kDoubleQuotedString;
								}
							}
							else if (ch == "'")
							{
								if (stringBuffer.length() > 0)
								{
									// single quote is part of the argument
									stringBuffer += ch;
								}
								else
								{
									// goto single quoted string
									state = kSingleQuotedString;
								}
							}
							else
							{
								stringBuffer += ch;
							}
							break;
							
						case kDoubleQuotedString:
							// double quote terminates the current string
							if (ch == "\"")
							{
								jobArgsArray.append(stringBuffer);
								stringBuffer.clear();
								state = kArgument;
							}
							else if (ch == "\\")
							{
								// escaped character
								MString nextCh = (++charIdx < jobArgsStr.numChars())
								? jobArgsStr.substringW(charIdx, charIdx) : "\\";
								if (nextCh == "n")       stringBuffer += "\n";
								else if (nextCh == "t")  stringBuffer += "\t";
								else if (nextCh == "r")  stringBuffer += "\r";
								else if (nextCh == "\\") stringBuffer += "\\";
								else if (nextCh == "'")  stringBuffer += "'";
								else if (nextCh == "\"") stringBuffer += "\"";
								else                     stringBuffer += nextCh;
							}
							else
							{
								stringBuffer += ch;
							}
							break;
							
						case kSingleQuotedString:
							// single quote terminates the current string
							if (ch == "'")
							{
								jobArgsArray.append(stringBuffer);
								stringBuffer.clear();
								state = kArgument;
							}
							else if (ch == "\\")
							{
								// escaped character
								MString nextCh = (++charIdx < jobArgsStr.numChars())
								? jobArgsStr.substringW(charIdx, charIdx) : "\\";
								if (nextCh == "n")       stringBuffer += "\n";
								else if (nextCh == "t")  stringBuffer += "\t";
								else if (nextCh == "r")  stringBuffer += "\r";
								else if (nextCh == "\\") stringBuffer += "\\";
								else if (nextCh == "'")  stringBuffer += "'";
								else if (nextCh == "\"") stringBuffer += "\"";
								else                     stringBuffer += nextCh;
							}
							else
							{
								stringBuffer += ch;
							}
							break;
					}
				}
				
					// the rest of the argument
					if (state != kArgument) {
						MGlobal::displayError(
							"Alembic -jobArg contains an unterminated quoted string.");
						return MS::kFailure;
					}
					if (stringBuffer.length() > 0)
				{
					jobArgsArray.append(stringBuffer);
				}
			}
			
#pragma mark job frama range
			// the frame range within this job
			std::vector<dommy::sampling::AbcFrameRange> frameRanges(1);
			frameRanges.back().startTime = timeRestore.originalTime().value();
			frameRanges.back().endTime = timeRestore.originalTime().value();
			frameRanges.back().strideTime = 1.0;
			
			bool hasRange = false;
			bool hasRoot = false;
			bool sampleGeo  = true; // whether or not to subsample geometry
			std::string fileName;
			bool asOgawa = true;
			
			unsigned int numJobArgs = jobArgsArray.length();
			for (unsigned int i = 0; i < numJobArgs; ++i)
			{
				MString arg = jobArgsArray[i];
				arg.toLowerCase();
				
				if (arg == "-f" || arg == "-file")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError("File incorrectly specified.");
						return MS::kFailure;
					}
					fileName = jobArgsArray[++i].asChar();
				}
				
				else if (arg == "-fr" || arg == "-framerange")
				{
					if (i+2 >= numJobArgs || !jobArgsArray[i+1].isDouble() ||
						!jobArgsArray[i+2].isDouble())
					{
						MGlobal::displayError("Frame Range incorrectly specified.");
						return MS::kFailure;
					}
					
					// this is not the first -frameRange argument, we are going
					// to add one more frame range to the frame range array.
					//最初のフレームレンジのみを使用してそれ以降に指定されたフレームレンジは無効とされます。
					if (hasRange)
					{
						frameRanges.push_back(dommy::sampling::AbcFrameRange());
					}
					
					hasRange = true;
					frameRanges.back().startTime = jobArgsArray[++i].asDouble();
					frameRanges.back().endTime = jobArgsArray[++i].asDouble();
					if (!std::isfinite(frameRanges.back().startTime) ||
						!std::isfinite(frameRanges.back().endTime)) {
						MGlobal::displayError("Frame Range values must be finite.");
						return MS::kFailure;
					}
					
					// make sure start frame is smaller or equal to endTime
					if (frameRanges.back().startTime > frameRanges.back().endTime)
					{
						std::swap(frameRanges.back().startTime,
								  frameRanges.back().endTime);
					}
				}
				
				else if (arg == "-frs" || arg == "-framerelativesample")
				{
					if (i+1 >= numJobArgs || !jobArgsArray[i+1].isDouble())
					{
						MGlobal::displayError( "Frame Relative Sample incorrectly specified.");
						return MS::kFailure;
					}
					const double shutterSample = jobArgsArray[++i].asDouble();
					if (!std::isfinite(shutterSample)) {
						MGlobal::displayError("Frame Relative Sample must be finite.");
						return MS::kFailure;
					}
					frameRanges.back().shutterSamples.insert(shutterSample);
					if (frameRanges.back().shutterSamples.size() >
						dommy::sampling::kMaxExportShutterSamples) {
						MGlobal::displayError("Too many frame-relative samples were requested.");
						return MS::kFailure;
					}
				}
				
				
				else if (arg == "-pr" || arg == "-preroll")
				{
					frameRanges.back().preRoll = true;
				}
				
				else if (arg == "-ro" || arg == "-renderableonly")
				{
					jobArgs.excludeInvisible = true;
				}
				
				else if (arg == "-s" || arg == "-step")
				{
					if (i+1 >= numJobArgs || !jobArgsArray[i+1].isDouble())
					{
						MGlobal::displayError("Step incorrectly specified.");
						return MS::kFailure;
					}
					frameRanges.back().strideTime = jobArgsArray[++i].asDouble();
					if (!std::isfinite(frameRanges.back().strideTime) ||
						frameRanges.back().strideTime <= 0.0) {
						MGlobal::displayError("Step must be finite and greater than zero.");
						return MS::kFailure;
					}
				}
				
				else if (arg == "-sl" || arg == "-selection")
				{
					jobArgs.useSelectionList = true;
				}
				
				else if (arg == "-sn" || arg == "-stripnamespaces")
				{
					if (i+1 >= numJobArgs || !jobArgsArray[i+1].isUnsigned())
					{
						// the strip all namespaces case
						// so we pick a very LARGE number
						jobArgs.stripNamespace = 0xffffffff;
					}
					else
					{
						jobArgs.stripNamespace = jobArgsArray[++i].asUnsigned();
					}
				}
				
				else if (arg == "-wfg" || arg == "-wholeframegeo")
				{
					sampleGeo = false;
				}
				
				else if (arg == "-ws" || arg == "-worldspace")
				{
					jobArgs.worldSpace = true;
				}
				
				
				else if (arg == "-mfc" || arg == "-melperframecallback")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "melPerFrameCallback incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.melPerFrameCallback = jobArgsArray[++i].asChar();
				}
				
				else if (arg == "-pfc" || arg == "-pythonperframecallback")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "pythonPerFrameCallback incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.pythonPerFrameCallback = jobArgsArray[++i].asChar();
				}
				
				else if (arg == "-mpc" || arg == "-melpostjobcallback")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "melPostJobCallback incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.melPostCallback = jobArgsArray[++i].asChar();
				}
				
				else if (arg == "-ppc" || arg == "-pythonpostjobcallback")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "pythonPostJobCallback incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.pythonPostCallback = jobArgsArray[++i].asChar();
				}
				
				// geomArbParams - attribute filtering stuff
				else if (arg == "-atp" || arg == "-attrprefix")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "attrPrefix incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.prefixFilters.push_back(jobArgsArray[++i].asChar());
				}
				
				else if (arg == "-a" || arg == "-attr")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "attr incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.attribs.insert(jobArgsArray[++i].asChar());
				}
				
				// userProperties - attribute filtering stuff
				else if (arg == "-uatp" || arg == "-userattrprefix")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "userAttrPrefix incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.userPrefixFilters.push_back(jobArgsArray[++i].asChar());
				}
				
				else if (arg == "-prmattr" || arg == "-primAttr")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "primAttr incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.primAttribs.insert(jobArgsArray[++i].asChar());
				}
				
				else if (arg == "-ptattr" || arg == "-pointAttr")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "pointAttr incorrectly specified.");
						return MS::kFailure;
					}
					jobArgs.pointAttribs.insert(jobArgsArray[++i].asChar());
				}
				
				else if (arg == "-rt" || arg == "-root")
				{
#pragma mark root flag
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError("root incorrectly specified.");
						return MS::kFailure;
					}
					hasRoot = true;
					MString root = jobArgsArray[++i];
					
					MSelectionList sel;
					if (sel.add(root) != MS::kSuccess)
					{
						MString warn = root;
						warn += " could not be select, skipping.";
						MGlobal::displayWarning(warn);
						continue;
					}
					
					unsigned int numRoots = sel.length();
                    jobArgs.exportRootAdmission->admitSelection(numRoots);
					for (unsigned int j = 0; j < numRoots; ++j)
					{
						MDagPath path;
						if (!ddata_export_util::readExportDagSelectionItem(sel, j, path))
						{
                            MString warn = root +
                                " contains a non-DAG selection item; skipping.";
							MGlobal::displayWarning(warn);
							continue;
						}
                        ddata_export_util::insertExportRoot(
                            jobArgs.dagPaths, path, *jobArgs.exportRootAdmission);
					}
				}
				else if (arg == "-ef" || arg == "-eulerfilter")
				{
					jobArgs.filterEulerRotations = true;
				}
				else if (arg == "-df" || arg == "-dataformat")
				{
					if (i+1 >= numJobArgs)
					{
						MGlobal::displayError( "dataFormat incorrectly specified.");
						return MS::kFailure;
					}
					MString dataFormat = jobArgsArray[++i];
					dataFormat.toLowerCase();
					if (dataFormat == "ogawa")
					{
						asOgawa = true;
					}
					else
					{
						MGlobal::displayError(
							"Only the Ogawa Alembic data format is supported.");
						return MS::kFailure;
					}
				}
				else
				{
					MString warn = "Ignoring unsupported flag: ";
					warn += jobArgsArray[i];
					MGlobal::displayWarning(warn);
				}
			} //  for i
			
			if (fileName == "")
			{
				MString error = "-file not specified.";
				MGlobal::displayError(error);
				return MS::kFailure;
			}
			
			{
				
#pragma mark プロジェクトルールの確認
				MString fileRule, expandName;
				MString alembicFileRule = "alembicCache";
				MString alembicFilePath = "cache/alembic";
				
				MString queryFileRuleCmd;
				queryFileRuleCmd.format("workspace -q -fre \"^1s\"",
										alembicFileRule);
				
				MString queryFolderCmd;
				queryFolderCmd.format("workspace -en `workspace -q -fre \"^1s\"`",
									  alembicFileRule);
				
				// Query the existing rule without changing or saving the user's
				// workspace. Missing rules use the conventional relative folder
				// for this export only.
				status = MGlobal::executeCommand(queryFileRuleCmd, fileRule);
				if (status == MS::kSuccess && fileRule.length() > 0)
				{
					// we have alembic file rule, query the folder
					status = MGlobal::executeCommand(queryFolderCmd, expandName);
					if (status != MS::kSuccess) {
						expandName.clear();
					}
				}
				
				// resolve the expanded file rule
				if (expandName.length() == 0)
				{
					expandName = alembicFilePath;
				}
				
				// get the path to the alembic file rule
				MFileObject directory;
				DOMMY_RETURN_IF_FAILURE(directory.setRawFullName(expandName));
				MString directoryName = directory.resolvedFullName();
				
				// make sure the cache folder exists
				if (!directory.exists())
				{
					// create the cache folder
					const std::string escapedDirectory =
						dommy::script::escapeMelStringLiteral(directoryName.asChar());
					MString createFolderCmd;
					createFolderCmd.format("sysFile -md \"^1s\"", escapedDirectory.c_str());
					status = MGlobal::executeCommand(createFolderCmd);
					if (status != MS::kSuccess || !directory.exists()) {
						MString error = "Unable to create Alembic output directory: ";
						error += directoryName;
						MGlobal::displayError(error);
                        return MS::kFailure;
                    }
                }

#pragma mark resolve the relative path　ファイルパスの確認
                MFileObject absoluteFile;
                DOMMY_RETURN_IF_FAILURE(absoluteFile.setRawFullName(fileName.c_str()));
                // #if MAYA_API_VERSION < 201300
                //                 if (absoluteFile.resolvedFullName() !=
                //                     absoluteFile.expandedFullName())
                //                 {
                // #else
                if (!MFileObject::isAbsolutePath(fileName.c_str())) {
                    // #endif
                    //  this is a relative path
                    MString absoluteFileName = directoryName + "/" + fileName.c_str();
                    DOMMY_RETURN_IF_FAILURE(absoluteFile.setRawFullName(absoluteFileName));
                    fileName = absoluteFile.resolvedFullName().asChar();
                } else {
                    fileName = absoluteFile.resolvedFullName().asChar();
                }

                if (!dommy::file::isSafeExportPath(fileName, ".abc")) {
                    MGlobal::displayError("Alembic output must be a bounded .abc path without "
                                          "control characters or '..' traversal components.");
                    return MS::kFailure;
                }

                if (!outputPaths.insert(fileName).second) {
                    MString error = "Multiple Alembic jobs target the same file: ";
                    error += fileName.c_str();
                    MGlobal::displayError(error);
                    return MS::kFailure;
                }

                // check the path must exist before writing ファイルパスの確認
                MFileObject absoluteFilePath;
                DOMMY_RETURN_IF_FAILURE(absoluteFilePath.setRawFullName(absoluteFile.resolvedPath()));
                if (!absoluteFilePath.exists()) {
                    MString error;
                    error.format("Path ^1s does not exist!", absoluteFilePath.resolvedFullName());
                    MGlobal::displayError(error);
                    return MS::kFailure;
                }

                // check the file is used by any AlembicNode in the scene　シーン内に同じAlembic
                // Fileを使ったノードを検索。あればエラーを表示
                DOMMY_RETURN_IF_FAILURE(checkAlembicDestinationUnused(absoluteFile));

            }
			
#pragma mark frame range の構築
			if (jobArgs.prefixFilters.empty())
			{
				jobArgs.prefixFilters.push_back("ABC_");
			}

			dommy::sampling::AbcSamplingPlan samplingPlan;
			if (!dommy::sampling::buildAbcSamplingPlan(
					frameRanges, hasRange, sampleGeo, samplingPlan)) {
				MGlobal::displayError(
					"Alembic frame sampling could not be represented safely.");
				return MS::kFailure;
			}

			std::size_t nextTotalSampleEvents = 0U;
			if (!dommy::memory::checkedAdd(
					totalSampleEvents, samplingPlan.sampleEventCount,
					nextTotalSampleEvents) ||
				nextTotalSampleEvents >
					dommy::sampling::kMaxExportSampleEvents) {
				MGlobal::displayError(
					"Alembic frame sampling exceeds the safe export limit.");
				return MS::kFailure;
			}
			totalSampleEvents = nextTotalSampleEvents;
			hasRange = samplingPlan.hasExplicitTimeSampling;

			const std::set<double>& transSamples =
				samplingPlan.transformFrames;
			const std::set<double>& geoSamples =
				samplingPlan.shapeFrames;
			const std::set<double>& preRollSamples =
				samplingPlan.preRollFrames;
			
#pragma mark dagpath check
            jobArgs.exportRootAdmission->validate();
			
#pragma mark no root action rootが指定されなかった時
			
			// no root is specified, and we aren't using a selection
			// so we'll try to translate the whole Maya scene by using all
			// children of the world as roots.
			if (!hasRoot && !jobArgs.useSelectionList)
			{
				MSelectionList sel;
				//#if MAYA_API_VERSION >= 201100
				status = sel.add("|*", true);
				if (status != MS::kSuccess) {
					MGlobal::displayError(
						"Unable to resolve the top-level Maya DAG nodes.");
					return status;
				}
				//#else
				// older versions of Maya will not be able to find top level nodes
				// within namespaces
				//                    sel.add("|*");
				//#endif
				unsigned int numRoots = sel.length();
                jobArgs.exportRootAdmission->admitSelection(numRoots);
				for (unsigned int i = 0; i < numRoots; ++i)
				{
					MDagPath path;
					status = sel.getDagPath(i, path);
					if (status != MS::kSuccess) {
						MGlobal::displayError(
							"Unable to resolve a top-level Maya DAG path.");
						return status;
					}
                    ddata_export_util::insertExportRoot(
                        jobArgs.dagPaths, path, *jobArgs.exportRootAdmission);
				}
				if (jobArgs.dagPaths.empty()) {
					MGlobal::displayError(
						"No top-level Maya DAG nodes are available to export.");
					return MS::kFailure;
				}
			}
			else if (hasRoot && jobArgs.dagPaths.empty())
			{
				MString errorMsg = "No valid root nodes were specified.";
				MGlobal::displayError(errorMsg);
				return MS::kFailure;
			}
			else if (jobArgs.useSelectionList)
			{
				MSelectionList activeList;
				status = MGlobal::getActiveSelectionList(activeList);
				if (status != MS::kSuccess) {
					MGlobal::displayError(
						"Unable to read the active Maya selection.");
					return status;
				}
				if (activeList.length() == 0)
				{
					MString errorMsg =
					"-selection specified but nothing is actively selected.";
					MGlobal::displayError(errorMsg);
					return MS::kFailure;
				}
                jobArgs.exportRootAdmission->admitSelection(activeList.length());
			}
            jobArgs.exportRootAdmission->validate();
			
			
#pragma mark AlembicのTime rangeの指定
			AbcA::TimeSamplingPtr transTime, geoTime, boundsTime;
			const double secondsPerFrame = ddata_export_util::spf();
			if (!std::isfinite(secondsPerFrame) || secondsPerFrame <= 0.0) {
				MGlobal::displayError("The Maya frame duration is invalid.");
				return MS::kFailure;
			}

			if (!createTimeSampling(
					samplingPlan.transformTime,
					secondsPerFrame, transTime)) {
				MGlobal::displayError(
					"Alembic transform sampling is invalid.");
				return MS::kFailure;
			}

			if (sampleGeo || !hasRange) {
				geoTime = transTime;
			} else if (!createTimeSampling(
					samplingPlan.shapeTime,
					secondsPerFrame, geoTime)) {
				MGlobal::displayError(
					"Alembic geometry sampling is invalid.");
				return MS::kFailure;
			}
			
			
#pragma mark Jobの作成
			if (!createTimeSampling(samplingPlan.boundsTime, secondsPerFrame, boundsTime)) {
				MGlobal::displayError("Alembic bounds sampling is invalid.");
				return MS::kFailure;
			}
			PendingAbcWriteJobPtr job(new PendingAbcWriteJob(
				fileName, asOgawa, transSamples, transTime,
				geoSamples, geoTime, boundsTime, jobArgs));
			
			jobList.push_front(job);
			allJobs.push_back(job);
			
			// make sure we add additional whole frames, if we arent skipping
			// the inbetween ones
			if (!skipFrame && !allFrameRange.empty())
			{
				double localMin = *(transSamples.begin());
				std::set<double>::iterator last = transSamples.end();
				last--;
				double localMax = *last;
				
				double globalMin = *(allFrameRange.begin());
				last = allFrameRange.end();
				last--;
				double globalMax = *last;
				
				// if the min of our current frame range is beyond
				// what we know about, pad a few more frames
				if (localMin > globalMax)
				{
					if (!checkedInsertUnitFrames(
							globalMax, localMin, allFrameRange)) {
						MGlobal::displayError(
							"Alembic evaluation padding exceeds the safe frame limit.");
						return MS::kFailure;
					}
				}
				
				// if the max of our current frame range is beyond
				// what we know about, pad a few more frames
				if (localMax < globalMin)
				{
					if (!checkedInsertUnitFrames(
							localMax, globalMin, allFrameRange)) {
						MGlobal::displayError(
							"Alembic evaluation padding exceeds the safe frame limit.");
						return MS::kFailure;
					}
				}
			}
			
			// right now we just copy over the translation samples since
			// they are guaranteed to contain all the geometry samples
			allFrameRange.insert(transSamples.begin(), transSamples.end());
			
			// copy over the pre-roll samples
			allFrameRange.insert(preRollSamples.begin(), preRollSamples.end());
			if (allFrameRange.size() >
				dommy::sampling::kMaxExportFrameSamples) {
				MGlobal::displayError(
					"Alembic evaluation range exceeds the safe frame limit.");
				return MS::kFailure;
			}
		}
		
		// add extra evaluation run up, if necessary
		if (startEvaluationTime != DBL_MAX && !allFrameRange.empty())
		{
			double firstFrame = *allFrameRange.begin();
			if (!checkedInsertUnitFrames(
					startEvaluationTime, firstFrame, allFrameRange)) {
				MGlobal::displayError(
					"Alembic pre-roll evaluation exceeds the safe frame limit.");
				return MS::kFailure;
			}
		}
        
        






		
#pragma mark Start Write
		std::set<double>::iterator it = allFrameRange.begin();
		std::set<double>::iterator itEnd = allFrameRange.end();
		
		dommy::maya::ScopedComputation computation;
		
		// loop through every frame in the list, if a job has that frame in it's
		// list of transform or shape frames, then it will write out data and
		// call the perFrameCallback, if that frame is also the last one it has
		// to work on then it will also call the postCallback.
		// If it doesn't have this frame, then it does nothing
		for (; it != itEnd; it++)
		{
			if (computation.isInterruptRequested()) {
				MGlobal::displayWarning("Alembic export was cancelled.");
				return MS::kFailure;
			}
			if (verbose)
			{
				double frame = *it;
				MString info;
				info = frame;
				MGlobal::displayInfo(info);
			}
			
			status = MGlobal::viewFrame(*it);
			if (status != MS::kSuccess) {
				return status;
			}
			std::list<PendingAbcWriteJobPtr>::iterator j = jobList.begin();
			std::list<PendingAbcWriteJobPtr>::iterator jend = jobList.end();
			while (j != jend)
			{
				if (computation.isInterruptRequested()) {
					MGlobal::displayWarning("Alembic export was cancelled.");
					return MS::kFailure;
				}
				
				bool lastFrame = (*j)->eval(*it);
				
				if (lastFrame)
				{
					j = jobList.erase(j);
				}
				else
					j++;
			}
		}
		if (!jobList.empty()) {
			MGlobal::displayError("Alembic export ended before every job completed.");
			return MS::kFailure;
		}
        // Completed jobs have already closed their archives. Restore Maya's
        // time before replacing any destination so a restore failure cannot
        // report command failure after publishing otherwise completed files.
        status = timeRestore.restore();
        if (status != MS::kSuccess) {
            MGlobal::displayError("Unable to restore Maya time after Alembic export.");
            return status;
        }
		computation.finish();
		for (std::vector<PendingAbcWriteJobPtr>::iterator job = allJobs.begin();
			 job != allJobs.end(); ++job) {
			(*job)->commit();
		}
#pragma mark End Write
		return MS::kSuccess;
	}
	catch (Alembic::Util::Exception & e)
	{
		MString theError("Alembic Exception encountered: ");
		theError += e.what();
		MGlobal::displayError(theError);
		return MS::kFailure;
	}
	catch (std::exception & e)
	{
		MString theError("std::exception encountered: ");
		theError += e.what();
		MGlobal::displayError(theError);
		return MS::kFailure;
	}
	catch (...)
	{
		MGlobal::displayError("Unknown exception encountered during Alembic export.");
		return MS::kFailure;
	}
	
}
