#pragma once
#include "arnold_parameter_api.h"
#include <map>
#include <string>
#include <stdexcept>
#include <cstring>

struct AtUserParamEntry { int type, category, arrayType; };
struct AtParamEntry { int type; };
struct AtNodeEntry { std::map<std::string,AtParamEntry> builtins; };
namespace parameterFixture {
std::map<const AtNode*,std::map<std::string,AtUserParamEntry>> users;
AtNodeEntry entry;
bool missingEntry=false, failDeclaration=false, omitDeclaredEntry=false;
bool failedDeclarationWithEntry=false, queryBadAlloc=true;
int wrongDeclaredType=-1, failDeclarationAt=-1, throwQueryAt=-1;
unsigned declarations=0U, queries=0U;
AtArray* builtinArray=nullptr;
int builtinArrayType=AI_TYPE_STRING;
void query() {
    if (int(queries++)==throwQueryAt) {
        if(queryBadAlloc)throw std::bad_alloc();
        throw std::runtime_error("injected Arnold metadata query failure");
    }
}
void reset() {
    users.clear(); entry.builtins.clear();
    for (const char* name : {"name","visibility","matrix","radius","points","color"})
        entry.builtins[name]={AI_TYPE_FLOAT};
    missingEntry=failDeclaration=omitDeclaredEntry=false;
    failedDeclarationWithEntry=false;queryBadAlloc=true;
    wrongDeclaredType=failDeclarationAt=throwQueryAt=-1;
    declarations=queries=0U; builtinArray=nullptr; builtinArrayType=AI_TYPE_STRING;
}
AtUserParamEntry descriptor(const char* text) {
    const std::string declaration(text);
    const int category=declaration.compare(0,8,"constant")==0?AI_USERDEF_CONSTANT:
        declaration.compare(0,7,"uniform")==0?AI_USERDEF_UNIFORM:AI_USERDEF_VARYING;
    int type=AI_TYPE_UNDEFINED,arrayType=AI_TYPE_UNDEFINED;
    if (declaration.find("ARRAY STRING")!=std::string::npos) {type=AI_TYPE_ARRAY;arrayType=AI_TYPE_STRING;}
    else if (declaration.find("UINT")!=std::string::npos) type=AI_TYPE_UINT;
    else if (declaration.find("INT")!=std::string::npos && declaration.find("POINT2")==std::string::npos) type=AI_TYPE_INT;
    else if (declaration.find("FLOAT")!=std::string::npos) type=AI_TYPE_FLOAT;
    else if (declaration.find("POINT2")!=std::string::npos) type=AI_TYPE_VECTOR2;
    else if (declaration.find("VECTOR")!=std::string::npos) type=AI_TYPE_VECTOR;
    else if (declaration.find("STRING")!=std::string::npos) type=AI_TYPE_STRING;
    return {type,category,arrayType};
}
bool declare(AtNode* node,const char* name,const char* text) {
    const unsigned call=declarations++;
    if (failDeclaration || int(call)==failDeclarationAt || entry.builtins.count(name) || users[node].count(name)) {
        if(failedDeclarationWithEntry)users[node][name]=descriptor(text);
        return false;
    }
    if (!omitDeclaredEntry) {
        auto value=descriptor(text);
        if (wrongDeclaredType>=0) value.type=wrongDeclaredType;
        users[node][name]=value;
    }
    return true;
}
}
inline const AtNodeEntry* AiNodeGetNodeEntry(const AtNode*) {
    parameterFixture::query();return parameterFixture::missingEntry?nullptr:&parameterFixture::entry;
}
inline const AtNodeEntry* AiNodeEntryLookUp(const char*) { return AiNodeGetNodeEntry(nullptr); }
inline const AtParamEntry* AiNodeEntryLookUpParameter(const AtNodeEntry* entry,const char* name) {
    parameterFixture::query(); if (!entry) return nullptr;
    const auto found=entry->builtins.find(name);return found==entry->builtins.end()?nullptr:&found->second;
}
inline const AtUserParamEntry* AiNodeLookUpUserParameter(const AtNode* node,const char* name) {
    parameterFixture::query();const auto owner=parameterFixture::users.find(node);
    if (owner==parameterFixture::users.end())return nullptr;
    const auto found=owner->second.find(name);return found==owner->second.end()?nullptr:&found->second;
}
inline std::uint8_t AiParamGetType(const AtParamEntry* entry) {parameterFixture::query();return std::uint8_t(entry->type);}
inline std::uint8_t AiUserParamGetType(const AtUserParamEntry* entry) {parameterFixture::query();return std::uint8_t(entry->type);}
inline std::uint8_t AiUserParamGetArrayType(const AtUserParamEntry* entry) {parameterFixture::query();return std::uint8_t(entry->arrayType);}
inline std::uint8_t AiUserParamGetCategory(const AtUserParamEntry* entry) {parameterFixture::query();return std::uint8_t(entry->category);}
inline AtArray* AiNodeGetArray(const AtNode*,const char*) {parameterFixture::query();return parameterFixture::builtinArray;}
inline std::uint8_t AiArrayGetType(const AtArray*) {parameterFixture::query();return std::uint8_t(parameterFixture::builtinArrayType);}
