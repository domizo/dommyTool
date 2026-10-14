//
//  DOpenVDBDataHeaders.h
//  dommyTool
//
//  Created by yuzo on 2/7/17.
//
//

#pragma once


#include "DOpenVDBData.h"
#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"
#include "DOpenVDBGeoUtil.h"

#include "dvdb_ids.h"

#include <openvdb/points/AttributeGroup.h>
#include <openvdb/points/AttributeSet.h>
#include <openvdb/openvdb.h>
#include <openvdb/Types.h>
#include <openvdb/Metadata.h>




#pragma mark OpenVdb type
typedef openvdb::points::TypedAttributeArray<openvdb::Index32>	DVDBAttrIndex;


typedef openvdb::points::TypedAttributeArray<bool, openvdb::points::NullCodec>			DVDBAttrBool;
typedef openvdb::points::TypedAttributeArray<int32_t, openvdb::points::NullCodec>		DVDBAttrInt;
typedef openvdb::points::TypedAttributeArray<uint32_t, openvdb::points::NullCodec>		DVDBAttrInt32;
typedef openvdb::points::TypedAttributeArray<int64_t, openvdb::points::NullCodec>		DVDBAttrInt64;
typedef openvdb::points::TypedAttributeArray<float, openvdb::points::NullCodec>			DVDBAttrFloat;
typedef openvdb::points::TypedAttributeArray<double, openvdb::points::NullCodec>		DVDBAttrDouble;
typedef openvdb::points::TypedAttributeArray<Imath::V2i, openvdb::points::NullCodec>	DVDBAttrVec2i;
typedef openvdb::points::TypedAttributeArray<Imath::V2f, openvdb::points::NullCodec>	DVDBAttrVec2f;
typedef openvdb::points::TypedAttributeArray<Imath::V2d, openvdb::points::NullCodec>	DVDBAttrVec2d;
typedef openvdb::points::TypedAttributeArray<Imath::V3i, openvdb::points::NullCodec>	DVDBAttrVec3i;
typedef openvdb::points::TypedAttributeArray<Imath::V3f, openvdb::points::NullCodec>	DVDBAttrVec3f;
typedef openvdb::points::TypedAttributeArray<Imath::V3d, openvdb::points::NullCodec>	DVDBAttrVec3d;
typedef openvdb::points::TypedAttributeArray<std::string, openvdb::points::NullCodec>	DVDBAttrString;
typedef openvdb::points::TypedAttributeArray<openvdb::Mat4d, openvdb::points::NullCodec>       DVDBAttrMat;
