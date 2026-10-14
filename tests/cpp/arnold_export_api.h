#pragma once

// Explicit Arnold 5-shaped test boundary, not a replacement SDK or runtime.
#include <cstdint>
#include <cstddef>
#include "arnold_parameter_api.h"

struct AtArray;
struct AtNode;
struct AtNodeIterator;
struct AtUserParamIterator;
struct AtVector { float x, y, z; };
struct AtVector2 { float x, y; };
struct AtMatrix { float data[4][4]; };
enum { AI_SUCCESS = 0, AI_NODE_ALL = 0xffff, AI_LOG_ALL = 0xffff };
void AiBegin();
void AiEnd();
int AiASSWrite(const char*, int, bool, bool);
void AiMsgError(const char*, ...);
void AiMsgSetLogFileName(const char*);
void AiMsgSetConsoleFlags(int);
AtNode* AiNode(const char*);
bool AiNodeDestroy(AtNode*);
void AiNodeIteratorDestroy(AtNodeIterator*);
void AiUserParamIteratorDestroy(AtUserParamIterator*);
AtArray* AiArrayAllocate(std::uint32_t, std::uint8_t, int);
void AiArrayDestroy(AtArray*);
void* AiArrayMap(AtArray*);
void AiArrayUnmap(AtArray*);
void AiNodeSetArray(AtNode*, const char*, AtArray*);
void AiNodeSetStr(AtNode*, const char*, const char*);
void AiNodeSetInt(AtNode*, const char*, int);
void AiNodeSetBool(AtNode*, const char*, bool);
void AiNodeSetByte(AtNode*, const char*, unsigned char);
void AiNodeSetFlt(AtNode*, const char*, float);
void AiNodeSetVec2(AtNode*, const char*, float, float);
void AiNodeSetVec(AtNode*, const char*, float, float, float);
void AiNodeSetRGB(AtNode*, const char*, float, float, float);
void AiNodeSetRGBA(AtNode*, const char*, float, float, float, float);
void AiNodeSetMatrix(AtNode*, const char*, const AtMatrix&);
bool AiNodeDeclare(AtNode*, const char*, const char*);
bool AiNodeLink(AtNode*, const char*, AtNode*);
bool AiArraySetInt(AtArray*, std::size_t, int);
bool AiArraySetFlt(AtArray*, std::size_t, float);
bool AiArraySetVec(AtArray*, std::size_t, AtVector);
bool AiArraySetVec2(AtArray*, std::size_t, AtVector2);
bool AiArraySetPtr(AtArray*, std::size_t, void*);
