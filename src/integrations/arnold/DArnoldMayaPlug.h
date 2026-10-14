#pragma once

#include "DNodeOperation.h"

#include <ai_msg.h>

#include <functional>
#include <string>

namespace dommy {
namespace arnold {

// MtoA translators expose parameters by name through FindMayaPlug(). Reading
// through this adapter preserves Maya's MStatus instead of silently accepting
// the default value returned by MPlug::as*() after a failed evaluation.
class PlugReader {
  public:
    typedef std::function<MPlug(const char*)> Resolver;

    PlugReader(const Resolver& resolver, const char* context)
        : resolver_(resolver), context_(context != nullptr ? context : "Arnold") {}

    template <typename Value>
    bool read(const char* name, Value& value) const {
        const MPlug plug = resolver_(name);
        if (plug.isNull() || readPlugValue(plug, value) != MS::kSuccess) {
            AiMsgError(
                "[dommyTool] %s could not read Maya parameter: %s",
                context_.c_str(), name);
            return false;
        }
        return true;
    }

    bool readString(const char* name, std::string& value) const {
        MString candidate;
        if (!read(name, candidate)) {
            return false;
        }
        value = candidate.asChar();
        return true;
    }

    // Preserve the historical absent-input path without consuming a failed
    // query's boolean result. Callers admit only their active inputs.
    bool connection(const MPlug& plug, const char* name, bool& connected) const {
        connected = false;
        if (plug.isNull()) return true;
        MStatus status;
        const bool candidate = plug.isConnected(&status);
        if (!status) {
            AiMsgError("[dommyTool] %s could not query Maya connection: %s",
                       context_.c_str(), name);
            return false;
        }
        connected = candidate;
        return true;
    }

    // A successful null source means an unconnected input. A non-null source
    // is already the connected upstream plug; do not query it a second time.
    bool source(const MPlug& plug, const char* name, MPlug& sourcePlug) const {
        sourcePlug = MPlug();
        if (plug.isNull()) {
            AiMsgError("[dommyTool] %s could not find Maya source input: %s",
                       context_.c_str(), name);
            return false;
        }
        MStatus status;
        const MPlug candidate = plug.source(&status);
        if (!status) {
            AiMsgError("[dommyTool] %s could not resolve Maya source: %s",
                       context_.c_str(), name);
            return false;
        }
        sourcePlug = candidate;
        return true;
    }

  private:
    Resolver resolver_;
    std::string context_;
};

}  // namespace arnold
}  // namespace dommy
