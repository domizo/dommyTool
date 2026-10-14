#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "common/utility/DCommandSyntax.h"

#include <maya/MDrawRegistry.h>
#include <maya/MFnPlugin.h>
#include <maya/MMessage.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

namespace dommy {

class PluginLifetime {
  public:
    typedef std::function<MStatus(MFnPlugin&)> CleanupAction;

    PluginLifetime() {}
    PluginLifetime(const PluginLifetime&) = delete;
    PluginLifetime& operator=(const PluginLifetime&) = delete;

    bool empty() const {
        return cleanupActions_.empty();
    }

    MStatus cleanup(MFnPlugin& plugin) noexcept {
        MStatus firstFailure = MS::kSuccess;
        for (std::vector<CleanupAction>::reverse_iterator it = cleanupActions_.rbegin();
             it != cleanupActions_.rend(); ++it) {
            MStatus status = MS::kFailure;
            try {
                status = (*it)(plugin);
            } catch (...) {
                // Plugin boundaries must never allow C++ exceptions to escape into Maya.
            }
            if (firstFailure == MS::kSuccess && status != MS::kSuccess) {
                firstFailure = status;
            }
            if (status == MS::kSuccess) *it = CleanupAction();
        }

        // Compact by noexcept moves, without allocating during rollback/unload.
        // Failed actions retain registration order for the next reverse pass.
        std::size_t retained = 0;
        for (std::size_t index = 0; index < cleanupActions_.size(); ++index) {
            if (cleanupActions_[index]) {
                if (retained != index)
                    cleanupActions_[retained] = std::move(cleanupActions_[index]);
                ++retained;
            }
        }
        cleanupActions_.resize(retained);
        return firstFailure;
    }

  private:
    std::vector<CleanupAction> cleanupActions_;

    friend class PluginRegistration;
};

class PluginRegistration {
  public:
    PluginRegistration(MFnPlugin& plugin, PluginLifetime& lifetime)
        : plugin_(plugin), lifetime_(lifetime),
          firstFailure_(lifetime.empty() ? MS::kSuccess : MS::kFailure),
          ownsLifetime_(lifetime.empty()), committed_(false) {}

    ~PluginRegistration() noexcept {
        if (ownsLifetime_ && !committed_)
            lifetime_.cleanup(plugin_);
    }

    PluginRegistration(const PluginRegistration&) = delete;
    PluginRegistration& operator=(const PluginRegistration&) = delete;

    // Prepare cleanup storage BEFORE the operation can mutate Maya/native
    // state. Keep it even on failure because subsystem setup may be partial.
    template <typename Operation>
    MStatus registerResource(Operation operation,
                             PluginLifetime::CleanupAction cleanup) {
        return performRegistration(operation, std::move(cleanup), true);
    }

    template <typename Factory>
    MStatus registerCallback(Factory factory) {
        if (firstFailure_ != MS::kSuccess) return firstFailure_;
        const auto callbackId = std::make_shared<MCallbackId>(0);
        return registerResource([&]() -> MStatus {
            MStatus status;
            *callbackId = factory(status);
            if (!status) return status;
            return *callbackId != 0 ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
        }, [callbackId](MFnPlugin&) -> MStatus {
            return *callbackId != 0 ? MMessage::removeCallback(*callbackId) : MStatus(MS::kSuccess);
        });
    }

    template <typename Name, typename... Args>
    MStatus registerNode(const Name& name, const MTypeId& typeId, Args&&... args) {
        return performRegistration([&]() {
            return plugin_.registerNode(name, typeId, std::forward<Args>(args)...);
        }, [typeId](MFnPlugin& plugin) { return plugin.deregisterNode(typeId); }, false);
    }

    template <typename... Args> MStatus registerCommand(const MString& name, Args&&... args) {
        const MString stableName(name);
        return performRegistration([&]() {
            return plugin_.registerCommand(stableName, std::forward<Args>(args)...);
        }, [stableName](MFnPlugin& plugin) { return plugin.deregisterCommand(stableName); }, false);
    }

    template <typename Command>
    MStatus registerCommandWithSyntax(const MString& name) {
        if (firstFailure_ != MS::kSuccess) return firstFailure_;
        const MStatus validation = maya::validateCommandSyntax<Command>(name.asChar());
        if (validation != MS::kSuccess) {
            recordFailure(validation);
            return validation;
        }
        const MStatus status = registerCommand(name, Command::creator, Command::newSyntax);
        if (status != MS::kSuccess) return status;
        // A host may invoke the syntax callback during registration. The
        // native command already exists on success, so retain its cleanup.
        const MStatus syntaxStatus = maya::commandSyntaxStatus<Command>();
        recordFailure(syntaxStatus);
        return syntaxStatus;
    }

    template <typename... Args>
    MStatus registerData(const MString& name, const MTypeId& typeId, Args&&... args) {
        return performRegistration([&]() {
            return plugin_.registerData(name, typeId, std::forward<Args>(args)...);
        }, [typeId](MFnPlugin& plugin) { return plugin.deregisterData(typeId); }, false);
    }

    template <typename Creator>
    MStatus registerGeometryOverrideCreator(const MString& classification,
                                            const MString& registrantId, Creator creator) {
        const MString stableClassification(classification);
        const MString stableRegistrantId(registrantId);
        return performRegistration([&]() {
            return MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
                stableClassification, stableRegistrantId, creator);
        }, [stableClassification, stableRegistrantId](MFnPlugin&) {
            return MHWRender::MDrawRegistry::deregisterGeometryOverrideCreator(
                stableClassification, stableRegistrantId);
        }, false);
    }

    template <typename Creator>
    MStatus registerDrawOverrideCreator(const MString& classification, const MString& registrantId,
                                        Creator creator) {
        const MString stableClassification(classification);
        const MString stableRegistrantId(registrantId);
        return performRegistration([&]() {
            return MHWRender::MDrawRegistry::registerDrawOverrideCreator(
                stableClassification, stableRegistrantId, creator);
        }, [stableClassification, stableRegistrantId](MFnPlugin&) {
            return MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
                stableClassification, stableRegistrantId);
        }, false);
    }

    MStatus commit() noexcept {
        if (firstFailure_ != MS::kSuccess)
            return firstFailure_;
        committed_ = true;
        return MS::kSuccess;
    }

  private:
    template <typename Operation>
    MStatus performRegistration(Operation operation,
                                PluginLifetime::CleanupAction cleanup,
                                bool retainOnFailure) {
        if (firstFailure_ != MS::kSuccess) return firstFailure_;
        if (!cleanup) {
            recordFailure(MS::kFailure);
            return firstFailure_;
        }
        try {
            lifetime_.cleanupActions_.push_back(std::move(cleanup));
        } catch (...) {
            recordFailure(MS::kFailure);
            return firstFailure_;
        }
        MStatus status;
        try {
            status = operation();
        } catch (...) {
            // A throwing setup can have partially succeeded; retain cleanup.
            recordFailure(MS::kFailure);
            return firstFailure_;
        }
        if (status != MS::kSuccess && !retainOnFailure)
            lifetime_.cleanupActions_.pop_back();
        recordFailure(status);
        return status;
    }

    void recordFailure(const MStatus& status) {
        if (firstFailure_ == MS::kSuccess && status != MS::kSuccess) {
            firstFailure_ = status;
        }
    }

    MFnPlugin& plugin_;
    PluginLifetime& lifetime_;
    MStatus firstFailure_;
    bool ownsLifetime_;
    bool committed_;
};

} // namespace dommy
