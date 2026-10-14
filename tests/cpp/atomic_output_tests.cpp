// Production transport, real disposable files; no Maya/SDK I/O replacement.
#include "common/file/DAtomicFile.h"
#include "pdbIO.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <new>
#include <sstream>
#include <vector>

#if !defined(_WIN32)
#include <csignal>
#include <dirent.h>
#include <sys/resource.h>
#if defined(__APPLE__)
#include <membership.h>
#endif
#endif

namespace {

bool makeDirectory(const std::string& path)
{
#if defined(_WIN32)
    return ::CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
    return ::mkdir(path.c_str(), 0700) == 0;
#endif
}

void removeDirectory(const std::string& path)
{
#if defined(_WIN32)
    assert(::RemoveDirectoryA(path.c_str()) != 0);
#else
    assert(::rmdir(path.c_str()) == 0);
#endif
}

struct Scratch final {
    std::string root;
    std::vector<std::string> files, directories;
    Scratch()
    {
        const char* temporary = std::getenv("TMPDIR");
#if defined(_WIN32)
        if (!temporary) temporary = std::getenv("TEMP");
#endif
        const std::string base = temporary ? temporary : ".";
        root = dommy::file::makeTemporarySiblingPath(base + "/dommy-transport-tests");
        assert(makeDirectory(root));
    }
    ~Scratch()
    {
        for (auto it = files.rbegin(); it != files.rend(); ++it) std::remove(it->c_str());
        for (auto it = directories.rbegin(); it != directories.rend(); ++it) removeDirectory(*it);
        removeDirectory(root);
    }
    std::string file(const std::string& name)
    {
        const std::string path = root + '/' + name;
        files.push_back(path);
        return path;
    }
    std::string directory(const std::string& name)
    {
        const std::string path = root + '/' + name;
        assert(makeDirectory(path));
        directories.push_back(path);
        return path;
    }
};

std::string readBytes(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    assert(input.is_open());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void writeFixture(const std::string& path, const std::string& value)
{
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    assert(output.is_open());
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.close();
    assert(!output.fail());
}

template <typename Operation>
void expectException(Operation operation)
{
    bool rejected = false;
    try { operation(); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
}

void ordinaryOutputs(Scratch& scratch)
{
    using Output = dommy::file::PendingOutputFile;
    const std::string destination = scratch.file("bytes.bin");
    std::string bytes(130003U, '\0');
    for (std::size_t i = 0U; i < bytes.size(); ++i) bytes[i] = static_cast<char>(i % 127U);
    std::string temporary;
    {
        Output output(destination);
        temporary = output.diagnosticTemporaryPath();
        expectException([&] { (void)output.trustedTemporaryPath(); });
        expectException([&] { (void)output.temporaryPath(); });
        output.stream().write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        assert(output.close() && output.close());
        assert(output.commit() && output.commit());
    }
    assert(readBytes(destination) == bytes && !dommy::file::isRegularFile(temporary));
    {
        Output abandoned(destination);
        abandoned.stream() << "partial";
    }
    assert(readBytes(destination) == bytes);
    {
        Output empty(destination);
        assert(empty.commit());
    }
    assert(readBytes(destination).empty());
    {
        Output text(destination, Output::Access::Handle, Output::Format::Text);
        text.stream() << "first\nsecond\n";
        assert(text.commit());
    }
#if defined(_WIN32)
    assert(readBytes(destination) == "first\r\nsecond\r\n");
#else
    assert(readBytes(destination) == "first\nsecond\n");
#endif
    {
        Output sdk(destination, Output::Access::TrustedPath);
        expectException([&] { (void)sdk.stream(); });
        writeFixture(sdk.trustedTemporaryPath(), "SDK path-only output");
        assert(sdk.commit());
    }
    assert(readBytes(destination) == "SDK path-only output");
    {
        Output failed(destination);
        failed.stream() << "partial";
        failed.stream().setstate(std::ios::badbit);
        assert(!failed.close() && !failed.commit());
    }
    assert(readBytes(destination) == "SDK path-only output");
    {
        Output closed(destination);
        assert(closed.close());
        closed.stream() << "after close";
        closed.stream().clear();
        assert(!closed.close() && !closed.commit());
    }
    assert(readBytes(destination) == "SDK path-only output");
    const std::string folder = scratch.directory("cannot-replace-folder");
    {
        Output failed(folder);
        failed.stream() << "not a directory";
        assert(!failed.commit());
    }
    expectException([&] { Output invalid(scratch.root + "/absent/child"); });
    expectException([&] { Output invalid(std::string("path\0hidden", 11U)); });
    expectException([&] { (void)dommy::file::makeTemporarySiblingPathWithSuffix(destination, ":stream"); });
    expectException([&] { (void)dommy::file::makeTemporarySiblingPathWithSuffix(destination, std::string(".tmp\0x", 6U)); });
}

void pdbOutput(Scratch& scratch)
{
    pdb_io_t codec;
    codec.m_time = 2.5f;
    codec.m_num_particles = 2;
    pdb_io_t::attribute_t attribute;
    attribute.m_name = "position";
    attribute.m_type = pdb_io_t::kVector;
    attribute.m_num_elements = 1;
    attribute.m_vector_data.push_back(pdb_io_t::vec3_t{1.0f, 2.0f, 3.0f});
    attribute.m_vector_data.push_back(pdb_io_t::vec3_t{-1.0f, -2.0f, -3.0f});
    codec.m_attributes.push_back(attribute);
    std::ostringstream expected(std::ios::binary);
    assert(codec.write(expected));
    const std::string destination = scratch.file("particles.pdb");
    {
        dommy::file::PendingOutputFile output(destination);
        assert(codec.write(output.stream()));
        assert(output.commit());
    }
    assert(readBytes(destination) == expected.str());
    std::ifstream input(destination.c_str(), std::ios::binary);
    pdb_io_t roundTrip;
    assert(roundTrip.read(input));
    assert(roundTrip.m_num_particles == 2 && roundTrip.m_time == 2.5f);
    assert(roundTrip.m_attributes.size() == 1U);
    assert(roundTrip.m_attributes[0].m_vector_data[1].z == -3.0f);
}

void temporaryNames(Scratch& scratch)
{
    const std::string prefixes[] = {
        "", "nested/", "/", scratch.root + '/',
#if defined(_WIN32)
        "C:\\", "C:", "C:/nested/", "\\\\server\\share\\", "nested\\",
#endif
    };
    for (const std::string& prefix : prefixes) {
        const std::string destination = prefix + std::string(4096U, 'x') + ".vdb";
        const std::string first = dommy::file::makeTemporarySiblingPathWithSuffix(destination, ".vdb");
        const std::string second = dommy::file::makeTemporarySiblingPathWithSuffix(destination, ".vdb");
        assert(first.compare(0U, prefix.size(), prefix) == 0 && first != second);
        const std::string leaf = first.substr(prefix.size());
        assert(leaf.size() <= 128U && leaf.find_first_of("/\\:") == std::string::npos);
        assert(leaf.substr(leaf.size() - 4U) == ".vdb");
        const std::string maximumSuffix(32U, 's');
        const std::string longest = dommy::file::makeTemporarySiblingPathWithSuffix(destination, maximumSuffix);
        assert(longest.size() <= prefix.size() + 128U);
        assert(longest.substr(longest.size() - maximumSuffix.size()) == maximumSuffix);
    }
}

#if !defined(_WIN32)
std::string parentOf(const std::string& path) { return path.substr(0U, path.find_last_of('/')); }

void longOutputNames(Scratch& scratch)
{
    using Output = dommy::file::PendingOutputFile;
    const long nameLimit = ::pathconf(scratch.root.c_str(), _PC_NAME_MAX);
    const std::size_t boundary = nameLimit > 0 ?
        std::min<std::size_t>(static_cast<std::size_t>(nameLimit), 255U) : 255U;
    assert(boundary >= 128U);
    const std::size_t lengths[] = { std::min<std::size_t>(240U, boundary), boundary };
    for (const std::size_t length : lengths) {
        const std::string destination = scratch.file(std::string(length - 4U, 'x') + ".vdb");
        writeFixture(destination, "original"); // The final basename itself is valid.
        std::string temporary;
        {
            Output abandoned(destination, Output::Access::TrustedPath);
            temporary = abandoned.trustedTemporaryPath();
            assert(parentOf(temporary) == scratch.root);
            writeFixture(temporary, "discarded");
        }
        assert(readBytes(destination) == "original" && !dommy::file::isRegularFile(temporary));
        {
            Output sdk(destination, Output::Access::TrustedPath);
            temporary = sdk.trustedTemporaryPath();
            assert(temporary.size() <= scratch.root.size() + 129U);
            writeFixture(temporary, "SDK long-name output");
            assert(sdk.commit());
        }
        assert(readBytes(destination) == "SDK long-name output" && !dommy::file::isRegularFile(temporary));
        {
            Output owned(destination);
            owned.stream() << "owned long-name output";
            assert(owned.commit());
        }
        assert(readBytes(destination) == "owned long-name output");
        const std::string suffixed = dommy::file::createTemporarySiblingFileWithSuffix(destination, ".vdb");
        scratch.files.push_back(suffixed);
        assert(parentOf(suffixed) == scratch.root && dommy::file::isRegularFile(suffixed));
        assert(suffixed.substr(suffixed.size() - 4U) == ".vdb");
        assert(std::remove(suffixed.c_str()) == 0);
    }
}

void substitutedPaths(Scratch& scratch)
{
    using Output = dommy::file::PendingOutputFile;
    const std::string destination = scratch.file("substitution.bin");
    const std::string victim = scratch.file("disposable-victim.bin");
    writeFixture(destination, "original");
    writeFixture(victim, "untouched");
    for (int substitution = 0; substitution < 2; ++substitution) {
        Output output(destination);
        const std::string payload = output.diagnosticTemporaryPath();
        struct stat mode;
        assert(::stat(parentOf(payload).c_str(), &mode) == 0);
        assert((mode.st_mode & 0777) == 0700 && mode.st_uid == ::geteuid());
        assert(::unlink(payload.c_str()) == 0);
        if (substitution == 0) assert(::symlink(victim.c_str(), payload.c_str()) == 0);
        else writeFixture(payload, "replacement");
        output.stream() << "owned handle only";
        assert(output.close() && !output.commit());
        assert(readBytes(victim) == "untouched" && readBytes(destination) == "original");
    }

    const std::string parent = scratch.directory("original-parent");
    const std::string moved = scratch.root + "/moved-parent";
    scratch.directories.push_back(moved);
    scratch.files.push_back(moved + "/result.bin");
    {
        Output output(parent + "/result.bin");
        assert(::rename(parent.c_str(), moved.c_str()) == 0);
        assert(makeDirectory(parent));
        output.stream() << "pinned parent";
        assert(output.commit());
        assert(readBytes(moved + "/result.bin") == "pinned parent");
        assert(!dommy::file::isRegularFile(parent + "/result.bin"));
    }

    {
        Output output(destination);
        const std::string stage = parentOf(output.diagnosticTemporaryPath());
        const std::string movedStage = scratch.root + "/renamed-stage";
        assert(::rename(stage.c_str(), movedStage.c_str()) == 0);
        scratch.directories.push_back(movedStage);
        assert(makeDirectory(stage));
        scratch.directories.push_back(stage);
        output.stream() << "pinned private stage";
        assert(output.commit());
        assert(readBytes(destination) == "pinned private stage");
        // A hostile renamed entry is deliberately not deleted by pathname.
        // The test owns both empty directories and cleans them explicitly.
    }
}

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

std::string badName(const std::string&) { return "../escape"; }
std::string throwingName(const std::string&) { throw std::bad_alloc(); }
std::string collisionName(const std::string&) { return "collision-stage"; }

void reservationFailures(Scratch& scratch)
{
    const std::string destination = scratch.file("reservation.bin");
    scratch.directory("collision-stage");
    const std::size_t before = descriptorCount();
    for (unsigned int attempt = 0U; attempt < 40U; ++attempt) {
        expectException([&] { dommy::file::detail::HandleOutput output(destination, &badName, false); });
        expectException([&] { dommy::file::detail::HandleOutput output(destination, &throwingName, false); });
        expectException([&] { dommy::file::detail::HandleOutput output(destination, &collisionName, false); });
        { dommy::file::PendingOutputFile discarded(destination); discarded.stream() << "discard"; }
    }
    assert(descriptorCount() == before);
}

void limitedFileFailure(Scratch& scratch)
{
    const std::string destination = scratch.file("short-write.bin");
    writeFixture(destination, "original");
    struct rlimit original, limited;
    assert(::getrlimit(RLIMIT_FSIZE, &original) == 0);
    limited = original;
    limited.rlim_cur = 1024U;
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    assert(previousSignal != SIG_ERR);
    assert(::setrlimit(RLIMIT_FSIZE, &limited) == 0);
    {
        dommy::file::PendingOutputFile output(destination);
        const std::string bytes(8192U, 'x');
        output.stream().write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        assert(!output.close());
        output.stream().clear();
        assert(!output.close() && !output.commit());
    }
    assert(::setrlimit(RLIMIT_FSIZE, &original) == 0);
    assert(std::signal(SIGXFSZ, previousSignal) != SIG_ERR);
    assert(readBytes(destination) == "original");
}

#if defined(__APPLE__)
void inheritedAcl(Scratch& scratch)
{
    const std::string parent = scratch.directory("acl-parent");
    acl_t inherited = ::acl_init(1);
    assert(inherited);
    acl_entry_t entry;
    assert(::acl_create_entry(&inherited, &entry) == 0);
    assert(::acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == 0);
    uuid_t principal;
    assert(::mbr_uid_to_uuid(::geteuid(), principal) == 0);
    assert(::acl_set_qualifier(entry, principal) == 0);
    acl_permset_t permissions;
    assert(::acl_get_permset(entry, &permissions) == 0);
    assert(::acl_add_perm(permissions, ACL_ADD_FILE) == 0);
    assert(::acl_add_perm(permissions, ACL_ADD_SUBDIRECTORY) == 0);
    assert(::acl_add_perm(permissions, ACL_DELETE_CHILD) == 0);
    assert(::acl_set_permset(entry, permissions) == 0);
    acl_flagset_t flags;
    assert(::acl_get_flagset_np(entry, &flags) == 0);
    assert(::acl_add_flag_np(flags, ACL_ENTRY_DIRECTORY_INHERIT) == 0);
    assert(::acl_add_flag_np(flags, ACL_ENTRY_FILE_INHERIT) == 0);
    assert(::acl_set_flagset_np(entry, flags) == 0);
    assert(::acl_set_file(parent.c_str(), ACL_TYPE_EXTENDED, inherited) == 0);
    assert(::acl_free(inherited) == 0);
    const std::string probe = scratch.directory("acl-parent/inheritance-probe");
    acl_t probeAcl = ::acl_get_file(probe.c_str(), ACL_TYPE_EXTENDED);
    assert(probeAcl && ::acl_get_entry(probeAcl, ACL_FIRST_ENTRY, &entry) == 0);
    assert(::acl_free(probeAcl) == 0);

    const std::string destination = scratch.file("acl-parent/result.bin");
    {
        dommy::file::PendingOutputFile output(destination);
        const std::string stage = parentOf(output.diagnosticTemporaryPath());
        acl_t privateAcl = ::acl_get_file(stage.c_str(), ACL_TYPE_EXTENDED);
        if (privateAcl) {
            assert(::acl_get_entry(privateAcl, ACL_FIRST_ENTRY, &entry) != 0);
            assert(::acl_free(privateAcl) == 0);
        } else assert(errno == ENOENT);
        output.stream() << "private ACL";
        assert(output.commit());
    }
    assert(readBytes(destination) == "private ACL");
}
#endif
#endif

} // namespace

void runAtomicOutputTests()
{
    Scratch scratch;
    ordinaryOutputs(scratch);
    pdbOutput(scratch);
    temporaryNames(scratch);
#if !defined(_WIN32)
    longOutputNames(scratch);
    substitutedPaths(scratch);
    reservationFailures(scratch);
    limitedFileFailure(scratch);
#if defined(__APPLE__)
    inheritedAcl(scratch);
#endif
#endif
}

#ifdef DOMMY_ATOMIC_OUTPUT_TEST_MAIN
int main() { runAtomicOutputTests(); }
#endif
