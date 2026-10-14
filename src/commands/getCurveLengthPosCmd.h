#ifndef GETCURVELENGTHPOSCMD_H
#define GETCURVELENGTHPOSCMD_H


#include <maya/MPxCommand.h>
#include <maya/MArgList.h>
#include <maya/MSyntax.h>
#include <maya/MString.h>
#include <maya/MPoint.h>



class getCurveLengthPosCmd final : public MPxCommand
{
public:
		getCurveLengthPosCmd();
		~getCurveLengthPosCmd() override;
		
		MStatus doIt ( const MArgList& args ) override;
		
		static void* creator();
		
		static MSyntax newSyntax();
    static MStatus buildSyntax(MSyntax& syntax);
		MStatus parseArgs(const MArgList& args);
		
private:
		double lengthPos;
		MString curve;
};




#endif
