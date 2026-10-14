//
//  DCurveTranslatorAi.hpp
//  dommyTool
//
//  Created by yuzo on 3/23/17.
//
//


#pragma once


//#define _DARWIN
#include <platform/Platform.h>
//#define DLLEXPORT __attribute__ ((visibility("default")))
#include <translators/DagTranslator.h>
#include <translators/NodeTranslator.h>
#include <translators/shape/ShapeTranslator.h>

class DCurveTranslatorAi : public CShapeTranslator
//class DCurveTranslatorAi : public CNodeTranslator
{
public:
	
	AtNode* CreateArnoldNodes();
	void Export(AtNode*);
	void Update(AtNode*);
    void ExportMotion(AtNode* atNode);
	static void* creator();
	
	void NodeChanged(MObject& node, MPlug& plug);
    virtual bool RequiresMotionData();
//    virtual void ExportUserAttrs( AtNode *node );
    
};


