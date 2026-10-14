#pragma once

#include <maya/MArgList.h>
#include <maya/MDGModifier.h>
#include <maya/MObjectHandle.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

class setParticleRGBCmd final : public MPxCommand {
	public:
		setParticleRGBCmd();
		~setParticleRGBCmd() override;
		
		MStatus doIt(const MArgList& args) override;
        MStatus redoIt() override;
        MStatus undoIt() override;
        bool isUndoable() const override { return planReady; }
		
		static void* creator();
		
		static MSyntax newSyntax();
    static MStatus buildSyntax(MSyntax& syntax);
		MStatus parseArgs(const MArgList& args);
		
	private:
        MStatus applyColors(MVectorArray& colors);

		bool onShadow;
		bool onReuseMap;
        MString cameraName;
        MObjectHandle particleHandle;
        MDGModifier attributeModifier;
        MVectorArray originalColors;
        MVectorArray targetColors;
        bool hadRgbAttribute;
        bool attributeEditsQueued;
        bool attributesApplied;
        bool planReady;
        bool applied;
};
