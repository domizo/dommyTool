

// Transform
#include "DPointToSurfaceTransformNode.h"
#include "DTDelayTransformMatrix.h"

// Dynamics
#include "shapeFillCmd.h"
#include "setParticleRGBCmd.h"

//////////Deformer//////////
#include "DDeformHeaders.h"

//////////Cmd//////////
#include "CmdHeaders.h"

//////////Generate//////////
#include "GeneratorHeaders.h"

#include "foamGenerateNode.h"
#include "applyVtxColorFromFileNode.h"
#include "DPolyWire.h"

// DataModifier
#include "DShapeSequencer.h"

// Mesh Modifier
#include "polyBlastCmd.h"
#include "polyBlastNode.h"

// Mesh Generate
#include "surfaceDuplicaterCmd.h"
#include "surfaceDuplicater.h"
#include "spaceDuplicaterCmd.h"
#include "spaceDuplicater.h"
#include "DCurveDuplicator.h"

// Motion Generate
#include "motionDuplicator.h"

// emitter
#include "curveEmitter.h"
#include "DEmitter.h"
#include "DPolygonEmitter.h"
#include "DMeshVolumeEmitter.h"
#include "DRadialGeometryEmitter.h"

// field
#include "DCurlNoiseField.h"
#include "DCurveField.h"
#include "DKillField.h"

// Utility
#include "DPolyFaceInfo.h"
#include "DCurvatureData.h"
#include "DStretchData.h"
#include "DComponentListGen.h"
#include "DNumericToFloatArray.h"
#include "DBSMirrorMesh.h"
#include "DPolyFaceToMat.h"
#include "DPolyEdgeToMat.h"
#include "DBSFlipMesh.h"
#include "DBSUpDownMesh.h"
#include "DSkinInfo.h"
#include "DPolySymmetry.h"
#include "DMatToVec.h"
#include "DPolyResetVtx.h"
#include "DPolyThickness.h"

// Curve
#include "DBSCurve.h"
#include "DResampleCurve.h"

// Render
#include "DCameraFrustum.h"
#include "PluginFeatures.h"

#if DOMMY_WITH_OPENVDB
#include <openvdb/openvdb.h>
#include "DOpenVDBHeaders.h"
#endif
// DPoint
#include "DPointHeaders.h"
// DCurve
#include "DCurveHeaders.h"
// DMesh
#include "DMeshHeaders.h"
// Bullet
#if DOMMY_WITH_BULLET
#include "DBulletHeaders.h"
#endif
#if DOMMY_WITH_JET
// Jet
#include "DJetHeaders.h"
#endif

// maya
#include <maya/MFnPlugin.h>
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include <maya/MDGMessage.h>

#include "registration/PluginRegistration.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"

namespace {
dommy::PluginLifetime gPluginLifetime;
}

MStatus initializePlugin(MObject object) try {
    MStatus status;
    MFnPlugin plugin(object, "Yuzo Furudomi", "1.1", "Any", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!gPluginLifetime.empty()) {
        DOMMY_RETURN_IF_FAILURE(gPluginLifetime.cleanup(plugin));
    }

    dommy::PluginRegistration registration(plugin, gPluginLifetime);
#if DOMMY_WITH_OPENVDB
    DOMMY_RETURN_IF_FAILURE(registration.registerResource([]() {
        openvdb::initialize();
        return MStatus(MS::kSuccess);
    }, [](MFnPlugin&) {
        openvdb::uninitialize();
        return MStatus(MS::kSuccess);
    }));
#endif

#include "registration/modules/CoreRegistrations.inl"
#include "registration/modules/PointRegistrations.inl"
#if DOMMY_WITH_OPENVDB
#include "registration/modules/OpenVDBRegistrations.inl"
#endif
#include "registration/modules/CurveRegistrations.inl"
#include "registration/modules/MeshRegistrations.inl"
#include "registration/modules/RenderRegistrations.inl"
#if DOMMY_WITH_BULLET
#include "registration/modules/BulletRegistrations.inl"
#endif
#if DOMMY_WITH_ARNOLD
#include "registration/modules/ArnoldRegistrations.inl"
#endif

    return registration.commit();
}
catch (...) {
    return dommy::maya::reportEntryPointException("dommyTool", "initializePlugin", std::current_exception());
}

MStatus uninitializePlugin(MObject object) try {
    MStatus status;
    MFnPlugin plugin(object, "Unknown", "Unknown", "Any", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    return gPluginLifetime.cleanup(plugin);
}
catch (...) {
    return dommy::maya::reportEntryPointException("dommyTool", "uninitializePlugin", std::current_exception());
}
