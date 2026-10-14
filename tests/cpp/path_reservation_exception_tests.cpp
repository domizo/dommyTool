// Real disposable files and production helpers. The syscall seam observes a
// successful exclusive reservation; only subsequent C++ allocation is faulted.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <type_traits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#endif
#endif

namespace fault {
enum class Mode { None, BeforeReservation, AfterReservation, CollisionOnce };
Mode mode = Mode::None;
bool failAllocation = false;
unsigned int failures = 0U, reservations = 0U;
char reservedPath[4096] = {}, collisionPath[4096] = {};

void record(char* target, const char* path)
{
    const std::size_t size = std::strlen(path);
    assert(size < 4096U);
    std::memcpy(target, path, size + 1U);
}

void created(const char* path)
{
    record(reservedPath, path);
    ++reservations;
    if (mode == Mode::AfterReservation) failAllocation = true;
}

void reset(Mode next)
{
    mode = next;
    failAllocation = next == Mode::BeforeReservation;
    failures = reservations = 0U;
    reservedPath[0] = collisionPath[0] = '\0';
}
} // namespace fault

void* operator new(std::size_t size)
{
    if (fault::failAllocation) {
        ++fault::failures;
        throw std::bad_alloc();
    }
    void* result = std::malloc(size ? size : 1U);
    if (!result) throw std::bad_alloc();
    return result;
}

void operator delete(void* value) noexcept { std::free(value); }

#if defined(_WIN32)
HANDLE reservationCreateFileA(LPCSTR path, DWORD access, DWORD sharing,
    LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD attributes, HANDLE templateFile)
{
    HANDLE result = ::CreateFileA(path, access, sharing, security, creation, attributes, templateFile);
    if (result != INVALID_HANDLE_VALUE && creation == CREATE_NEW) {
        if (fault::mode == fault::Mode::CollisionOnce) {
            fault::record(fault::collisionPath, path);
            DWORD written = 0;
            assert(::WriteFile(result, "collision", 9U, &written, nullptr) && written == 9U);
            assert(::CloseHandle(result));
            fault::mode = fault::Mode::None;
            ::SetLastError(ERROR_FILE_EXISTS);
            return INVALID_HANDLE_VALUE;
        }
        fault::created(path);
    }
    return result;
}
#define CreateFileA reservationCreateFileA
#else
int reservationOpen(const char* path, int flags) { return ::open(path, flags); }

int reservationOpen(const char* path, int flags, mode_t permissions)
{
    const int result = ::open(path, flags, permissions);
    if (result != -1 && (flags & O_EXCL) != 0) {
        if (fault::mode == fault::Mode::CollisionOnce) {
            fault::record(fault::collisionPath, path);
            assert(::write(result, "collision", 9U) == 9);
            assert(::close(result) == 0);
            fault::mode = fault::Mode::None;
            errno = EEXIST;
            return -1;
        }
        fault::created(path);
    }
    return result;
}
#define open reservationOpen
#endif

#include "common/file/DAtomicFile.h"

#if defined(_WIN32)
#undef CreateFileA
#else
#undef open
#endif

using ReservationCleanup = dommy::file::detail::ScopedReservedPathCleanup;
static_assert(std::is_nothrow_constructible<ReservationCleanup, const std::string&>::value &&
              !std::is_copy_constructible<ReservationCleanup>::value &&
              !std::is_move_constructible<ReservationCleanup>::value &&
              !std::is_constructible<ReservationCleanup, std::string&&>::value &&
              !std::is_constructible<ReservationCleanup, const std::string&&>::value,
              "Reservation cleanup must reference a stable lvalue without allocating or sharing ownership");

namespace {
std::string readBytes(const std::string& path)
{
    // Avoid libc++'s iterator-only scope-guard linkage under -fno-elide-
    // constructors. The fixture payloads are bounded, literal byte strings.
    FILE* stream = std::fopen(path.c_str(), "rb");
    assert(stream);
    char bytes[32];
    const std::size_t count = std::fread(bytes, 1U, sizeof(bytes), stream);
    assert(count < sizeof(bytes) && !std::ferror(stream));
    assert(std::fclose(stream) == 0);
    return std::string(bytes, count);
}

void reserveAndDiscard(const std::string& destination, unsigned int kind)
{
    if (kind == 2U) {
        dommy::file::PendingOutputFile output(destination,
            dommy::file::PendingOutputFile::Access::TrustedPath);
        assert(dommy::file::isRegularFile(output.trustedTemporaryPath()));
    } else {
        const std::string path = kind == 0U ?
            dommy::file::createTemporarySiblingFileWithSuffix(destination, ".vdb") :
            dommy::file::createTemporarySiblingFile(destination);
        assert(dommy::file::isRegularFile(path));
        assert(std::remove(path.c_str()) == 0);
    }
}

void failedAllocation(const std::string& destination, unsigned int kind, fault::Mode mode)
{
    fault::reset(mode);
    bool caught = false;
    try { reserveAndDiscard(destination, kind); }
    catch (const std::bad_alloc&) { caught = true; }
    fault::failAllocation = false;
    assert(caught && fault::failures == 1U);
    if (mode == fault::Mode::AfterReservation) {
        assert(fault::reservations == 1U && fault::reservedPath[0]);
        assert(!dommy::file::isRegularFile(fault::reservedPath));
    } else assert(fault::reservations == 0U && !fault::reservedPath[0]);
    assert(readBytes(destination) == "original");
}

#if !defined(_WIN32)
std::size_t descriptorCount()
{
    DIR* directory = ::opendir("/dev/fd");
    assert(directory);
    std::size_t count = 0U;
    while (const dirent* entry = ::readdir(directory)) {
        if (entry->d_name[0] != '.') ++count;
    }
    assert(::closedir(directory) == 0);
    return count;
}
#endif
} // namespace

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string destination = std::string(argv[1]) + "/original.bin";
    {
        std::ofstream stream(destination.c_str(), std::ios::binary);
        assert(stream.is_open());
        stream << "original";
        stream.close();
        assert(!stream.fail());
    }
#if !defined(_WIN32)
    const std::size_t descriptors = descriptorCount();
#endif
    for (unsigned int kind = 0U; kind < 3U; ++kind) {
        fault::reset(fault::Mode::None);
        reserveAndDiscard(destination, kind);
        assert(fault::reservations == 1U && !dommy::file::isRegularFile(fault::reservedPath));
        for (unsigned int attempt = 0U; attempt < 16U; ++attempt) {
            failedAllocation(destination, kind, fault::Mode::BeforeReservation);
            failedAllocation(destination, kind, fault::Mode::AfterReservation);
        }
        // A failed exclusive-create attempt must never arm deletion of the
        // colliding entry. The next attempt reserves and transfers normally.
        fault::reset(fault::Mode::CollisionOnce);
        reserveAndDiscard(destination, kind);
        assert(fault::reservations == 1U && fault::collisionPath[0]);
        assert(readBytes(fault::collisionPath) == "collision");
        assert(std::remove(fault::collisionPath) == 0);
        assert(!dommy::file::isRegularFile(fault::reservedPath));
        assert(readBytes(destination) == "original");
    }
    fault::reset(fault::Mode::None);
    bool rejected = false;
    try { reserveAndDiscard(std::string(argv[1]) + "/absent/result.bin", 0U); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && fault::reservations == 0U);
#if !defined(_WIN32)
    assert(descriptorCount() == descriptors);
#endif
    assert(readBytes(destination) == "original" && std::remove(destination.c_str()) == 0);
    std::puts("Path reservation: 96 allocation failures, transfer/discard, collisions and original preservation passed");
}
