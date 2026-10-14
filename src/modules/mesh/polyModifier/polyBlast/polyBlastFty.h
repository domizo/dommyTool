//
//  polyBlastFty.h
//  dommyTool
//
//  Created by yuzo on 1/30/13.
//
//

#ifndef __dommyTool__polyBlastFty__
#define __dommyTool__polyBlastFty__

#include "basePolyModifierFty.h"

// General Includes
//
#include <maya/MObject.h>
#include <maya/MIntArray.h>

namespace dommy {
namespace poly_blast {

constexpr double kDefaultDeletePercent = 50.0;

} // namespace poly_blast
} // namespace dommy

class polyBlastFty final : public basePolyModifierFty {
public:
	polyBlastFty() = default;
	~polyBlastFty() override = default;
	
	void		setMesh(const MObject& mesh);
	void		setFaces(const MIntArray& faceIds);
	// polyModifierFty inherited methods
	//
	MStatus		doIt() override;
	
private:
    // The mesh handle is only retained for one command/node evaluation.
	MObject		fMesh;
	
	// Selected Faces
	//
	MIntArray	pSelFaces;
};


#endif /* defined(__dommyTool__polyBlastFty__) */
