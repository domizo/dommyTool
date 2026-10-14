#include "alembic_callback_host.h"
#include "callback_methods.inc"
#include <iostream>

struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
void reset() { executed.clear(); warnings = 0U; nativeFailure = nativeException = lastMel = false; }
int main(int argc, char** argv) {
    assert(argc == 2);
    const MBoundingBox bounds(MPoint(1000000.125, -0.12345678901234567, 0),
                             MPoint(1000000.875, 2.1234567890123457, 1));
    for (bool mel : {false, true}) {
        reset();
        processBestEffortCallback("a=#FRAME#; b=#FRAME#; c=#BOUNDSARRAY#; d=#BOUNDSARRAY#; #BOUNDS#; #BOUNDS#",
                                  mel, 0.12345678901234567, bounds);
        assert(executed.size() == 1U && warnings == 0U && lastMel == mel);
        assert(executed.front().find('#') == std::string::npos);
        assert(executed.front().find("1000000.125") != std::string::npos &&
               executed.front().find("1000000.875") != std::string::npos);
        assert(executed.front().find("a=0.12345678901234566; b=0.12345678901234566;") == 0U);
        assert(executed.front().find(mel ? " {" : " [") != std::string::npos);
    }
    const std::size_t maximum = 4U * 1024U * 1024U;
    std::string partiallyPrepared = std::string(maximum - 21U, 'x') + "#FRAME# #BOUNDSARRAY#";
    const auto original = partiallyPrepared;
    assert(!prepareExportCallback(partiallyPrepared, false, 1.5, bounds) &&
           partiallyPrepared == original);
    const auto prior = std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
    reset(); processBestEffortCallback("frame=#FRAME#; bounds=#BOUNDSARRAY#", false, 1.5, bounds);
    std::locale::global(prior);
    assert(executed.size() == 1U && executed.front().find("frame=1.5") == 0U);
    assert(executed.front().find("1000000.125") != std::string::npos);
    for (const std::string& invalid : {std::string("bad\0tail", 8U), std::string(maximum + 1U, 'x'),
         std::string(maximum - 7U, 'x') + "#FRAME#",
         std::string(maximum - 13U, 'x') + "#BOUNDSARRAY#"}) {
        reset(); auto candidate = invalid;
        assert(!prepareExportCallback(candidate, false, std::numeric_limits<double>::max(), bounds));
        assert(candidate == invalid);
        processBestEffortCallback(invalid, false, std::numeric_limits<double>::max(), bounds);
        assert(executed.empty() && warnings == 1U);
    }
    for (double bad : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        reset(); processBestEffortCallback("frame=#FRAME#", false, bad, bounds);
        assert(executed.empty() && warnings == 1U);
        reset(); MBoundingBox invalidBounds(MPoint(bad, 0, 0), MPoint(1, 1, 1));
        processBestEffortCallback("bounds=#BOUNDSARRAY#", true, 1.0, invalidBounds);
        assert(executed.empty() && warnings == 1U);
    }
    reset(); processBestEffortCallback("", false, 0, bounds); assert(executed.empty() && warnings == 0U);
    reset(); processBestEffortCallback(std::string(maximum, 'x'), false, 1.0, bounds);
    assert(executed.size() == 1U && executed.front().size() == maximum);
    reset(); nativeFailure = true; processBestEffortCallback("command", true, 0, bounds);
    assert(executed.size() == 1U && warnings == 1U);
    reset(); nativeException = true; bool failed = false; std::string temporary;
    const std::string destination = argv[1];
    { std::ofstream output(destination); output << "existing"; }
    try {
        dommy::file::PendingOutputFile file(destination, dommy::file::PendingOutputFile::Access::TrustedPath); temporary = file.trustedTemporaryPath();
        processBestEffortCallback("command", false, 0, bounds); file.commit();
    } catch (const std::runtime_error&) { failed = true; }
    assert(failed && !dommy::file::isRegularFile(temporary));
    std::ifstream input(destination); assert(std::string((std::istreambuf_iterator<char>(input)), {}) == "existing");
    std::string value = "token token";
    assert(!dommy::file::replaceAll(value, "token", "longer replacement", 12U) && value == "token token");
    assert(dommy::file::replaceAll(value, "token", "", 12U) && value == " ");
    value = "x"; assert(dommy::file::replaceAll(value, "x", "xx", 2U) && value == "xx");
    value = "abc"; assert(dommy::file::replaceAll(value, value, value, 3U) && value == "abc");
    assert(!dommy::file::replaceAll(value, "", "x", 100U) && value == "abc");
    std::string repeated;
    for (unsigned int i = 0U; i < 100000U; ++i) repeated += "#FRAME#;";
    reset(); processBestEffortCallback(repeated, false, 1.5, bounds);
    assert(executed.size() == 1U && executed.front().size() == 400000U &&
           executed.front().find('#') == std::string::npos);
    std::cout << "Callbacks: double precision, fixed locale, repeated tokens, linear bounded expansion, invalid admission and best-effort status: OK\n";
}
