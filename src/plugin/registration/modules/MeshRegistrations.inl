#pragma mark ---------------DMesh---------------

	
    //Group and atribute
    CHECK_MSTATUS(registration.registerNode( "DMGroup", DMGroup::typeId, DMGroup::creator, DMGroup::initialize ));
    CHECK_MSTATUS(registration.registerNode("DMGroupManip",DMGroupManip::id,
                                        &DMGroupManip::creator,
                                        &DMGroupManip::initialize,
                                        MPxNode::kManipContainer));
    CHECK_MSTATUS(registration.registerNode( "DMGroupDelete", DMGroupDelete::typeId, DMGroupDelete::creator, DMGroupDelete::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DMGroupFromDP", DMGroupFromDP::typeId, DMGroupFromDP::creator, DMGroupFromDP::initialize ));
	CHECK_MSTATUS(registration.registerNode( "DMGroupToIndex", DMGroupToIndex::typeId, DMGroupToIndex::creator, DMGroupToIndex::initialize ));
    
    
    
	CHECK_MSTATUS(registration.registerNode( "DMAttributeCopy", DMAttributeCopy::typeId, DMAttributeCopy::creator, DMAttributeCopy::initialize ));
    CHECK_MSTATUS(registration.registerNode( "DMAttributeTransfer", DMAttributeTransfer::typeId, DMAttributeTransfer::creator, DMAttributeTransfer::initialize ));
    
    

	
    
    //Display
    CHECK_MSTATUS(registration.registerNode( DMeshDisplay::typeName, DMeshDisplay::typeId, &DMeshDisplay::creator, &DMeshDisplay::initialize, MPxNode::kLocatorNode, &DMeshDisplay::classification));
	    CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DMeshDisplay::classification, DMeshDisplay::registrantId, DMeshDisplayOverride::Creator));
    
    CHECK_MSTATUS(registration.registerNode( DMMarker::typeName, DMMarker::typeId, &DMMarker::creator, &DMMarker::initialize, MPxNode::kLocatorNode, &DMMarker::classification));
	    CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DMMarker::classification, DMMarker::registrantId, DMMarkerOverride::Creator));
    
    
    
    
	
	//Convert
	CHECK_MSTATUS(registration.registerNode("DMConvertFromMeshes", DMConvertFromMeshes::typeId, DMConvertFromMeshes::creator, DMConvertFromMeshes::initialize));
	CHECK_MSTATUS(registration.registerNode("DMToMesh", DMToMesh::typeId, DMToMesh::creator, DMToMesh::initialize));
    
    
    //Genrate
    CHECK_MSTATUS(registration.registerNode("DMWireGen", DMWireGen::typeId, DMWireGen::creator, DMWireGen::initialize));
    CHECK_MSTATUS(registration.registerNode("DMTimeShapeGen", DMTimeShapeGen::typeId, DMTimeShapeGen::creator, DMTimeShapeGen::initialize));
    
    
	
    //Utility
    CHECK_MSTATUS(registration.registerNode("DMColor", DMColor::typeId, DMColor::creator, DMColor::initialize));
    CHECK_MSTATUS(registration.registerNode("DMRemesh", DMRemesh::typeId, DMRemesh::creator, DMRemesh::initialize));
    CHECK_MSTATUS(registration.registerNode("DMSlicer", DMSlicer::typeId, DMSlicer::creator, DMSlicer::initialize));
    CHECK_MSTATUS(registration.registerNode("DMBoolean", DMBoolean::typeId, DMBoolean::creator, DMBoolean::initialize));
    CHECK_MSTATUS(registration.registerNode("DMCharactor", DMCharactor::typeId, DMCharactor::creator, DMCharactor::initialize));
    CHECK_MSTATUS(registration.registerNode("DMExplodeView", DMExplodeView::typeId, DMExplodeView::creator, DMExplodeView::initialize));
	CHECK_MSTATUS(registration.registerNode("DMFaceInOut", DMFaceInOut::typeId, DMFaceInOut::creator, DMFaceInOut::initialize));
    CHECK_MSTATUS(registration.registerNode("DMDelete", DMDelete::typeId, DMDelete::creator, DMDelete::initialize));
    CHECK_MSTATUS(registration.registerNode("DMTransform", DMTransform::typeId, DMTransform::creator, DMTransform::initialize));
    CHECK_MSTATUS(registration.registerNode("DMSolver", DMSolver::typeId, DMSolver::creator, DMSolver::initialize));
    CHECK_MSTATUS(registration.registerNode("DMVectorDeform", DMVectorDeform::typeId, DMVectorDeform::creator, DMVectorDeform::initialize));
    CHECK_MSTATUS(registration.registerNode("DMNoise", DMNoise::typeId, DMNoise::creator, DMNoise::initialize));
    CHECK_MSTATUS(registration.registerNode("DMSubDivide", DMSubDivide::typeId, DMSubDivide::creator, DMSubDivide::initialize));
	CHECK_MSTATUS(registration.registerNode("DMCopyUV", DMCopyUV::typeId, DMCopyUV::creator, DMCopyUV::initialize));
#if DOMMY_WITH_FASTNOISE
	CHECK_MSTATUS(registration.registerNode("DMFastNoise", DMFastNoise::typeId, DMFastNoise::creator, DMFastNoise::initialize));
#endif
	CHECK_MSTATUS(registration.registerNode("DMNormal", DMNormal::typeId, DMNormal::creator, DMNormal::initialize));
    CHECK_MSTATUS(registration.registerNode("DMMerge", DMMerge::typeId, DMMerge::creator, DMMerge::initialize));
    CHECK_MSTATUS(registration.registerNode("DMMotionDeform", DMMotionDeform::typeId, DMMotionDeform::creator, DMMotionDeform::initialize));
    CHECK_MSTATUS(registration.registerNode("DMDebrisGen", DMDebrisGen::typeId, DMDebrisGen::creator, DMDebrisGen::initialize));
	CHECK_MSTATUS(registration.registerNode("DMEdgeDisplace", DMEdgeDisplace::typeId, DMEdgeDisplace::creator, DMEdgeDisplace::initialize));
    CHECK_MSTATUS(registration.registerNode("DMExtrude", DMExtrude::typeId, DMExtrude::creator, DMExtrude::initialize));
    CHECK_MSTATUS(registration.registerNode("DMConnetedPieces", DMConnetedPieces::typeId, DMConnetedPieces::creator, DMConnetedPieces::initialize));
    CHECK_MSTATUS(registration.registerNode("DMConnectTargets", DMConnectTargets::typeId, DMConnectTargets::creator, DMConnectTargets::initialize));
    CHECK_MSTATUS(registration.registerNode("DMCopyTransform", DMCopyTransform::typeId, DMCopyTransform::creator, DMCopyTransform::initialize));
    CHECK_MSTATUS(registration.registerNode("DMMakePiece", DMMakePiece::typeId, DMMakePiece::creator, DMMakePiece::initialize));
    CHECK_MSTATUS(registration.registerNode("DMEdgeDisplaceDir", DMEdgeDisplaceDir::typeId, DMEdgeDisplaceDir::creator, DMEdgeDisplaceDir::initialize));
    CHECK_MSTATUS(registration.registerNode("DMInfo", DMInfo::typeId, DMInfo::creator, DMInfo::initialize));
    
    
    
    
	//Dynamics
	CHECK_MSTATUS(registration.registerNode("DMnRigidGen", DMnRigidGen::typeId, DMnRigidGen::creator, DMnRigidGen::initialize));
	CHECK_MSTATUS(registration.registerNode("DMRest", DMRest::typeId, DMRest::creator, DMRest::initialize));
    CHECK_MSTATUS(registration.registerNode("DCRest", DCRest::typeId, DCRest::creator, DCRest::initialize));
#if DOMMY_WITH_BULLET
	CHECK_MSTATUS(registration.registerNode("DMbtRigid", DMbtRigid::typeId, DMbtRigid::creator, DMbtRigid::initialize));
    CHECK_MSTATUS(registration.registerNode("DCbtRigid2D", DCbtRigid2D::typeId, DCbtRigid2D::creator, DCbtRigid2D::initialize));
    
	CHECK_MSTATUS(registration.registerNode("DMbtSoft", DMbtSoft::typeId, DMbtSoft::creator, DMbtSoft::initialize));
    CHECK_MSTATUS(registration.registerNode("DCbtRope", DCbtRope::typeId, DCbtRope::creator, DCbtRope::initialize));
    CHECK_MSTATUS(registration.registerNode("DMbtFracture", DMbtFracture::typeId, DMbtFracture::creator, DMbtFracture::initialize));
    CHECK_MSTATUS(registration.registerNode("DBPointConstraint", DBPointConstraint::typeId, DBPointConstraint::creator, DBPointConstraint::initialize));
    CHECK_MSTATUS(registration.registerNode("DBLinkConstraint", DBLinkConstraint::typeId, DBLinkConstraint::creator, DBLinkConstraint::initialize));
    
    
    //Constraint
    CHECK_MSTATUS(registration.registerNode("BTP2PConstraint", BTP2PConstraint::typeId, BTP2PConstraint::creator, BTP2PConstraint::initialize));
    CHECK_MSTATUS(registration.registerNode("BTFixConstraint", BTFixConstraint::typeId, BTFixConstraint::creator, BTFixConstraint::initialize));
    CHECK_MSTATUS(registration.registerNode("BTConeTwistConstraint", BTConeTwistConstraint::typeId, BTConeTwistConstraint::creator, BTConeTwistConstraint::initialize));
    CHECK_MSTATUS(registration.registerNode("BT6DofConstraint", BT6DofConstraint::typeId, BT6DofConstraint::creator, BT6DofConstraint::initialize));
#endif
    
    
    
    
