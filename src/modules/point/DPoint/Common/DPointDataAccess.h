#pragma once

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

class DPData;

// Return a borrowed, topology-validated input only after successful acquisition.
// Empty data is valid; acquisition/type/topology failures are not empty input.
// The supplying data block must remain alive while the pointer is consumed.
MStatus getValidInputDP(const MObject& object, MDataBlock& data, const DPData*& inputData);
// A successful null view means only absent, unconnected data. Stored valid
// data is accepted even without a connection; connected null data and every
// acquisition/binding/topology failure retain an error status.
MStatus getOptionalInputDP(const MObject& node, const MObject& attribute,
                           MDataBlock& data, const DPData*& inputData);
const DPData* getInputDP(const MObject& object, MDataBlock& data);
const DPData* getInputDPfromPlug(const MPlug& plug, MObject& object);
const DPData* getInputDPfromHandle(MDataHandle handle);
