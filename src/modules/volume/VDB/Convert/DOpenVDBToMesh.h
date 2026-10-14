//
//  DOpenVDBToMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#pragma once

#include <maya/MPxNode.h>

class DOpenVDBToMesh final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    static MObject aVdbInput;

    static MObject aIsovalue;
    static MObject aAdaptivity;
    static MObject aVdbAllGridNames;
    static MObject aVdbSelectedGridNames;

    static MObject outMesh;
    static MObject outDM;
};
