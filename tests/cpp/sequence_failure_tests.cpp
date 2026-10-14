// Production string helpers under deterministic allocation failure. This is a
// separate executable so the global allocator seam cannot affect other tests.
#include "common/file/DAtomicFile.h"
#include "common/file/DSequencePath.h"
#include "numeric_locale_fixture.h"
#include "allocation_failure_fixture.h"

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
enum class Kind { Integer, Padding, Frame, Legacy, Hash, Temporary };

bool generate(Kind kind, const std::string& source, std::string& result)
{
    switch (kind) {
    case Kind::Integer:
        return dommy::file::formatSignedInteger(LLONG_MIN, 32U, result);
    case Kind::Padding:
        return dommy::file::padSignedValue(source, 32U, result);
    case Kind::Frame:
        return dommy::file::replaceFrameToken(source, -1234, 6U, result);
    case Kind::Legacy:
        return dommy::file::replaceLegacyFrameToken(source, 1e100, result);
    case Kind::Hash:
        return dommy::file::replaceHashFrameToken(source, 1234567890123456.0, 24.0, 1, result);
    case Kind::Temporary:
        result = dommy::file::makeTemporarySiblingPathWithSuffix(source, ".vdb");
        return true;
    }
    return false;
}

void checkTemporaryName(const std::string& result)
{
    const std::string prefix("/owned/deep/folder/output.dommytool-");
    const std::string suffix(".vdb");
    assert(result.compare(0U, prefix.size(), prefix) == 0);
    assert(result.size() > prefix.size() + suffix.size());
    assert(result.compare(result.size() - suffix.size(), suffix.size(), suffix) == 0);
    // These three fields must be complete, canonical digits, never locale
    // grouping or a truncated stream buffer. The factory performs no file I/O.
    std::size_t fields = 1U, fieldDigits = 0U;
    for (std::size_t index = prefix.size(); index < result.size() - suffix.size(); ++index) {
        const char value = result[index];
        if (value == '-') {
            assert(fieldDigits != 0U);
            ++fields;
            fieldDigits = 0U;
        } else {
            assert(value >= '0' && value <= '9');
            ++fieldDigits;
        }
    }
    assert(fields == 3U && fieldDigits != 0U);
}

void checkFailures(Kind kind, const std::string& source, bool inPlace)
{
    std::string expected;
    assert(generate(kind, source, expected)); // Warm up the library before faulting.
    if (kind == Kind::Temporary) checkTemporaryName(expected);
    unsigned int failures = 0U, rejections = 0U;
    for (std::size_t selected = 1U; selected <= 64U; ++selected) {
        std::string input(source), separate("unchanged-output");
        std::string& result = inPlace ? input : separate;
        const std::string before(result);
        fault::arm(selected);
        bool accepted = false;
        try {
            accepted = generate(kind, input, result);
        } catch (const std::bad_alloc&) {
            // Container/string construction propagates this to the caller.
        } catch (const std::runtime_error&) {
            // Temporary-name formatting explicitly rejects a failed stream.
            assert(kind == Kind::Temporary && fault::triggered);
        }
        fault::armed = false;
        if (fault::triggered) ++failures;
        if (accepted) {
            if (kind == Kind::Temporary) checkTemporaryName(result);
            else assert(result == expected);
        } else {
            ++rejections;
            assert(fault::triggered && result == before);
        }
        if (!inPlace) assert(input == source);
    }
    assert(failures != 0U && rejections != 0U);
}
} // namespace

int main()
{
    const std::string prefix(128U, 'x');
    checkFailures(Kind::Integer, "unused", false);
    for (bool inPlace : {false, true}) {
        checkFailures(Kind::Padding, "-7", inPlace);
        checkFailures(Kind::Frame, prefix + "/cache.$F/$F.vdb", inPlace);
        checkFailures(Kind::Legacy, prefix + "/cache.$FF.vdb", inPlace);
        checkFailures(Kind::Hash, prefix + "/cache.####.vdb", inPlace);
    }
    checkFailures(Kind::Temporary, "/owned/deep/folder/result.vdb", false);
    const std::locale original;
    for (unsigned int variant = 0U; variant < 2U; ++variant) {
        dommy_test::ScopedNumericLocale localized(variant == 0U ? ',' : '.',
                                                 variant == 0U ? '.' : ',');
        checkTemporaryName(dommy::file::makeTemporarySiblingPathWithSuffix(
            "/owned/deep/folder/result.vdb", ".vdb"));
        assert(std::locale() == localized.selected());
    }
    assert(std::locale() == original);
    std::puts("Sequence allocation failures, complete temporary names and transactional outputs: OK");
}
