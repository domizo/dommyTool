#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <memory>
#include <locale>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "DHandleOutput.h"

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace dommy {
namespace file {

inline unsigned long currentProcessId()
{
#if defined(_WIN32)
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

inline std::string makeTemporarySiblingPathWithSuffix(
    const std::string& destination, const std::string& suffix)
{
    if (destination.empty() || destination.find('\0') != std::string::npos) {
        throw std::invalid_argument("Atomic output requires a destination path");
    }
    if (suffix.empty() || suffix.size() > 32U ||
        suffix.find('/') != std::string::npos ||
        suffix.find('\\') != std::string::npos ||
        suffix.find(':') != std::string::npos ||
        suffix.find('\0') != std::string::npos) {
        throw std::invalid_argument("Temporary output suffix is invalid");
    }

    static std::atomic<unsigned long long> counter(0U);
    const unsigned long long sequence = counter.fetch_add(1U);
    const long long timestamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    // Preserve the exact directory prefix, but never extend the destination's
    // basename: an otherwise valid NAME_MAX-length output must remain writable.
#if defined(_WIN32)
    const std::size_t separator = destination.find_last_of("/\\");
    const std::size_t prefixLength = separator != std::string::npos ? separator + 1U :
        destination.size() >= 2U && destination[1] == ':' ? 2U : 0U;
#else
    const std::size_t separator = destination.find_last_of('/');
    const std::size_t prefixLength = separator == std::string::npos ? 0U : separator + 1U;
#endif
    std::ostringstream path;
    path.imbue(std::locale::classic());
    path << destination.substr(0U, prefixLength) << "output.dommytool-" << currentProcessId() << '-'
         << timestamp << '-' << sequence << suffix;
    if (!path) throw std::runtime_error("Temporary output path formatting failed");
    return path.str();
}

inline std::string makeTemporarySiblingPath(const std::string& destination)
{
    return makeTemporarySiblingPathWithSuffix(destination, ".tmp");
}

inline bool createExclusiveEmptyFile(const std::string& path)
{
#if defined(_WIN32)
    HANDLE handle = ::CreateFileA(
        path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (::CloseHandle(handle) == 0) {
        ::DeleteFileA(path.c_str());
        return false;
    }
    return true;
#else
    const int descriptor = ::open(
        path.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (descriptor == -1) {
        return false;
    }
    if (::close(descriptor) != 0) {
        std::remove(path.c_str());
        return false;
    }
    return true;
#endif
}

namespace detail {

// Only for a successfully reserved path in the caller's trusted namespace.
// The referenced path outlives this allocation-free, noncopyable local guard.
// It does not establish protection against pathname substitution.
class ScopedReservedPathCleanup final
{
public:
    explicit ScopedReservedPathCleanup(const std::string& path) noexcept
        : path_(path), owned_(true)
    {
    }

    ~ScopedReservedPathCleanup() noexcept
    {
        if (owned_) std::remove(path_.c_str());
    }

    void release() noexcept { owned_ = false; }

    ScopedReservedPathCleanup(const ScopedReservedPathCleanup&) = delete;
    ScopedReservedPathCleanup& operator=(const ScopedReservedPathCleanup&) = delete;
    ScopedReservedPathCleanup(std::string&&) = delete;
    ScopedReservedPathCleanup(const std::string&&) = delete;

private:
    const std::string& path_;
    bool owned_;
};

} // namespace detail

inline std::string createTemporarySiblingFileWithSuffix(
    const std::string& destination, const std::string& suffix)
{
    // Path-only SDK reservation. The caller must ensure that other principals
    // cannot rename/replace the file or any directory in its namespace. This
    // function does not establish that trust and must not back stream writers.
    const unsigned int kMaximumAttempts = 128U;
    for (unsigned int attempt = 0U; attempt < kMaximumAttempts; ++attempt) {
        const std::string path =
            makeTemporarySiblingPathWithSuffix(destination, suffix);
        if (createExclusiveEmptyFile(path)) {
            detail::ScopedReservedPathCleanup cleanup(path);
            // Construct the returned value while cleanup is still armed.
            // After release, C++11 implicit moves (even without NRVO) and the
            // PendingOutputFile member assignment must not allocate or throw.
            static_assert(std::is_nothrow_move_constructible<std::string>::value &&
                          std::is_nothrow_move_assignable<std::string>::value,
                          "Path reservation requires nonthrowing string ownership transfer");
            std::string result(path);
            cleanup.release();
            return result;
        }
#if defined(_WIN32)
        if (::GetLastError() != ERROR_FILE_EXISTS &&
            ::GetLastError() != ERROR_ALREADY_EXISTS) {
            break;
        }
#else
        if (errno != EEXIST) break;
#endif
    }
    throw std::runtime_error(
        "Temporary output could not create an exclusive file");
}

inline std::string createTemporarySiblingFile(const std::string& destination)
{
    return createTemporarySiblingFileWithSuffix(destination, ".tmp");
}

inline std::string sanitizeFileNameComponent(const std::string& source)
{
    const std::size_t kMaximumLength = 80U;
    std::string result;
    result.reserve(source.size() < kMaximumLength ? source.size() :
                                                   kMaximumLength);
    for (std::size_t index = 0U;
         index < source.size() && result.size() < kMaximumLength; ++index) {
        const char value = source[index];
        const bool allowed =
            (value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '_';
        result += allowed ? value : '_';
    }
    return result.empty() ? std::string("unnamed") : result;
}

inline bool isRegularFile(const std::string& path)
{
#if defined(_WIN32)
    const DWORD attributes = ::GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & (FILE_ATTRIBUTE_DIRECTORY |
                       FILE_ATTRIBUTE_REPARSE_POINT)) == 0U;
#else
    struct stat status;
    return ::lstat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode);
#endif
}

inline bool replaceFile(const std::string& source,
                        const std::string& destination)
{
#if defined(_WIN32)
    return ::MoveFileExA(source.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(source.c_str(), destination.c_str()) == 0;
#endif
}

class PendingOutputFile final
{
public:
    // TrustedPath is an explicit caller precondition, NOT a directory ACL check.
    enum class Access { Handle, TrustedPath };
    enum class Format { Binary, Text };

    explicit PendingOutputFile(const std::string& destination,
                               Access access = Access::Handle,
                               Format format = Format::Binary)
        : destination_(destination)
        , handle_(access == Access::Handle ? new detail::HandleOutput(
              destination, &makeTemporarySiblingPath, format == Format::Text) : nullptr)
        , buffer_(handle_.get())
        , stream_(&buffer_)
        , committed_(false)
    {
        temporary_ = handle_ ? handle_->path() : createTemporarySiblingFile(destination);
    }

    ~PendingOutputFile() noexcept
    {
        if (!handle_ && !committed_) {
            std::remove(temporary_.c_str());
        }
    }

    const std::string& destinationPath() const { return destination_; }
    // Retain the old name only for callers that explicitly select path access.
    // Default handle writers must not accidentally reopen the reserved path.
    const std::string& temporaryPath() const { return trustedTemporaryPath(); }
    const std::string& diagnosticTemporaryPath() const { return temporary_; }

    std::ostream& stream()
    {
        if (!handle_) throw std::logic_error("Path-only output has no owned stream");
        return stream_;
    }

    bool close()
    {
        if (!handle_) return false;
        stream_.flush();
        const bool closed = handle_->close();
        return stream_.good() && closed;
    }

    // External SDKs that can only reopen a path require a trusted, stable
    // directory namespace. This is not interchangeable with stream().
    const std::string& trustedTemporaryPath() const
    {
        if (handle_) throw std::logic_error("Use the owned output stream");
        return temporary_;
    }

    bool commit()
    {
        if (committed_) {
            return true;
        }
        if (handle_) {
            if (!close() || !handle_->publish()) return false;
            committed_ = true;
            return true;
        }
        if (!isRegularFile(temporary_)) {
            return false;
        }
        if (!replaceFile(temporary_, destination_)) {
            return false;
        }
        committed_ = true;
        return true;
    }

private:
    PendingOutputFile(const PendingOutputFile&);
    PendingOutputFile& operator=(const PendingOutputFile&);

    std::string destination_;
    std::string temporary_;
    std::unique_ptr<detail::HandleOutput> handle_;
    detail::HandleOutputBuffer buffer_;
    std::ostream stream_;
    bool committed_;
};

} // namespace file
} // namespace dommy
