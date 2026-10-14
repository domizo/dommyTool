#pragma once

#include <maya/MDoubleArray.h>
#include <maya/MDataBlock.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MVectorArray.h>

// Focused declarations for Maya typed-array outputs. Implementations remain in
// DNodeOperation.cpp so historical callers retain their existing ABI.
MStatus DOutputDataMayaIntArray(MDataBlock& data,
                                const MObject& dirtyPlug,
                                const MObject& output,
                                const MIntArray& dataArray);
MStatus DOutputDataMayaFloatArray(MDataBlock& data,
                                  const MObject& dirtyPlug,
                                  const MObject& output,
                                  const MFloatArray& dataArray);
MStatus DOutputDataMayaDoubleArray(MDataBlock& data,
                                   const MObject& dirtyPlug,
                                   const MObject& output,
                                   const MDoubleArray& dataArray);
MStatus DOutputDataMayaVectorArray(MDataBlock& data,
                                   const MObject& dirtyPlug,
                                   const MObject& output,
                                   const MVectorArray& dataArray);
MStatus DOutputDataMayaPointArray(MDataBlock& data,
                                  const MObject& dirtyPlug,
                                  const MObject& output,
                                  const MPointArray& dataArray);
