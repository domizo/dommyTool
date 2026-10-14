#pragma mark ---------------DBullet---------------
    
	#pragma mark solver
	    DOMMY_RETURN_IF_FAILURE(registration.registerResource([]() {
	        dbSolver_t::initialize();
	        return MStatus(MS::kSuccess);
	    }, [](MFnPlugin&) {
	        dbSolver_t::cleanup();
	        return MStatus(MS::kSuccess);
	    }));
    
    CHECK_MSTATUS(registration.registerNode( DBSolverNode::typeName, DBSolverNode::typeId,
                        DBSolverNode::creator,
                        DBSolverNode::initialize,
                        DBSolverNode::kLocatorNode ));
    
#if DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE
    CHECK_MSTATUS(registration.registerNode(
        DMBulletTest::typeName, DMBulletTest::typeId,
        DMBulletTest::creator, DMBulletTest::initialize,
        DMBulletTest::kLocatorNode));
#endif
    CHECK_MSTATUS(registration.registerNode( DCBullet2DSolver::typeName, DCBullet2DSolver::typeId,
                                        DCBullet2DSolver::creator,
                                        DCBullet2DSolver::initialize,
                                        DCBullet2DSolver::kLocatorNode ));
    
    
#if DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER
    CHECK_MSTATUS(registration.registerNode( DBulletFractureSolver::typeName, DBulletFractureSolver::typeId,
                                        DBulletFractureSolver::creator,
                                        DBulletFractureSolver::initialize,
                                        DBulletFractureSolver::kLocatorNode ));
#endif

    
#pragma mark body type
    CHECK_MSTATUS(registration.registerNode( DBRigidBodyNode::typeName, DBRigidBodyNode::typeId,
                        DBRigidBodyNode::creator,
                        DBRigidBodyNode::initialize,
                        MPxNode::kLocatorNode ));
    
    
    CHECK_MSTATUS(registration.registerNode( DBRigidBodyArrayNode::typeName, DBRigidBodyArrayNode::typeId,
                        DBRigidBodyArrayNode::creator,
                        DBRigidBodyArrayNode::initialize,
                        MPxNode::kLocatorNode ));
    
    
    
    
    
    
    CHECK_MSTATUS(registration.registerNode( DBSoftBodyNode::typeName, DBSoftBodyNode::typeId,
                        DBSoftBodyNode::creator,
                        DBSoftBodyNode::initialize,
                        MPxNode::kLocatorNode ));
    
    
    
#pragma mark collision
    CHECK_MSTATUS(registration.registerNode( DBCollisionShapeNode::typeName, DBCollisionShapeNode::typeId,
                                 DBCollisionShapeNode::creator,
                                 DBCollisionShapeNode::initialize,
                                 MPxNode::kDependNode ));
    
#pragma mark constraint
    CHECK_MSTATUS(registration.registerNode( DBHingeConstraintNode::typeName, DBHingeConstraintNode::typeId,
                        DBHingeConstraintNode::creator,
                        DBHingeConstraintNode::initialize,
                        MPxNode::kLocatorNode ));
    
    CHECK_MSTATUS(registration.registerNode( DBNailConstraintNode::typeName, DBNailConstraintNode::typeId,
                        DBNailConstraintNode::creator,
                        DBNailConstraintNode::initialize,
                        MPxNode::kLocatorNode ));
    
    
    
    CHECK_MSTATUS(registration.registerNode( DBSixdofConstraintNode::typeName, DBSixdofConstraintNode::typeId,
                        DBSixdofConstraintNode::creator,
                        DBSixdofConstraintNode::initialize,
                        MPxNode::kLocatorNode ));
    
    
    CHECK_MSTATUS(registration.registerNode( DBSliderConstraintNode::typeName, DBSliderConstraintNode::typeId,
                        DBSliderConstraintNode::creator,
                        DBSliderConstraintNode::initialize,
                        MPxNode::kLocatorNode ));

    // Register callbacks only after every related node type has registered successfully.
    DOMMY_RETURN_IF_FAILURE(registration.registerResource(
        []() { return DBSolverNode::registerCallbacks(); },
        [](MFnPlugin&) { return DBSolverNode::removeCallbacks(); }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBRigidBodyNode::nodeRemoved, DBRigidBodyNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBRigidBodyArrayNode::nodeRemoved, DBRigidBodyArrayNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBSoftBodyNode::nodeRemoved, DBSoftBodyNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBHingeConstraintNode::nodeRemoved, DBHingeConstraintNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBNailConstraintNode::nodeRemoved, DBNailConstraintNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBSixdofConstraintNode::nodeRemoved, DBSixdofConstraintNode::typeName, nullptr, &status);
    }));
    DOMMY_RETURN_IF_FAILURE(registration.registerCallback([](MStatus& status) {
        return MDGMessage::addNodeRemovedCallback(
            DBSliderConstraintNode::nodeRemoved, DBSliderConstraintNode::typeName, nullptr, &status);
    }));
