#pragma mark---------------DCurve---------------

// Display
CHECK_MSTATUS(registration.registerNode(DCurveDisplay::typeName, DCurveDisplay::typeId,
                                        &DCurveDisplay::creator, &DCurveDisplay::initialize,
                                        MPxNode::kLocatorNode, &DCurveDisplay::classification));
CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DCurveDisplay::classification,
                                                           DCurveDisplay::registrantId,
                                                           DCurveDisplayOverride::Creator));
CHECK_MSTATUS(registration.registerNode(DCMarker::typeName, DCMarker::typeId, &DCMarker::creator,
                                        &DCMarker::initialize, MPxNode::kLocatorNode,
                                        &DCMarker::classification));
CHECK_MSTATUS(registration.registerGeometryOverrideCreator(DCMarker::classification,
                                                           DCMarker::registrantId,
                                                           DCMarkerOverride::Creator));

// Group and atribute
CHECK_MSTATUS(registration.registerNode("DCGroup", DCGroup::typeId, DCGroup::creator,
                                        DCGroup::initialize));
CHECK_MSTATUS(registration.registerNode("DCGroupManip", DCGroupManip::id, &DCGroupManip::creator,
                                        &DCGroupManip::initialize, MPxNode::kManipContainer));
CHECK_MSTATUS(registration.registerNode("DCGroupDelete", DCGroupDelete::typeId,
                                        DCGroupDelete::creator, DCGroupDelete::initialize));
CHECK_MSTATUS(registration.registerNode("DCGroupFromDP", DCGroupFromDP::typeId,
                                        DCGroupFromDP::creator, DCGroupFromDP::initialize));

CHECK_MSTATUS(registration.registerNode("DCAttributeCopy", DCAttributeCopy::typeId,
                                        DCAttributeCopy::creator, DCAttributeCopy::initialize));
CHECK_MSTATUS(registration.registerNode("DCAttributeTransfer", DCAttributeTransfer::typeId,
                                        DCAttributeTransfer::creator,
                                        DCAttributeTransfer::initialize));

// Cmd
#if DOMMY_WITH_ALEMBIC
CHECK_MSTATUS(registration.registerCommandWithSyntax<DExportAbcCmd>("DExportAbcCmd"));
#endif

// Convert
CHECK_MSTATUS(registration.registerNode("DCConvertFromCurves", DCConvertFromCurves::typeId,
                                        DCConvertFromCurves::creator,
                                        DCConvertFromCurves::initialize));
CHECK_MSTATUS(registration.registerNode("DCToCurves", DCToCurves::typeId, DCToCurves::creator,
                                        DCToCurves::initialize));

// Generate
CHECK_MSTATUS(registration.registerNode("DCurveFromMesh", DCurveFromMesh::typeId,
                                        DCurveFromMesh::creator, DCurveFromMesh::initialize));
CHECK_MSTATUS(registration.registerNode("DCurveFromSurface", DCurveFromSurface::typeId,
                                        DCurveFromSurface::creator, DCurveFromSurface::initialize));
CHECK_MSTATUS(registration.registerNode("DCurveFromCurve", DCurveFromCurve::typeId,
                                        DCurveFromCurve::creator, DCurveFromCurve::initialize));

CHECK_MSTATUS(registration.registerNode("DCurveClothFromSurface", DCurveClothFromSurface::typeId,
                                        DCurveClothFromSurface::creator,
                                        DCurveClothFromSurface::initialize));

CHECK_MSTATUS(registration.registerNode("DCurveLinkGen", DCurveLinkGen::typeId,
                                        DCurveLinkGen::creator, DCurveLinkGen::initialize));
CHECK_MSTATUS(registration.registerNode("DCurveFromDP", DCurveFromDP::typeId, DCurveFromDP::creator,
                                        DCurveFromDP::initialize));
CHECK_MSTATUS(registration.registerNode("DCAhoGen", DCAhoGen::typeId, DCAhoGen::creator,
                                        DCAhoGen::initialize));
CHECK_MSTATUS(registration.registerNode("DCurveFromPfx", DCurveFromPfx::typeId,
                                        DCurveFromPfx::creator, DCurveFromPfx::initialize));
CHECK_MSTATUS(registration.registerNode("DCFurGen", DCFurGen::typeId, DCFurGen::creator,
                                        DCFurGen::initialize));
CHECK_MSTATUS(registration.registerNode("DCurveCircleGen", DCurveCircleGen::typeId,
                                        DCurveCircleGen::creator, DCurveCircleGen::initialize));
#if DOMMY_WITH_FREETYPE_TEXT
CHECK_MSTATUS(registration.registerNode("DCText", DCText::typeId, DCText::creator,
                                        DCText::initialize));
#endif
CHECK_MSTATUS(registration.registerNode("DCEyelashGen", DCEyelashGen::typeId, DCEyelashGen::creator,
                                        DCEyelashGen::initialize));

// IO
// Utility
CHECK_MSTATUS(registration.registerNode("DCReform", DCReform::typeId, DCReform::creator,
                                        DCReform::initialize));
CHECK_MSTATUS(registration.registerNode("DCDivide", DCDivide::typeId, DCDivide::creator,
                                        DCDivide::initialize));
CHECK_MSTATUS(registration.registerNode("DCCharactor", DCCharactor::typeId, DCCharactor::creator,
                                        DCCharactor::initialize));
CHECK_MSTATUS(registration.registerNode("DCCopy", DCCopy::typeId, DCCopy::creator,
                                        DCCopy::initialize));
CHECK_MSTATUS(registration.registerNode("DCUVMap", DCUVMap::typeId, DCUVMap::creator,
                                        DCUVMap::initialize));
CHECK_MSTATUS(registration.registerNode("DCDelete", DCDelete::typeId, DCDelete::creator,
                                        DCDelete::initialize));
CHECK_MSTATUS(registration.registerNode("DCColor", DCColor::typeId, DCColor::creator,
                                        DCColor::initialize));
CHECK_MSTATUS(registration.registerNode("DCMotionPath", DCMotionPath::typeId, DCMotionPath::creator,
                                        DCMotionPath::initialize));
CHECK_MSTATUS(registration.registerNode("DCNoise", DCNoise::typeId, DCNoise::creator,
                                        DCNoise::initialize));
CHECK_MSTATUS(registration.registerNode("DCResample", DCResample::typeId, DCResample::creator,
                                        DCResample::initialize));
CHECK_MSTATUS(registration.registerNode("DCWeightMap", DCWeightMap::typeId, DCWeightMap::creator,
                                        DCWeightMap::initialize));
CHECK_MSTATUS(registration.registerNode("DCNormal", DCNormal::typeId, DCNormal::creator,
                                        DCNormal::initialize));
CHECK_MSTATUS(registration.registerNode("DCFitCopy", DCFitCopy::typeId, DCFitCopy::creator,
                                        DCFitCopy::initialize));
CHECK_MSTATUS(registration.registerNode("DCReplicator", DCReplicator::typeId, DCReplicator::creator,
                                        DCReplicator::initialize));
CHECK_MSTATUS(registration.registerNode("DCMeshCopy", DCMeshCopy::typeId, DCMeshCopy::creator,
                                        DCMeshCopy::initialize));
CHECK_MSTATUS(registration.registerNode("DCConnect", DCConnect::typeId, DCConnect::creator,
                                        DCConnect::initialize));
CHECK_MSTATUS(registration.registerNode("DCExtend", DCExtend::typeId, DCExtend::creator,
                                        DCExtend::initialize));
CHECK_MSTATUS(registration.registerNode("DCOffset", DCOffset::typeId, DCOffset::creator,
                                        DCOffset::initialize));
CHECK_MSTATUS(registration.registerNode("DCBend", DCBend::typeId, DCBend::creator,
                                        DCBend::initialize));
CHECK_MSTATUS(registration.registerNode("DCStitch", DCStitch::typeId, DCStitch::creator,
                                        DCStitch::initialize));
CHECK_MSTATUS(registration.registerNode("DCCurl", DCCurl::typeId, DCCurl::creator,
                                        DCCurl::initialize));
CHECK_MSTATUS(registration.registerNode("DCCutObject", DCCutObject::typeId, DCCutObject::creator,
                                        DCCutObject::initialize));
CHECK_MSTATUS(registration.registerNode("DCFillBetween", DCFillBetween::typeId,
                                        DCFillBetween::creator, DCFillBetween::initialize));
CHECK_MSTATUS(registration.registerNode("DCConvex2D", DCConvex2D::typeId, DCConvex2D::creator,
                                        DCConvex2D::initialize));
CHECK_MSTATUS(registration.registerNode("DCReplace", DCReplace::typeId, DCReplace::creator,
                                        DCReplace::initialize));
CHECK_MSTATUS(registration.registerNode("DCPartition", DCPartition::typeId, DCPartition::creator,
                                        DCPartition::initialize));
CHECK_MSTATUS(registration.registerNode("DCExplodeView", DCExplodeView::typeId,
                                        DCExplodeView::creator, DCExplodeView::initialize));
CHECK_MSTATUS(registration.registerNode("DCClumping", DCClumping::typeId, DCClumping::creator,
                                        DCClumping::initialize));
CHECK_MSTATUS(registration.registerNode("DCReflect", DCReflect::typeId, DCReflect::creator,
                                        DCReflect::initialize));

// Solver
CHECK_MSTATUS(registration.registerNode("DCSolver", DCSolver::typeId, DCSolver::creator,
                                        DCSolver::initialize));

// Deform
CHECK_MSTATUS(registration.registerNode("DCVectorDeform", DCVectorDeform::typeId,
                                        DCVectorDeform::creator, DCVectorDeform::initialize));
