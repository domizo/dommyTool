#pragma once
#include <maya/MPxNode.h>

// Existing Maya mesh attribute identities are retained by derived nodes.
class basePolyModifierNode : public MPxNode {
public:
    basePolyModifierNode();
    ~basePolyModifierNode() override;
    static MObject inMesh;
    static MObject outMesh;
};


