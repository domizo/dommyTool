#pragma once

#include <exception>

namespace openvdb {

class Exception : public std::exception
{
public:
    const char* what() const noexcept override { return "OpenVDB exception"; }
};

} // namespace openvdb
