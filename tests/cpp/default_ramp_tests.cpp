#include "curve_copy_doubles.h"
#include <initializer_list>

using MFloatArray = Array<float>;
using MIntArray = Array<int>;
struct MColor {
    float r, g, b, a;
    MColor(float r = 0, float g = 0, float b = 0, float a = 1) : r(r), g(g), b(b), a(a) {}
};
using MColorArray = Array<MColor>;
std::ostream& operator<<(std::ostream& out, const MColor& c) {
    return out << c.r << ',' << c.g << ',' << c.b << ',' << c.a;
}
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("ramp", __LINE__);                                                         \
    } while (false)
namespace {
std::map<std::string, std::string> ramps;
std::vector<std::string> writes;
unsigned failedCountValue = 0U;
template <class T> std::string entries(const Array<T>& values) {
    std::ostringstream out;
    out << std::setprecision(17);
    for (const auto& value : values)
        out << ' ' << value;
    return out.str();
}
struct RampBase {
    std::string key;
    bool valid = false;
    RampBase(const MPlug& p, MStatus* status) : key(p.attribute) {
        const auto s = api("ramp.bind " + key);
        valid = bool(s);
        if (status)
            *status = s;
    }
    unsigned getNumEntries(MStatus* status = nullptr) {
        assert(valid);
        const auto s = api("ramp.count " + key);
        if (status)
            *status = s;
        if (!s)
            return failedCountValue;
        return ramps.count(key) ? 1U : 0U;
    }
    MStatus set(const std::string& value) {
        assert(valid);
        const auto s = api("ramp.set " + key);
        if (s) {
            ramps[key] = value;
            writes.push_back(key + value);
        }
        return s;
    }
};
} // namespace
struct TestCurveAttribute : RampBase {
    TestCurveAttribute(const MPlug& p, MStatus* s = nullptr) : RampBase(p, s) {}
    MStatus setCurve(const MFloatArray& values, const MFloatArray& positions) {
        assert(values.length() == positions.length());
        return set(" curve values" + entries(values) + " positions" + entries(positions));
    }
};
struct TestRampAttribute : RampBase {
    enum { kNone = 0, kLinear = 1, kSmooth = 2, kSpline = 3 };
    TestRampAttribute(const MPlug& p, MStatus* s = nullptr) : RampBase(p, s) {}
    template <class T>
    MStatus setRamp(const Array<T>& values, const MFloatArray& positions,
                    const MIntArray& interpolation) {
        assert(values.length() == positions.length() && values.length() == interpolation.length());
        return set(" ramp values" + entries(values) + " positions" + entries(positions) +
                   " interpolation" + entries(interpolation));
    }
};
#include "default_ramp.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    std::string contract;
    int cases = 0;
    const bool dump = std::string(argv[1]) == "--dump";
    for (const auto& node : nodes) {
        reset();
        ramps.clear();
        writes.clear();
        node.second();
        const auto trace = operations;
        const auto defaults = ramps;
        contract += node.first + "\n";
        for (const auto& w : writes)
            contract += w + "\n";
        if (dump)
            continue;
        for (unsigned i = 0; i < trace.size(); ++i) {
            for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                reset(i);
                injectedStatus = status;
                ramps.clear();
                writes.clear();
                node.second();
                assert(callCount == int(i + 1));
                // Earlier independent ramps may have succeeded, but the failing
                // ramp and every later ramp must remain untouched.
                for (const auto& r : ramps)
                    assert(defaults.at(r.first) == r.second);
                ++cases;
            }
            reset();
            throwAt = i;
            ramps.clear();
            writes.clear();
            node.second();
            assert(callCount == int(i + 1));
            ++cases;
            if (trace[i] == "array.resize" || trace[i] == "array.copy") {
                reset();
                shortAt = i;
                ramps.clear();
                writes.clear();
                node.second();
                assert(shortApplied && callCount <= int(i + 2));
                ++cases;
            }
        }
        // Existing user/scene entries must never be replaced.
        reset();
        ramps = defaults;
        for (auto& r : ramps)
            r.second = "existing";
        const auto existing = ramps;
        writes.clear();
        node.second();
        assert(ramps == existing && writes.empty());
        const auto countTrace = operations;
        for (unsigned i = 0; i < countTrace.size(); ++i) {
            if (countTrace[i].find("ramp.count ") == 0) {
                reset(i);
                failedCountValue = 0;
                ramps = existing;
                writes.clear();
                node.second();
                assert(ramps == existing && writes.empty() && callCount == int(i + 1));
                ++cases;
            }
        }
    }
    if (dump) {
        std::cout << contract;
        return 0;
    }
    std::ifstream expected(argv[1]);
    assert(expected.good());
    const std::string frozen((std::istreambuf_iterator<char>(expected)), {});
    assert(contract == frozen);
    std::cout << "Default ramp checks: " << nodes.size() << " callbacks, " << cases
              << " failure cases passed\n";
}
