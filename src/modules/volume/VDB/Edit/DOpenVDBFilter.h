//
//  DOpenVDBFilter.hpp
//  dommyTool
//
//  Created by yuzo on 2016/04/18.
//
//

#ifndef DOpenVDBFilter_h
#define DOpenVDBFilter_h


#include <maya/MPxNode.h>


class DOpenVDBFilter final : public MPxNode
{
public:
    DOpenVDBFilter() {}
    ~DOpenVDBFilter() override = default;
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbOutput;
    static MObject aVdbSelectedGridNames;
    static MObject aFilter;
    static MObject aRadius;
    static MObject aOffset;
    static MObject aIterations;
};



#endif /* DOpenVDBFilter_h */
