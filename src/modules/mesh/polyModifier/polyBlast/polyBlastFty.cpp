//
//  polyBlastFty.cpp
//  dommyTool
//
//  Created by yuzo on 1/30/13.
//
//

#include "polyBlastFty.h"

void polyBlastFty::setMesh(const MObject& mesh) {
	fMesh = mesh;
}

void polyBlastFty::setFaces(const MIntArray& faceIds)
{
	pSelFaces = faceIds;
}
