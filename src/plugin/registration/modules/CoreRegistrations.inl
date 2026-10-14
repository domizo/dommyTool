#pragma mark register cmd
	CHECK_MSTATUS(registration.registerCommand ( "DBlindDataSelectCmd", DBlindDataSelectCmd::creator));
	CHECK_MSTATUS(registration.registerCommand ( "DMetadataSelectCmd", DMetadataSelectCmd::creator));
	
    
    CHECK_MSTATUS(registration.registerCommandWithSyntax<DClusterWeightCmd>("DClusterWeightCmd"));
    
    
		CHECK_MSTATUS(registration.registerCommandWithSyntax<shapeFillCmd>("shapeFillCmd"));

	CHECK_MSTATUS(registration.registerCommandWithSyntax<setParticleRGBCmd>("setParticleRGBCmd"));
	
	
	
	
#pragma mark curve cmd
	
	CHECK_MSTATUS(registration.registerCommand( "woolCurveCmd", woolCurveCmd::creator ));
	
	CHECK_MSTATUS(registration.registerCommandWithSyntax<getCurveLengthPosCmd>("getCurveLengthPosCmd"));
	
	
	
	
#pragma mark Duplicator cmd
	CHECK_MSTATUS(registration.registerCommand( "surfaceDuplicaterCmd", surfaceDuplicaterCmd::creator ));
	CHECK_MSTATUS(registration.registerCommand( "spaceDuplicaterCmd", spaceDuplicaterCmd::creator ));
	
	
	
#pragma mark mesh Cmd
    CHECK_MSTATUS(registration.registerCommand ( "icosahedronCmd", icosahedronCmd::creator));
    
	
	
	
	
	
#pragma mark mesh modifier
	CHECK_MSTATUS(registration.registerCommand ( "polyBlast", polyBlast::creator));
    CHECK_MSTATUS(registration.registerNode( "polyBlastNode", polyBlastNode::id, polyBlastNode::creator, polyBlastNode::initialize ));
	
	
	
	//register node
#pragma mark data modifier
	CHECK_MSTATUS(registration.registerNode( "DShapeSequencer", DShapeSequencer::typeId, DShapeSequencer::creator, DShapeSequencer::initialize ));
	
    
#pragma mark transform
    CHECK_MSTATUS(registration.registerNode( "DPointToSurfaceTransformNode", DPointToSurfaceTransformNode::typeId, DPointToSurfaceTransformNode::creator, DPointToSurfaceTransformNode::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DTDelayTransformMatrix", DTDelayTransformMatrix::typeId, DTDelayTransformMatrix::creator, DTDelayTransformMatrix::initialize ));
	
#pragma mark Curve
	CHECK_MSTATUS(registration.registerNode( "DSpiralCurve", DSpiralCurve::typeId, DSpiralCurve::creator, DSpiralCurve::initialize ));
	CHECK_MSTATUS(registration.registerNode( "woolCurve", woolCurve::typeId, woolCurve::creator, woolCurve::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DRigController", DRigController::typeId, DRigController::creator, DRigController::initialize ));
	
	
	CHECK_MSTATUS(registration.registerNode( "remakeCurveNode", remakeCurveNode::typeId, remakeCurveNode::creator, remakeCurveNode::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DMotionTrailCurveNode", DMotionTrailCurveNode::typeId, DMotionTrailCurveNode::creator, DMotionTrailCurveNode::initialize ));
    
    CHECK_MSTATUS(registration.registerNode( "DBSCurve", DBSCurve::typeId, DBSCurve::creator, DBSCurve::initialize ));
    
    CHECK_MSTATUS(registration.registerNode( "DResampleCurve", DResampleCurve::typeId, DResampleCurve::creator, DResampleCurve::initialize ));
    
    
	
	
#pragma mark Duplicator
	CHECK_MSTATUS(registration.registerNode( "surfaceDuplicater", surfaceDuplicater::typeId, surfaceDuplicater::creator, surfaceDuplicater::initialize ));
	CHECK_MSTATUS(registration.registerNode( "spaceDuplicater", spaceDuplicater::typeId, spaceDuplicater::creator, spaceDuplicater::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DTMotionDuplicator", DTMotionDuplicator::typeId, DTMotionDuplicator::creator, DTMotionDuplicator::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DCurveDuplicator", DCurveDuplicator::typeId, DCurveDuplicator::creator, DCurveDuplicator::initialize ));
	
#pragma mark Mesh
	CHECK_MSTATUS(registration.registerNode( "icosahedronNode", icosahedronNode::typeId, icosahedronNode::creator, icosahedronNode::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DArcCurveNode", DArcCurveNode::typeId, DArcCurveNode::creator, DArcCurveNode::initialize ));
	CHECK_MSTATUS(registration.registerNode("foamGenerateNode", foamGenerateNode::typeId, foamGenerateNode::creator, foamGenerateNode::initialize));
    CHECK_MSTATUS(registration.registerNode("DBubbleCrystal", DBubbleCrystal::typeId, DBubbleCrystal::creator, DBubbleCrystal::initialize));
	
    
	//Human Generate
	CHECK_MSTATUS(registration.registerNode("DHEyeGen", DHEyeGen::typeId, DHEyeGen::creator, DHEyeGen::initialize));
    CHECK_MSTATUS(registration.registerNode("DHEyelashGen", DHEyelashGen::typeId, DHEyelashGen::creator, DHEyelashGen::initialize));
    
    
    //Leaf
	CHECK_MSTATUS(registration.registerNode("LeafGen", LeafGen::typeId, LeafGen::creator, LeafGen::initialize));
	
	
	CHECK_MSTATUS(registration.registerNode("applyVtxColorFromFileNode", applyVtxColorFromFileNode::typeId, applyVtxColorFromFileNode::creator, applyVtxColorFromFileNode::initialize));
	CHECK_MSTATUS(registration.registerNode("DPolyWire", DPolyWire::typeId, DPolyWire::creator, DPolyWire::initialize));
	
    
    CHECK_MSTATUS(registration.registerNode("DBSMirrorMesh", DBSMirrorMesh::typeId, DBSMirrorMesh::creator, DBSMirrorMesh::initialize));
    CHECK_MSTATUS(registration.registerNode("DPolyFaceToMat", DPolyFaceToMat::typeId, DPolyFaceToMat::creator, DPolyFaceToMat::initialize));
    CHECK_MSTATUS(registration.registerNode("DPolyEdgeToMat", DPolyEdgeToMat::typeId, DPolyEdgeToMat::creator, DPolyEdgeToMat::initialize));
    CHECK_MSTATUS(registration.registerNode("DBSFlipMesh", DBSFlipMesh::typeId, DBSFlipMesh::creator, DBSFlipMesh::initialize));
	CHECK_MSTATUS(registration.registerNode("DBSUpDownMesh", DBSUpDownMesh::typeId, DBSUpDownMesh::creator, DBSUpDownMesh::initialize));
    CHECK_MSTATUS(registration.registerNode("DPolySymmetry", DPolySymmetry::typeId, DPolySymmetry::creator, DPolySymmetry::initialize));
    CHECK_MSTATUS(registration.registerNode("DPolyResetVtx", DPolyResetVtx::typeId, DPolyResetVtx::creator, DPolyResetVtx::initialize));
    CHECK_MSTATUS(registration.registerNode("DPolyThickness", DPolyThickness::typeId, DPolyThickness::creator, DPolyThickness::initialize));
    
    
    
    
    CHECK_MSTATUS(registration.registerNode("DSkinInfo", DSkinInfo::typeId, DSkinInfo::creator, DSkinInfo::initialize));
    
    
    
    
#pragma mark deformer
	
	CHECK_MSTATUS(registration.registerNode( pushDeformer::typeName,  pushDeformer::typeId,  pushDeformer::creator,  pushDeformer::initialize,  MPxNode::kDeformerNode ));
	CHECK_MSTATUS(registration.registerNode( surfaceDeformer::typeName,  surfaceDeformer::typeId,  surfaceDeformer::creator,  surfaceDeformer::initialize,  MPxNode::kDeformerNode ));
	CHECK_MSTATUS(registration.registerNode( noiseDeformer::typeName, noiseDeformer::typeId, noiseDeformer::creator, noiseDeformer::initialize, MPxNode::kDeformerNode ));
	CHECK_MSTATUS(registration.registerNode( DCollisionDeformer::typeName, DCollisionDeformer::typeId, DCollisionDeformer::creator, DCollisionDeformer::initialize, MPxNode::kDeformerNode ));
	CHECK_MSTATUS(registration.registerNode( DMotionDeformer::typeName, DMotionDeformer::typeId, DMotionDeformer::creator, DMotionDeformer::initialize, MPxNode::kDeformerNode ));
	CHECK_MSTATUS(registration.registerNode( DRayDeformer::typeName, DRayDeformer::typeId, DRayDeformer::creator, DRayDeformer::initialize, MPxNode::kDeformerNode ));
	
    
    
    
#pragma mark emitter
	CHECK_MSTATUS(registration.registerNode( "curveEmitter",  curveEmitter::id,  curveEmitter::creator, curveEmitter::initialize, MPxNode::kEmitterNode ));
	CHECK_MSTATUS(registration.registerNode( "DEmitter", DEmitter::id, DEmitter::creator, DEmitter::initialize, MPxNode::kEmitterNode ));
	CHECK_MSTATUS(registration.registerNode( "DPolygonEmitter", DPolygonEmitter::id, DPolygonEmitter::creator, DPolygonEmitter::initialize, MPxNode::kEmitterNode ));
	CHECK_MSTATUS(registration.registerNode( "DMeshVolumeEmitter", DMeshVolumeEmitter::id, DMeshVolumeEmitter::creator, DMeshVolumeEmitter::initialize, MPxNode::kEmitterNode ));
    CHECK_MSTATUS(registration.registerNode( "DRadialGeometryEmitter", DRadialGeometryEmitter::id, DRadialGeometryEmitter::creator, DRadialGeometryEmitter::initialize, MPxNode::kEmitterNode ));
    
    
#pragma mark field
    CHECK_MSTATUS(registration.registerNode( "DCurlNoiseField", DCurlNoiseField::id,&DCurlNoiseField::creator, &DCurlNoiseField::initialize,MPxNode::kFieldNode));
    CHECK_MSTATUS(registration.registerNode( "DCurveField", DCurveField::id,&DCurveField::creator, &DCurveField::initialize,MPxNode::kFieldNode));
	CHECK_MSTATUS(registration.registerNode( "DKillField", DKillField::id,&DKillField::creator, &DKillField::initialize,MPxNode::kFieldNode));
	
    
    
    
#pragma mark utility
    CHECK_MSTATUS(registration.registerNode( "DPolyFaceInfo", DPolyFaceInfo::typeId, DPolyFaceInfo::creator, DPolyFaceInfo::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DCurvatureData", DCurvatureData::typeId, DCurvatureData::creator, DCurvatureData::initialize ));
	#if DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE
	CHECK_MSTATUS(registration.registerNode( "DStretchData", DStretchData::typeId, DStretchData::creator, DStretchData::initialize ));
	#endif
    CHECK_MSTATUS(registration.registerNode( "DComponentListGen", DComponentListGen::typeId, DComponentListGen::creator, DComponentListGen::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DNumericToFloatArray", DNumericToFloatArray::typeId, DNumericToFloatArray::creator, DNumericToFloatArray::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DMatToVec", DMatToVec::typeId, DMatToVec::creator, DMatToVec::initialize ));
    
    
#pragma mark register Custom Data
		CHECK_MSTATUS(registration.registerData(DPData::typeName, DPData::id, DPData::creator ));
		CHECK_MSTATUS(registration.registerData(DCurveData::typeName, DCurveData::id, DCurveData::creator));
		CHECK_MSTATUS(registration.registerData(DMeshData::typeName, DMeshData::id, DMeshData::creator));
	#if DOMMY_WITH_OPENVDB
	    CHECK_MSTATUS(registration.registerData(DOpenVDBData::typeName, DOpenVDBData::id, DOpenVDBData::creator));
	#endif

#if DOMMY_WITH_JET
    CHECK_MSTATUS(registration.registerNode(DJetSmokeSolver::typeName, DJetSmokeSolver::typeId,
                                            DJetSmokeSolver::creator, DJetSmokeSolver::initialize,
                                            DJetSmokeSolver::kLocatorNode));
#endif
	
    
	
	//register Custom Shape
	
	
