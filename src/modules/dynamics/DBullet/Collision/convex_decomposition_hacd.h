// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/convex_decomposition_hacd.h

//
//  convex_decomposition_hacd.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//

#pragma once

class btCompoundShape;

class convex_decomposition_hacd
{
public:
    convex_decomposition_hacd() = default;
    ~convex_decomposition_hacd() = default;

	convex_decomposition_hacd(const convex_decomposition_hacd&) = delete;
	convex_decomposition_hacd& operator=(const convex_decomposition_hacd&) = delete;
    
    // The caller owns the returned compound, which in turn owns its children.
    // Invalid/failed decomposition returns nullptr; exceptions publish nothing.
    btCompoundShape* ConvexDecomp(int numVertices, const float* vertices,
                                int numIndices, const unsigned int* indices);
    
};
