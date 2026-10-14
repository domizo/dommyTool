#pragma mark ---------------Arnold---------------

#if DOMMY_WITH_DG_FILE_WRITERS
CHECK_MSTATUS(registration.registerNode(
    "DPWriteToAi",
    DPWriteToAi::typeId,
    DPWriteToAi::creator,
    DPWriteToAi::initialize));
#endif

CHECK_MSTATUS(registration.registerNode(
    DPInstancer::typeName,
    DPInstancer::typeId,
    &DPInstancer::creator,
    &DPInstancer::initialize,
    MPxNode::kLocatorNode,
    &DPInstancer::classification));
CHECK_MSTATUS(registration.registerGeometryOverrideCreator(
    DPInstancer::classification,
    DPInstancer::registrantId,
    DPInstancerOverride::Creator));

CHECK_MSTATUS(registration.registerNode(
    DPLightGen::typeName,
    DPLightGen::typeId,
    &DPLightGen::creator,
    &DPLightGen::initialize,
    MPxNode::kLocatorNode,
    &DPLightGen::classification));
CHECK_MSTATUS(registration.registerDrawOverrideCreator(
    DPLightGen::classification,
    DPLightGen::registrantId,
    DPLightGenOverride::Creator));

#if DOMMY_WITH_DG_FILE_WRITERS
CHECK_MSTATUS(registration.registerNode(
    "DWriteDCurveToAi",
    DWriteDCurveToAi::typeId,
    DWriteDCurveToAi::creator,
    DWriteDCurveToAi::initialize));
#endif
