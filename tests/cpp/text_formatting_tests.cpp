// Actual callback functions and grid naming fragments are generated from the
// production source. No Maya command or OpenVDB native operation is executed.
#include "alembic_callback_host.h"
#include "numeric_locale_fixture.h"
#include "allocation_failure_fixture.h"
#include "common/string/DStreamFormat.h"

#include <cstdio>
#include <cstring>
#include <limits>

struct NameGrid {
    std::string name;
    const std::string* expected;
    bool invalidSet = false;
    unsigned int setters = 0U;
    NameGrid(const std::string& source, const std::string& desired)
        : name(source), expected(&desired) {}
    const std::string& getName() const { return name; }
    void setName(const std::string& value) {
        invalidSet = invalidSet || value != *expected;
        ++setters;
        name = value;
    }
};
#include "text_formatting_methods.inc"

namespace {
void checkCallback(const std::string& source, bool mel)
{
    const MBoundingBox bounds(MPoint(1000000.125, -0.12345678901234567, 0),
                              MPoint(1000000.875, 2.1234567890123457, 1));
    const double frame = -std::numeric_limits<double>::max();
    std::string expected(source);
    assert(prepareExportCallback(expected, mel, frame, bounds));
    unsigned int faults = 0U, rejections = 0U;
    for (std::size_t selected = 1U; selected <= 64U; ++selected) {
        std::string candidate(source);
        fault::arm(selected);
        bool accepted = false;
        try { accepted = prepareExportCallback(candidate, mel, frame, bounds); }
        catch (const std::bad_alloc&) {}
        fault::armed = false;
        if (fault::triggered) ++faults;
        if (accepted) assert(candidate == expected);
        else {
            ++rejections;
            assert(fault::triggered && candidate == source);
        }

        executed.clear();
        warnings = 0U;
        nativeFailure = nativeException = lastMel = false;
        fault::arm(selected);
        bool caught = false;
        try { processBestEffortCallback(source, mel, frame, bounds); }
        catch (const std::bad_alloc&) { caught = true; }
        fault::armed = false;
        assert(executed.size() <= 1U);
        if (!executed.empty()) assert(executed.front() == expected && lastMel == mel);
        else if (!caught) assert(warnings == 1U && fault::triggered);
    }
    assert(faults != 0U && rejections != 0U);
}

void checkGridNames(bool fracture, const std::string& source, int tupleSize)
{
    const std::string expected = source + (fracture ? "_1234" : tupleSize == 1 ? "" : "_2");
    NameGrid normal(source, expected);
    if (fracture) nameFractureGrid(&normal, 1234U);
    else nameAttributeGrid(&normal, source, tupleSize, 2);
    assert(!normal.invalidSet && normal.setters == 1U && normal.name == expected);
    unsigned int faults = 0U, rejections = 0U;
    for (std::size_t selected = 1U; selected <= 64U; ++selected) {
        NameGrid target(source, expected);
        fault::arm(selected);
        bool accepted = false;
        try {
            if (fracture) nameFractureGrid(&target, 1234U);
            else nameAttributeGrid(&target, source, tupleSize, 2);
            accepted = true;
        } catch (const std::bad_alloc&) {
        } catch (const std::runtime_error&) {
            assert(fault::triggered);
        }
        fault::armed = false;
        if (fault::triggered) ++faults;
        assert(!target.invalidSet);
        if (accepted) assert(target.name == expected && target.setters == 1U);
        else {
            ++rejections;
            assert(fault::triggered && target.name == source);
        }
    }
    assert(faults != 0U && rejections != 0U);
}

void checkGridLocales()
{
    const std::locale prior;
    for (unsigned int variant = 0U; variant < 2U; ++variant) {
        dommy_test::ScopedNumericLocale localized(variant == 0U ? ',' : '.',
                                                 variant == 0U ? '.' : ',');
        for (std::size_t index : {0U, 999U, 1000U, 1234U}) {
            const std::string expected = "density_" + std::to_string(index);
            NameGrid target("density", expected);
            nameFractureGrid(&target, index);
            assert(!target.invalidSet && target.name == expected);
        }
        const std::string expected("velocity_2");
        NameGrid target("unused", expected);
        nameAttributeGrid(&target, "velocity", 3, 2);
        assert(!target.invalidSet && target.name == expected);
        assert(std::locale() == localized.selected());
    }
    assert(std::locale() == prior);
}

void checkLightNames()
{
    const std::string prefix("dommy_dlight_12345678_1234_1234_1234_123456789012_");
    const std::locale prior;
    for (char separator : {'.', ',', '_'}) {
        dommy_test::ScopedNumericLocale locale(',', separator);
        for (std::size_t index : {0U, 999U, 1000U, 1234U, 1000000U}) {
            const std::string suffix = std::to_string(index);
            assert(translatorLightName(prefix, index) == prefix + "light_" + suffix);
            assert(exportLightName(index) == "dlight" + suffix);
        }
        assert(std::locale() == locale.selected());
    }
    assert(std::locale() == prior);
    unsigned int faults = 0U, rejected = 0U;
    const std::string expected = prefix + "light_1234";
    for (std::size_t failure = 1U; failure <= 64U; ++failure) {
        std::string output("prior");
        bool returned = false;
        fault::arm(failure);
        try { output = translatorLightName(prefix, 1234U); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        faults += fault::triggered;
        rejected += !returned;
        assert(returned ? output == expected : output == "prior");
    }
    assert(faults != 0U && rejected != 0U);
    faults = rejected = 0U;
    // Formatter-level width test, not permission to generate SIZE_MAX lights.
    const std::size_t index = std::numeric_limits<std::size_t>::max();
    const std::string expectedExport = "dlight" + std::to_string(index);
    for (std::size_t failure = 1U; failure <= 64U; ++failure) {
        std::string output("prior");
        bool returned = false;
        fault::arm(failure);
        try { output = exportLightName(index); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        faults += fault::triggered;
        rejected += !returned;
        assert(returned ? output == expectedExport : output == "prior");
    }
    assert(faults != 0U && rejected != 0U);
}
} // namespace

int main(int argc, char** argv)
{
    assert(argc == 2);
    const bool all = std::strcmp(argv[1], "all") == 0;
    const bool callbacks = all || std::strcmp(argv[1], "callbacks") == 0;
    const bool grids = all || std::strcmp(argv[1], "grids") == 0;
    const bool lights = all || std::strcmp(argv[1], "lights") == 0;
    assert(callbacks || grids || lights);
    if (callbacks) {
        for (bool mel : {false, true}) {
            for (const std::string& source : {"record(#FRAME#)", "record(#BOUNDS#)",
                "record(#BOUNDSARRAY#)", "record(#BOUNDSARRAY#); record(#BOUNDS#)",
                "record(#FRAME#); record(#BOUNDSARRAY#); record(#BOUNDS#)"})
                checkCallback(source, mel);
        }
        std::puts("Callback allocation failures: no partial preparation or command execution: OK");
    }
    if (grids) {
        // A canonical numeric suffix must not sanitize/rewrite the base name.
        for (const std::string& source : {std::string(128U, 'x'),
             std::string(128U, 'x') + u8".density_密度"}) {
            checkGridNames(false, source, 1);
            checkGridNames(false, source, 3);
            checkGridNames(true, source, 1);
        }
        checkGridLocales();
        std::puts("Grid naming allocation failures and canonical suffixes: OK");
    }
    if (lights) {
        checkLightNames();
        std::puts("Arnold light naming: canonical suffixes and complete-or-error allocation behavior: OK");
    }
}
