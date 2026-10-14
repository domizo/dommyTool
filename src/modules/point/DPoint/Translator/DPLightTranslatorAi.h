//
//  DPLightTranslatorAi.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/27.
//
//

#pragma once


#include "translators/light/LightTranslator.h"
#include "integrations/arnold/DArnoldResources.h"

#include <array>
#include <string>

class DPLightTranslatorAi : public CLightTranslator
{
public:
    
    AtNode* CreateArnoldNodes();
    void Export(AtNode*);
    void Update(AtNode*);
    static void* creator();
    
    void NodeChanged(MObject& node, MPlug& plug);

private:
    dommy::arnold::NodeBatch pendingNodes_;
    struct Parameters;

    template <typename Value>
    bool readParameter(const char* name, Value& value);
    bool readVector2Parameter(
        const char* name, std::array<double, 2>& value);
    bool readStringParameter(const char* name, std::string& value);
    bool readParameters(Parameters& parameters);
};
