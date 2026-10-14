#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "math/DLinearInterpolation.h"

#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>

// Allow the archived source to run in compatibility/mutation checks.
#define CHECK_MSTATUS(expression)                                                                  \
    do {                                                                                           \
        const MStatus result = (expression);                                                       \
        if (!result)                                                                               \
            result.pAPIerror(__FILE__, __LINE__);                                                  \
    } while (false)

namespace {
int shortAt = -1, throwAt = -1, publications = 0;
bool faultReached = false;
double currentContext = 12.0;
std::vector<double> published, sampledTimes;
std::vector<float> rampPositions;
std::function<float(float)> rampFunction = [](float position) { return position; };
MStatus api(const std::string& name) {
    const int index = callCount;
    const MStatus status = operation(name);
    if (!status || index == shortAt || index == throwAt)
        faultReached = true;
    if (index == throwAt)
        throw std::runtime_error("injected API exception");
    return status;
}
void resetRun(int failure = -1) {
    resetCalls(failure);
    shortAt = throwAt = -1;
    faultReached = false;
    publications = 0;
    published.clear();
    sampledTimes.clear();
    rampPositions.clear();
    currentContext = 12.0;
}
void readValue() {
    assert(!faultReached);
}
} // namespace

template <class T> const char* arrayName();
template <> const char* arrayName<int>() {
    return "int";
}
template <> const char* arrayName<float>() {
    return "float";
}
template <> const char* arrayName<double>() {
    return "double";
}
template <class T> struct Array {
    std::vector<T> values;
    Array() = default;
    explicit Array(unsigned int count, T initial = T()) {
        if (setLength(count))
            std::fill(values.begin(), values.end(), initial);
    }
    MStatus setLength(unsigned int count) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".allocate");
        if (status)
            values.resize(index == shortAt && count ? count - 1 : count);
        return status;
    }
    MStatus copy(const Array& source) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".copy");
        if (status) {
            values = source.values;
            if (index == shortAt && !values.empty())
                values.pop_back();
        }
        return status;
    }
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    T& operator[](unsigned int index) {
        readValue();
        return values.at(index);
    }
    const T& operator[](unsigned int index) const {
        readValue();
        return values.at(index);
    }
};
using MIntArray = Array<int>;
using MFloatArray = Array<float>;
using MDoubleArray = Array<double>;
using int2 = int[2];
using float2 = float[2];

struct Input {
    double number = 0;
    int2 integers = {-3, 4};
    float2 floats = {-3, 4};
    MFnData::Type type = MFnData::kNumeric;
    bool connected = false;
    std::vector<double> array;
};
struct MDataHandle {
    bool valid = true;
    Input* input = nullptr;
    std::string name;
    void check() const {
        assert(valid && input);
        readValue();
    }
    short asShort() const {
        check();
        return static_cast<short>(input->number);
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    float asFloat() const {
        check();
        return static_cast<float>(input->number);
    }
    double asDouble() const {
        check();
        return input->number;
    }
    const int2& asInt2() const {
        check();
        return input->integers;
    }
    const float2& asFloat2() const {
        check();
        return input->floats;
    }
    MFnData::Type type() const {
        check();
        return input->type;
    }
    MObject data() const {
        check();
        return {name};
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = api("input " + attribute.name);
        MDataHandle result;
        result.valid = bool(*status);
        result.input = &inputs[attribute.name];
        result.name = attribute.name;
        return result;
    }
};
namespace {
MDataBlock* activeData = nullptr;
}
struct MPlug {
    MObject attribute;
    explicit MPlug(const MObject& attribute) : attribute(attribute) {}
    MPlug(const MObject&, const MObject& attribute) : attribute(attribute) {}
    bool operator!=(const MObject& other) const {
        return attribute.name != other.name;
    }
    bool isNull() const {
        return attribute.isNull();
    }
    bool isConnected(MStatus* status = nullptr) const {
        const MStatus result = api("connection " + attribute.name);
        if (status) *status = result;
        return activeData->inputs[attribute.name].connected;
    }
};
template <class T> struct ArrayData {
    MObject object;
    ArrayData(const MObject& object, MStatus* status) : object(object) {
        *status = api(std::string(arrayName<T>()) + " bind " + object.name);
    }
    Array<T> array(MStatus* status) const {
        *status = api(std::string(arrayName<T>()) + " source " + object.name);
        Array<T> result;
        if (*status) {
            const auto& source = activeData->inputs[object.name].array;
            result.values.assign(source.begin(), source.end());
        }
        return result;
    }
};
using MFnFloatArrayData = ArrayData<float>;
using MFnDoubleArrayData = ArrayData<double>;
struct SamplingRamp : MRampAttribute {
    SamplingRamp(const MPlug&, MStatus* status) {
        *status = api("ramp bind");
    }
    void getValueAtPosition(float position, float& value, MStatus* status) {
        *status = api("ramp sample");
        if (*status) {
            rampPositions.push_back(position);
            value = rampFunction(position);
        }
    }
};
#define MRampAttribute SamplingRamp
struct SamplingTime : MTime {
    using MTime::MTime;
    static Unit uiUnit() {
        return kFilm;
    }
};
#define MTime SamplingTime
struct MDGContext {
    MTime time;
    explicit MDGContext(MTime time) : time(time) {}
};
struct MDGContextGuard {
    double previous;
    explicit MDGContextGuard(const MDGContext& context) : previous(currentContext) {
        currentContext = context.time.value();
    }
    ~MDGContextGuard() {
        currentContext = previous;
    }
};
namespace dommy {
namespace maya {
MStatus getEvaluationTime(MDataBlock&, MTime& time) {
    const MStatus status = api("evaluation time");
    if (status)
        time = MTime(currentContext);
    return status;
}
struct ScopedPlugDataHandle {
    Input input;
    MDataHandle value;
    MDataHandle& handle() {
        return value;
    }
    MStatus read(const MPlug&) {
        const MStatus status = api("sample value");
        value.valid = bool(status);
        value.input = &input;
        if (status) {
            sampledTimes.push_back(currentContext);
            input.number = currentContext;
        }
        return status;
    }
};
} // namespace maya
} // namespace dommy
template <class T> MStatus publish(const Array<T>& values) {
    const MStatus status = api("publish");
    if (status) {
        ++publications;
        published.assign(values.values.begin(), values.values.end());
    }
    return status;
}
MStatus DOutputDataMayaIntArray(MDataBlock&, const MPlug&, const MObject&,
                                const MIntArray& values) {
    return publish(values);
}
MStatus DOutputDataMayaFloatArray(MDataBlock&, const MPlug&, const MObject&,
                                  const MFloatArray& values) {
    return publish(values);
}
#define SCALAR_NODE(name)                                                                          \
    struct name : AttributeNode {                                                                  \
        static MObject generateType, value, valueSize, aSeed, minRange, maxRange, aShiftRange,     \
            aValueRamp, aRampRange, aGlobalOffset, aInOffsetArray, aInStretchArray, outDataArray;  \
        static MStatus initialize();                                                               \
        MStatus compute(const MPlug&, MDataBlock&);                                                \
        MObject thisMObject() const {                                                              \
            return {"node"};                                                                       \
        }                                                                                          \
    };
SCALAR_NODE(DPIntArrayGen)
SCALAR_NODE(DPDoubleArrayGen)
#include "scalar_array_methods.inc"

namespace {
MDataBlock configured(int mode, int count = 4) {
    MDataBlock data;
    data.inputs["generateType"].number = mode;
    data.inputs["valueSize"].number = count;
    data.inputs["value"].number = -2;
    data.inputs["Seed"].number = -17;
    data.inputs["minRange"].number = 4;
    data.inputs["maxRange"].number = -3;
    data.inputs["GlobalOffset"].number = 2;
    return data;
}
template <class Node> MStatus run(MDataBlock& data) {
    activeData = &data;
    Node node;
    return node.compute(MPlug(Node::outDataArray), data);
}
void expect(const std::vector<double>& values) {
    assert(publications == 1 && published == values && currentContext == 12.0);
}
template <class Run> int failures(Run run, bool exceptions = true,
                                 const std::vector<double>& prior = {}) {
    resetRun();
    assert(run() == MS::kSuccess);
    const auto expected = published;
    const auto trace = operations;
    int tested = 0;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
            resetRun(static_cast<int>(i));
            injectedStatus = status;
            published = prior;
            assert(run() == status);
            assert(callCount == static_cast<int>(i) + 1 && publications == 0 &&
                   published == prior);
            assert(currentContext == 12.0);
            ++tested;
        }
        if (trace[i].find(".allocate") != std::string::npos ||
            trace[i].find(".copy") != std::string::npos) {
            resetRun();
            shortAt = static_cast<int>(i);
            published = prior;
            assert(run() == MS::kFailure);
            assert(callCount == static_cast<int>(i) + 1 && publications == 0 &&
                   published == prior);
            ++tested;
        }
        if (exceptions) {
            resetRun();
            throwAt = static_cast<int>(i);
            published = prior;
            assert(run() == MS::kFailure && publications == 0 && published == prior);
            assert(callCount == static_cast<int>(i) + 1 && currentContext == 12.0);
            ++tested;
        }
        resetRun();
        assert(run() == MS::kSuccess && published == expected);
    }
    return tested;
}
template <class Node> int testNode(bool integer, bool normalOnly) {
    int tested = 0;
    for (int mode = 0; mode <= 4; ++mode) {
        for (int count : {0, 1, 4}) {
            auto data = configured(mode, count);
            resetRun();
            assert(run<Node>(data) == MS::kSuccess);
            std::vector<double> expected(count, -2);
            if (mode == 1) {
                if (integer) {
                    std::vector<int> values;
                    dommy::random::generateIntValues(static_cast<std::uint32_t>(-17), -3, 4, count,
                                                     values);
                    expected.assign(values.begin(), values.end());
                } else {
                    std::vector<float> values;
                    dommy::random::generateRealValues(static_cast<std::uint32_t>(-17), -3.0f, 4.0f,
                                                      count, values, false);
                    expected.assign(values.begin(), values.end());
                }
            } else if (mode == 2 || mode == 3) {
                for (int i = 0; i < count; ++i) {
                    const double fraction =
                        count <= 1
                            ? (mode == 3 ? 1.0 : 0.0)
                            : (mode == 3 ? static_cast<double>(static_cast<float>(i) / (count - 1))
                                         : static_cast<double>(i) / (count - 1));
                    const double value = -3 + 7 * fraction;
                    expected[i] = integer ? static_cast<int>(value) : static_cast<float>(value);
                }
            }
            expect(expected);
            if (mode == 3 && count == 1)
                assert(rampPositions == std::vector<float>{1});
            if (normalOnly) {
                std::cout << integer << ':' << mode << ':' << count;
                for (double value : published)
                    std::cout << ':' << text(value);
                std::cout << '\n';
            }
        }
        if (!normalOnly) {
            auto data = configured(mode);
            if (mode == 4) {
                data.inputs["value"].connected = true;
                auto& offset = data.inputs["InOffsetArray"];
                offset.connected = true;
                offset.type = MFnData::kFloatArray;
                offset.array = {1, 2};
                auto& stretch = data.inputs["InStretchArray"];
                stretch.connected = true;
                stretch.type = MFnData::kDoubleArray;
                stretch.array = {2, 0, 4, 1, 99};
                resetRun();
                assert(run<Node>(data) == MS::kSuccess);
                expect({7, 12, 5, 14});
                assert(sampledTimes == published);
            }
            tested += failures([&]() { return run<Node>(data); });
        }
    }
    if (normalOnly)
        return tested;
    for (int count : {-1, 10000001}) {
        auto data = configured(0, count);
        resetRun();
        assert(run<Node>(data) == MS::kInvalidParameter && publications == 0);
        assert(callCount == 3);
    }
    for (int mode : {-1, 5}) {
        auto data = configured(mode);
        resetRun();
        assert(run<Node>(data) == MS::kInvalidParameter && publications == 0);
    }
    auto data = configured(0);
    activeData = &data;
    resetRun();
    Node node;
    assert(node.compute(MPlug(MObject{"unrelated"}), data) == MS::kUnknownParameter &&
           callCount == 0);
    return tested;
}
int testFloatShiftBoundaries() {
    const float limit=std::numeric_limits<float>::max();
    const float tiny=std::numeric_limits<float>::denorm_min();
    const std::vector<std::array<float,2>> ranges{
        {{-3,4}},{{-5.25f,3.125f}},{{-limit,limit}},{{-limit/2,limit}},
        {{-limit/3,limit}},{{0,limit}},{{-limit,1}},{{-limit,0}},
        {{std::nextafter(limit,0.0f),limit}},{{0,tiny}},{{-tiny,tiny}},
        {{limit,limit}},{{-limit,-limit}},{{0.0f,-0.0f}}};
    int tested=0;
    for(const auto& range:ranges)for(bool reversed:{false,true})
        for(int count:{0,1,2,3,10,28,42,101,1000}) {
            auto data=configured(2,count);
            auto& input=data.inputs["ShiftRange"].floats;
            input[0]=range[reversed?1:0];input[1]=range[reversed?0:1];
            const float minimum=std::min(input[0],input[1]);
            const float maximum=std::max(input[0],input[1]);
            resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess);
            assert(publications==1 && published.size()==std::size_t(count));
            const double step=count>1 ? (double(maximum)-minimum)/(count-1):0;
            for(int index=0;index<count;++index) {
                float expected=minimum;
                if(index>0 && index+1==count)expected=maximum;
                else if(index>0) {
                    // Freeze the ordinary interior rounding, not the new fallback.
                    const double historical=minimum+step*index;
                    assert(dommy::numeric::checkedFloatCast(historical,expected));
                }
                assert(published[index]==expected && std::isfinite(published[index]));
                assert(published[index]>=minimum && published[index]<=maximum);
                if(expected==0)assert(std::signbit(published[index])==std::signbit(expected));
                if(index>0)assert(published[index]>=published[index-1]);
            }
            ++tested;
        }
    return tested;
}
int testFloatShiftFailures() {
    const float limit=std::numeric_limits<float>::max();
    auto data=configured(2,28);
    data.inputs["ShiftRange"].floats[0]=-limit;
    data.inputs["ShiftRange"].floats[1]=limit;
    resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess);
    const auto expected=published;
    const auto trace=operations;
    const std::vector<double> prior{99,-17};
    int tested=0;
    for(std::size_t index=0;index<trace.size();++index) {
        for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
            resetRun(index);injectedStatus=status;published=prior;
            assert(run<DPDoubleArrayGen>(data)==status && publications==0 && published==prior);
            assert(callCount==int(index)+1);
            resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess && published==expected);
            ++tested;
        }
        resetRun();throwAt=index;published=prior;
        assert(run<DPDoubleArrayGen>(data)==MS::kFailure && publications==0 && published==prior);
        assert(callCount==int(index)+1);
        resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess && published==expected);
        ++tested;
        if(trace[index]=="float.allocate") {
            resetRun();shortAt=index;published=prior;
            assert(run<DPDoubleArrayGen>(data)==MS::kFailure && faultReached &&
                   publications==0 && published==prior);
            resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess && published==expected);
            ++tested;
        }
    }
    for(float invalid:{INFINITY,-INFINITY,NAN})for(int axis:{0,1}) {
        auto bad=data;bad.inputs["ShiftRange"].floats[axis]=invalid;
        resetRun();published=prior;
        assert(run<DPDoubleArrayGen>(bad)==MS::kInvalidParameter && publications==0 && published==prior);
        assert(std::find(operations.begin(),operations.end(),"float.allocate")==operations.end());
        resetRun();assert(run<DPDoubleArrayGen>(data)==MS::kSuccess && published==expected);
        ++tested;
    }
    return tested;
}
template <class Node> int testRampBoundaries(bool integer) {
    const float limit = std::numeric_limits<float>::max();
    const float tiny = std::numeric_limits<float>::denorm_min();
    const std::vector<std::array<float, 2>> ranges{
        {{-3, 4}}, {{-5.25f, 3.125f}}, {{-limit, 1}}, {{-limit, -1}},
        {{-limit, limit}}, {{0, limit}}, {{-limit, 0}},
        {{std::nextafter(limit, 0.0f), limit}}, {{0, tiny}}, {{-tiny, tiny}},
        {{limit, limit}}, {{-limit, -limit}}, {{-0.0f, 0.0f}},
        {{1.25f, 3.75f}}, {{-3.75f, -1.25f}},
        {{2147483520.0f, 2147483648.0f}}, {{-2147483904.0f, -2147483648.0f}},
        {{-2147483648.0f, 2147483520.0f}},
        {{-limit, 2147483648.0f}}, {{-limit, -2147483904.0f}}};
    const std::vector<double> prior{99, -17};
    int tested = 0;
    for (const auto& range : ranges) {
        for (bool reversed : {false, true}) {
            for (int count : {0, 1, 2, 3, 28}) {
                for (float sample : {0.0f, -0.0f, 1.0f}) {
                    auto data = configured(3, count);
                    auto& input = data.inputs["RampRange"].floats;
                    input[0] = range[reversed ? 1 : 0];
                    input[1] = range[reversed ? 0 : 1];
                    // Endpoint expectations do not repeat the cancellation-prone
                    // interpolation expression or use the new implementation.
                    const float endpoint = sample == 0.0f
                        ? std::min(input[0], input[1]) : std::max(input[0], input[1]);
                    const double expected = integer ? std::trunc(double(endpoint)) : endpoint;
                    const bool representable = !integer ||
                        (expected >= std::numeric_limits<int>::min() &&
                         expected <= std::numeric_limits<int>::max());
                    rampFunction = [sample](float) { return sample; };
                    resetRun();
                    published = prior;
                    const MStatus status = run<Node>(data);
                    if (count == 0 || representable) {
                        assert(status == MS::kSuccess);
                        expect(std::vector<double>(count, expected));
                        assert(rampPositions.size() == std::size_t(count));
                        if (count == 1) assert(rampPositions[0] == 1.0f);
                        if (!integer && expected == 0.0) {
                            for (double value : published)
                                assert(std::signbit(value) == std::signbit(expected));
                        }
                    } else {
                        assert(status == MS::kInvalidParameter && publications == 0 &&
                               published == prior);
                    }
                    ++tested;
                }
            }
        }
    }

    struct RampCase {
        std::array<float, 2> range;
        std::vector<float> samples;
        std::vector<double> floatValues, intValues;
    };
    // Independent golden values protect ordinary rounding, truncation and
    // extrapolation. Reversed endpoint samples also forbid index-based pinning.
    const std::vector<RampCase> cases{
        {{{-3, 4}}, {1, 0, -0.5f, 1.5f, 0.25f, 0.75f},
         {4, -3, -6.5, 7.5, -1.25, 2.25}, {4, -3, -6, 7, -1, 2}},
        {{{4, -3}}, {1, 0, -0.5f, 1.5f, 0.25f, 0.75f},
         {4, -3, -6.5, 7.5, -1.25, 2.25}, {4, -3, -6, 7, -1, 2}},
        {{{1.25f, 3.75f}}, {0, 1, -0.5f, 1.5f, 0.5f},
         {1.25, 3.75, 0, 5, 2.5}, {1, 3, 0, 5, 2}},
        {{{-3.75f, -1.25f}}, {0, 1, -0.5f, 1.5f, 0.5f},
         {-3.75, -1.25, -5, 0, -2.5}, {-3, -1, -5, 0, -2}},
        {{{2, 2}}, {1, 0, -2, 3, 0.5f}, {2, 2, 2, 2, 2}, {2, 2, 2, 2, 2}}};
    for (const auto& test : cases) {
        auto data = configured(3, static_cast<int>(test.samples.size()));
        data.inputs["RampRange"].floats[0] = test.range[0];
        data.inputs["RampRange"].floats[1] = test.range[1];
        rampFunction = [&test](float position) {
            const auto index = static_cast<std::size_t>(
                std::lround(position * (test.samples.size() - 1U)));
            return test.samples.at(index);
        };
        resetRun();
        assert(run<Node>(data) == MS::kSuccess);
        expect(integer ? test.intValues : test.floatValues);
        ++tested;
    }
    rampFunction = [](float position) { return position; };
    return tested;
}
template <class Node> int testRampFailures(bool integer) {
    const float limit = std::numeric_limits<float>::max();
    const std::vector<double> prior{99, -17};
    auto data = configured(3, 3);
    data.inputs["RampRange"].floats[0] = -limit;
    data.inputs["RampRange"].floats[1] = 1.0f;
    auto retry = [&]() {
        rampFunction = [](float) { return 1.0f; };
        resetRun();
        assert(run<Node>(data) == MS::kSuccess);
        expect({1, 1, 1});
    };
    retry();
    int tested = failures([&]() { return run<Node>(data); }, true, prior);

    for (float invalid : {INFINITY, -INFINITY, NAN}) {
        for (int axis : {0, 1}) {
            for (int count : {0, 1, 3}) {
                auto bad = data;
                bad.inputs["valueSize"].number = count;
                bad.inputs["RampRange"].floats[axis] = invalid;
                resetRun();
                published = prior;
                assert(run<Node>(bad) == MS::kInvalidParameter && publications == 0 &&
                       published == prior);
                retry();
                ++tested;
            }
        }
        for (int location : {0, 1, 2}) {
            rampFunction = [invalid, location](float position) {
                return position == 0.5f * location ? invalid : 1.0f;
            };
            resetRun();
            published = prior;
            assert(run<Node>(data) == MS::kFailure && publications == 0 && published == prior);
            assert(rampPositions.size() == std::size_t(location + 1));
            retry();
            ++tested;
        }
    }
    for (float sample : {integer ? 0.0f : -1.0f, 2.0f}) {
        for (int location : {0, 1, 2}) {
            auto bad = data;
            if (!integer) bad.inputs["RampRange"].floats[1] = limit;
            rampFunction = [sample, location, integer](float position) {
                return position == 0.5f * location ? sample : (integer ? 1.0f : 0.5f);
            };
            resetRun();
            published = prior;
            const auto expected = integer ? MS::kInvalidParameter : MS::kFailure;
            assert(run<Node>(bad) == expected && publications == 0 && published == prior);
            assert(rampPositions.size() == std::size_t(location + 1));
            retry();
            ++tested;
        }
    }
    rampFunction = [](float position) { return position; };
    return tested;
}
void testIntegerBoundaries() {
    const int minimum = std::numeric_limits<int>::min(), maximum = std::numeric_limits<int>::max();
    auto data = configured(2, 7);
    auto& range = data.inputs["ShiftRange"];
    range.integers[0] = maximum;
    range.integers[1] = minimum;
    resetRun();
    assert(run<DPIntArrayGen>(data) == MS::kSuccess);
    assert(published.front() == minimum && published.back() == maximum);
    for (double endpoint : {0.0, -100.0, 100.0, static_cast<double>(minimum), 2147483520.0}) {
        data = configured(3);
        data.inputs["RampRange"].floats[0] = data.inputs["RampRange"].floats[1] =
            static_cast<float>(endpoint);
        resetRun();
        assert(run<DPIntArrayGen>(data) == MS::kSuccess);
        expect(std::vector<double>(4, endpoint));
    }
    for (float endpoint : {2147483648.0f, -2147483904.0f, 1.0e10f, -1.0e10f}) {
        data = configured(3);
        data.inputs["RampRange"].floats[0] = data.inputs["RampRange"].floats[1] = endpoint;
        resetRun();
        assert(run<DPIntArrayGen>(data) == MS::kInvalidParameter && publications == 0);
    }
    // Fractional values outside the endpoints still have representable truncations.
    data = configured(3, 1);
    data.inputs["RampRange"].floats[0] = 2147483520.0f;
    data.inputs["RampRange"].floats[1] = 2147483648.0f;
    rampFunction = [](float) { return 127.5f / 128.0f; };
    resetRun();
    assert(run<DPIntArrayGen>(data) == MS::kSuccess);
    expect({double(maximum)});
    data.inputs["RampRange"].floats[0] = -2147483904.0f;
    data.inputs["RampRange"].floats[1] = -2147483648.0f;
    rampFunction = [](float) { return 255.5f / 256.0f; };
    resetRun();
    assert(run<DPIntArrayGen>(data) == MS::kSuccess);
    expect({double(minimum)});
    rampFunction = [](float position) { return position; };
}
template <class Node> void testInvalidNumbers() {
    for (float number :
         {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto data = configured(3);
        data.inputs["RampRange"].floats[0] = number;
        resetRun();
        assert(run<Node>(data) == MS::kInvalidParameter && publications == 0);
        data = configured(3);
        rampFunction = [number](float) { return number; };
        resetRun();
        assert(run<Node>(data) == MS::kFailure && publications == 0);
        rampFunction = [](float position) { return position; };
        data = configured(4);
        data.inputs["value"].connected = true;
        data.inputs["GlobalOffset"].number = number;
        resetRun();
        assert(run<Node>(data) == MS::kInvalidParameter && publications == 0);
    }
}
int testTiming() {
    auto data = configured(4);
    activeData = &data;
    MDoubleArray offsets, stretches;
    auto runTiming = [&](std::size_t count) {
        return dommy::maya::readTimeOffsetAndStretch(data, {"node"}, {"InOffsetArray"},
                                                     {"InStretchArray"}, count, offsets, stretches);
    };
    resetRun();
    assert(runTiming(3) == MS::kSuccess);
    assert(offsets.values == std::vector<double>({0, 0, 0}) &&
           stretches.values == std::vector<double>({1, 1, 1}));
    int tested = failures([&]() { return runTiming(3); }, false);
    resetRun();
    assert(runTiming(0) == MS::kSuccess && offsets.length() == 0 && stretches.length() == 0);
    for (std::size_t count : {dommy::generation::kMaxGeneratedPointCount + 1,
                              std::numeric_limits<std::size_t>::max()}) {
        resetRun();
        assert(runTiming(count) == MS::kInvalidParameter && callCount == 0);
    }
    auto& offset = data.inputs["InOffsetArray"];
    auto& stretch = data.inputs["InStretchArray"];
    offset.connected = stretch.connected = true;
    offset.number = 2;
    stretch.number = 0;
    resetRun();
    assert(runTiming(3) == MS::kSuccess);
    assert(offsets.values == std::vector<double>({2, 2, 2}) &&
           stretches.values == std::vector<double>({0, 0, 0}));
    tested += failures([&]() { return runTiming(3); }, false);
    for (auto type : {MFnData::kFloatArray, MFnData::kDoubleArray}) {
        offset.type = stretch.type = type;
        offset.array = {3};
        stretch.array = {2, 4, 0, 99};
        resetRun();
        assert(runTiming(3) == MS::kSuccess);
        assert(offsets.values == std::vector<double>({3, 0, 0}) &&
               stretches.values == std::vector<double>({2, 4, 0}));
        tested += failures([&]() { return runTiming(3); }, false);
        offset.array = {std::numeric_limits<double>::quiet_NaN()};
        resetRun();
        assert(runTiming(3) == MS::kInvalidParameter);
    }
    offset.type = MFnData::kInvalid;
    resetRun();
    assert(runTiming(3) == MS::kInvalidParameter);
    offset.type = MFnData::kNumeric;
    offset.number = std::numeric_limits<double>::infinity();
    resetRun();
    assert(runTiming(3) == MS::kInvalidParameter);
    double sample = 0;
    assert(dommy::maya::computeSampleTime(12, 3, 0, 2, sample) && sample == 11);
    assert(dommy::maya::computeSampleTime(12, 3, 2, 2, sample) && sample == 5);
    assert(!dommy::maya::computeSampleTime(1, 0, std::numeric_limits<double>::denorm_min(), 0,
                                           sample));
    return tested;
}
} // namespace

int main(int argc, char** argv) {
    const std::string schema = schemaTrace("DPIntArrayGen", DPIntArrayGen::initialize) +
                               schemaTrace("DPDoubleArrayGen", DPDoubleArrayGen::initialize);
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--normal") {
        testNode<DPIntArrayGen>(true, true);
        testNode<DPDoubleArrayGen>(false, true);
        return 0;
    }
    assert(argc == 2);
    std::ifstream fixture(argv[1]);
    assert(fixture);
    const std::string expected((std::istreambuf_iterator<char>(fixture)),
                               std::istreambuf_iterator<char>());
    assert(schema == expected);
    const int initializationFailures =
        testFailures("DPIntArrayGen", DPIntArrayGen::initialize, 74) +
        testFailures("DPDoubleArrayGen", DPDoubleArrayGen::initialize, 75);
    const int computeFailures =
        testNode<DPIntArrayGen>(true, false) + testNode<DPDoubleArrayGen>(false, false);
    const int floatShiftCases=testFloatShiftBoundaries();
    const int floatShiftFailures=testFloatShiftFailures();
    const int rampCases = testRampBoundaries<DPIntArrayGen>(true) +
                          testRampBoundaries<DPDoubleArrayGen>(false);
    const int rampFailures = testRampFailures<DPIntArrayGen>(true) +
                             testRampFailures<DPDoubleArrayGen>(false);
    testIntegerBoundaries();
    testInvalidNumbers<DPIntArrayGen>();
    testInvalidNumbers<DPDoubleArrayGen>();
    const int timingFailures = testTiming();
    std::cout << "Scalar arrays: " << initializationFailures << " initializer, " << computeFailures
              << " compute, " << timingFailures
              << " timing failure cases; " << floatShiftCases << " float shift boundary cases and "
              << floatShiftFailures << " float shift failures/retries; "
              << rampCases << " ramp boundary/golden cases and "
              << rampFailures << " ramp failures/retries passed\n";
}
