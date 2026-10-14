#pragma once

#include <ai_array.h>
#include <ai_node_entry.h>
#include <ai_nodes.h>
#include <ai_params.h>

#include <cstring>
#include <stdexcept>

namespace dommy {
namespace arnold {
namespace parameter_detail {

struct Spec { int type; int category; int arrayType; };

// Only the project's fixed legacy declaration forms are accepted. Keep POINT2
// spelling for Arnold 5, while metadata uses the VECTOR2 type identifier.
inline Spec parse(const char* declaration) {
    if (!declaration) throw std::invalid_argument("Missing Arnold parameter declaration.");
    int category = 0;
    const char* type = nullptr;
    if (std::strncmp(declaration, "constant ", 9U) == 0) {
        category = AI_USERDEF_CONSTANT; type = declaration + 9U;
    } else if (std::strncmp(declaration, "uniform ", 8U) == 0) {
        category = AI_USERDEF_UNIFORM; type = declaration + 8U;
    } else if (std::strncmp(declaration, "varying ", 8U) == 0) {
        category = AI_USERDEF_VARYING; type = declaration + 8U;
    } else {
        throw std::invalid_argument("Unsupported Arnold parameter category.");
    }
    int storage = AI_TYPE_UNDEFINED;
    if (std::strcmp(type, "INT") == 0) storage = AI_TYPE_INT;
    else if (std::strcmp(type, "UINT") == 0) storage = AI_TYPE_UINT;
    else if (std::strcmp(type, "FLOAT") == 0) storage = AI_TYPE_FLOAT;
    else if (std::strcmp(type, "POINT2") == 0) storage = AI_TYPE_VECTOR2;
    else if (std::strcmp(type, "VECTOR") == 0) storage = AI_TYPE_VECTOR;
    else if (std::strcmp(type, "STRING") == 0) storage = AI_TYPE_STRING;
    else if (category == AI_USERDEF_CONSTANT && std::strcmp(type, "ARRAY STRING") == 0)
        return {AI_TYPE_ARRAY, category, AI_TYPE_STRING};
    else throw std::invalid_argument("Unsupported Arnold parameter type.");
    return {storage, category, AI_TYPE_UNDEFINED};
}

inline bool isOwnershipTag(const char* name) {
    return name && (std::strcmp(name, "dInstMeshTag") == 0 ||
        std::strcmp(name, "dInstObjTag") == 0 || std::strcmp(name, "dlightTag") == 0);
}

inline void validateName(const AtNodeEntry* entry, const char* name, bool ownership) {
    if (!entry) throw std::runtime_error("Missing Arnold node entry for parameter validation.");
    if (!name || !*name) throw std::invalid_argument("Empty Arnold user parameter name.");
    if (ownership != isOwnershipTag(name))
        throw std::invalid_argument("Arnold ownership tag name is reserved.");
    if (AiNodeEntryLookUpParameter(entry, name))
        throw std::invalid_argument("Arnold user parameter conflicts with a built-in parameter.");
}

inline bool matches(const AtUserParamEntry* parameter, const Spec& expected) {
    return parameter && AiUserParamGetType(parameter) == expected.type &&
        AiUserParamGetCategory(parameter) == expected.category &&
        (expected.type != AI_TYPE_ARRAY ||
         AiUserParamGetArrayType(parameter) == expected.arrayType);
}

inline void require(AtNode* node, const char* name, const char* declaration, bool ownership) {
    if (!node) throw std::invalid_argument("Missing Arnold node for parameter declaration.");
    const Spec expected = parse(declaration);
    validateName(AiNodeGetNodeEntry(node), name, ownership);
    const AtUserParamEntry* existing = AiNodeLookUpUserParameter(node, name);
    if (existing) {
        if (!matches(existing, expected))
            throw std::invalid_argument("Incompatible existing Arnold user parameter declaration.");
        return; // A normal update/duplicate selection reuses the same schema.
    }
    if (!AiNodeDeclare(node, name, declaration))
        throw std::runtime_error("Arnold user parameter declaration failed.");
    if (!matches(AiNodeLookUpUserParameter(node, name), expected))
        throw std::runtime_error("Arnold user parameter declaration could not be verified.");
}

} // namespace parameter_detail

// Exceptions are contained by the existing translator/compute callback guards.
// Admission precedes every value write/array-owner release. No graph rollback
// is promised for mutations made earlier in an update.
inline void requireUserParameter(AtNode* node, const char* name, const char* declaration) {
    parameter_detail::require(node, name, declaration, false);
}

inline void requireOwnershipTag(AtNode* node, const char* name) {
    parameter_detail::require(node, name, "constant STRING", true);
}

// Reading ownership must not interpret absent, array, numeric or varying data
// as a scalar UUID string. Invalid/foreign schemas are not deletion authority;
// metadata-query exceptions propagate to the existing callback boundary.
inline bool hasOwnershipTagSchema(const AtNode* node, const char* name) {
    return node && parameter_detail::isOwnershipTag(name) &&
        parameter_detail::matches(AiNodeLookUpUserParameter(node, name),
            {AI_TYPE_STRING, AI_USERDEF_CONSTANT, AI_TYPE_UNDEFINED});
}

// A null destination is the instancer's existing validation-only pass, before
// previous-node retirement. It admits names without creating a renderer node.
inline void requireInstanceUserParameter(AtNode* node, const char* name, const char* declaration) {
    if (node) {
        requireUserParameter(node, name, declaration);
    } else {
        (void)parameter_detail::parse(declaration);
        parameter_detail::validateName(AiNodeEntryLookUp("ginstance"), name, false);
    }
}

// This one renderer-owned control may be a built-in in an Arnold version.
// Never grant this exception to arbitrary user-selected attribute names.
inline void requireVelocityGridsParameter(AtNode* node) {
    if (!node) throw std::invalid_argument("Missing Arnold volume node.");
    const AtNodeEntry* entry = AiNodeGetNodeEntry(node);
    if (!entry) throw std::runtime_error("Missing Arnold volume node entry.");
    const AtParamEntry* builtin = AiNodeEntryLookUpParameter(entry, "velocity_grids");
    if (builtin) {
        if (AiParamGetType(builtin) != AI_TYPE_ARRAY)
            throw std::invalid_argument("Incompatible Arnold velocity-grids parameter.");
        const AtArray* value = AiNodeGetArray(node, "velocity_grids");
        // An unset built-in array has no element metadata. This fixed control
        // is assigned a STRING array by the caller; existing values must agree.
        if (value && AiArrayGetType(value) != AI_TYPE_STRING)
            throw std::invalid_argument("Incompatible Arnold velocity-grid array type.");
        return;
    }
    requireUserParameter(node, "velocity_grids", "constant ARRAY STRING");
}

} // namespace arnold
} // namespace dommy
