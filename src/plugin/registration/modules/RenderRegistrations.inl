#pragma mark ---------------Render---------------
	
	
	CHECK_MSTATUS(registration.registerNode( DCameraFrustum::typeName, DCameraFrustum::typeId, &DCameraFrustum::creator, &DCameraFrustum::initialize, MPxNode::kLocatorNode, &DCameraFrustum::classification));
		CHECK_MSTATUS(registration.registerDrawOverrideCreator(DCameraFrustum::classification, DCameraFrustum::registrantId, DCameraFrustumOverride::Creator));
