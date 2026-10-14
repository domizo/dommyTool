#pragma once
#include <new>
#include <string>
#include <vector>
#include "maya/MStatus.h"

namespace syntax_host {
extern int calls, failAt, throwAt;
extern bool throwConstructor, throwUnknown;
extern int constructorCalls, failConstructorAt, copyCalls, failCopyAt;
extern bool persistentConstructorFailure, persistentCopyFailure;
inline void valueFailure() {
    if (throwUnknown) throw 42;
    throw std::bad_alloc();
}
inline MStatus operation() {
    ++calls;
    if (calls == throwAt) {
        if (throwUnknown) throw 42;
        throw std::bad_alloc();
    }
    return calls == failAt ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
}
}

class MSyntax {
  public:
    enum MArgType { kNoArg, kBoolean, kLong, kDouble, kString };
    std::vector<std::string> operations;
    MSyntax() {
        ++syntax_host::constructorCalls;
        if (syntax_host::persistentConstructorFailure ||
            syntax_host::constructorCalls == syntax_host::failConstructorAt) {
            syntax_host::valueFailure();
        }
        if (syntax_host::throwConstructor) {
            syntax_host::throwConstructor = false;
            throw std::bad_alloc();
        }
    }
    MSyntax(const MSyntax& other) {
        ++syntax_host::copyCalls;
        if (syntax_host::persistentCopyFailure ||
            syntax_host::copyCalls == syntax_host::failCopyAt) {
            syntax_host::valueFailure();
        }
        operations = other.operations;
    }
    MSyntax& operator=(const MSyntax&) = default;
    MStatus addFlag(const char* shortName, const char* longName,
                    MArgType first = kNoArg, MArgType second = kNoArg,
                    MArgType third = kNoArg, MArgType fourth = kNoArg,
                    MArgType fifth = kNoArg, MArgType sixth = kNoArg) {
        const char* names[] = {"kNoArg", "kBoolean", "kLong", "kDouble", "kString"};
        std::string record = std::string("addFlag:") + shortName + ':' + longName;
        for (MArgType type : {first, second, third, fourth, fifth, sixth}) {
            record += ':';
            record += names[type];
        }
        operations.push_back(record);
        return syntax_host::operation();
    }
    MStatus makeFlagMultiUse(const char* flag) {
        operations.push_back(std::string("makeFlagMultiUse:") + flag);
        return syntax_host::operation();
    }
    void enableQuery(bool enabled) {
        operations.push_back(std::string("enableQuery:") + (enabled ? "true" : "false"));
    }
    void enableEdit(bool enabled) {
        operations.push_back(std::string("enableEdit:") + (enabled ? "true" : "false"));
    }
};
