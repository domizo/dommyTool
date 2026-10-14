#pragma mark ---------------DPoint---------------
#pragma mark DData Utility
	//Cmd
	CHECK_MSTATUS(registration.registerCommandWithSyntax<DPointDataExportCmd>("DPointDataExportCmd"));
	
    //IO
    CHECK_MSTATUS(registration.registerNode( "ReadDPointDataNode", ReadDPointDataNode::typeId, ReadDPointDataNode::creator, ReadDPointDataNode::initialize ));
	
	
#pragma mark  Display
	CHECK_MSTATUS(registration.registerNode( DPointDisplay::typeName, DPointDisplay::typeId, &DPointDisplay::creator, &DPointDisplay::initialize, MPxNode::kLocatorNode, &DPointDisplay::classification));
		CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DPointDisplay::classification, DPointDisplay::registrantId, DPointDisplayOverride::Creator));
	
    CHECK_MSTATUS(registration.registerNode( DPMarker::typeName, DPMarker::typeId, &DPMarker::creator, &DPMarker::initialize, MPxNode::kLocatorNode, &DPMarker::classification));
	    CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DPMarker::classification, DPMarker::registrantId, DPMarkerOverride::Creator));
	
	
    
#pragma mark Attribute
    CHECK_MSTATUS(registration.registerNode( "DPAttributeCreate", DPAttributeCreate::typeId, DPAttributeCreate::creator, DPAttributeCreate::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAttributeDelete", DPAttributeDelete::typeId, DPAttributeDelete::creator, DPAttributeDelete::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAttributeBlend", DPAttributeBlend::typeId, DPAttributeBlend::creator, DPAttributeBlend::initialize ));
#if DOMMY_WITH_OPENVDB
	CHECK_MSTATUS(registration.registerNode( "DPAttrSmooth", DPAttrSmooth::typeId, DPAttrSmooth::creator, DPAttrSmooth::initialize ));
#endif
	CHECK_MSTATUS(registration.registerNode( "DPAttributeTransfer", DPAttributeTransfer::typeId, DPAttributeTransfer::creator, DPAttributeTransfer::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAttributeSeExpr", DPAttributeSeExpr::typeId, DPAttributeSeExpr::creator, DPAttributeSeExpr::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPAttributeCast", DPAttributeCast::typeId, DPAttributeCast::creator, DPAttributeCast::initialize ));
#if DOMMY_WITH_OPENVDB
	CHECK_MSTATUS(registration.registerNode( "DPAttributeVDB", DPAttributeVDB::typeId, DPAttributeVDB::creator, DPAttributeVDB::initialize ));
#endif
	CHECK_MSTATUS(registration.registerNode( "DPAttributeToDataArray", DPAttributeToDataArray::typeId, DPAttributeToDataArray::creator, DPAttributeToDataArray::initialize ));
    
    
#pragma mark Group
	CHECK_MSTATUS(registration.registerNode( "DPGroup", DPGroup::typeId, DPGroup::creator, DPGroup::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPGroupDelete", DPGroupDelete::typeId, DPGroupDelete::creator, DPGroupDelete::initialize ));
	
#pragma mark DP emitter
	
	//particle
    CHECK_MSTATUS(registration.registerNode( "DPEmitter",  DPEmitter::id,  DPEmitter::creator, DPEmitter::initialize, MPxNode::kEmitterNode ));
	
	//fluid
	CHECK_MSTATUS(registration.registerNode( "DPFluidEmitter", DPFluidEmitter::typeId,&DPFluidEmitter::creator, &DPFluidEmitter::initialize,MPxNode::kFluidEmitterNode ));
	
	
	
#pragma mark DP field
	CHECK_MSTATUS(registration.registerNode( "DPGoalField", DPGoalField::typeId, &DPGoalField::creator,
										&DPGoalField::initialize,
										MPxNode::kFieldNode));
    CHECK_MSTATUS(registration.registerNode( "DPFluidVectorField", DPFluidVectorField::typeId, &DPFluidVectorField::creator,
										&DPFluidVectorField::initialize,
										MPxNode::kFieldNode));
    CHECK_MSTATUS(registration.registerNode( "DPFluidNoiseField", DPFluidNoiseField::typeId, &DPFluidNoiseField::creator,
                                        &DPFluidNoiseField::initialize,
                                        MPxNode::kFieldNode));
#pragma mark DP NObject
	CHECK_MSTATUS(registration.registerNode ( "DPnParticle", DPnParticle::typeId, DPnParticle::creator, DPnParticle::initialize ));
	CHECK_MSTATUS(registration.registerNode ( "DPnPopSolver", DPnPopSolver::typeId, DPnPopSolver::creator, DPnPopSolver::initialize ));
    
    
#pragma mark Modify
    CHECK_MSTATUS(registration.registerNode( "DPEditDouble", DPEditDouble::typeId, DPEditDouble::creator, DPEditDouble::initialize ));
    
#pragma mark Deform
    CHECK_MSTATUS(registration.registerNode( "DPConvertWeights", DPConvertWeights::typeId, DPConvertWeights::creator, DPConvertWeights::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVectorDeform", DPVectorDeform::typeId, DPVectorDeform::creator, DPVectorDeform::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPPointDeform", DPPointDeform::typeId, DPPointDeform::creator, DPPointDeform::initialize ));
    
    
#pragma mark Manip
	CHECK_MSTATUS(registration.registerNode("DPDirectionManip",DPDirectionManip::id,
										&DPDirectionManip::creator,
										&DPDirectionManip::initialize,
										MPxNode::kManipContainer));
	CHECK_MSTATUS(registration.registerNode("DPTransformManip",DPTransformManip::id,
										&DPTransformManip::creator,
										&DPTransformManip::initialize,
										MPxNode::kManipContainer));
	CHECK_MSTATUS(registration.registerNode("DPGroupManip",DPGroupManip::id,
										&DPGroupManip::creator,
										&DPGroupManip::initialize,
										MPxNode::kManipContainer));
	
	
	
#pragma mark Utility
    
	CHECK_MSTATUS(registration.registerNode( "DPDelete", DPDelete::typeId, DPDelete::creator, DPDelete::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPTransform", DPTransform::typeId, DPTransform::creator, DPTransform::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPMerge", DPMerge::typeId, DPMerge::creator, DPMerge::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPFillSpace", DPFillSpace::typeId, DPFillSpace::creator, DPFillSpace::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecCompare", DPVecCompare::typeId, DPVecCompare::creator, DPVecCompare::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPArraySize", DPArraySize::typeId, DPArraySize::creator, DPArraySize::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPDistance", DPDistance::typeId, DPDistance::creator, DPDistance::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPPointToFloat", DPPointToFloat::typeId, DPPointToFloat::creator, DPPointToFloat::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPDirection", DPDirection::typeId, DPDirection::creator, DPDirection::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPColor", DPColor::typeId, DPColor::creator, DPColor::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPSort", DPSort::typeId, DPSort::creator, DPSort::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPTimeShift", DPTimeShift::typeId, DPTimeShift::creator, DPTimeShift::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPRay", DPRay::typeId, DPRay::creator, DPRay::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPBlend", DPBlend::typeId, DPBlend::creator, DPBlend::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPNoise", DPNoise::typeId, DPNoise::creator, DPNoise::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPDFNoise", DPDFNoise::typeId, DPDFNoise::creator, DPDFNoise::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPSmooth", DPSmooth::typeId, DPSmooth::creator, DPSmooth::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPAdjustSpace", DPAdjustSpace::typeId, DPAdjustSpace::creator, DPAdjustSpace::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAffect", DPAffect::typeId, DPAffect::creator, DPAffect::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPPyroNoise", DPPyroNoise::typeId, DPPyroNoise::creator, DPPyroNoise::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPMagnet", DPMagnet::typeId, DPMagnet::creator, DPMagnet::initialize ));
    
    CHECK_MSTATUS(registration.registerNode( "DPSolver", DPSolver::typeId, DPSolver::creator, DPSolver::initialize ));
#if DOMMY_WITH_OPENVDB
    CHECK_MSTATUS(registration.registerNode( "DPSelfCollision", DPSelfCollision::typeId, DPSelfCollision::creator, DPSelfCollision::initialize ));
#endif
    CHECK_MSTATUS(registration.registerNode( "DPGoal", DPGoal::typeId, DPGoal::creator, DPGoal::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPParticleAttribute", DPParticleAttribute::typeId, DPParticleAttribute::creator, DPParticleAttribute::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPDuplicator", DPDuplicator::typeId, DPDuplicator::creator, DPDuplicator::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPNull", DPNull::typeId, DPNull::creator, DPNull::initialize ));
#if DOMMY_WITH_OPENVDB
	CHECK_MSTATUS(registration.registerNode( "DPCache", DPCache::typeId, DPCache::creator, DPCache::initialize ));
#endif
    CHECK_MSTATUS(registration.registerNode("DPAvoid", DPAvoid::typeId, DPAvoid::creator, DPAvoid::initialize));
    CHECK_MSTATUS(registration.registerNode("DPSpiralField", DPSpiralField::typeId, DPSpiralField::creator, DPSpiralField::initialize));
#if DOMMY_WITH_FASTNOISE
    CHECK_MSTATUS(registration.registerNode("DPFastNoise", DPFastNoise::typeId, DPFastNoise::creator, DPFastNoise::initialize));
#endif
	CHECK_MSTATUS(registration.registerNode("DPSearchPoint", DPSearchPoint::typeId, DPSearchPoint::creator, DPSearchPoint::initialize));
    #if DOMMY_WITH_OPENVDB
    CHECK_MSTATUS(registration.registerNode("DPClosestPointVDB", DPClosestPointVDB::typeId, DPClosestPointVDB::creator, DPClosestPointVDB::initialize));
    #endif
    CHECK_MSTATUS(registration.registerNode("DPCopyTransform", DPCopyTransform::typeId, DPCopyTransform::creator, DPCopyTransform::initialize));
    
    
#pragma mark Generate
#if DOMMY_WITH_OPENVDB
	CHECK_MSTATUS(registration.registerNode( "DPointPlaneEmit", DPointPlaneEmit::typeId, DPointPlaneEmit::creator, DPointPlaneEmit::initialize ));
#endif
	CHECK_MSTATUS(registration.registerNode( "DPointMeshEmit", DPointMeshEmit::typeId, DPointMeshEmit::creator, DPointMeshEmit::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPointCurveEmit", DPointCurveEmit::typeId, DPointCurveEmit::creator, DPointCurveEmit::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPointSurfaceEmit", DPointSurfaceEmit::typeId, DPointSurfaceEmit::creator, DPointSurfaceEmit::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPointCircleEmit", DPointCircleEmit::typeId, DPointCircleEmit::creator, DPointCircleEmit::initialize ));
#if DOMMY_WITH_OPENVDB
	CHECK_MSTATUS(registration.registerNode( "DPointVolumeEmit", DPointVolumeEmit::typeId, DPointVolumeEmit::creator, DPointVolumeEmit::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPScatter", DPScatter::typeId, DPScatter::creator, DPScatter::initialize ));
#endif
    CHECK_MSTATUS(registration.registerNode( "DPAddPoint", DPAddPoint::typeId, DPAddPoint::creator, DPAddPoint::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPointFromFluid", DPointFromFluid::typeId, DPointFromFluid::creator, DPointFromFluid::initialize ));
	
	
    CHECK_MSTATUS(registration.registerNode( "DPAnimationEmit", DPAnimationEmit::typeId, DPAnimationEmit::creator, DPAnimationEmit::initialize ));
    
    
#pragma mark DP Data Gen
    CHECK_MSTATUS(registration.registerNode( "DPIntArrayGen", DPIntArrayGen::typeId, DPIntArrayGen::creator, DPIntArrayGen::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPDoubleArrayGen", DPDoubleArrayGen::typeId, DPDoubleArrayGen::creator, DPDoubleArrayGen::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVectorArrayGen", DPVectorArrayGen::typeId, DPVectorArrayGen::creator, DPVectorArrayGen::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPStringArrayGen", DPStringArrayGen::typeId, DPStringArrayGen::creator, DPStringArrayGen::initialize ));
	
    CHECK_MSTATUS(registration.registerNode( "DPSignal", DPSignal::typeId, DPSignal::creator, DPSignal::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPSignal3D", DPSignal3D::typeId, DPSignal3D::creator, DPSignal3D::initialize ));
#if DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE
	CHECK_MSTATUS(registration.registerNode( "DPArrayToCompound", DPArrayToCompound::typeId, DPArrayToCompound::creator, DPArrayToCompound::initialize ));
#endif
    
    
#pragma mark DP Curve

	
#pragma mark DP Nurbs
	CHECK_MSTATUS(registration.registerNode( "DPUVGen", DPUVGen::typeId, DPUVGen::creator, DPUVGen::initialize ));
	
#pragma mark DP Mesh
	CHECK_MSTATUS(registration.registerNode( "DPMeshDuplicator", DPMeshDuplicator::typeId, DPMeshDuplicator::creator, DPMeshDuplicator::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVoronoi", DPVoronoi::typeId, DPVoronoi::creator, DPVoronoi::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPToMesh", DPToMesh::typeId, DPToMesh::creator, DPToMesh::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPFoamMesh", DPFoamMesh::typeId, DPFoamMesh::creator, DPFoamMesh::initialize ));
    
    
#pragma mark DPAnim
    CHECK_MSTATUS(registration.registerNode( "DPAnimMatrix", DPAnimMatrix::typeId, DPAnimMatrix::creator, DPAnimMatrix::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPMatrix", DPMatrix::typeId, DPMatrix::creator, DPMatrix::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPMotionCreate", DPMotionCreate::typeId, DPMotionCreate::creator, DPMotionCreate::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPRotation", DPRotation::typeId, DPRotation::creator, DPRotation::initialize ));
	
	
#pragma mark DPMath
	CHECK_MSTATUS(registration.registerNode( "DPMath", DPMath::typeId, DPMath::creator, DPMath::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAdd", DPAdd::typeId, DPAdd::creator, DPAdd::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPSubtract", DPSubtract::typeId, DPSubtract::creator, DPSubtract::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPMultiply", DPMultiply::typeId, DPMultiply::creator, DPMultiply::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPDivide", DPDivide::typeId, DPDivide::creator, DPDivide::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPModulo", DPModulo::typeId, DPModulo::creator, DPModulo::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPPow", DPPow::typeId, DPPow::creator, DPPow::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPMin", DPMin::typeId, DPMin::creator, DPMin::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DPMax", DPMax::typeId, DPMax::creator, DPMax::initialize ));
	
    CHECK_MSTATUS(registration.registerNode( "DPMathFunc", DPMathFunc::typeId, DPMathFunc::creator, DPMathFunc::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPNegative", DPNegative::typeId, DPNegative::creator, DPNegative::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPSqrt", DPSqrt::typeId, DPSqrt::creator, DPSqrt::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPFloor", DPFloor::typeId, DPFloor::creator, DPFloor::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPRound", DPRound::typeId, DPRound::creator, DPRound::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPCeil", DPCeil::typeId, DPCeil::creator, DPCeil::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPExponential", DPExponential::typeId, DPExponential::creator, DPExponential::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPAbsolute", DPAbsolute::typeId, DPAbsolute::creator, DPAbsolute::initialize ));
    
    CHECK_MSTATUS(registration.registerNode( "DPDotProduct", DPDotProduct::typeId, DPDotProduct::creator, DPDotProduct::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecAngle", DPVecAngle::typeId, DPVecAngle::creator, DPVecAngle::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecLength", DPVecLength::typeId, DPVecLength::creator, DPVecLength::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecMinComponent", DPVecMinComponent::typeId, DPVecMinComponent::creator, DPVecMinComponent::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecMaxComponent", DPVecMaxComponent::typeId, DPVecMaxComponent::creator, DPVecMaxComponent::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPVecAverageComponent", DPVecAverageComponent::typeId, DPVecAverageComponent::creator, DPVecAverageComponent::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPNormalize", DPNormalize::typeId, DPNormalize::creator, DPNormalize::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPCrossProduct", DPCrossProduct::typeId, DPCrossProduct::creator, DPCrossProduct::initialize ));
	
	CHECK_MSTATUS(registration.registerNode( "DPFitRange", DPFitRange::typeId, DPFitRange::creator, DPFitRange::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DPRemap", DPRemap::typeId, DPRemap::creator, DPRemap::initialize ));
	
	
	
