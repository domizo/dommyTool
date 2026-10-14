// Real production planners/metadata helpers with small host-boundary doubles.
// This does not emulate native geometry, Maya allocation or DG publication.
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int admissions = 0, splits = 0;
static MStatus splitStatus = MS::kSuccess;
struct MStringArray;
struct MString {
    std::string text;
    std::size_t reportedBytes = 0U;
    MString(const char* value = "") : text(value) {}
    explicit MString(const std::string& value) : text(value) {}
    const char* asChar() const { return text.c_str(); }
    // Maya length() is the asChar byte count, not numChars().
    std::size_t length() const { return reportedBytes ? reportedBytes : text.size(); }
    MStatus split(char separator, MStringArray& result) const;
};
struct MStringArray {
    std::vector<MString> values;
    unsigned length() const { return static_cast<unsigned>(values.size()); }
    const MString& operator[](unsigned i) const { return values.at(i); }
};
MStatus MString::split(char separator, MStringArray& result) const {
    assert(separator == ' ');
    ++splits;
    if (splitStatus != MS::kSuccess) return splitStatus;
    std::istringstream stream(text);
    std::string token;
    while (std::getline(stream, token, separator)) {
        if (!token.empty()) result.values.emplace_back(token);
    }
    return MS::kSuccess;
}

struct DPData {
    std::size_t count = 2U;
    std::map<std::string, DPAttributeData::ConstPtr> fields;
    std::size_t numberOfPoints() const { return count; }
    bool isExistAttr(const std::string& name) const { return fields.count(name) != 0; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const { return fields.at(name); }
    template <typename T> const std::vector<T>& attrListValue(const std::string& name) const {
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T> >(fields.at(name))->value();
    }
    template <typename T> T attrDefaultValue(const std::string& name) const {
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T> >(fields.at(name))->getDefaultValue();
    }
};
struct Curve {
    std::size_t count;
    bool valid;
    Curve(std::size_t n, bool ok = true) : count(n), valid(ok) {}
    bool hasValidCurveTopology() const { return valid; }
    std::size_t numberOfPoints() const { return count; }
};
struct DCurveData : DPData {
    std::vector<std::shared_ptr<Curve> > curves;
    std::vector<std::string> transferOrder;
    using Iterator = std::vector<std::shared_ptr<Curve> >::const_iterator;
    Iterator beginCurves() const { return curves.begin(); }
    Iterator endCurves() const { return curves.end(); }
    template <typename T> void addAttr(const std::string& name, const TypedDPAttributeData<T>& value) {
        fields[name] = value.copy();
        transferOrder.push_back(name);
    }
};

#include "fur_aho_storage.inc"

using namespace dommy::generation;
template <typename Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); }
    catch (const std::length_error&) { rejected = true; }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
template <typename T>
void field(DPData& input, const char* name, const std::vector<T>& values, const T& fallback) {
    input.fields[name] = std::make_shared<TypedDPAttributeData<T> >(values, fallback);
}

void plans() {
    FurGenerationPlan fur;
    AhoGenerationPlan aho;
    assert(buildFurGenerationPlan(20, 9, fur));
    assert(fur.curveCount == 20 && fur.pointsPerCurve == 10 && fur.totalPointCount == 200);
    const std::size_t expected = 20U * 2048U + 200U * 96U + 10U * 512U + 20U * 256U;
    assert(fur.estimatedBytes == expected);
    assert(buildAhoGenerationPlan(2, 10, 10, nullptr, 0, aho));
    assert(aho.outputCurveCount == 20 && aho.totalPointCount == 200);
    assert(aho.estimatedBytes == expected - 10U * 256U);

    // Boundary arithmetic only: never allocate the requested large geometry.
    for (std::size_t points : {3U, 10U, 1000U, 100000U}) {
        const std::size_t curves = (kMaxCurveGenerationBytes - points * 512U) /
                                   (2048U + points * 96U + 256U);
        assert(buildFurGenerationPlan(curves, static_cast<int>(points - 1U), fur));
        assert(fur.estimatedBytes == curves * (2048U + points * 96U + 256U) + points * 512U);
        const FurGenerationPlan before = fur;
        assert(!buildFurGenerationPlan(curves + 1, static_cast<int>(points - 1U), fur));
        assert(fur.estimatedBytes == before.estimatedBytes && fur.curveCount == before.curveCount);
        assert(buildAhoGenerationPlan(1, static_cast<int>(curves), static_cast<int>(points), nullptr, 0, aho));
        const AhoGenerationPlan saved = aho;
        assert(!buildAhoGenerationPlan(1, static_cast<int>(curves + 1), static_cast<int>(points), nullptr, 0, aho));
        assert(aho.estimatedBytes == saved.estimatedBytes && aho.outputCurveCount == saved.outputCurveCount);
    }
    assert(!buildFurGenerationPlan(1000000, 9, fur));
    assert(!buildAhoGenerationPlan(1, 1000000, 10, nullptr, 0, aho));
    assert(!buildAhoGenerationPlan(1, 1, 10000000, nullptr, 0, aho));
    for (int invalid : {INT_MIN, -1, 0, 1, INT_MAX})
        assert(!buildFurGenerationPlan(1, invalid, fur));
    assert(!buildFurGenerationPlan(std::numeric_limits<std::size_t>::max(), 2, fur));
    assert(!buildAhoGenerationPlan(std::numeric_limits<std::size_t>::max(), 1, 2, nullptr, 0, aho));
    assert(!buildAhoGenerationPlan(1, INT_MAX, 2, nullptr, 0, aho));
    assert(!buildAhoGenerationPlan(1, 1, 2, nullptr, 1, aho));
    const double invalid[] = {std::numeric_limits<double>::quiet_NaN()};
    assert(!buildAhoGenerationPlan(1, 1, 2, invalid, 1, aho));
    assert(buildAhoGenerationPlan(1, 0, 10, nullptr, 0, aho) && aho.estimatedBytes == 5120);
    assert(!buildAhoGenerationPlan(1, 0, 10000000, nullptr, 0, aho));
    assert(buildFurGenerationPlan(0, 9, fur) && fur.estimatedBytes == 5120);

    // Aho still traverses input curves when no copies are requested, so its
    // input-index seed must remain representable independently of output count.
    assert(buildAhoGenerationPlan(10, 0, 10, nullptr, 0, aho));
    assert(ahoSeedAdmission(1, aho));
    assert(ahoSeedAdmission(INT_MAX - 13, aho));
    assert(!ahoSeedAdmission(INT_MAX - 12, aho));
    assert(!ahoSeedAdmission(INT_MAX - 3, aho));
    assert(!ahoSeedAdmission(-1, aho));
    assert(buildAhoGenerationPlan(2, 10, 10, nullptr, 0, aho));
    assert(ahoSeedAdmission(INT_MAX - 23, aho));
    assert(!ahoSeedAdmission(INT_MAX - 22, aho));

    std::size_t bytes = kMaxCurveGenerationBytes - 1U;
    assert(addCurveGenerationBytes(1, 1, bytes) && bytes == kMaxCurveGenerationBytes);
    assert(!addCurveGenerationBytes(1, 1, bytes) && bytes == kMaxCurveGenerationBytes);
    assert(!addCurveGenerationBytes(std::numeric_limits<std::size_t>::max(), 2, bytes));
    CurveGenerationStorage storage(kMaxCurveGenerationBytes - 10U);
    storage.add(2, 5);
    rejects([&] { storage.add(1, 1); });
    assert(storage.estimatedBytes() == kMaxCurveGenerationBytes);
    rejects([] { CurveGenerationStorage tooLarge(kMaxCurveGenerationBytes + 1U); });
}

void metadata() {
    DPData input;
    field<int32_t>(input, "ids", {7, 9}, -1);
    field<std::string>(input, "labels", {"one", "four"}, "fallback");
    field<DIntArray>(input, "arrays", {{1, 2}, {3}}, {8, 9, 10, 11});
    const std::size_t ids = 4U * (3U * sizeof(int32_t) + 3U);
    const std::size_t labels = 4U * (3U * sizeof(std::string) + 6U + 7U + 8U);
    const std::size_t arrays = 4U * (3U * sizeof(DIntArray) + 6U + 7U * sizeof(int));
    const MString names("ids labels missing arrays ids");
    const std::size_t needed = names.length() * 64U + 2U * ids + labels + arrays;
    CurveGenerationStorage account(123);
    MStringArray tokens;
    assert(preflightFurTransfers(input, names, 2, account, tokens) == MS::kSuccess);
    assert(account.estimatedBytes() == 123 + needed && tokens.length() == 5);
    assert(tokens[0].text == "ids" && tokens[1].text == "labels" &&
           tokens[2].text == "missing" && tokens[3].text == "arrays" && tokens[4].text == "ids");
    CurveGenerationStorage exact(kMaxCurveGenerationBytes - needed);
    MStringArray exactNames;
    assert(preflightFurTransfers(input, names, 2, exact, exactNames) == MS::kSuccess);
    assert(exact.estimatedBytes() == kMaxCurveGenerationBytes);
    rejects([&] {
        CurveGenerationStorage over(kMaxCurveGenerationBytes - needed + 1U);
        MStringArray list;
        preflightFurTransfers(input, names, 2, over, list);
    });

    // Missing names still consume bounded token storage and preserve split order.
    MString unicode("\xc3\xa9");
    CurveGenerationStorage unicodeAccount(0);
    MStringArray unicodeNames;
    assert(preflightFurTransfers(input, unicode, 2, unicodeAccount, unicodeNames) == MS::kSuccess);
    assert(unicodeAccount.estimatedBytes() == 128);
    MString inconsistent("alpha"); inconsistent.reportedBytes = 1U;
    CurveGenerationStorage invalidAccount(0); MStringArray invalidNames;
    const int beforeInvalid = splits;
    assert(preflightFurTransfers(input, inconsistent, 2, invalidAccount, invalidNames) != MS::kSuccess &&
           splits == beforeInvalid);
    rejects([&] {
        CurveGenerationStorage full(kMaxCurveGenerationBytes);
        MStringArray list;
        const int before = splits;
        try { preflightFurTransfers(input, "ids", 2, full, list); }
        catch (...) { assert(splits == before); throw; }
    });
    splitStatus = MS::kFailure;
    CurveGenerationStorage failed(0);
    MStringArray failedNames;
    assert(preflightFurTransfers(input, "ids", 2, failed, failedNames) == MS::kFailure);
    splitStatus = MS::kSuccess;

    for (const std::string name : {"ids", "labels", "arrays"}) {
        rejects([&] {
            CurveGenerationStorage budget(0); MStringArray list;
            preflightFurTransfers(input, MString(name), 3, budget, list);
        });
    }
    input.fields["null"] = nullptr;
    field<float>(input, "badDefault", {1, 2}, std::numeric_limits<float>::infinity());
    field<double>(input, "badValue", {1, std::numeric_limits<double>::quiet_NaN()}, 0);
    field<std::string>(input, "badString", {"ok", std::string("a\0b", 3)}, "");
    for (const char* name : {"null", "badDefault", "badValue", "badString"}) {
        rejects([&] {
            CurveGenerationStorage budget(0); MStringArray list;
            preflightFurTransfers(input, name, 2, budget, list);
        });
    }
    dommy::maya::NameListBudget nameBudget;
    MStringArray excessive;
    assert(dommy::maya::splitCheckedNameList("a b", kMaxWorkItemCount, nameBudget, excessive) == MS::kInvalidParameter);
    assert(nameBudget.bytes == 0 && nameBudget.names == 0 && nameBudget.workItems == 0);

    // Actual selected-transfer templates: ordered subset and dynamic defaults.
    DCurveData output;
    for (const auto& token : tokens.values) {
        if (input.isExistAttr(token.text)) assert(copySelectedAttribute(input, token.text, {1, 0, 1}, output));
    }
    assert((output.transferOrder == std::vector<std::string>{"ids", "labels", "arrays", "ids"}));
    assert((output.attrListValue<int32_t>("ids") == std::vector<int32_t>{9, 7, 9}));
    assert((output.attrListValue<std::string>("labels") == std::vector<std::string>{"four", "one", "four"}));
    assert(output.attrDefaultValue<std::string>("labels") == "fallback");
    assert((output.attrDefaultValue<DIntArray>("arrays") == DIntArray{8, 9, 10, 11}));
    const auto before = output.transferOrder;
    assert(!copySelectedAttribute(input, "ids", {2}, output));
    assert(output.transferOrder == before);

    admissions = 0;
    assert(furAdmission(&input, 2, "ids") == MS::kSuccess && admissions == 1);
    assert(furAdmission(&input, 2, "badDefault") != MS::kSuccess && admissions == 1);
    splitStatus = MS::kFailure;
    assert(furAdmission(&input, 2, "ids") != MS::kSuccess && admissions == 1);
    splitStatus = MS::kSuccess;
    DPData large; large.count = 1000000;
    assert(furAdmission(&large, 9, "") != MS::kSuccess && admissions == 1);
}

void nativeInput() {
    DCurveData input;
    input.curves = {std::make_shared<Curve>(10), std::make_shared<Curve>(20)};
    CurveGenerationStorage account(512);
    assert(preflightAhoInputStorage(input, account) == MS::kSuccess);
    assert(account.estimatedBytes() == 512 + 20 * 128);
    CurveGenerationStorage exact(kMaxCurveGenerationBytes - 20 * 128);
    assert(preflightAhoInputStorage(input, exact) == MS::kSuccess);
    rejects([&] {
        CurveGenerationStorage over(kMaxCurveGenerationBytes - 20 * 128 + 1);
        preflightAhoInputStorage(input, over);
    });
    AhoGenerationPlan plan;
    assert(buildAhoGenerationPlan(2, 1, 3, nullptr, 0, plan));
    admissions = 0;
    assert(ahoAdmission(&input, plan) == MS::kSuccess && admissions == 1);
    input.curves[0]->valid = false;
    assert(ahoAdmission(&input, plan) == MS::kInvalidParameter && admissions == 1);
    input.curves[0].reset();
    assert(ahoAdmission(&input, plan) == MS::kInvalidParameter && admissions == 1);
    input.curves = {std::make_shared<Curve>(std::numeric_limits<std::size_t>::max())};
    assert(ahoAdmission(&input, plan) != MS::kSuccess && admissions == 1);
    input.curves.clear();
    CurveGenerationStorage empty(17);
    assert(preflightAhoInputStorage(input, empty) == MS::kSuccess && empty.estimatedBytes() == 17);
}

int main() {
    plans();
    metadata();
    nativeInput();
    std::cout << "Fur/Aho: aggregate byte boundaries, transfer/default payload, native input and admission checks passed\n";
}
