//
//  DMeshTranslatorAi.hpp
//  dommyTool
//
//  Created by yuzo on 5/5/17.
//
//




#pragma once



#include "platform/Platform.h"


#include "translators/DagTranslator.h"
#include "translators/NodeTranslator.h"
#include "translators/shape/ShapeTranslator.h"

class DMeshTranslatorAi : public CShapeTranslator
{
public:
	
	AtNode* CreateArnoldNodes();
	void Export(AtNode*);
	void Update(AtNode*);
	void ExportMotion(AtNode* atNode);
	static void* creator();
	
	void NodeChanged(MObject& node, MPlug& plug);
	virtual bool RequiresMotionData();
	
	
};



