#pragma once

namespace MS {
enum MStatusCode { kSuccess = 0, kFailure = 1, kInvalidParameter = 3, kUnknownParameter = 5 };
}

class MStatus {
public:
    static constexpr MS::MStatusCode kUnknownParameter = MS::kUnknownParameter;
    static constexpr MS::MStatusCode kSuccess = MS::kSuccess;
    static constexpr MS::MStatusCode kFailure = MS::kFailure;
    MStatus(MS::MStatusCode status = MS::kSuccess) : status_(status) {}

    bool operator==(MS::MStatusCode status) const {
        return status_ == status;
    }
    bool operator!=(MS::MStatusCode status) const {
        return status_ != status;
    }
    explicit operator bool() const {
        return status_ == MS::kSuccess;
    }
    void pAPIerror(const char*, int) const {}
    void perror(const char*) const {}

  private:
    MS::MStatusCode status_;
};
