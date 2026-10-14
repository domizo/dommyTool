#pragma once

struct AtNode {};
struct AtUserParamIterator {};

bool AiNodeDestroy(AtNode* node);
AtNode* AiNode(const char* type);
void AiUserParamIteratorDestroy(AtUserParamIterator* iterator);
