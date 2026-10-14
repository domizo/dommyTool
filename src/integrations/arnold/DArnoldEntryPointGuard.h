#pragma once

#include <ai_msg.h>
#include <ai_nodes.h>

#include <exception>
#include <utility>

#include "common/utility/DPointerFactoryGuard.h"

namespace dommy {
namespace arnold {

inline void hideIncompleteUpdate(AtNode* node) noexcept {
    try {
        if (node != nullptr) {
            static const AtString visibilityParameter("visibility");
            AiNodeSetByte(node, visibilityParameter, 0);
        }
    } catch (...) {
        // Cleanup must not replace the original failure during unwinding.
    }
}

// Use only inside an active, connected geometry update. A normal disconnected
// or valid-empty input retains its historical policy; complete() never enables
// visibility or overwrites render flags. All incomplete exits fail closed.
class UpdateVisibilityGuard {
public:
    explicit UpdateVisibilityGuard(AtNode* node) noexcept : node_(node) {}
    ~UpdateVisibilityGuard() noexcept {
        if (!complete_) hideIncompleteUpdate(node_);
    }

    UpdateVisibilityGuard(const UpdateVisibilityGuard&) = delete;
    UpdateVisibilityGuard& operator=(const UpdateVisibilityGuard&) = delete;

    void complete() noexcept { complete_ = true; }

private:
    AtNode* node_;
    bool complete_ = false;
};

inline void reportTranslatorException(
    const char* entryPoint, AtNode* node,
    const std::exception_ptr& exception) noexcept {
    hideIncompleteUpdate(node);
    try {
        if (exception) {
            std::rethrow_exception(exception);
        }
        AiMsgError("[dommyTool] %s failed with an unknown error.", entryPoint);
    } catch (const std::exception& error) {
        try {
            AiMsgError(
                "[dommyTool] %s failed: %s", entryPoint, error.what());
        } catch (...) {
            // Reporting must not terminate the host or replace the failure.
        }
    } catch (...) {
        try {
            AiMsgError("[dommyTool] %s failed with an unknown error.", entryPoint);
        } catch (...) {
        }
    }
}

template <typename Factory>
auto createObject(const char* ownerName, Factory&& factory) noexcept
    -> decltype(factory())
{
    return ::dommy::guardPointerFactory(std::forward<Factory>(factory),
        [ownerName](const std::exception_ptr& exception) {
            reportTranslatorException(ownerName, nullptr, exception);
        });
}

}  // namespace arnold
}  // namespace dommy

#define DOMMY_ARNOLD_TRANSLATOR_CATCH(entryPoint, node) \
    catch (...) { \
        ::dommy::arnold::reportTranslatorException( \
            entryPoint, node, std::current_exception()); \
    }
