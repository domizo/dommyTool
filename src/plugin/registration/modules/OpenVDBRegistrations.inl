#pragma mark ---------------OpenVDB---------------

    // Nodes
    CHECK_MSTATUS(registration.registerNode("DReadVDB", DReadOpenVDB::typeId, DReadOpenVDB::creator, DReadOpenVDB::initialize));
#if DOMMY_WITH_DG_FILE_WRITERS
    CHECK_MSTATUS(registration.registerNode("DWriteVDB", DWriteOpenVDB::typeId, DWriteOpenVDB::creator, DWriteOpenVDB::initialize));
#endif

    // Display
    CHECK_MSTATUS(registration.registerNode(DOpenVDBDisplayVP2::typeName, DOpenVDBDisplayVP2::typeId, &DOpenVDBDisplayVP2::creator, &DOpenVDBDisplayVP2::initialize, MPxNode::kLocatorNode, &DOpenVDBDisplayVP2::classification));
    CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DOpenVDBDisplayVP2::classification, DOpenVDBDisplayVP2::registrantId, DOpenVDBDisplayVP2Override::Creator));

    // Generation
    CHECK_MSTATUS(registration.registerNode("DVDBFromMesh", DOpenVDBFromMesh::typeId, DOpenVDBFromMesh::creator, DOpenVDBFromMesh::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFromFluid", DOpenVDBFromFluid::typeId, DOpenVDBFromFluid::creator, DOpenVDBFromFluid::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFromParticle", DOpenVDBFromParticle::typeId, DOpenVDBFromParticle::creator, DOpenVDBFromParticle::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFromDP", DOpenVDBFromDP::typeId, DOpenVDBFromDP::creator, DOpenVDBFromDP::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBSamplingDP", DOpenVDBSamplingDP::typeId, DOpenVDBSamplingDP::creator, DOpenVDBSamplingDP::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBScatter", DOpenVDBScatter::typeId, DOpenVDBScatter::creator, DOpenVDBScatter::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFoamGen", DVDBFoamGen::typeId, DVDBFoamGen::creator, DVDBFoamGen::initialize));

    // Conversion
    CHECK_MSTATUS(registration.registerNode("DVDBToMesh", DOpenVDBToMesh::typeId, DOpenVDBToMesh::creator, DOpenVDBToMesh::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBConvert", DOpenVDBConvert::typeId, DOpenVDBConvert::creator, DOpenVDBConvert::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBToSpheres", DOpenVDBToSpheres::typeId, DOpenVDBToSpheres::creator, DOpenVDBToSpheres::initialize));

    // Editing
    CHECK_MSTATUS(registration.registerNode("DVDBFilter", DOpenVDBFilter::typeId, DOpenVDBFilter::creator, DOpenVDBFilter::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBCombine", DOpenVDBCombine::typeId, DOpenVDBCombine::creator, DOpenVDBCombine::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFilterLevelSet", DOpenVDBFilterLevelSet::typeId, DOpenVDBFilterLevelSet::creator, DOpenVDBFilterLevelSet::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBResample", DOpenVDBResample::typeId, DOpenVDBResample::creator, DOpenVDBResample::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBNoise", DOpenVDBNoise::typeId, DOpenVDBNoise::creator, DOpenVDBNoise::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBPlatonic", DOpenVDBPlatonic::typeId, DOpenVDBPlatonic::creator, DOpenVDBPlatonic::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFracture", DOpenVDBFracture::typeId, DOpenVDBFracture::creator, DOpenVDBFracture::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBTransform", DOpenVDBTransform::typeId, DOpenVDBTransform::creator, DOpenVDBTransform::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBCopy", DOpenVDBCopy::typeId, DOpenVDBCopy::creator, DOpenVDBCopy::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBClip", DOpenVDBClip::typeId, DOpenVDBClip::creator, DOpenVDBClip::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBMorphLevelSet", DOpenVDBMorphLevelSet::typeId, DOpenVDBMorphLevelSet::creator, DOpenVDBMorphLevelSet::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBVectorMerge", DOpenVDBVectorMerge::typeId, DOpenVDBVectorMerge::creator, DOpenVDBVectorMerge::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBVectorSplit", DOpenVDBVectorSplit::typeId, DOpenVDBVectorSplit::creator, DOpenVDBVectorSplit::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBAdvect", DOpenVDBAdvect::typeId, DOpenVDBAdvect::creator, DOpenVDBAdvect::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBAdvectPoints", DOpenVDBAdvectPoints::typeId, DOpenVDBAdvectPoints::creator, DOpenVDBAdvectPoints::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBAnalysis", DOpenVDBAnalysis::typeId, DOpenVDBAnalysis::creator, DOpenVDBAnalysis::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBRemap", DOpenVDBRemap::typeId, DOpenVDBRemap::creator, DOpenVDBRemap::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBPrune", DOpenVDBPrune::typeId, DOpenVDBPrune::creator, DOpenVDBPrune::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBFill", DOpenVDBFill::typeId, DOpenVDBFill::creator, DOpenVDBFill::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBRemoveDivergence", DOpenVDBRemoveDivergence::typeId, DOpenVDBRemoveDivergence::creator, DOpenVDBRemoveDivergence::initialize));
    CHECK_MSTATUS(registration.registerNode("DVDBPotentialFlow", DVDBPotentialFlow::typeId, DVDBPotentialFlow::creator, DVDBPotentialFlow::initialize));

    // Field
    CHECK_MSTATUS(registration.registerNode("DVDBField", DOpenVDBField::id, &DOpenVDBField::creator, &DOpenVDBField::initialize, MPxNode::kFieldNode));
