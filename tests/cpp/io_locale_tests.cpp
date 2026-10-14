// Production text/sequence helpers; no Maya/SDK substitutes or installed locale
// packages. Facets affect only this test process and are restored after each case.
#include "common/file/DLegacyTextData.h"
#include "common/file/DSequencePath.h"
#include "numeric_locale_fixture.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace {

void checkReaders(const std::locale& active)
{
    const std::string pointText =
        "POINT_DATA_FROM_MESH mesh 3\n"
        "P 1.234 -2.345 6.789\nP 0.25 -0.5 1\nP 1.25e3 -4.5e-2 -0\n";
    for (bool localizedInput : {false, true}) {
        std::istringstream input(pointText);
        const std::locale inputLocale = localizedInput ? active : std::locale::classic();
        input.imbue(inputLocale);
        std::vector<dommy::file::Triple> points;
        assert(dommy::file::readPointData(input, points) && points.size() == 3U);
        assert(points[0][0] == 1.234 && points[0][1] == -2.345 && points[0][2] == 6.789);
        assert(points[1][0] == 0.25 && points[1][1] == -0.5 && points[1][2] == 1.0);
        assert(points[2][0] == 1250.0 && points[2][1] == -0.045 && std::signbit(points[2][2]));
        assert(input.getloc() == inputLocale && std::locale() == active);
    }

    std::ostringstream canonicalColors;
    canonicalColors.imbue(std::locale::classic());
    canonicalColors << "VTXCOLOR 1025\n";
    for (unsigned int index = 0U; index < 1025U; ++index)
        canonicalColors << "VC 1.234 0.25 -2.345\n";
    std::istringstream colorsInput(canonicalColors.str());
    colorsInput.imbue(active);
    std::vector<dommy::file::Triple> values;
    assert(dommy::file::readVertexColors(colorsInput, values) && values.size() == 1025U);
    assert(values.back()[0] == 1.234 && values.back()[1] == 0.25 && values.back()[2] == -2.345);
    assert(colorsInput.getloc() == active && std::locale() == active);

    // Do not guess a locale from ambiguous delimiters. The format has no locale
    // tag; noncanonical commas/grouped counts must fail and clear partial data.
    for (const std::string& malformed : {
        "POINT_DATA_FROM_MESH mesh 1\nP 1,234 2 3\n",
        "POINT_DATA_FROM_MESH mesh 1,025\nP 1 2 3\n",
        "POINT_DATA_FROM_MESH mesh 2\nP 1 2 3\nP 0,25 2 3\n"}) {
        std::istringstream input(malformed);
        values.push_back(dommy::file::Triple{{9, 9, 9}});
        assert(!dommy::file::readPointData(input, values) && values.empty());
        assert(std::locale() == active);
    }
    for (const std::string& malformed : {
        "VTXCOLOR 1\nVC 1,234 2 3\n", "VTXCOLOR 1,025\nVC 1 2 3\n",
        "VTXCOLOR 2\nVC 1 2 3\nVC 0,25 2 3\n"}) {
        std::istringstream input(malformed);
        values.push_back(dommy::file::Triple{{9, 9, 9}});
        assert(!dommy::file::readVertexColors(input, values) && values.empty());
        assert(std::locale() == active);
    }
}

void checkSequencePaths(const std::locale& active)
{
    std::string result;
    assert(dommy::file::formatSignedInteger(1234, 0U, result) && result == "1234");
    assert(dommy::file::formatSignedInteger(-1234, 8U, result) && result == "-00001234");
    assert(dommy::file::formatSignedInteger(0, 4U, result) && result == "0000");
    assert(dommy::file::formatSignedInteger(LLONG_MIN, 0U, result) && result == "-9223372036854775808");
    assert(dommy::file::formatSignedInteger(LLONG_MAX, 0U, result) && result == "9223372036854775807");
    assert(dommy::file::formatSignedInteger(-7, 32U, result) && result == '-' + std::string(31U, '0') + '7');
    result = "unchanged";
    assert(!dommy::file::formatSignedInteger(1, 33U, result) && result == "unchanged");
    assert(dommy::file::replaceFrameToken("cache.$F/$F.vtx", -1234, 6U, result) &&
           result == "cache.-001234/-001234.vtx");
    assert(dommy::file::replaceLegacyFrameToken("cache.$F4.$FF", 1234.5, result) &&
           result == "cache.1234.1234.50");
    assert(dommy::file::replaceLegacyFrameToken("cache.$F4.$FF", -1234.5, result) &&
           result == "cache.-1234.-1234.50");
    assert(dommy::file::replaceLegacyFrameToken("$F6/$F/cache.$FF", 1.25, result) &&
           result == "000001/1/cache.1.25");
    assert(dommy::file::replaceHashFrameToken("volume.####.vdb", 1234.5, 24.0, 1, result) &&
           result == "volume.1234.5.vdb");
    assert(dommy::file::replaceHashFrameToken("volume.####.vdb", -1.25, 24.0, 0, result) &&
           result == "volume.-2.188.vdb");
    assert(dommy::file::replaceHashFrameToken("volume.####.vdb", -1.25, 24.0, 2, result) &&
           result == "volume.-0313.vdb");
    assert(std::locale() == active);

    for (double frame : {1234.5, -1234.5, 0.0, -0.25, 1.25, 1e15}) {
        for (double fps : {24.0, 30.0, 23.976}) {
            for (int scheme = 0; scheme < 3; ++scheme) {
                std::string expected;
                {
                    dommy_test::ScopedNumericLocale classic(std::locale::classic());
                    assert(dommy::file::replaceHashFrameToken(
                        "cache.####/take.######.vdb", frame, fps, scheme, expected));
                }
                assert(std::locale() == active);
                assert(dommy::file::replaceHashFrameToken(
                    "cache.####/take.######.vdb", frame, fps, scheme, result) && result == expected);
                assert(std::locale() == active);
            }
        }
    }
    assert(!dommy::file::replaceLegacyFrameToken("cache.$F33", 1.0, result));
    assert(!dommy::file::replaceLegacyFrameToken("cache.$FF", std::numeric_limits<double>::infinity(), result));
    assert(!dommy::file::replaceHashFrameToken("cache.#", 1.0, 0.0, 1, result));
    assert(!dommy::file::replaceHashFrameToken("cache.#", 1.0, 24.0, 3, result));
    assert(!dommy::file::replaceHashFrameToken("cache.#", std::numeric_limits<double>::quiet_NaN(), 24.0, 1, result));
    assert(std::locale() == active);
}

void checkSequenceIntegerBoundaries()
{
    const double upper = std::ldexp(1.0, std::numeric_limits<long long>::digits);
    const double largestDouble = std::nextafter(upper, 0.0);
    const double belowMinimum = std::nextafter(-upper, -std::numeric_limits<double>::infinity());
    const double boundaryFps = std::ldexp(6000.0, -std::numeric_limits<long long>::digits);
    for (int scheme = 0; scheme < 3; ++scheme) {
        for (double invalidFrame : {upper, std::nextafter(upper, std::numeric_limits<double>::infinity()), belowMinimum}) {
            std::string result("unchanged");
            assert(!dommy::file::replaceHashFrameToken("cache.####.vdb", invalidFrame, 6000.0, scheme, result));
            assert(result == "unchanged");
        }
        std::string result;
        assert(dommy::file::replaceHashFrameToken("cache.####.vdb", largestDouble, 6000.0, scheme, result));
        assert(!result.empty());
        assert(dommy::file::replaceHashFrameToken("cache.####.vdb", -upper, 6000.0, scheme, result));
        if (scheme != 1) assert(result == "cache.-9223372036854775808.vdb");
        for (double invalidFps : {boundaryFps, std::nextafter(boundaryFps, 0.0)}) {
            result = "unchanged";
            assert(!dommy::file::replaceHashFrameToken("cache.####.vdb", 0.0, invalidFps, scheme, result));
            assert(result == "unchanged");
        }
        assert(dommy::file::replaceHashFrameToken("cache.####.vdb", 0.0,
            std::nextafter(boundaryFps, std::numeric_limits<double>::infinity()), scheme, result));
    }
    std::string result("unchanged");
    assert(!dommy::file::replaceHashFrameToken("cache.####.vdb", upper / 2.0, 3000.0, 2, result));
    assert(result == "unchanged");
    assert(dommy::file::replaceHashFrameToken("cache.####.vdb", -upper / 2.0, 3000.0, 2, result));
    assert(result == "cache.-9223372036854775808.vdb");
    long long integer = 17;
    assert(!dommy::file::checkedTruncatedFrame(upper, integer) && integer == 17);
    assert(dommy::file::checkedTruncatedFrame(-upper, integer) && integer == LLONG_MIN);
}

void checkInPlaceSequences()
{
    std::string legacy("cache.$F4.$FF.vdb");
    assert(dommy::file::replaceLegacyFrameToken(legacy, 1.25, legacy) && legacy == "cache.0001.1.25.vdb");
    std::string hash("cache.####.vdb");
    assert(dommy::file::replaceHashFrameToken(hash, 1.25, 24.0, 1, hash) && hash == "cache.1.25.vdb");
    std::string unchanged("cache.without-tokens.vdb");
    assert(dommy::file::replaceLegacyFrameToken(unchanged, 1.25, unchanged) && unchanged == "cache.without-tokens.vdb");
    assert(dommy::file::replaceHashFrameToken(unchanged, 1.25, 24.0, 1, unchanged) && unchanged == "cache.without-tokens.vdb");
    for (bool inPlace : {false, true}) {
        std::string invalidLegacy("prefix/$F4/bad.$F33.vdb");
        const std::string legacyBefore(invalidLegacy);
        std::string result("sentinel");
        std::string& legacyOutput = inPlace ? invalidLegacy : result;
        assert(!dommy::file::replaceLegacyFrameToken(invalidLegacy, 1.25, legacyOutput));
        assert(invalidLegacy == legacyBefore && result == "sentinel");
        std::string invalidHash = "prefix/####/bad." + std::string(33U, '#') + ".vdb";
        const std::string hashBefore(invalidHash);
        std::string& hashOutput = inPlace ? invalidHash : result;
        assert(!dommy::file::replaceHashFrameToken(invalidHash, 1.25, 24.0, 1, hashOutput));
        assert(invalidHash == hashBefore && result == "sentinel");
    }
}

} // namespace

void runIoLocaleTests()
{
    const std::locale original;
    {
        dommy_test::ScopedNumericLocale classic(std::locale::classic());
        checkReaders(classic.selected());
        checkSequencePaths(classic.selected());
    }
    for (unsigned int variant = 0U; variant < 2U; ++variant) {
        dommy_test::ScopedNumericLocale localized(variant == 0U ? ',' : '.',
                                                 variant == 0U ? '.' : ',');
        checkReaders(localized.selected());
        checkSequencePaths(localized.selected());
    }
    assert(std::locale() == original);
    checkSequenceIntegerBoundaries();
    checkInPlaceSequences();
    std::puts("Text/sequence locale invariance, canonical admission and host-state preservation: OK");
}

#ifdef DOMMY_IO_LOCALE_TEST_MAIN
int main() { runIoLocaleTests(); }
#endif
