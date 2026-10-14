#ifndef shapeFillCmd_H
#define shapeFillCmd_H


#include <maya/MPxCommand.h>
#include <maya/MArgList.h>
#include <maya/MSyntax.h>
#include <maya/MString.h>
#include <maya/MObjectArray.h>
#include <maya/MDagModifier.h>




class shapeFillCmd final : public MPxCommand
{
	public:
		shapeFillCmd();
		~shapeFillCmd() override;
		
		MStatus doIt ( const MArgList& args ) override;
		MStatus undoIt() override;
		MStatus redoIt() override;
		bool isUndoable() const override { return true; }
		
		static void* creator();
		
		static MSyntax newSyntax();
    static MStatus buildSyntax(MSyntax& syntax);
		MStatus parseArgs(const MArgList& args);
		
	private:
		int density;
		double thickness;
		double noise;
		MString camName;
		MString sgName;
		bool onColor;
		bool onShadow;
		bool onReuseMap;
		MObjectArray createdParticleShapes;
		MDagModifier deleteModifier;
		bool deleteOperationsQueued;
		unsigned int queuedDeleteCount;
		bool particlesDeleted;
		
		
};









#endif
