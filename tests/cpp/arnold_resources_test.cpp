#include <cassert>
#include <utility>

#include "integrations/arnold/DArnoldResources.h"

namespace {

int gDestroyedNodes = 0;
int gDestroyedArrays = 0;
int gDestroyedNodeIterators = 0;
int gDestroyedUserParamIterators = 0;
int gMappedArrays = 0;
int gUnmappedArrays = 0;

}  // namespace

void AiMsgError(const char*, ...) {}
bool AiNodeDestroy(AtNode* node) {
    ++gDestroyedNodes;
    delete node;
    return true;
}

void AiArrayDestroy(AtArray* array) {
    ++gDestroyedArrays;
    delete array;
}

void* AiArrayMap(AtArray* array) {
    ++gMappedArrays;
    return array->mapSucceeds ? &array->storage : nullptr;
}

void AiArrayUnmap(AtArray*) {
    ++gUnmappedArrays;
}

void AiNodeIteratorDestroy(AtNodeIterator* iterator) {
    ++gDestroyedNodeIterators;
    delete iterator;
}

void AiUserParamIteratorDestroy(AtUserParamIterator* iterator) {
    ++gDestroyedUserParamIterators;
    delete iterator;
}

int main() {
    dommy::arnold::requireArrayWrite(true);
    bool rejectedWrite = false;
    try {
        dommy::arnold::requireArrayWrite(false);
    } catch (const std::runtime_error&) {
        rejectedWrite = true;
    }
    assert(rejectedWrite);
    {
        dommy::arnold::NodeOwner first(new AtNode);
        dommy::arnold::NodeOwner second(new AtNode);
        second = std::move(first);
        assert(!first);
        assert(second);
        assert(gDestroyedNodes == 1);

        AtNode* const released = second.release();
        assert(!second);
        AiNodeDestroy(released);
        assert(gDestroyedNodes == 2);

        second.reset(new AtNode);
        second.reset(second.get());
        assert(gDestroyedNodes == 2);
    }
    assert(gDestroyedNodes == 3);

    {
        dommy::arnold::ArrayOwner first(new AtArray{true, 0});
        dommy::arnold::ArrayOwner second(new AtArray{true, 0});
        second = std::move(first);
        assert(!first);
        assert(second);
        assert(gDestroyedArrays == 1);

        AtArray* const released = second.release();
        AiArrayDestroy(released);
        assert(gDestroyedArrays == 2);

        second.reset(new AtArray{true, 0});
        second.reset(second.get());
        assert(gDestroyedArrays == 2);

        {
            dommy::arnold::ArrayMapGuard mapped(second.get());
            assert(mapped);
            assert(mapped.data() == &second.get()->storage);
        }
        assert(gMappedArrays == 1);
        assert(gUnmappedArrays == 1);

        AtArray failedMap{false, 0};
        {
            dommy::arnold::ArrayMapGuard mapped(&failedMap);
            assert(!mapped);
        }
        assert(gMappedArrays == 2);
        assert(gUnmappedArrays == 1);
    }
    assert(gDestroyedArrays == 3);

    {
        dommy::arnold::NodeIteratorPtr iterator(new AtNodeIterator);
        dommy::arnold::UserParamIteratorPtr userIterator(
            new AtUserParamIterator);
    }
    assert(gDestroyedNodeIterators == 1);
    assert(gDestroyedUserParamIterators == 1);

    return 0;
}
