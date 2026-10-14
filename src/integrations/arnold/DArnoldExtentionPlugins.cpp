//
//  DArnoldExtentionPlugins.cpp
//  dommyTool
//
//  Created by yuzo on 3/24/17.
//
//

#include "DMayaHeaders.h"

#include "DPointTranslatorAi.h"
#include "DCurveDataHeaders.h"

#include "DCurveDisplay.h"
#include "DCurveShape.h"

#include "DCurveTranslatorAi.h"
#include "DMeshTranslatorAi.h"
#include "DVDBTranslatorAi.h"

#include "DPLightTranslatorAi.h"
#include "DPInstancerTranslatorAi.h"

#include "extension/Extension.h"

extern "C" {

DLLEXPORT void initializeExtension(CExtension& plugin) {

    plugin.RegisterTranslator("DPointDisplay", "", DPointTranslatorAi::creator);

    plugin.RegisterNode("DCurveShape", DCurveShape::id, DCurveShape::creator,
                        DCurveShape::initialize, MPxNode::kSurfaceShape);

    plugin.RegisterTranslator("DCurveDisplay", "", DCurveTranslatorAi::creator);

    plugin.RegisterTranslator("DMeshDisplay", "", DMeshTranslatorAi::creator);

    plugin.RegisterTranslator("DOpenVDBDisplayVP2", "", DVDBTranslatorAi::creator);

    plugin.RegisterTranslator("DPLightGen", "", DPLightTranslatorAi::creator);
    plugin.RegisterTranslator("DPInstancer", "", DPInstancerTranslatorAi::creator);
}

DLLEXPORT void deinitializeExtension(CExtension& plugin) {
    (void)plugin;
}
}
