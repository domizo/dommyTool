#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MFnMesh.h>
#include <maya/MObjectArray.h>
#include <maya/MFnSet.h>
#include <maya/MMatrix.h>
#include <maya/MFloatMatrix.h>
#include <maya/M3dView.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>

#include "setParticleRGBCmd.h"
#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "DGlobalTex.h"






namespace {

const char kOnShadowFlag[] = "-os";
const char kOnShadowLongFlag[] = "-onShadow";
const char kOnReuseMapFlag[] = "-orm";
const char kOnReuseMapLongFlag[] = "-onReuseMap";
const char kCameraFlag[] = "-cam";
const char kCameraLongFlag[] = "-camera";

MStatus commandFailure(const MString& message) {
    MGlobal::displayError(message);
    return MS::kFailure;
}

MStatus extendToShape(MDagPath& path, MFn::Type expectedType,
                      const MString& selectionDescription) {
    MStatus status;
    MObject node = path.node(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    if (node.hasFn(expectedType)) {
        return MS::kSuccess;
    }
    if (!node.hasFn(MFn::kTransform) || path.extendToShape() != MS::kSuccess) {
        return commandFailure(selectionDescription);
    }
    node = path.node(&status);
    if (status != MS::kSuccess || !node.hasFn(expectedType)) {
        return commandFailure(selectionDescription);
    }
    return MS::kSuccess;
}

} // namespace


setParticleRGBCmd::setParticleRGBCmd()
    : onShadow(false),
      onReuseMap(false),
      hadRgbAttribute(false),
      attributeEditsQueued(false),
      attributesApplied(false),
      planReady(false),
      applied(false)
{
}

setParticleRGBCmd::~setParticleRGBCmd() = default;

void* setParticleRGBCmd::creator()
{
    return ::dommy::maya::createObject("setParticleRGBCmd", [&]() {
        return new setParticleRGBCmd;
    });
}


MStatus setParticleRGBCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kOnShadowFlag, kOnShadowLongFlag, MSyntax::kBoolean));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kOnReuseMapFlag, kOnReuseMapLongFlag, MSyntax::kBoolean));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kCameraFlag, kCameraLongFlag, MSyntax::kString));
    
    return MS::kSuccess;
}

MSyntax setParticleRGBCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<setParticleRGBCmd>("setParticleRGBCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<setParticleRGBCmd>(
        "setParticleRGBCmd", std::current_exception());
    return MSyntax();
}

MStatus setParticleRGBCmd::parseArgs(const MArgList &args)
{
    onShadow = false;
    onReuseMap = false;
    cameraName.clear();

	MSyntax commandSyntax;
    MStatus status = dommy::maya::buildCommandSyntax<setParticleRGBCmd>(
        commandSyntax, "setParticleRGBCmd");
    if (!status) return status;
    MArgDatabase argData(commandSyntax, args, &status);
	if (status != MS::kSuccess) {
		return status;
	}
	
	if (argData.isFlagSet(kOnShadowFlag))
	{
		status = argData.getFlagArgument(kOnShadowFlag, 0, onShadow);
		if (!status) {
			status.perror("onShadow flag parsing failed");
			return status;
		}
	}
	
	if (argData.isFlagSet(kOnReuseMapFlag))
	{
		status = argData.getFlagArgument(kOnReuseMapFlag, 0, onReuseMap);
		if (!status) {
			status.perror("onReuseMap flag parsing failed");
			return status;
		}
	}

    if (argData.isFlagSet(kCameraFlag))
    {
        status = argData.getFlagArgument(kCameraFlag, 0, cameraName);
        if (!status) {
            status.perror("camera flag parsing failed");
            return status;
        }
    }
	
	return MS::kSuccess;
}	


MStatus setParticleRGBCmd::doIt(const MArgList& args) {
    try {
    planReady = false;
    applied = false;
    attributesApplied = false;
    particleHandle = MObjectHandle();
    MStatus status = originalColors.clear();
    if (!status) return status;
    status = targetColors.clear();
    if (!status) return status;

    status = parseArgs(args);
    if (status != MS::kSuccess) {
        return status;
    }

    MSelectionList selection;
    status = MGlobal::getActiveSelectionList(selection);
    if (status != MS::kSuccess) {
        return status;
    }
    if (selection.length() < 2) {
        return commandFailure("Select a mesh first and a particle system second.");
    }

    MDagPath meshPath;
    MDagPath particlePath;
    MObject component;
    status = selection.getDagPath(0, meshPath, component);
    if (status != MS::kSuccess) {
        return status;
    }
    status = extendToShape(meshPath, MFn::kMesh,
                           "The first selection must be a mesh.");
    if (status != MS::kSuccess) {
        return status;
    }

    status = selection.getDagPath(1, particlePath, component);
    if (status != MS::kSuccess) {
        return status;
    }
    status = extendToShape(particlePath, MFn::kParticle,
                           "The second selection must be a particle system.");
    if (status != MS::kSuccess) {
        return status;
    }

    MFnMesh meshFn(meshPath, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    MFnParticleSystem particleFn;
    const MObject particleObject = particlePath.node(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = particleFn.setObject(particleObject);
    if (status != MS::kSuccess) {
        return status;
    }
    particleHandle = MObjectHandle(particleObject);

    MObjectArray shadingSets;
    MObjectArray shadingComponents;
    const unsigned int instanceNumber = meshPath.instanceNumber(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = meshFn.getConnectedSetsAndMembers(
        instanceNumber, shadingSets, shadingComponents, true);
    if (status != MS::kSuccess) {
        return status;
    }
    if (shadingSets.length() == 0) {
        return commandFailure("The selected mesh has no connected shading group.");
    }
    if (shadingSets.length() > 1) {
        MGlobal::displayWarning(
            "The mesh has multiple shading groups; setParticleRGB uses the first group.");
    }

    MFnSet shadingSetFn(shadingSets[0], &status);
    if (status != MS::kSuccess) {
        return status;
    }
    const MString shadingGroupName = shadingSetFn.name(&status);
    if (status != MS::kSuccess || shadingGroupName.length() == 0) {
        return commandFailure("Unable to resolve the connected shading group name.");
    }

    MVectorArray particlePositions;
    particleFn.position(particlePositions);
    if (particlePositions.length() == 0) {
        return MS::kSuccess;
    }
    if (particlePositions.length() >
        dommy::generation::kMaxGeneratedPointCount) {
        return commandFailure(
            "The particle count exceeds the supported sampling budget.");
    }

    MFloatArray uCoordinates;
    MFloatArray vCoordinates;
    MFloatPointArray samplePoints;
    MFloatVectorArray sampleNormals;
    for (unsigned int index = 0; index < particlePositions.length(); ++index) {
        const MVector& position = particlePositions[index];
        MPoint closestPoint;
        MVector normal;
        status = meshFn.getClosestPointAndNormal(
            MPoint(position.x, position.y, position.z), closestPoint, normal,
            MSpace::kWorld);
        if (status != MS::kSuccess) {
            return status;
        }

        float uv[2] = {0.0f, 0.0f};
        status = meshFn.getUVAtPoint(closestPoint, uv, MSpace::kWorld);
        if (status != MS::kSuccess) {
            return commandFailure(
                "Unable to sample mesh UVs. Ensure the selected mesh has a valid UV set.");
        }
        status = samplePoints.append(MFloatPoint(
            static_cast<float>(closestPoint.x), static_cast<float>(closestPoint.y),
            static_cast<float>(closestPoint.z)));
        if (status != MS::kSuccess) {
            return status;
        }
        status = sampleNormals.append(MFloatVector(normal));
        if (status != MS::kSuccess) {
            return status;
        }
        status = uCoordinates.append(uv[0]);
        if (status != MS::kSuccess) {
            return status;
        }
        status = vCoordinates.append(uv[1]);
        if (status != MS::kSuccess) {
            return status;
        }
    }

    MDagPath cameraPath;
    if (cameraName.length() > 0U) {
        MSelectionList cameraSelection;
        status = cameraSelection.add(cameraName);
        if (!status || cameraSelection.length() != 1U) {
            return commandFailure(
                "The camera flag must resolve to exactly one camera.");
        }
        status = cameraSelection.getDagPath(0U, cameraPath, component);
        if (status) {
            status = extendToShape(
                cameraPath, MFn::kCamera,
                "The camera flag must identify a camera.");
        }
    } else {
        status = M3dView::active3dView().getCamera(cameraPath);
    }
    if (status != MS::kSuccess) {
        return commandFailure(
            "Unable to resolve a camera. In batch mode, pass -camera.");
    }
    const MMatrix cameraMatrix = cameraPath.inclusiveMatrix(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    MFloatMatrix floatCameraMatrix = cameraMatrix.matrix;

    MFloatVectorArray sampledColors;
    MFloatVectorArray sampledTransparency;
    status = sampleShadingNetworkInBatches(
        shadingGroupName, onShadow, onReuseMap, floatCameraMatrix,
        samplePoints, &uCoordinates, &vCoordinates, &sampleNormals,
        &samplePoints, nullptr, nullptr, nullptr, sampledColors,
        sampledTransparency);
    if (status != MS::kSuccess) {
        return commandFailure("Failed to sample the mesh shading network.");
    }
    if (sampledColors.length() != particlePositions.length()) {
        return commandFailure("The shading network returned an unexpected color count.");
    }

    status = targetColors.clear();
    if (!status) return status;
    for (unsigned int index = 0; index < sampledColors.length(); ++index) {
        status = targetColors.append(MVector(sampledColors[index]));
        if (status != MS::kSuccess) {
            return status;
        }
    }

    MFnDependencyNode particleNode(particleObject, &status);
    if (!status) {
        return status;
    }
    hadRgbAttribute = particleNode.hasAttribute("rgbPP", &status);
    if (!status) {
        return status;
    }
    if (hadRgbAttribute) {
        const unsigned int colorCount = particleFn.getPerParticleAttribute(
            "rgbPP", originalColors, &status);
        if (!status) {
            return status;
        }
        if (colorCount != particlePositions.length() ||
            originalColors.length() != particlePositions.length()) {
            return commandFailure(
                "The existing rgbPP data does not match the particle count.");
        }
    }

    status = dommy::queueMissingParticleRgbAttributes(
        particleFn, attributeModifier, attributeEditsQueued);
    if (!status) {
        return status;
    }

    planReady = true;
    return redoIt();
    } DOMMY_COMMAND_CATCH("setParticleRGBCmd")
}

MStatus setParticleRGBCmd::applyColors(MVectorArray& colors)
{
    if (!planReady || !particleHandle.isValid() ||
        !particleHandle.isAlive()) {
        return MS::kFailure;
    }

    MStatus status;
    MFnParticleSystem particleFn(particleHandle.object(), &status);
    if (!status) {
        return status;
    }
    particleFn.setPerParticleAttribute("rgbPP", colors, &status);
    return status;
}

MStatus setParticleRGBCmd::redoIt()
{
    try {
        bool attributeAttempted = false;
        bool colorsAttempted = false;
        return dommy::maya::runCommandTransaction("setParticleRGBCmd::redoIt", [&]() -> MStatus {
            if (!planReady) return MS::kFailure;
            if (applied) return MS::kSuccess;
            if (attributeEditsQueued) {
                attributeAttempted = true;
                attributesApplied = true;
                const MStatus status = attributeModifier.doIt();
                if (!status) return status;
            }
            colorsAttempted = true;
            const MStatus status = applyColors(targetColors);
            if (status) applied = true;
            return status;
        }, [&]() -> MStatus {
            MStatus firstFailure = MS::kSuccess;
            if (colorsAttempted && hadRgbAttribute) {
                firstFailure = dommy::maya::invokeCommandOperation(
                    "setParticleRGBCmd", "color rollback", [&]() { return applyColors(originalColors); });
            }
            // Attribute cleanup is independent: attempt it even if colors
            // cannot be restored, and keep pending ownership on failure.
            if (attributeAttempted) {
                const MStatus status = dommy::maya::invokeCommandOperation(
                    "setParticleRGBCmd", "attribute rollback", [&]() { return attributeModifier.undoIt(); });
                if (status) attributesApplied = false;
                if (firstFailure && !status) firstFailure = status;
            }
            return firstFailure;
        });
    } DOMMY_COMMAND_CATCH("setParticleRGBCmd::redoIt")
}

MStatus setParticleRGBCmd::undoIt()
{
    try {
        bool colorsAttempted = false;
        bool attributeUndoAttempted = false;
        return dommy::maya::runCommandTransaction("setParticleRGBCmd::undoIt", [&]() -> MStatus {
            if (!planReady || !applied) return MS::kFailure;
            if (hadRgbAttribute) {
                colorsAttempted = true;
                const MStatus status = applyColors(originalColors);
                if (!status) return status;
            }
            if (attributesApplied) {
                attributeUndoAttempted = true;
                const MStatus status = attributeModifier.undoIt();
                if (!status) return status;
                attributesApplied = false;
            }
            applied = false;
            return MS::kSuccess;
        }, [&]() -> MStatus {
            MStatus firstFailure = MS::kSuccess;
            if (attributeUndoAttempted) {
                firstFailure = dommy::maya::invokeCommandOperation(
                    "setParticleRGBCmd", "attribute restore", [&]() { return attributeModifier.doIt(); });
                if (firstFailure) attributesApplied = true;
            }
            if (colorsAttempted || attributeUndoAttempted) {
                const MStatus status = dommy::maya::invokeCommandOperation(
                    "setParticleRGBCmd", "color restore", [&]() { return applyColors(targetColors); });
                if (firstFailure && !status) firstFailure = status;
            }
            return firstFailure;
        });
    } DOMMY_COMMAND_CATCH("setParticleRGBCmd::undoIt")
}
