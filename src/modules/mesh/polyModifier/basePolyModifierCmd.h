#pragma once

#include <maya/MPxCommand.h>
#include <maya/MDagPath.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <memory>

// Project-owned edit transaction with the existing subclass-facing interface.
class basePolyModifierCmd : public MPxCommand {
public:
    basePolyModifierCmd();
    ~basePolyModifierCmd() override;
protected:
    void setMeshNode(MDagPath);
    MDagPath getMeshNode() const;
    void setModifierNodeType(MTypeId);
    void setModifierNodeName(MString);
    MTypeId getModifierNodeType() const;
    MString getModifierNodeName() const;
    virtual MStatus initModifierNode(MObject);
    virtual MStatus directModifier(MObject);
    MStatus doModifyPoly();
    MStatus redoModifyPoly();
    MStatus undoModifyPoly();
    MStatus cancelModifyPoly();
    bool canStartPolyEdit() const;
    bool isPolyEditApplied() const;
private:
    struct State;
    std::unique_ptr<State> edit_;
};
