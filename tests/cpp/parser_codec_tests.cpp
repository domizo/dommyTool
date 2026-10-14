// The shared parser header and complete point writer/wrapper/ASS reader bodies
// execute below. There is no Maya scene, MArgList codec or Arnold file transport.
#include "parser_codec_host.h"
#include "numeric_locale_fixture.h"
#include "allocation_failure_fixture.h"
#include "common/file/DLegacyTextData.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <streambuf>
#include "parser_codec_methods.inc"

namespace {
void checkPatternFailures()
{
    const std::string zeros(40U, '0');
    for (const std::string& pattern : {"2 " + zeros + "1-3 !",
         "! 2 " + zeros + "1-3", "0;" + zeros + "2-5;8"}) {
        bool expectedReverse = false;
        std::vector<std::size_t> expected;
        assert(dommy::component::parseIndexPattern(pattern, 10U, 10U, expectedReverse, expected));
        unsigned int faults = 0U, rejections = 0U;
        for (std::size_t selected = 1U; selected <= 64U; ++selected) {
            bool reverse = true;
            std::vector<std::size_t> indices(1U, 99U);
            fault::arm(selected);
            bool accepted = false;
            try { accepted = dommy::component::parseIndexPattern(pattern, 10U, 10U, reverse, indices); }
            catch (const std::bad_alloc&) {}
            fault::armed = false;
            if (fault::triggered) ++faults;
            if (accepted) {
                assert(reverse == expectedReverse && indices == expected);
                std::vector<bool> actualSelection(10U, false), expectedSelection(10U, false);
                assert(dommy::component::mergeIndexPatternSelection(indices, reverse, actualSelection));
                assert(dommy::component::mergeIndexPatternSelection(expected, expectedReverse, expectedSelection));
                assert(actualSelection == expectedSelection);
            } else {
                ++rejections;
                assert(fault::triggered && reverse && indices == std::vector<std::size_t>(1U, 99U));
            }
        }
        assert(faults != 0U && rejections != 0U);
    }
    for (const std::string& empty : {"", " \t\r\n", "; ;"}) {
        bool reverse = true;
        std::vector<std::size_t> indices(1U, 99U);
        assert(dommy::component::parseIndexPattern(empty, 10U, 0U, reverse, indices));
        assert(!reverse && indices.empty());
    }
    bool reverse = true;
    std::vector<std::size_t> indices(1U, 99U);
    assert(!dommy::component::parseIndexPattern("+1", 10U, 10U, reverse, indices));
    assert(reverse && indices == std::vector<std::size_t>(1U, 99U));
    std::puts("Index pattern failures preserve indices/reverse and never publish a partial selection: OK");
}

void checkPointLocales()
{
    DPData data;
    data.pointArray.assign(1025U, DPoint{{1.25f, -2.5f, 3.75f}});
    std::ostringstream baseline;
    {
        dommy_test::ScopedNumericLocale classic(std::locale::classic());
        assert(data.writeASCII(baseline) == MS::kSuccess);
    }
    const std::string expected = baseline.str();
    assert(expected.find("\n\n\t\t\"p\"   1025\n\t\t1.25   -2.5   3.75") == 0U);
    const std::locale prior;
    for (unsigned int variant = 0U; variant < 2U; ++variant) {
        dommy_test::ScopedNumericLocale localized(variant == 0U ? ',' : '.',
                                                 variant == 0U ? '.' : ',');
        for (bool localizedOutput : {false, true}) {
            std::ostringstream out;
            const std::locale selected = localizedOutput ? localized.selected() : std::locale::classic();
            out.imbue(selected);
            out.precision(2);
            const auto flags = out.flags();
            assert(data.writeASCII(out) == MS::kSuccess && out.str() == expected);
            assert(out.getloc() == selected && out.flags() == flags && out.precision() == 2);
            assert(std::locale() == localized.selected());
        }
    }
    assert(std::locale() == prior);

    DPData extrema;
    extrema.pointArray.push_back(DPoint{{-0.0f, std::numeric_limits<float>::denorm_min(),
                                       std::numeric_limits<float>::max()}});
    std::ostringstream normal;
    assert(extrema.writeASCII(normal) == MS::kSuccess);
    {
        dommy_test::ScopedNumericLocale localized(',', '.');
        std::ostringstream out;
        assert(extrema.writeASCII(out) == MS::kSuccess && out.str() == normal.str());
    }
    for (bool groups : {false, true}) {
        DPData unsupported;
        (groups ? unsupported.mGrooupList : unsupported.mAttributeList).push_back(1);
        std::ostringstream out;
        out << "unchanged";
        assert(unsupported.writeASCII(out) == MS::kFailure && out.str() == "unchanged");
    }
    std::puts("Position-only ASCII: canonical bytes/counts, float extrema, borrowed state and unsupported payload rejection: OK");
}

class FixedOutput : public std::streambuf {
    std::vector<char> storage_;
public:
    explicit FixedOutput(std::size_t capacity) : storage_(capacity) {
        setp(storage_.data(), storage_.data() + storage_.size());
    }
    std::size_t written() const { return static_cast<std::size_t>(pptr() - pbase()); }
    std::string bytes() const { return std::string(pbase(), pptr()); }
};

void checkPointAllocationFailures()
{
    DPData data;
    data.pointArray.assign(1025U, DPoint{{1.25f, -2.5f, 3.75f}});
    std::ostringstream baseline;
    assert(data.writeASCII(baseline) == MS::kSuccess);
    const std::string expected = baseline.str();
    unsigned int faults = 0U, rejections = 0U;
    for (std::size_t selected = 1U; selected <= 64U; ++selected) {
        // Preallocate the transport so this seam measures serialization only,
        // not partial failure of Maya's borrowed output stream.
        FixedOutput storage(expected.size());
        std::ostream out(&storage);
        fault::arm(selected);
        const MStatus status = data.writeASCII(out);
        fault::armed = false;
        if (fault::triggered) ++faults;
        if (status == MS::kSuccess) assert(storage.bytes() == expected);
        else {
            ++rejections;
            assert(fault::triggered && storage.written() == 0U);
        }
    }
    assert(faults != 0U && rejections != 0U);
}

bool samePoint(const MPoint& first, const MPoint& second)
{
    return first.x == second.x && first.y == second.y && first.z == second.z;
}

void checkNumericTokenContract()
{
    using dommy::file::NumericTokenRead;
    for (const std::string& text : {"", " \t\r"}) {
        std::istringstream input(text);
        double value = 42.0;
        assert(dommy::file::readFiniteDoubleToken(input, value) == NumericTokenRead::End && value == 42.0);
    }
    for (const std::string& text : {"4e+", "1e309", "6junk", "6,5", "6.5.0"}) {
        std::istringstream input(text);
        double value = 42.0;
        assert(dommy::file::readFiniteDoubleToken(input, value) == NumericTokenRead::Invalid && value == 42.0);
    }
    std::istringstream valid(" -0 1.25e3 6.5\tmetadata");
    valid.imbue(std::locale::classic());
    double value = 42.0;
    assert(dommy::file::readFiniteDoubleToken(valid, value) == NumericTokenRead::Value && std::signbit(value));
    assert(dommy::file::readFiniteDoubleToken(valid, value) == NumericTokenRead::Value && value == 1250.0);
    assert(dommy::file::readFiniteDoubleToken(valid, value) == NumericTokenRead::Value && value == 6.5);
    assert(valid.getloc() == std::locale::classic());
    for (bool allowHash : {false, true}) {
        std::istringstream input("6#comment");
        value = 42.0;
        assert(dommy::file::readFiniteDoubleToken(input, value, allowHash) ==
               (allowHash ? NumericTokenRead::Value : NumericTokenRead::Invalid));
        assert(value == (allowHash ? 6.0 : 42.0));
    }
    std::istringstream failed("6");
    failed.setstate(std::ios::badbit);
    value = 42.0;
    assert(dommy::file::readFiniteDoubleToken(failed, value) == NumericTokenRead::Invalid && value == 42.0);
}

bool readLegacy(bool colors, std::istream& input, std::vector<dommy::file::Triple>& output)
{
    return colors ? dommy::file::readVertexColors(input, output) :
                    dommy::file::readPointData(input, output);
}

void checkLegacyCountTokens()
{
    for (bool colors : {false, true}) {
        for (const std::string& count : {"-18446744073709551615", "-18446744073709551614",
             "-1", "-0", "18446744073709551616", "10000001", "1junk", "1.0", "1,0", "+"}) {
            const std::string header = colors ? "VTXCOLOR " : "POINT_DATA_FROM_MESH mesh ";
            const std::string record = colors ? "VC 1 2 3\n" : "P 1 2 3\n";
            std::istringstream input(header + count + "\n" + record +
                (count == "-18446744073709551614" ? record : ""));
            std::vector<dommy::file::Triple> output(1U, dommy::file::Triple{{91, 92, 93}});
            assert(!readLegacy(colors, input, output) && output.empty());
        }
        for (const std::string& count : {"1", "+1", "0001", "+0001"}) {
            const std::string header = colors ? "VTXCOLOR " : "POINT_DATA_FROM_MESH mesh ";
            std::istringstream input(header + count + "\n" +
                (colors ? "VC 1 2 3\n" : "P 1 2 3\n"));
            std::vector<dommy::file::Triple> output;
            assert(readLegacy(colors, input, output) && output.size() == 1U && output[0][2] == 3.0);
        }
        const std::string header = colors ? "VTXCOLOR +0000" : "POINT_DATA_FROM_MESH mesh +0000";
        std::istringstream empty(header);
        std::vector<dommy::file::Triple> output;
        assert(readLegacy(colors, empty, output) && output.empty());
    }
    std::puts("Legacy counts: negative/wrapped/overflow tokens rejected; plus and leading zeros preserved: OK");
}

void checkUnsignedDecimalContract()
{
    const std::string maximum = std::to_string(std::numeric_limits<std::size_t>::max());
    for (const std::string& token : {"", "-0", "-1", "+", " 1", "1 ", "1.0", "1e2", "1,000"}) {
        std::size_t value = 42U;
        assert(!dommy::numeric::parseUnsignedDecimal(token, value, true) && value == 42U);
    }
    for (const std::string& token : {maximum + "0", std::string("1\0x", 3U), std::string("\xD9\xA1")}) {
        std::size_t value = 42U;
        assert(!dommy::numeric::parseUnsignedDecimal(token, value, true) && value == 42U);
    }
    for (bool plus : {false, true}) {
        std::size_t value = 42U;
        assert(dommy::numeric::parseUnsignedDecimal(maximum, value, plus) &&
               value == std::numeric_limits<std::size_t>::max());
        value = 42U;
        assert(dommy::numeric::parseUnsignedDecimal("+0001", value, plus) == plus);
        assert(value == (plus ? 1U : 42U));
        assert(dommy::numeric::parseUnsignedDecimal("0000", value, plus) && value == 0U);
    }
    for (const std::string& token : {"", "-18446744073709551615", "1junk"}) {
        std::istringstream input(token);
        const auto mask = input.exceptions();
        const auto locale = input.getloc();
        std::size_t value = 42U;
        assert(!dommy::file::readNonnegativeSizeToken(input, value) && value == 42U);
        assert(input.exceptions() == mask && input.getloc() == locale);
    }
    std::istringstream failed("1");
    failed.setstate(std::ios::badbit);
    std::size_t value = 42U;
    assert(!dommy::file::readNonnegativeSizeToken(failed, value) && value == 42U);
}

void checkLegacyNumericFailures()
{
    for (bool colors : {false, true}) {
        for (const std::string& tail : {"4e+", "4e-", "4.0e+", "1e309"}) {
            for (bool terminated : {false, true}) {
                const std::string text = (colors ? "VTXCOLOR 1\nVC " : "POINT_DATA_FROM_MESH mesh 1\nP ") +
                    std::string("1 2 3 ") + tail + (terminated ? "\n" : "");
                std::istringstream input(text);
                std::vector<dommy::file::Triple> output(1U, dommy::file::Triple{{91, 92, 93}});
                assert(!readLegacy(colors, input, output) && output.empty());
            }
        }
        std::istringstream valid(colors ? "VTXCOLOR 2\nVC 1 2 3 4 5 6  \n" :
                                          "POINT_DATA_FROM_MESH mesh 2\nP 1 2 3 4 5 6  \n");
        std::vector<dommy::file::Triple> output;
        assert(readLegacy(colors, valid, output) && output.size() == 2U && output.back()[2] == 6.0);
    }
    std::puts("Legacy numeric tails: syntax failure at EOF is not a successful record: OK");
}

void checkLegacyAllocationFailures()
{
    struct Case { bool colors; bool valid; std::string text; };
    const std::string trailing(128U, 'x');
    const Case cases[] = {
        {false, false, "POINT_DATA_FROM_PARTICLE particle 1\nP 1 2 3\nP 4 5 6\n"},
        {false, false, "POINT_DATA_FROM_MESH mesh 1 " + trailing + "\nP 1 2 3\n"},
        {true, false, "VTXCOLOR 1 " + trailing + "\nVC 1 2 3\n"},
        {false, true, "POINT_DATA_FROM_PARTICLE particle 2\nP 1 2 3\nP 4 5 6\n"},
        {true, true, "VTXCOLOR 2\nVC 1 2 3\nVC 4 5 6\n"},
    };
    for (const Case& test : cases) {
        std::istringstream baseline(test.text);
        std::vector<dommy::file::Triple> expected;
        assert(readLegacy(test.colors, baseline, expected) == test.valid);
        unsigned int faults = 0U;
        for (std::size_t selected = 1U; selected <= 64U; ++selected) {
            std::istringstream input(test.text);
            std::vector<dommy::file::Triple> output(1U, dommy::file::Triple{{91, 92, 93}});
            fault::arm(selected);
            const bool accepted = readLegacy(test.colors, input, output);
            fault::armed = false;
            if (fault::triggered) ++faults;
            if (accepted) assert(test.valid && output == expected);
            else assert(output.empty());
        }
        assert(faults != 0U);
    }
    std::puts("Legacy header/record allocation failures: counts and trailing fields cannot be bypassed: OK");
}

void checkAssNumericTokens()
{
    for (const std::string& token : {"6,5", "6junk", "6.5.0", "6e+"}) {
        std::istringstream input("bounds 0 0 0 2 4 " + token + "\n");
        MPoint minimum(91, 92, 93), maximum(94, 95, 96);
        assert(!readAssBoundingBoxFromStream(input, minimum, maximum));
        assert(samePoint(minimum, MPoint(91, 92, 93)) && samePoint(maximum, MPoint(94, 95, 96)));
    }
    for (const std::string& suffix : {"", "  \t\r", " # comment", "#comment", " extra metadata"}) {
        std::istringstream input("bounds 0 0 0 2 4 6" + suffix + "\n");
        MPoint minimum, maximum;
        assert(readAssBoundingBoxFromStream(input, minimum, maximum));
        assert(samePoint(minimum, MPoint(-1, -2, -3)) && samePoint(maximum, MPoint(1, 2, 3)));
    }
    std::puts("ASS numeric tokens: no prefix acceptance; whitespace/comment/metadata compatibility retained: OK");
}

void checkAssLocales()
{
    const std::locale prior;
    for (const std::string& text : {"bounds 1.234 0 0 2.468 4 6\n",
         "bounds 1.234 -2.345 0 2.468 4.69 6\n",
         "# comment\nbounds 1.25e3 -4.5e-2 0 2.5e3 0.045 6\n"}) {
        MPoint expectedMin, expectedMax;
        {
            dommy_test::ScopedNumericLocale classic(std::locale::classic());
            std::istringstream input(text);
            assert(readAssBoundingBoxFromStream(input, expectedMin, expectedMax));
        }
        for (unsigned int variant = 0U; variant < 2U; ++variant) {
            dommy_test::ScopedNumericLocale localized(variant == 0U ? ',' : '.',
                                                     variant == 0U ? '.' : ',');
            for (bool localizedInput : {false, true}) {
                const std::locale selected = localizedInput ? localized.selected() : std::locale::classic();
                std::istringstream input(text);
                input.imbue(selected);
                MPoint minimum(91, 92, 93), maximum(94, 95, 96);
                assert(readAssBoundingBoxFromStream(input, minimum, maximum));
                assert(samePoint(minimum, expectedMin) && samePoint(maximum, expectedMax));
                assert(input.getloc() == selected && std::locale() == localized.selected());
            }
        }
    }
    for (const std::string& text : {"bounds 1,234 0 0 2,468 4 6\n",
         "bounds 0 0 0 1,025 4 6\n", "bounds 0 0 0 2\n"}) {
        dommy_test::ScopedNumericLocale localized(',', '.');
        std::istringstream input(text);
        MPoint minimum(91, 92, 93), maximum(94, 95, 96);
        assert(!readAssBoundingBoxFromStream(input, minimum, maximum));
        assert(samePoint(minimum, MPoint(91, 92, 93)) && samePoint(maximum, MPoint(94, 95, 96)));
    }
    assert(std::locale() == prior);
    std::puts("ASS bounds: canonical decimals/exponents, unchanged scale/state and selected malformed rejection: OK");
}
} // namespace

int main(int argc, char** argv)
{
    assert(argc == 2);
    const bool all = std::strcmp(argv[1], "all") == 0;
    if (all || std::strcmp(argv[1], "tokens") == 0) checkNumericTokenContract();
    if (all || std::strcmp(argv[1], "patterns") == 0) checkPatternFailures();
    if (all || std::strcmp(argv[1], "points") == 0) {
        checkPointLocales();
        checkPointAllocationFailures();
    }
    if (all || std::strcmp(argv[1], "ass") == 0) checkAssLocales();
    if (all || std::strcmp(argv[1], "legacy-numbers") == 0) checkLegacyNumericFailures();
    if (all || std::strcmp(argv[1], "legacy-counts") == 0) {
        checkLegacyCountTokens();
        checkUnsignedDecimalContract();
    }
    if (all || std::strcmp(argv[1], "legacy-allocation") == 0) checkLegacyAllocationFailures();
    if (all || std::strcmp(argv[1], "ass-tokens") == 0) checkAssNumericTokens();
}
