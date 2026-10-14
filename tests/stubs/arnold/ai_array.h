#pragma once

struct AtArray {
    bool mapSucceeds;
    unsigned char storage;
};

void AiArrayDestroy(AtArray* array);
void* AiArrayMap(AtArray* array);
void AiArrayUnmap(AtArray* array);
