//
//  DPointTranslatorAi.hpp
//  dommyTool
//
//  Created by yuzo on 3/26/17.
//
//

#pragma once

#include "platform/Platform.h"

#include "translators/DagTranslator.h"
#include "translators/NodeTranslator.h"
#include "translators/shape/ShapeTranslator.h"

class DPointTranslatorAi : public CShapeTranslator
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
