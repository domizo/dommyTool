//
//  DVDBTranslatorAi.hpp
//  dommyTool
//
//  Created by yuzo on 3/26/17.
//
//




#pragma once

#include <translators/shape/ShapeTranslator.h>
#include <string>



class DVDBTranslatorAi : public CShapeTranslator {
public:
	~DVDBTranslatorAi() override;
	
	AtNode* CreateArnoldNodes();
	void Export(AtNode*);
	void Update(AtNode*);
	
	static void* creator();
	
	void NodeChanged(MObject& node, MPlug& plug);

private:
	std::string m_cacheFilePath;
};


