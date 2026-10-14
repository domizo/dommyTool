#pragma once

#include "DViewportCallbackGuard.h"
#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MViewport2Renderer.h>

#include <exception>
#include <memory>

namespace dommy {
namespace viewport {

struct RenderItemDeleter {
    const char* ownerName;

    void operator()(MHWRender::MRenderItem* item) const noexcept
    {
        try {
            MHWRender::MRenderItem::Destroy(item);
        } catch (...) {
            maya::reportViewportCallbackException(
                ownerName, "destroyPendingRenderItem", std::current_exception());
        }
    }
};

// Maya owns items only after a successful append. The caller releases this
// owner immediately afterwards; until then, failures unwind the pending item.
using PendingRenderItem = std::unique_ptr<MHWRender::MRenderItem, RenderItemDeleter>;

class RenderItemUpdateScope final {
public:
    explicit RenderItemUpdateScope(bool& retry) : retry_(retry), completed_(false) { retry_ = false; }
    ~RenderItemUpdateScope() noexcept { if (!completed_) retry_ = true; }
    void complete() { completed_ = true; }
private:
    RenderItemUpdateScope(const RenderItemUpdateScope&);
    RenderItemUpdateScope& operator=(const RenderItemUpdateScope&);
    bool& retry_;
    bool completed_;
};

// Query the current manager only when a cache entry is missing. Successful
// shader instances remain owned by the override's existing release path;
// this per-operation helper neither replaces them nor retains a manager pointer.
class StockShaderAcquisition final {
public:
    StockShaderAcquisition(bool& retry, const char* ownerName)
        : retry_(retry), ownerName_(ownerName), manager_(nullptr), queriedManager_(false) {}

    void acquire(MHWRender::MShaderManager::MStockShader type,
                 MHWRender::MShaderInstance*& shader) noexcept
    {
        try {
            if (shader) return;
            if (!queriedManager_) {
                queriedManager_ = true;
                MHWRender::MRenderer* const renderer = MHWRender::MRenderer::theRenderer();
                manager_ = renderer ? renderer->getShaderManager() : nullptr;
            }
            if (manager_) shader = manager_->getStockShader(type);
            if (!shader) retry_ = true;
        } catch (...) {
            retry_ = true;
            maya::reportViewportCallbackException(
                ownerName_, "acquireStockShader", std::current_exception());
        }
    }

private:
    StockShaderAcquisition(const StockShaderAcquisition&);
    StockShaderAcquisition& operator=(const StockShaderAcquisition&);
    bool& retry_;
    const char* ownerName_;
    const MHWRender::MShaderManager* manager_;
    bool queriedManager_;
};

// Destructor cleanup is best effort: detach before calling the native release
// API, never retry a possibly released pointer, and keep releasing other entries
// after an exception. A missing native manager cannot establish resource release.
class StockShaderRelease final {
public:
    explicit StockShaderRelease(const char* ownerName) noexcept
        : ownerName_(ownerName), manager_(nullptr), queriedManager_(false) {}

    void release(MHWRender::MShaderInstance*& shader) noexcept
    {
        if (!shader) return;
        MHWRender::MShaderInstance* const instance = shader;
        shader = nullptr;
        try {
            if (!queriedManager_) {
                queriedManager_ = true;
                MHWRender::MRenderer* const renderer = MHWRender::MRenderer::theRenderer();
                manager_ = renderer ? renderer->getShaderManager() : nullptr;
            }
            if (manager_) manager_->releaseShader(instance);
        } catch (...) {
            maya::reportViewportCallbackException(
                ownerName_, "releaseStockShader", std::current_exception());
        }
    }

private:
    StockShaderRelease(const StockShaderRelease&);
    StockShaderRelease& operator=(const StockShaderRelease&);
    const char* ownerName_;
    const MHWRender::MShaderManager* manager_;
    bool queriedManager_;
};

// Disable first, so a failed parameter update cannot draw stale shader state.
// A separate retry flag survives successful geometry upload. Maya still owns
// the item and shader; this guard changes only configuration and visibility.
class RenderItemShaderUpdate final {
public:
    RenderItemShaderUpdate(MHWRender::MRenderItem& item, bool& retry)
        : item_(item), retry_(retry), parametersValid_(true), assigned_(false), completed_(false)
    {
        try {
            item_.enable(false);
        } catch (...) {
            retry_ = true;
            throw;
        }
    }

    ~RenderItemShaderUpdate() noexcept {
        if (!completed_) retry_ = true;
    }

    template <typename Name, typename Value>
    void setParameter(MHWRender::MShaderInstance* shader, const Name& name, const Value& value) {
        if (!shader || !shader->setParameter(name, value)) {
            parametersValid_ = false;
            retry_ = true;
        }
    }

    void setShader(MHWRender::MShaderInstance* shader, const MString* name) {
        assigned_ = parametersValid_ && shader && item_.setShader(shader, name);
        if (!assigned_) retry_ = true;
    }

    void enable(bool visible) {
        if (visible && !assigned_) retry_ = true;
        item_.enable(visible && assigned_);
        completed_ = true;
    }

private:
    RenderItemShaderUpdate(const RenderItemShaderUpdate&);
    RenderItemShaderUpdate& operator=(const RenderItemShaderUpdate&);
    MHWRender::MRenderItem& item_;
    bool& retry_;
    bool parametersValid_;
    bool assigned_;
    bool completed_;
};

} // namespace viewport
} // namespace dommy
