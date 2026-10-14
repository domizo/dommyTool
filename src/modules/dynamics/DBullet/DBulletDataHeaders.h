//
//  DBulletDataHeader.h
//  dommyTool
//
//  Created by yuzo on 2017/10/01.
//

#pragma once


enum {
    D_COL_CONVEX_HULL = 0,
    D_COL_MESH,
    D_COL_CYLINDER,
    D_COL_CAPSULE,
    D_COL_BOX,
    D_COL_SPHERE,
    D_COL_PLANE,
    D_COL_BVH_MESH,
    D_COL_HACD,
    DBulletCollisionType_COUNT
};typedef short DBulletCollisionType;


enum {
    DB_CONST_TAR_POINT = 0,
    DB_CONST_TAR_RB,
    DB_CONST_TAR_SB,
    DB_CONST_TAR_DP,
    DB_CONST_TAR_DC,
    DB_CONST_TAR_DM,
    DBConstraintTargetType_COUNT
};typedef short DBConstraintTargetType;


enum {
    DBT_CONST_INVALID = 0,
    DBT_CONST_P2P,
    DBT_CONST_SPRING,
    DBT_CONST_FIX,
    DBT_CONST_CONE_TWIST,
    DBT_CONST_6DOF,
    DBT_CONST_6DOF_SPRING,
    DBT_CONST_HINGE,
    DBT_CONST_SLIDER,
    DBT_CONST_TYPE_COUNT
};typedef short DBT_CONST_TYPE;





