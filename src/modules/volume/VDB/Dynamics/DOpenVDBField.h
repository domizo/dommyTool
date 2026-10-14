//
//  DOpenVDBField.h
//  dommyTool
//
//  Created by yuzo on 2017/12/24.
//


#pragma once

#include <maya/MVectorArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MPxFieldNode.h>
#include <maya/MTypeId.h>

class DOpenVDBField final : public MPxFieldNode
{
public:
    DOpenVDBField() = default;
    ~DOpenVDBField() override = default;
    
    static void* creator();
    static MStatus initialize();
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    void draw(M3dView& view, const MDagPath& path,
              M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;
    static MTypeId id;
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;

private:
    MStatus applyNoMaxDist(MDataBlock& block,
                          const MVectorArray& points,
                          const MVectorArray& velocities,
                          const MDoubleArray& masses,
                          MVectorArray& outputForce);
};
