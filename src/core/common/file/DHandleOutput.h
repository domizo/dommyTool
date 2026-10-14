#pragma once

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#endif
#endif

namespace dommy {
namespace file {
namespace detail {

// Internal transport: reserve once, adopt that handle, and never reopen the
// payload by pathname. POSIX publication is relative to pinned directories.
class HandleOutput final {
public:
    using NameMaker = std::string (*)(const std::string&);

    HandleOutput(const std::string& destination, NameMaker makeName, bool text)
    {
        if (!makeName || destination.empty() ||
            destination.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid handle-output destination");
#if defined(_WIN32)
        const std::size_t separator = destination.find_last_of("/\\");
#else
        const std::size_t separator = destination.find_last_of('/');
#endif
        parent_ = separator == std::string::npos ? "." :
                  separator == 0U ? destination.substr(0U, 1U) : destination.substr(0U, separator);
#if defined(_WIN32)
        // C:\\file refers to the drive root, not the drive's working directory C:.
        if (separator == 2U && destination[1] == ':')
            parent_ = destination.substr(0U, 3U);
#endif
        leaf_ = separator == std::string::npos ? destination : destination.substr(separator + 1U);
        if (leaf_.empty() || leaf_ == "." || leaf_ == "..")
            throw std::invalid_argument("Handle output requires a file basename");
        try {
#if defined(_WIN32)
            createWindows(makeName, text);
#else
            createPosix(makeName, text);
#endif
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~HandleOutput() noexcept { cleanup(); }
    HandleOutput(const HandleOutput&) = delete;
    HandleOutput& operator=(const HandleOutput&) = delete;

    const std::string& path() const { return path_; } // Diagnostics only.

    std::streamsize write(const char* data, std::streamsize count)
    {
        if (count <= 0) return 0;
        if (!file_ || failed_ || !data) { failed_ = true; return 0; }
        std::streamsize completed = 0;
        unsigned int interruptions = 0U;
        while (completed < count) {
            const std::size_t remaining = static_cast<std::size_t>(
                std::min<std::streamsize>(count - completed, 65536));
            const std::size_t written = std::fwrite(data + completed, 1U, remaining, file_);
            completed += static_cast<std::streamsize>(written);
            if (written != remaining) {
                if (std::ferror(file_) && errno == EINTR && interruptions++ < 128U) {
                    std::clearerr(file_);
                    continue;
                }
                failed_ = true;
                break;
            }
        }
        return completed;
    }

    bool flush()
    {
        if (file_ && std::fflush(file_) != 0) failed_ = true;
        return !failed_;
    }

    bool close()
    {
        flush();
        if (file_) {
            FILE* closing = file_;
            file_ = nullptr;
            if (std::fclose(closing) != 0) failed_ = true;
        }
        return !failed_;
    }

    bool publish()
    {
        if (published_) return true;
        if (!close()) return false;
#if defined(_WIN32)
        if (!renameWindows()) return false;
#else
        struct stat candidate;
        if (::fstatat(stage_, "payload.tmp", &candidate, AT_SYMLINK_NOFOLLOW) != 0 ||
            !S_ISREG(candidate.st_mode) || candidate.st_dev != identity_.st_dev ||
            candidate.st_ino != identity_.st_ino) return false;
        // The source entry is in a creator-only directory. Renaming that
        // directory or an ancestor cannot redirect either pinned endpoint.
        if (::renameat(stage_, "payload.tmp", parentHandle_, leaf_.c_str()) != 0) return false;
#endif
        published_ = true;
        return true;
    }

private:
    static bool validGeneratedName(const std::string& value)
    {
        return !value.empty() && value.size() <= 240U && value != "." && value != ".." &&
            value.find_first_of("/\\:\0", 0U, 4U) == std::string::npos;
    }

#if !defined(_WIN32)
    void createPosix(NameMaker makeName, bool text)
    {
        parentHandle_ = ::open(parent_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (parentHandle_ == -1) throw std::runtime_error("Cannot open output parent directory");
        bool created = false;
        for (unsigned int attempt = 0U; attempt < 128U; ++attempt) {
            stageName_ = makeName("dommytool-output");
            if (!validGeneratedName(stageName_)) throw std::invalid_argument("Invalid staging name");
            path_ = parent_ + '/' + stageName_ + "/payload.tmp";
            if (::mkdirat(parentHandle_, stageName_.c_str(), S_IRWXU) == 0) {
                created = true;
                break;
            }
            if (errno != EEXIST) break;
        }
        if (!created) throw std::runtime_error("Cannot reserve output staging directory");
        stageCreated_ = true;
        if (::fstatat(parentHandle_, stageName_.c_str(), &stageIdentity_, AT_SYMLINK_NOFOLLOW) != 0 ||
            !S_ISDIR(stageIdentity_.st_mode) || stageIdentity_.st_uid != ::geteuid() ||
            (stageIdentity_.st_mode & 0777) != 0700)
            throw std::runtime_error("Output staging directory ownership changed");
        stage_ = ::openat(parentHandle_, stageName_.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        struct stat opened;
        if (stage_ == -1 || ::fstat(stage_, &opened) != 0 ||
            opened.st_dev != stageIdentity_.st_dev || opened.st_ino != stageIdentity_.st_ino)
            throw std::runtime_error("Output staging directory ownership changed");
#if defined(__APPLE__)
        // macOS inherited allow-ACEs can grant access independently of 0700.
        // Remove them on the pinned, newly-created directory before creating
        // a payload. Failure is closed; no existing/user directory is changed.
        acl_t emptyAcl = ::acl_init(0);
        if (!emptyAcl) throw std::runtime_error("Cannot allocate private staging ACL");
        const int aclStatus = ::acl_set_fd_np(stage_, emptyAcl, ACL_TYPE_EXTENDED);
        ::acl_free(emptyAcl);
        if (aclStatus != 0) throw std::runtime_error("Cannot restrict output staging ACL");
#endif
        if (::fchmod(stage_, S_IRWXU) != 0)
            throw std::runtime_error("Cannot restrict output staging permissions");
        const int descriptor = ::openat(stage_, "payload.tmp", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC,
                                        S_IRUSR | S_IWUSR);
        if (descriptor == -1) throw std::runtime_error("Cannot reserve output payload");
        if (::fstat(descriptor, &identity_) != 0 || !S_ISREG(identity_.st_mode)) {
            ::close(descriptor);
            throw std::runtime_error("Invalid reserved output payload");
        }
        file_ = ::fdopen(descriptor, text ? "w" : "wb");
        if (!file_) {
            ::close(descriptor);
            throw std::runtime_error("Cannot adopt output descriptor");
        }
    }

    void cleanup() noexcept
    {
        close();
        if (stage_ != -1) {
            if (!published_) ::unlinkat(stage_, "payload.tmp", 0);
            ::close(stage_);
            stage_ = -1;
        }
        if (parentHandle_ != -1) {
            if (stageCreated_) {
                struct stat current;
                if (::fstatat(parentHandle_, stageName_.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
                    current.st_dev == stageIdentity_.st_dev && current.st_ino == stageIdentity_.st_ino)
                    ::unlinkat(parentHandle_, stageName_.c_str(), AT_REMOVEDIR);
            }
            ::close(parentHandle_);
            parentHandle_ = -1;
        }
    }

    int parentHandle_ = -1;
    int stage_ = -1;
    bool stageCreated_ = false;
    std::string stageName_;
    struct stat stageIdentity_ = {};
    struct stat identity_ = {};
#else
    struct HeapDeleter {
        void operator()(FILE_RENAME_INFO* value) const noexcept {
            if (value) ::HeapFree(::GetProcessHeap(), 0, value);
        }
    };

    void createWindows(NameMaker makeName, bool text)
    {
        if (leaf_.find(':') != std::string::npos)
            throw std::invalid_argument("Output file streams are not supported");
        parentHandle_ = ::CreateFileA(parent_.c_str(), FILE_LIST_DIRECTORY | FILE_ADD_FILE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (parentHandle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot pin output directory");
        HANDLE payload = INVALID_HANDLE_VALUE;
        for (unsigned int attempt = 0U; attempt < 128U; ++attempt) {
            const std::string name = makeName("dommytool-output");
            if (!validGeneratedName(name)) throw std::invalid_argument("Invalid staging name");
            path_ = parent_ + '/' + name;
            payload = ::CreateFileA(path_.c_str(), GENERIC_WRITE | DELETE, 0, nullptr,
                                    CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (payload != INVALID_HANDLE_VALUE) break;
            if (::GetLastError() != ERROR_FILE_EXISTS && ::GetLastError() != ERROR_ALREADY_EXISTS) break;
        }
        if (payload == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot reserve output payload");
        if (!::DuplicateHandle(::GetCurrentProcess(), payload, ::GetCurrentProcess(), &renameHandle_,
                               0, FALSE, DUPLICATE_SAME_ACCESS)) {
            FILE_DISPOSITION_INFO disposition = {};
            disposition.DeleteFile = TRUE;
            ::SetFileInformationByHandle(payload, FileDispositionInfo, &disposition, sizeof(disposition));
            ::CloseHandle(payload);
            throw std::runtime_error("Cannot retain output publication handle");
        }
        const int descriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(payload),
                                                _O_WRONLY | (text ? _O_TEXT : _O_BINARY));
        if (descriptor == -1) { ::CloseHandle(payload); throw std::runtime_error("Cannot adopt output handle"); }
        file_ = ::_fdopen(descriptor, text ? "w" : "wb");
        if (!file_) { ::_close(descriptor); throw std::runtime_error("Cannot adopt output descriptor"); }
    }

    bool renameWindows()
    {
        const int characters = ::MultiByteToWideChar(CP_ACP, 0, leaf_.c_str(), -1, nullptr, 0);
        if (characters <= 1 || characters > 32768) return false;
        const std::size_t bytes = offsetof(FILE_RENAME_INFO, FileName) +
                                  static_cast<std::size_t>(characters) * sizeof(WCHAR);
        std::unique_ptr<FILE_RENAME_INFO, HeapDeleter> info(static_cast<FILE_RENAME_INFO*>(
            ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, bytes)));
        if (!info) return false;
        if (::MultiByteToWideChar(CP_ACP, 0, leaf_.c_str(), -1, info->FileName, characters) != characters)
            return false;
        info->ReplaceIfExists = TRUE;
        info->RootDirectory = parentHandle_;
        info->FileNameLength = static_cast<DWORD>((characters - 1) * sizeof(WCHAR));
        return ::SetFileInformationByHandle(renameHandle_, FileRenameInfo, info.get(),
                                           static_cast<DWORD>(bytes)) != 0;
    }

    void cleanup() noexcept
    {
        close();
        if (renameHandle_ != INVALID_HANDLE_VALUE) {
            if (!published_) {
                FILE_DISPOSITION_INFO disposition = {};
                disposition.DeleteFile = TRUE;
                ::SetFileInformationByHandle(renameHandle_, FileDispositionInfo, &disposition, sizeof(disposition));
            }
            ::CloseHandle(renameHandle_);
            renameHandle_ = INVALID_HANDLE_VALUE;
        }
        if (parentHandle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(parentHandle_);
            parentHandle_ = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE parentHandle_ = INVALID_HANDLE_VALUE;
    HANDLE renameHandle_ = INVALID_HANDLE_VALUE;
#endif

    std::string parent_, leaf_, path_;
    FILE* file_ = nullptr;
    bool failed_ = false;
    bool published_ = false;
};

class HandleOutputBuffer final : public std::streambuf {
public:
    explicit HandleOutputBuffer(HandleOutput* output) : output_(output) {}
private:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        return output_ ? output_->write(data, count) : 0;
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        const char character = traits_type::to_char_type(value);
        return output_ && output_->write(&character, 1) == 1 ? value : traits_type::eof();
    }
    int sync() override { return output_ && output_->flush() ? 0 : -1; }
    HandleOutput* output_;
};

} // namespace detail
} // namespace file
} // namespace dommy
