// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2012,
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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaUtility.cpp

//
//  DCExportUtil.cpp
//  dommyTool
//
//  Created by yuzo on 2017/03/16.
//
//

#include "DDataExportUtil.h"
#include "common/utility/DWorkBudget.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace ddata_export_util {
void requireExportQuery(const MStatus& status, const char* message)
{
    if (!status) throw std::runtime_error(message);
}

std::string exportDagPathName(const MDagPath& path)
{
    MStatus status;
    const MString value = path.fullPathName(&status);
    requireExportQuery(status, "Unable to read an Alembic root path name");
    if (value.length() > dommy::exporting::kMaxAbcRootPathBytes)
        throw std::length_error("Alembic root path name is too long");
    const std::string name(value.asChar());
    if (name.empty() || name.size() > dommy::exporting::kMaxAbcRootPathBytes)
        throw std::invalid_argument("Invalid Alembic root path name");
    return name;
}

unsigned int exportDagDepth(const MDagPath& path)
{
    MStatus status;
    const unsigned int depth = path.length(&status);
    requireExportQuery(status, "Unable to read an Alembic root path depth");
    if (depth == 0U || depth > dommy::exporting::kMaxAbcDagDepth)
        throw std::length_error("Invalid Alembic root path depth");
    return depth;
}

bool readExportDagSelectionItem(const MSelectionList& selection,
                                unsigned int index, MDagPath& path)
{
    MDagPath candidate;
    const MStatus status = selection.getDagPath(index, candidate);
    if (!status) {
        MObject object;
        requireExportQuery(selection.getDependNode(index, object),
                           "Unable to classify an Alembic selection item");
        if (!object.hasFn(MFn::kDagNode)) return false;
        requireExportQuery(status, "Unable to read a selected Alembic DAG path");
    }
    path = candidate;
    return true;
}

MString exportObjectName(const MFnDependencyNode& node, unsigned int stripDepth)
{
    const MString source = exportNodeName(node);
    if (source.length() == 0U || source.length() > dommy::exporting::kMaxAbcRootPathBytes)
        throw std::length_error("Invalid or excessive Alembic object name");
    const MString result = stripNamespaces(source, stripDepth);
    const std::string bytes(result.asChar());
    if (bytes.empty() || bytes.size() > dommy::exporting::kMaxAbcRootPathBytes)
        throw std::length_error("Invalid or excessive Alembic object name bytes");
    return result;
}

void insertExportRoot(ShapeSet& roots, const MDagPath& path,
                      dommy::exporting::RootAdmission& admission)
{
    (void)exportDagDepth(path);
    const std::string name = exportDagPathName(path);
    if (admission.admit(name, sizeof(MDagPath)))
        roots.insert(path);
}

void prepareExportSelection(MSelectionList& expanded, ShapeSet& roots,
                            const MSelectionList& active,
                            dommy::exporting::RootAdmission& admission)
{
    const bool inferRoots = roots.empty();
    const unsigned int count = active.length();
    admission.admitSelection(count);
    expanded = active;
    bool foundPath = false;
    std::size_t visits = count;
    for (unsigned int index = 0U; index < count; ++index) {
        MDagPath path;
        if (!readExportDagSelectionItem(active, index, path)) continue;
        foundPath = true;
        unsigned int depth = exportDagDepth(path);
        while (depth > 1U) {
            if (++visits > dommy::generation::kMaxConnectedGeometryCount)
                throw std::length_error("Alembic selection expansion exceeds its item limit");
            admission.consume(1U);
            requireExportQuery(path.pop(), "Unable to walk an active Maya DAG path");
            requireExportQuery(expanded.add(path, MObject::kNullObj, true),
                               "Unable to expand the active Maya selection");
            --depth;
        }
        if (inferRoots) insertExportRoot(roots, path, admission);
    }
    if (!foundPath || (inferRoots && roots.empty()))
        throw std::invalid_argument("The active selection contains no exportable Maya DAG paths");
    admission.validate();
}

void requireUniqueExportRootNames(const ShapeSet& roots, unsigned int stripDepth,
                                  dommy::exporting::RootAdmission& admission)
{
    admission.admitSelection(roots.size());
    std::map<std::string, std::string> names;
    std::size_t totalBytes = 0U, maximumNameBytes = 0U;
    for (const auto& path : roots) {
        const std::string pathName = exportDagPathName(path);
        MStatus status;
        MFnDependencyNode node(exportDagNode(path), &status);
        requireExportQuery(status, "Unable to bind an Alembic root name query");
        const std::string name = exportObjectName(node, stripDepth).asChar();
        std::size_t bytes = 0U, records = 0U, storage = 0U;
        if (!dommy::memory::checkedAdd(pathName.size(), name.size(), bytes) ||
            !dommy::generation::checkedSum(totalBytes, bytes,
                dommy::generation::kMaxGeneratedStringByteCount, bytes) ||
            !dommy::memory::checkedMultiply(names.size() + 1U,
                2U * sizeof(std::string) + 4U * sizeof(void*), records) ||
            !dommy::memory::checkedAdd(bytes, records, storage))
            throw std::length_error("Alembic root output names exceed their storage budget");
        admission.requireTemporaryStorage(storage);
        admission.consume(pathName.size() + name.size() + 1U);
        maximumNameBytes = std::max(maximumNameBytes, name.size());
        if (!admission.work().consumeProduct(
                dommy::generation::orderedContainerWork(names.size() + 1U), 2U * maximumNameBytes))
            throw std::length_error("Alembic root output-name comparisons exceed the work budget");
        const auto inserted = names.emplace(name, pathName);
        if (!inserted.second)
            throw std::invalid_argument("Conflicting Alembic root output name '" + name +
                                       "': " + inserted.first->second + " and " + pathName);
        totalBytes = bytes;
    }
}
} // namespace ddata_export_util

struct NodesToCheckStruct
{
	MObject node;
	bool    checkParent;
};



// return seconds per frame
double ddata_export_util::spf()
{
    static const MTime sec(1.0, MTime::kSeconds);
    return 1.0 / sec.as(MTime::uiUnit());
}


bool ddata_export_util::isAncestorDescendentRelationship(const MDagPath & path1,
                                                      const MDagPath & path2)
{
    const unsigned int length1 = exportDagDepth(path1);
    const unsigned int length2 = exportDagDepth(path2);
    unsigned int diff;
    
    if (length1 == length2 && !(path1 == path2))
        return false;
    
    MDagPath ancestor, descendent;
    if (length1 > length2)
    {
        ancestor = path2;
        descendent = path1;
        diff = length1 - length2;
    }
    else
    {
        ancestor = path1;
        descendent = path2;
        diff = length2 - length1;
    }
    
    if (descendent.pop(diff) != MS::kSuccess)
    {
        throw std::runtime_error(
            "Unable to compare Maya DAG ancestor paths");
    }
    
    bool ret = (ancestor == descendent);
    
    if (ret)
    {
        MString err = (exportDagPathName(path1) + " and " +
                      exportDagPathName(path2) + " have parenting relationships").c_str();
        MGlobal::displayError(err);
    }
    return ret;
}



// returns 0 if static, 1 if sampled, and 2 if a curve
int ddata_export_util::getSampledType(const MPlug& root, dommy::generation::WorkBudget& work)
{
    const std::size_t maximumDepth = 256U;
    struct Frame {
        explicit Frame(const MPlug& value) : plug(value) {}
        MPlug plug;
        unsigned int index = 0U, count = 0U;
        bool array = false, expanded = false;
    };
    std::vector<Frame> stack;
    stack.emplace_back(root);
    while (!stack.empty()) {
        Frame& frame = stack.back();
        const MPlug iPlug = frame.plug;
        MStatus status;
        if (frame.expanded) {
            if (frame.index == frame.count) { stack.pop_back(); continue; }
            if (stack.size() > maximumDepth)
                throw std::length_error("Alembic sampling plug depth exceeds its limit");
            const unsigned int index = frame.index;
            const MPlug child = frame.array
                ? iPlug.connectionByPhysicalIndex(index, &status) : iPlug.child(index, &status);
            requireExportQuery(status, "Unable to acquire an Alembic sampling child/element");
            ++frame.index;
            stack.emplace_back(child);
            continue;
        }
        if (!work.consume(1U))
            throw std::length_error("Alembic sampling inspection exceeds its work budget");
        MPlugArray connections;
        iPlug.connectedTo(connections, true, false, &status);
        requireExportQuery(status, "Unable to query Alembic sampling connections");
        if (connections.length() > dommy::generation::kMaxConnectedGeometryCount ||
            !work.consume(connections.length()))
            throw std::length_error("Too many Alembic sampling connections");
        if (connections.length() != 0U) {
            const int type = [&]() -> int {
                MFnDependencyNode nodeFn;
                for (unsigned int index = 0U; index < connections.length(); ++index) {
                    const MObject object = connections[index].node(&status);
                    requireExportQuery(status, "Unable to acquire an Alembic sampling source node");
                    switch (object.apiType()) {
                    case MFn::kAnimCurveTimeToAngular:
                    case MFn::kAnimCurveTimeToDistance:
                    case MFn::kAnimCurveTimeToTime:
                    case MFn::kAnimCurveTimeToUnitless: {
                        requireExportQuery(nodeFn.setObject(object), "Unable to bind an Alembic animation curve");
                        const MPlug incoming = nodeFn.findPlug("i", true, &status);
                        requireExportQuery(status, "Unable to find an Alembic animation curve input");
                        const bool connected = incoming.isConnected(&status);
                        requireExportQuery(status, "Unable to query an Alembic animation curve input");
                        return connected ? 1 : 2;
                    }
                    case MFn::kMute: {
                        requireExportQuery(nodeFn.setObject(object), "Unable to bind an Alembic mute node");
                        const MPlug mute = nodeFn.findPlug("mute", true, &status);
                        requireExportQuery(status, "Unable to find an Alembic mute input");
                        const bool muted = mute.asBool(&status);
                        requireExportQuery(status, "Unable to read an Alembic mute input");
                        return muted ? 0 : 2;
                    }
                    default: break;
                    }
                }
                return 1;
            }();
            if (type > 0) return type;
            // A static/muted child must not stop inspection of its siblings.
            stack.pop_back();
            continue;
        }
        // Array elements and compound children can be connected independently.
        const bool array = iPlug.isArray(&status);
        requireExportQuery(status, "Unable to identify an Alembic sampling array");
        frame.array = array;
        if (array) {
            frame.count = iPlug.numConnectedElements(&status);
            requireExportQuery(status, "Unable to count Alembic sampling elements");
        } else {
            const bool compound = iPlug.isCompound(&status);
            requireExportQuery(status, "Unable to identify an Alembic sampling compound");
            if (compound) {
                const unsigned int connectedChildren = iPlug.numConnectedChildren(&status);
                requireExportQuery(status, "Unable to count connected Alembic sampling children");
                if (connectedChildren > 0U) {
                    frame.count = iPlug.numChildren(&status);
                    requireExportQuery(status, "Unable to count Alembic sampling children");
                }
            }
        }
        if (frame.count > dommy::generation::kMaxConnectedGeometryCount || !work.consume(frame.count))
            throw std::length_error("Too many Alembic sampling children/elements");
        frame.expanded = true;
    }
    return 0;
}

int ddata_export_util::getSampledType(const MPlug& iPlug)
{
    dommy::generation::WorkBudget work;
    return getSampledType(iPlug, work);
}


// 0 dont write, 1 write static 0, 2 write anim 0, 3 write anim -1
int ddata_export_util::getVisibilityType(const MPlug & iPlug)
{
    const int type = getSampledType(iPlug);
    MStatus status;
    const bool visible = iPlug.asBool(&status);
    requireExportQuery(status, "Unable to read Alembic visibility");
    return type == 0 ? (visible ? 0 : 1) : (visible ? 3 : 2);
}


// does this cover all cases?
bool ddata_export_util::isAnimated(MObject & object, bool checkParent)
{
	MStatus stat;
	MItDependencyGraph iter(object, MFn::kInvalid,
							MItDependencyGraph::kUpstream,
							MItDependencyGraph::kDepthFirst,
							MItDependencyGraph::kPlugLevel,
							&stat);
	
	if (stat!= MS::kSuccess)
	{
		MGlobal::displayError("Unable to create DG iterator ");
	}
	
	// MAnimUtil::isAnimated(node) will search the history of the node
	// for any animation curve nodes. It will return true for those nodes
	// that have animation curve in their history.
	// The average time complexity is O(n^2) where n is the number of history
	// nodes. But we can improve the best case by split the loop into two.
	std::vector<NodesToCheckStruct> nodesToCheckAnimCurve;
	
	NodesToCheckStruct nodeStruct;
	for (; !iter.isDone(); iter.next())
	{
		MObject node = iter.currentItem();
		
		if (node.hasFn(MFn::kPluginDependNode) ||
			node.hasFn( MFn::kConstraint ) ||
			node.hasFn(MFn::kPointConstraint) ||
			node.hasFn(MFn::kAimConstraint) ||
			node.hasFn(MFn::kOrientConstraint) ||
			node.hasFn(MFn::kScaleConstraint) ||
			node.hasFn(MFn::kGeometryConstraint) ||
			node.hasFn(MFn::kNormalConstraint) ||
			node.hasFn(MFn::kTangentConstraint) ||
			node.hasFn(MFn::kParentConstraint) ||
			node.hasFn(MFn::kPoleVectorConstraint) ||
			node.hasFn(MFn::kParentConstraint) ||
			node.hasFn(MFn::kTime) ||
			node.hasFn(MFn::kJoint) ||
			node.hasFn(MFn::kGeometryFilt) ||
			node.hasFn(MFn::kTweak) ||
			node.hasFn(MFn::kPolyTweak) ||
			node.hasFn(MFn::kSubdTweak) ||
			node.hasFn(MFn::kCluster) ||
			node.hasFn(MFn::kFluid) ||
			node.hasFn(MFn::kPolyBoolOp))
		{
			return true;
		}
		
		if (node.hasFn(MFn::kExpression))
		{
			MFnExpression fn(node, &stat);
			if (stat == MS::kSuccess && fn.isAnimated())
			{
				return true;
			}
		}
		
		// skip shading nodes
		if (!node.hasFn(MFn::kShadingEngine))
		{
			MPlug plug = iter.thisPlug();
			MFnAttribute attr(plug.attribute(), &stat);
			bool checkNodeParent = false;
			if (stat == MS::kSuccess && attr.isWorldSpace())
			{
				checkNodeParent = true;
			}
			
			nodeStruct.node = node;
			nodeStruct.checkParent = checkParent || checkNodeParent;
			nodesToCheckAnimCurve.push_back(nodeStruct);
		}
		else
		{
			// and don't traverse the rest of their subgraph
			iter.prune();
		}
	}
	
	for (size_t i = 0; i < nodesToCheckAnimCurve.size(); i++)
	{
		if (MAnimUtil::isAnimated(nodesToCheckAnimCurve[i].node, nodesToCheckAnimCurve[i].checkParent))
		{
			return true;
		}
	}
	
	return false;
}

bool ddata_export_util::isDrivenByFBIK(const MFnIkJoint & iJoint)
{
	// check joints that are driven by Maya FBIK
	// Maya FBIK has no connection to joints' TRS plugs
	// but TRS of joints are driven by FBIK, they are not static
	// Maya 2012's new HumanIK has connections to joints.
	// FBIK is a special case.
	MStatus status = MS::kSuccess;
    const MString name = iJoint.hikJointName(&status);
    requireExportQuery(status, "Unable to inspect Alembic FBIK animation");
    return name.length() > 0U;
}

bool ddata_export_util::isDrivenBySplineIK(const MFnIkJoint & iJoint)
{
	// spline IK can drive the starting joint's translate channel but
	// it has no connection to the translate plug.
	// we treat the joint as animated in this case.
	// find the ikHandle node.
	MStatus status;
    const MPlug msgPlug = requiredExportPlug(iJoint, "message");
	MPlugArray msgPlugDst;
	msgPlug.connectedTo(msgPlugDst, false, true, &status);
    requireExportQuery(status, "Unable to inspect Alembic joint message connections");
    if (msgPlugDst.length() > dommy::generation::kMaxConnectedGeometryCount) {
        throw std::length_error("Too many Alembic joint message connections");
    }
    dommy::generation::WorkBudget work;
    if (!work.consume(msgPlugDst.length())) {
        throw std::length_error("Alembic IK inspection exceeds the work budget");
    }
	for (unsigned int i = 0; i < msgPlugDst.length(); i++) {
        const MObject handle = msgPlugDst[i].node(&status);
        requireExportQuery(status, "Unable to acquire an Alembic IK handle");
		MFnDependencyNode ikHandle(handle, &status);
        requireExportQuery(status, "Unable to bind an Alembic IK handle");
		if (!handle.hasFn(MFn::kIkHandle)) continue;
		
		// find the ikSolver node.
		const MPlug ikSolverPlug = requiredExportPlug(ikHandle, "ikSolver");
		MPlugArray ikSolverDst;
		ikSolverPlug.connectedTo(ikSolverDst, true, false, &status);
        requireExportQuery(status, "Unable to inspect an Alembic IK solver connection");
        if (ikSolverDst.length() > dommy::generation::kMaxConnectedGeometryCount ||
            !work.consume(ikSolverDst.length())) {
            throw std::length_error("Too many Alembic IK solver connections");
        }
		for (unsigned int j = 0; j < ikSolverDst.length(); j++) {
			
			// return true if the ikSolver is a spline solver.
            const MObject solver = ikSolverDst[j].node(&status);
            requireExportQuery(status, "Unable to acquire an Alembic spline solver");
			if (solver.hasFn(MFn::kSplineSolver)) {
				return true;
			}
		}
	}
	
	return false;
}






bool ddata_export_util::getRotOrder(MTransformationMatrix::RotationOrder iOrder,
					   unsigned int & oXAxis, unsigned int & oYAxis, unsigned int & oZAxis)
{
	switch (iOrder)
	{
		case MTransformationMatrix::kXYZ:
		{
			oXAxis = 0;
			oYAxis = 1;
			oZAxis = 2;
		}
			break;
			
		case MTransformationMatrix::kYZX:
		{
			oXAxis = 1;
			oYAxis = 2;
			oZAxis = 0;
		}
			break;
			
		case MTransformationMatrix::kZXY:
		{
			oXAxis = 2;
			oYAxis = 0;
			oZAxis = 1;
		}
			break;
			
		case MTransformationMatrix::kXZY:
		{
			oXAxis = 0;
			oYAxis = 2;
			oZAxis = 1;
		}
			break;
			
		case MTransformationMatrix::kYXZ:
		{
			oXAxis = 1;
			oYAxis = 0;
			oZAxis = 2;
		}
			break;
			
		case MTransformationMatrix::kZYX:
		{
			oXAxis = 2;
			oYAxis = 1;
			oZAxis = 0;
		}
			break;
			
		default:
		{
			return false;
		}
	}
	return true;
}



bool ddata_export_util::isIntermediate(const MObject & object)
{
    MStatus status;
    MFnDagNode node(object, &status);
    requireExportQuery(status, "Unable to bind an Alembic intermediate-object query");
    const bool intermediate = node.isIntermediateObject(&status);
    requireExportQuery(status, "Unable to inspect an Alembic intermediate object");
    return intermediate;
}




MString ddata_export_util::stripNamespaces(const MString& nodeName, unsigned int depth)
{
    if (nodeName.length() > dommy::exporting::kMaxAbcRootPathBytes)
        throw std::length_error("Alembic object name is too long");
    if (depth == 0U) return nodeName;
    MStringArray parts;
    requireExportQuery(nodeName.split(':', parts), "Unable to split an Alembic object namespace");
    const unsigned int count = parts.length();
    if (count == 0U) return nodeName;
    if (depth >= count - 1U) return parts[count - 1U];
    MString result;
    for (unsigned int index = depth; index < count - 1U; ++index) {
        result += parts[index];
        result += ":";
    }
    result += parts[count - 1U];
    return result;
}

MString ddata_export_util::getHelpText()
{
    MString ret =
        "DExportAbcCmd [options]\n"
        "Options:\n"
        "-h / -help  Print this message.\n"
        "\n"
        "-prs / -preRollStartFrame double\n"
        "The frame to start scene evaluation at.  This is used to set the\n"
        "starting frame for time dependent translations and can be used to evaluate\n"
        "run-up that isn't actually translated.\n"
        "\n"
        "-duf / -dontSkipUnwrittenFrames\n"
        "When evaluating multiple translate jobs, the presence of this flag decides\n"
        "whether to evaluate frames between jobs when there is a gap in their frame\n"
        "ranges.\n"
        "\n"
        "-v / -verbose\n"
        "Prints the current frame that is being evaluated.\n"
        "\n"
        "-j / -jobArg string REQUIRED\n"
        "String which contains flags for writing data to a particular file.\n"
        "Multiple jobArgs can be specified.\n"
        "\n"
        "-jobArg flags:\n"
        "\n"
        "-a / -attr string\n"
        "A specific geometric attribute to write out.\n"
        "This flag may occur more than once.\n"
        "\n"
        "-df / -dataFormat string\n"
        "The data format to use to write the file.  Only Ogawa is supported.\n"
        "The default is Ogawa.\n"
        "\n"
        "-atp / -attrPrefix string (default ABC_)\n"
        "Prefix filter for determining which geometric attributes to write out.\n"
        "This flag may occur more than once.\n"
        "\n"
        "-ef / -eulerFilter\n"
        "If this flag is present, apply Euler filter while sampling rotations.\n"
        "\n"
        "-f / -file string REQUIRED\n"
        "File location to write the Alembic data.\n"
        "\n"
        "-fr / -frameRange double double\n"
        "The frame range to write.\n"
        "Multiple occurrences of -frameRange are supported within a job. Each\n"
        "-frameRange defines a new frame range. -step or -frs will affect the\n"
        "current frame range only.\n"
        "\n"
        "-frs / -frameRelativeSample double\n"
        "frame relative sample that will be written out along the frame range.\n"
        "This flag may occur more than once.\n"
        "\n"
        "-pr / -preRoll\n"
        "If this flag is present, this frame range will not be sampled.\n"
        "\n"
        "-ro / -renderableOnly\n"
        "If this flag is present non-renderable hierarchy (invisible, or templated)\n"
        "will not be written out.\n"
        "\n"
        "-rt / -root\n"
        "Maya dag path which will be parented to the root of the Alembic file.\n"
        "This flag may occur more than once.  If unspecified, it defaults to '|' which\n"
        "means the entire scene will be written out.\n"
        "\n"
        "-s / -step double (default 1.0)\n"
        "The time interval (expressed in frames) at which the frame range is sampled.\n"
        "Additional samples around each frame can be specified with -frs.\n"
        "\n"
        "-sl / -selection\n"
        "If this flag is present, write out all all selected nodes from the active\n"
        "selection list that are descendents of the roots specified with -root.\n"
        "\n"
        "-sn / -stripNamespaces (optional int)\n"
        "If this flag is present all namespaces will be stripped off of the node before\n"
        "being written to Alembic.  If an optional int is specified after the flag\n"
        "then that many namespaces will be stripped off of the node name. Be careful\n"
        "that the new stripped name does not collide with other sibling node names.\n\n"
        "Examples: \n"
        "taco:foo:bar would be written as just bar with -sn\n"
        "taco:foo:bar would be written as foo:bar with -sn 1\n"
        "\n"
        "-ptattr / -pointAttr string\n"
        "A specific point attribute to write out.  This flag may occur more than once.\n"
        "\n"
        "-prmattr / -primAttr string\n"
        "A specific primitive attribute to write out.  This flag may occur more than once.\n"
        "\n"
        "-uatp / -userAttrPrefix string\n"
        "Prefix filter for determining which user attributes to write out.\n"
        "This flag may occur more than once.\n"
        "\n"
        "-wfg / -wholeFrameGeo\n"
        "If this flag is present data for geometry will only be written out on whole\n"
        "frames.\n"
        "\n"
        "-ws / -worldSpace\n"
        "If this flag is present, any root nodes will be stored in world space.\n"
        "\n"
        "-mfc / -melPerFrameCallback string\n"
        "When each frame (and the static frame) is evaluated the string specified is\n"
        "evaluated as a Mel command. See below for special processing rules.\n"
        "\n"
        "-mpc / -melPostJobCallback string\n"
        "When the translation has finished the string specified is evaluated as a Mel\n"
        "command. See below for special processing rules.\n"
        "\n"
        "-pfc / -pythonPerFrameCallback string\n"
        "When each frame (and the static frame) is evaluated the string specified is\n"
        "evaluated as a python command. See below for special processing rules.\n"
        "\n"
        "-ppc / -pythonPostJobCallback string\n"
        "When the translation has finished the string specified is evaluated as a\n"
        "python command. See below for special processing rules.\n"
        "\n"
        "Special callback information:\n"
        "Callback strings execute with the current Maya session's permissions. "
        "Only use trusted MEL or Python code.\n"
        "On the callbacks, special tokens are replaced with other data, these tokens\n"
        "and what they are replaced with are as follows:\n"
        "\n"
        "#FRAME# replaced with the frame number being evaluated.\n"
        "#FRAME# is ignored in the post callbacks.\n"
        "\n"
        "#BOUNDS# replaced with a string holding bounding box values in minX minY minZ\n"
        "maxX maxY maxZ space seperated order.\n"
        "\n"
        "#BOUNDSARRAY# replaced with the bounding box values as above, but in\n"
        "array form.\n"
        "In Mel: {minX, minY, minZ, maxX, maxY, maxZ}\n"
        "In Python: [minX, minY, minZ, maxX, maxY, maxZ]\n"
        "\n"
        "Examples:\n"
        "\n"
        "DExportAbcCmd -j \"-root |group|foo -root |test|path|bar -file /tmp/test.abc\"\n"
        "Writes out everything at foo and below and bar and below to /tmp/test.abc.\n"
        "foo and bar are siblings parented to the root of the Alembic scene.\n"
        "\n"
        "DExportAbcCmd -j \"-frameRange 1 5 -step 0.5 -root |group|foo -file /tmp/test.abc\"\n"
        "Writes out everything at foo and below to /tmp/test.abc sampling at frames:\n"
        "1 1.5 2 2.5 3 3.5 4 4.5 5\n"
        "\n"
        "DExportAbcCmd -j \"-fr 0 10 -frs -0.1 -frs 0.2 -step 5 -file /tmp/test.abc\"\n"
        "Writes out everything in the scene to /tmp/test.abc sampling at frames:\n"
        "-0.1 0.2 4.9 5.2 9.9 10.2\n"
        "\n"
        "Note: The difference between your highest and lowest frameRelativeSample can\n"
        "not be greater than your step size.\n"
        "\n"
        "DExportAbcCmd -j \"-step 0.25 -frs 0.3 -frs 0.60 -fr 1 5 -root foo -file test.abc\"\n"
        "\n"
        "Is illegal because the highest and lowest frameRelativeSamples are 0.3 frames\n"
        "apart.\n"
        "\n"
        "DExportAbcCmd -j \"-sl -root |group|foo -file /tmp/test.abc\"\n"
        "Writes out all selected nodes and it's ancestor nodes including up to foo.\n"
        "foo will be parented to the root of the Alembic scene.\n"
        "\n";

    return ret;
}
