#pragma once

#include <ai_array.h>
#include <ai_msg.h>
#include <ai_nodes.h>
#include <ai_universe.h>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include "common/utility/DGenerationLimits.h"

namespace dommy {
namespace arnold {

// Element setters return false on failure and may leave allocated storage
// unwritten. Stop before releasing the array to a node. Existing translator
// and compute boundaries contain the exception and retire local resources.
inline void requireArrayWrite(bool success)
{
    if (!success) throw std::runtime_error("Unable to write an Arnold array element.");
}

struct NodeIteratorDeleter {
    void operator()(AtNodeIterator* iterator) const noexcept {
        if (iterator != nullptr) {
            AiNodeIteratorDestroy(iterator);
        }
    }
};

struct UserParamIteratorDeleter {
    void operator()(AtUserParamIterator* iterator) const noexcept {
        if (iterator != nullptr) {
            AiUserParamIteratorDestroy(iterator);
        }
    }
};

typedef std::unique_ptr<AtNodeIterator, NodeIteratorDeleter> NodeIteratorPtr;
typedef std::unique_ptr<AtUserParamIterator, UserParamIteratorDeleter>
    UserParamIteratorPtr;

class NodeOwner {
  public:
    explicit NodeOwner(AtNode* node = nullptr) noexcept : node_(node) {}
    ~NodeOwner() noexcept { retire(); }

    NodeOwner(const NodeOwner&) = delete;
    NodeOwner& operator=(const NodeOwner&) = delete;

    NodeOwner(NodeOwner&& other) noexcept : node_(other.release()) {}
    NodeOwner& operator=(NodeOwner&& other) {
        if (this != &other) {
            if (!retire()) throw std::runtime_error("Unable to retire an Arnold node before move assignment.");
            node_ = other.release();
        }
        return *this;
    }

    AtNode* get() const noexcept { return node_; }
    explicit operator bool() const noexcept { return node_ != nullptr; }

    AtNode* release() noexcept {
        AtNode* released = node_;
        node_ = nullptr;
        return released;
    }

    // On failure both the current node and the caller-owned replacement retain
    // their ownership. Production raw-pointer adoption sites use empty owners.
    bool reset(AtNode* replacement = nullptr) noexcept {
        if (node_ == replacement) return true;
        if (!retire()) return false;
        node_ = replacement;
        return true;
    }

    bool retire() noexcept {
        if (!node_) return true;
        try {
            if (AiNodeDestroy(node_)) {
                node_ = nullptr;
                return true;
            }
        } catch (...) {
        }
        try {
            AiMsgError("[dommyTool] Unable to retire an owned Arnold node.");
        } catch (...) {
        }
        return false;
    }

  private:
    AtNode* node_;
};

// Creation order is dependency order (bases/textures before instances/lights).
// Explicit reverse retirement stops on failure, retaining the failed node and
// all dependencies for retry. Keep the batch on its translator across updates.
class NodeBatch {
public:
    ~NodeBatch() noexcept {
        if (!retire()) releaseAll(); // Universe lifetime is the final fallback.
    }
    NodeBatch() = default;
    NodeBatch(const NodeBatch&) = delete;
    NodeBatch& operator=(const NodeBatch&) = delete;

    void reserve(std::size_t count) {
        if (!nodes_.empty() || count > dommy::generation::kMaxGeneratedTransformCount)
            throw std::length_error("Invalid Arnold node-batch reservation.");
        nodes_.reserve(count);
        limit_ = count;
    }
    AtNode* create(const char* type) {
        requireSlot(); // No vector allocation or ownership gap after AiNode.
        AtNode* const node = AiNode(type);
        if (node) nodes_.emplace_back(node);
        return node;
    }
    void adopt(NodeOwner&& owner) {
        if (!owner) return;
        requireSlot();
        nodes_.emplace_back(std::move(owner));
    }
    bool retire() noexcept {
        for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
            if (!it->retire()) return false;
        }
        nodes_.clear();
        return true;
    }
    void releaseAll() noexcept {
        for (NodeOwner& owner : nodes_) owner.release();
        nodes_.clear();
    }
    bool empty() const noexcept { return nodes_.empty(); }

private:
    void requireSlot() const {
        if (nodes_.size() >= limit_)
            throw std::length_error("Arnold node batch exceeds its admitted size.");
    }
    std::vector<NodeOwner> nodes_;
    std::size_t limit_ = 0U;
};

class NodeBatchScope {
public:
    explicit NodeBatchScope(NodeBatch& nodes) noexcept : nodes_(nodes) {}
    ~NodeBatchScope() noexcept { if (!committed_) nodes_.retire(); }
    NodeBatchScope(const NodeBatchScope&) = delete;
    NodeBatchScope& operator=(const NodeBatchScope&) = delete;
    void commit() noexcept { nodes_.releaseAll(); committed_ = true; }
private:
    NodeBatch& nodes_;
    bool committed_ = false;
};

// Arnold assumes ownership after AiNodeSetArray. Use release() at that exact
// handoff. Arrays used only as temporary motion-key staging buffers stay owned
// by this object and are destroyed automatically.
class ArrayOwner {
  public:
    explicit ArrayOwner(AtArray* array = nullptr) noexcept : array_(array) {}
    ~ArrayOwner() {
        if (array_ != nullptr) {
            AiArrayDestroy(array_);
        }
    }

    ArrayOwner(const ArrayOwner&) = delete;
    ArrayOwner& operator=(const ArrayOwner&) = delete;

    ArrayOwner(ArrayOwner&& other) noexcept : array_(other.release()) {}
    ArrayOwner& operator=(ArrayOwner&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    AtArray* get() const noexcept { return array_; }
    explicit operator bool() const noexcept { return array_ != nullptr; }

    AtArray* release() noexcept {
        AtArray* released = array_;
        array_ = nullptr;
        return released;
    }

    void reset(AtArray* replacement = nullptr) noexcept {
        if (array_ != replacement && array_ != nullptr) {
            AiArrayDestroy(array_);
        }
        array_ = replacement;
    }

  private:
    AtArray* array_;
};

class ArrayMapGuard {
  public:
    explicit ArrayMapGuard(AtArray* array)
        : array_(array), data_(array != nullptr ? AiArrayMap(array) : nullptr) {}
    ~ArrayMapGuard() {
        if (array_ != nullptr && data_ != nullptr) {
            AiArrayUnmap(array_);
        }
    }

    ArrayMapGuard(const ArrayMapGuard&) = delete;
    ArrayMapGuard& operator=(const ArrayMapGuard&) = delete;

    void* data() const noexcept { return data_; }
    explicit operator bool() const noexcept { return data_ != nullptr; }

  private:
    AtArray* array_;
    void* data_;
};

}  // namespace arnold
}  // namespace dommy
