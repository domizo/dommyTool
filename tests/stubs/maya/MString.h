#pragma once

#include <string>

class MString
{
public:
    MString(const char* value = "") : value_(value ? value : "") {}
    const char* asChar() const { return value_.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(value_.size()); }
    void clear() { value_.clear(); }
    bool operator==(const MString& other) const { return value_ == other.value_; }
    MString operator+(const char* suffix) const {
        return MString((value_ + (suffix ? suffix : "")).c_str());
    }
    MString& operator+=(unsigned int value) {
        value_ += std::to_string(value);
        return *this;
    }

private:
    std::string value_;
};
