// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2011,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaNurbsCurveWriter.h

//
//  DCurveWriter.hpp
//  dommyTool
//
//  Created by yuzo on 3/16/17.
//
//

#pragma once



#include "MayaTransformWriter.h"
#include "MayaAttributeWriter.h"
#include <Alembic/AbcGeom/OCurves.h>
#include "DDataExportUtil.h"

#include "DCurveDataHeaders.h"

#include <map>
#include <string>
#include <list>


class DCurveWriter
{
public:
	
	DCurveWriter(MDagPath & iDag, Alembic::Abc::OObject & iParent,
						 Alembic::Util::uint32_t iTimeIndex,
						 const JobArgs & iArgs);
	
	void write();
	bool isAnimated() const;
	size_t getNumCVs();
	size_t getNumCurves();
    MBoundingBox getSampleBounds() const { return mSampleBounds; }
//	MayaAttributeWriterPtr getAttrs() {return mAttrs;};
	
private:
    
    void getDCurvePrimAttribute(const DCurveData &dCurves, const std::list<std::string> &attributeList, bool isAnim);
    void getDCurvePointAttribute(const DCurveData &dCurves, const std::list<std::string> &attributeList, bool isAnim);
    
    
//    void fillTopology();
	
	bool mIsAnimated;
	MDagPath mRootDagPath;
	
    
    DAbcPointAttrList pointAttributeList;
    dommy::attribute::ExportAttributeState mAttributeState;
    ddata_export_util::BuiltinSampleState mBuiltinState;
    
    
    
	Alembic::AbcGeom::OCurvesSchema mSchema;
    
    
	bool mIsCurveGrp;
	unsigned int mCVCount;
	size_t mCurveCount;
    MBoundingBox mSampleBounds;
	
	Alembic::Util::uint32_t mTimeIndex;
};


