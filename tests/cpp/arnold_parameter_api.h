#pragma once

// Explicit legacy Arnold parameter-query boundary, not a native SDK.
#include <cstdint>
struct AtNode;
struct AtArray;
struct AtNodeEntry;
struct AtParamEntry;
struct AtUserParamEntry;
enum { AI_TYPE_BYTE=0, AI_TYPE_INT=1, AI_TYPE_UINT=2, AI_TYPE_FLOAT=4,
       AI_TYPE_VECTOR=7, AI_TYPE_VECTOR2=9, AI_TYPE_STRING=10, AI_TYPE_NODE=12,
       AI_TYPE_ARRAY=13, AI_TYPE_UNDEFINED=255,
       AI_USERDEF_CONSTANT=1, AI_USERDEF_UNIFORM=2, AI_USERDEF_VARYING=3 };
const AtNodeEntry* AiNodeGetNodeEntry(const AtNode*);
const AtNodeEntry* AiNodeEntryLookUp(const char*);
const AtParamEntry* AiNodeEntryLookUpParameter(const AtNodeEntry*, const char*);
const AtUserParamEntry* AiNodeLookUpUserParameter(const AtNode*, const char*);
std::uint8_t AiParamGetType(const AtParamEntry*);
std::uint8_t AiUserParamGetType(const AtUserParamEntry*);
std::uint8_t AiUserParamGetArrayType(const AtUserParamEntry*);
std::uint8_t AiUserParamGetCategory(const AtUserParamEntry*);
AtArray* AiNodeGetArray(const AtNode*, const char*);
std::uint8_t AiArrayGetType(const AtArray*);
bool AiNodeDeclare(AtNode*, const char*, const char*);
