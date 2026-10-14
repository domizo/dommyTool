#pragma once
#include <maya/MStatus.h>
#include "common/utility/DAbcRootAdmission.h"
#include "common/file/DAtomicFile.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef DOMMY_SDK_CHECK
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MGlobal.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MStringArray.h>
#else
std::size_t queries = 0U;
int failAt = -1, faultKind = 0;
MStatus query(MStatus* output = nullptr) {
    const bool fail = int(queries++) == failAt;
    if (fail && faultKind == 2) throw std::bad_alloc();
    if (fail && faultKind == 3) throw std::runtime_error("root API failure");
    const MStatus status = fail ? MS::kFailure : MS::kSuccess;
    if (output) *output = status;
    return status;
}
struct MStringArray;
struct MString {
    std::string text;
    MString(const char* value = "") : text(value) {}
    const char* asChar() const { return text.c_str(); }
    unsigned int length() const { return text.size(); }
    MString operator+(const char* value) const { return MString((text + value).c_str()); }
    MString& operator+=(const MString& value) { text += value.text; return *this; }
    MStatus split(char separator, MStringArray& output) const;
};
struct MStringArray {
    std::vector<MString> values;
    unsigned int length() const { return values.size(); }
    const MString& operator[](unsigned int index) const { return values.at(index); }
};
MStatus MString::split(char separator, MStringArray& output) const {
    const auto status = query(); if (!status) return status;
    std::size_t start = 0U;
    for (std::size_t i = 0U; i <= text.size(); ++i) {
        if (i != text.size() && text[i] != separator) continue;
        if (i != start) output.values.emplace_back(text.substr(start, i - start).c_str());
        start = i + 1U;
    }
    return status;
}
namespace MFn { enum Type { kDagNode }; }
struct MObject {
    bool dag = true;
    std::string name = "node";
    static const MObject kNullObj;
    bool hasFn(MFn::Type) const { return dag; }
};
const MObject MObject::kNullObj{};
struct MDagPath {
    std::string name;
    MDagPath(const char* value = "|root") : name(value) {}
    unsigned int length(MStatus* status = nullptr) const {
        query(status);
        return std::count(name.begin(), name.end(), '|');
    }
    MString fullPathName(MStatus* status = nullptr) const { query(status); return name.c_str(); }
    MObject node(MStatus* status = nullptr) const {
        query(status); MObject object; object.name = name.substr(name.find_last_of('|') + 1U); return object;
    }
    MStatus pop(unsigned int count = 1U) {
        const auto status = query();
        if (!status) return status;
        while (count--) {
            const auto position = name.find_last_of('|');
            if (position == std::string::npos) return MS::kFailure;
            name.resize(position);
        }
        return status;
    }
    bool operator==(const MDagPath& other) const { return name == other.name; }
};
struct MFnDependencyNode {
    MObject object;
    MFnDependencyNode(const MObject& value, MStatus* status = nullptr) : object(value) { query(status); }
    MString name(MStatus* status = nullptr) const { query(status); return object.name.c_str(); }
};
struct MSelectionList {
    std::vector<MDagPath> paths;
    std::vector<bool> dagItems;
    unsigned int countOverride = 0U;
    unsigned int length() const { return countOverride ? countOverride : paths.size(); }
    MStatus getDagPath(unsigned int index, MDagPath& path) const {
        const auto status = query();
        path = paths.at(index);
        if (!status) return status;
        if (!dagItems.at(index)) return MS::kFailure;
        return status;
    }
    MStatus getDependNode(unsigned int index, MObject& object) const {
        const auto status = query(); object.dag = dagItems.at(index); return status;
    }
    MStatus add(const MDagPath& path, const MObject&, bool) {
        const auto status = query(); if (status) { paths.push_back(path); dagItems.push_back(true); }
        return status;
    }
};
struct MGlobal { static void displayError(const MString&) {} static void displayWarning(const MString&) {} };
#endif
namespace ddata_export_util {
void requireExportQuery(const MStatus&, const char*);
std::string exportDagPathName(const MDagPath&);
unsigned int exportDagDepth(const MDagPath&);
bool readExportDagSelectionItem(const MSelectionList&, unsigned int, MDagPath&);
MString stripNamespaces(const MString&, unsigned int);
MString exportObjectName(const MFnDependencyNode&, unsigned int);
#include "root_declarations.inc"
using ShapeSet = std::set<MDagPath, cmpDag>;
void insertExportRoot(ShapeSet&, const MDagPath&, dommy::exporting::RootAdmission&);
void prepareExportSelection(MSelectionList&, ShapeSet&, const MSelectionList&,
                            dommy::exporting::RootAdmission&);
bool isAncestorDescendentRelationship(const MDagPath&, const MDagPath&);
void requireUniqueExportRootNames(const ShapeSet&, unsigned int, dommy::exporting::RootAdmission&);
}
struct RootJobArgs {
    ddata_export_util::ShapeSet dagPaths;
    std::shared_ptr<dommy::exporting::RootAdmission> exportRootAdmission =
        std::make_shared<dommy::exporting::RootAdmission>();
};
