#pragma once

#include <maya/MStatus.h>
#include <type_traits>
#include <utility>

// Unlike CHECK_MSTATUS_AND_RETURN_IT(expression), never evaluate a failed
// expression again in the return statement. Safe for reads and staged edits.
#define DOMMY_RETURN_IF_FAILURE(expression) \
    do { \
        const MStatus dommyCheckedStatus = (expression); \
        if (dommyCheckedStatus != MS::kSuccess) { \
            dommyCheckedStatus.pAPIerror(__FILE__, __LINE__); \
            return dommyCheckedStatus; \
        } \
    } while (false)

namespace dommy {
namespace maya {

// Retain legacy void population callbacks while propagating status from
// checked operations. Exceptions are handled by the owning Maya boundary.
template <typename Operation, typename Value>
typename std::enable_if<
    std::is_void<decltype(std::declval<Operation>()(std::declval<Value&>()))>::value,
    MStatus>::type
invokeStatusOperation(Operation operation, Value& value)
{
    operation(value);
    return MS::kSuccess;
}

template <typename Operation, typename Value>
typename std::enable_if<
    !std::is_void<decltype(std::declval<Operation>()(std::declval<Value&>()))>::value,
    MStatus>::type
invokeStatusOperation(Operation operation, Value& value)
{
    return operation(value);
}

} // namespace maya
} // namespace dommy
