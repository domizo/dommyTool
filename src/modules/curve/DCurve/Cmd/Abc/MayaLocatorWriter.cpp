// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2012,
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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaLocatorWriter.cpp

//
//  MayaLocatorWriter.cpp
//  dommyTool
//
//  Created by yuzo on 3/18/17.
//
//

#include "MayaLocatorWriter.h"

#include "DDataExportUtil.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace {
std::array<double, 6> readLocatorSample(const MFnDagNode& node)
{
    const char* names[] = {"localPositionX", "localPositionY", "localPositionZ",
                           "localScaleX", "localScaleY", "localScaleZ"};
    std::array<double, 6> values;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        values[index] = ddata_export_util::readExportValue<double>(
            ddata_export_util::requiredExportPlug(node, names[index]));
        if (!std::isfinite(values[index]))
            throw std::invalid_argument("Non-finite Alembic locator value");
    }
    return values;
}
} // namespace
MayaLocatorWriter::MayaLocatorWriter(MDagPath & iDag,
									 Alembic::Abc::OObject & iParent,
									 Alembic::Util::uint32_t iTimeIndex,
									 const JobArgs & iArgs) :
mIsAnimated(false), mDagPath(iDag)
{
	MStatus status = MS::kSuccess;
	MFnDagNode fnLocator(mDagPath, &status);
    ddata_export_util::requireExportQuery(status, "Unable to bind an Alembic locator");
	const MString name = ddata_export_util::exportObjectName(fnLocator, iArgs.stripNamespace);
	
	mXform = Alembic::AbcGeom::OXform(iParent, name.asChar(), iTimeIndex);
	
	Alembic::Abc::OCompoundProperty cp = mXform.getProperties();
	
	// of type double[6]
	Alembic::AbcCoreAbstract::DataType dType(Alembic::Util::kFloat64POD, 6);
	
	MPlug posX = ddata_export_util::requiredExportPlug(fnLocator, "localPositionX");
	MPlug posY = ddata_export_util::requiredExportPlug(fnLocator, "localPositionY");
	MPlug posZ = ddata_export_util::requiredExportPlug(fnLocator, "localPositionZ");
	MPlug pos = ddata_export_util::requiredExportPlug(fnLocator, "localPosition");
	
	MPlug scaleX = ddata_export_util::requiredExportPlug(fnLocator, "localScaleX");
	MPlug scaleY = ddata_export_util::requiredExportPlug(fnLocator, "localScaleY");
	MPlug scaleZ = ddata_export_util::requiredExportPlug(fnLocator, "localScaleZ");
	MPlug scale = ddata_export_util::requiredExportPlug(fnLocator, "localScale");
	
	if ( iTimeIndex != 0 && (ddata_export_util::getSampledType(posX) != 0 ||
        ddata_export_util::getSampledType(posY) != 0 ||
        ddata_export_util::getSampledType(posZ) != 0 ||
        ddata_export_util::getSampledType(pos) != 0 ||
        ddata_export_util::getSampledType(scaleX) != 0 ||
        ddata_export_util::getSampledType(scaleY) != 0 ||
        ddata_export_util::getSampledType(scaleZ) != 0 ||
        ddata_export_util::getSampledType(scale) != 0) )
	{
		mIsAnimated = true;
		mSp = Alembic::Abc::OScalarProperty(cp,
											"locator", dType, iTimeIndex);
	}
	else
	{
		mSp = Alembic::Abc::OScalarProperty(cp,
											"locator", dType);
	}
	
    const auto values = readLocatorSample(fnLocator);
    mSp.set(values.data());
	
	Alembic::Abc::OCompoundProperty arbGeom;
	Alembic::Abc::OCompoundProperty userProps;
	if (MayaAttributeWriter::hasAnyAttr(fnLocator, iArgs))
	{
		arbGeom = mXform.getSchema().getArbGeomParams();
		userProps = mXform.getSchema().getUserProperties();
	}
	
	mAttrs = MayaAttributeWriterPtr(new MayaAttributeWriter(arbGeom, userProps,
													  mXform, fnLocator,
													  iTimeIndex, iArgs));
}


MayaLocatorWriter::~MayaLocatorWriter()
{
	mSp.reset();
}

void MayaLocatorWriter::write()
{
	MStatus status = MS::kSuccess;
	MFnDagNode fnLocator(mDagPath, &status);
    ddata_export_util::requireExportQuery(status, "Unable to bind an Alembic locator");
    const auto values = readLocatorSample(fnLocator);
    mSp.set(values.data());
}

bool MayaLocatorWriter::isAnimated() const
{
	return mIsAnimated;
}


