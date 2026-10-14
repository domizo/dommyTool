// The complete DStringOp implementation and production metadata report class
// are included by the runner. Maya metadata traversal is not modeled here.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "numeric_locale_fixture.h"
#include "DDataUtility/Data/DPAttributeData.h"
#ifndef DOMMY_DIAGNOSTIC_SDK_CHECK
typedef std::vector<std::string> DStringList;
#endif
#include "diagnostic_text_methods.inc"
#ifdef DOMMY_DIAGNOSTIC_ALLOCATION_TESTS
#include "allocation_failure_fixture.h"
#endif

namespace description_fixture {
// Use the actual attribute implementation. Group payloads are opaque because
// the production name formatter accesses only the stored map keys.
using ::DPAttrList;
using DPGrpList = std::map<std::string, std::shared_ptr<int>>;
#include "description_methods.inc"
}
#include "diagnostic_vdb_fixture.h"

namespace {
struct FailedInsertion { bool bad; };
std::ostream& operator<<(std::ostream& stream, const FailedInsertion& value)
{
    stream << "partial";
    stream.setstate(value.bad ? std::ios::badbit : std::ios::failbit);
    return stream;
}

void checkReport()
{
    BoundedMetadataReport report;
    assert(report.appendLine("count:", 42));
    assert(report.finish() == "count:42\n");
    for (bool bad : {false, true}) {
        bool rejected = false;
        try { report.appendLine("failed:", FailedInsertion{bad}); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected && report.finish() == "count:42\n" && !report.full());
        rejected = false;
        try { report.appendLine("structure name:", "member", ' ', FailedInsertion{bad}); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected && report.finish() == "count:42\n");
    }
    assert(report.appendLine("structure name:", "member", ' ', "complete"));
    assert(report.finish() == "count:42\nstructure name:member complete\n");
    BoundedMetadataReport bytes;
    assert(!bytes.append(std::string(256U * 1024U + 1U, 'x')));
    assert(bytes.full());
    const std::string marker = "\n[metadata report truncated]\n";
    assert(bytes.finish() == std::string(256U * 1024U, 'x') + marker);
    const std::string capped = bytes.finish();
    assert(!bytes.append("ignored") && bytes.finish() == capped);
    BoundedMetadataReport lines;
    for (unsigned int line = 0; line < 9999U; ++line) assert(lines.append("x\n"));
    assert(!lines.append("last\n") && lines.full());
    assert(lines.finish().substr(lines.finish().size() - marker.size()) == marker);
}

void checkMerge()
{
    assert(mergeStringList({}) == "");
    assert(mergeStringList({"", "x", ""}) == " x ");
    assert(mergeStringList({"first", "last"}) == "first last");
    assert(formattedFileSize(0U) == "0 bytes");
    assert(formattedFileSize(1024U) == "1 KB");
    assert(formattedFileSize(1024U * 1024U) == "1 MB");
    assert(formattedFileSize(1024ULL * 1024ULL * 1024ULL) == "1 GB");
    dommy_test::ScopedNumericLocale locale(',', '_');
    std::ostringstream reference;
    reference << 1000ULL << " bytes";
    assert(formattedFileSize(1000U) == reference.str());
    BoundedMetadataReport report;
    assert(report.appendLine("value:", 1234.5));
    std::ostringstream line;
    line << "value:" << 1234.5 << '\n';
    assert(report.finish() == line.str());
}

class FailedNumbers final : public std::num_put<char> {
    iter_type do_put(iter_type, std::ios_base&, char_type, unsigned long) const override
    { throw std::runtime_error("injected number insertion failure"); }
    iter_type do_put(iter_type, std::ios_base&, char_type, unsigned long long) const override
    { throw std::runtime_error("injected number insertion failure"); }
};

void checkDescriptions()
{
    using namespace description_fixture;
    const auto attribute = std::make_shared<TypedDPAttributeData<float>>(std::vector<float>{1.0f});
    const DPAttrList attributes{{"velocity", attribute}};
    assert(GetAllAttributeListInfo(attributes) == DStringList({"@velocity : float"}));
    const description_fixture::DPGrpList groups{{"selected", std::make_shared<int>(0)}};
    DStringList descriptions{"existing"};
    GetAllGroupListInfo(groups, descriptions);
    assert(descriptions == DStringList({"existing", "*selected"}));
    const description_fixture::DPGrpList empty;
    GetAllGroupListInfo(empty, descriptions);
    assert(descriptions == DStringList({"existing", "*selected"}));
    TypedDPAttributeData<float> typed(std::vector<float>(1234U, 1.0f));
    {
        dommy_test::ScopedNumericLocale locale(',', '_');
        std::ostringstream expected;
        expected << "float[" << typed.attrSize() << ']';
        assert(typed.str() == expected.str());
    }
    {
        dommy_test::ScopedNumericLocale locale(std::locale(std::locale::classic(), new FailedNumbers));
        bool rejected = false;
        std::string output("prior");
        try { output = typed.str(); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected && output == "prior");
    }
#ifdef DOMMY_DIAGNOSTIC_ALLOCATION_TESTS
    const std::string name(128U, 'x');
    const DPAttrList longAttributes{{name, attribute}};
    const description_fixture::DPGrpList longGroups{{name, std::make_shared<int>(0)},
                                                   {name + "b", std::make_shared<int>(0)}};
    const DStringList desiredAttributes{"@" + name + " : float"};
    const DStringList priorGroups{"existing"};
    const DStringList desiredGroups{"existing", "*" + name, "*" + name + "b"};
    unsigned int attributeFaults = 0U, groupFaults = 0U;
    for (std::size_t failure = 1U; failure <= 64U; ++failure) {
        DStringList result{"prior"};
        bool returned = false;
        fault::arm(failure);
        try { result = GetAllAttributeListInfo(longAttributes); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        attributeFaults += fault::triggered;
        assert(returned ? result == desiredAttributes : result == DStringList({"prior"}));
        result = priorGroups;
        fault::arm(failure);
        returned = false;
        try { GetAllGroupListInfo(longGroups, result); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        groupFaults += fault::triggered;
        assert(returned ? result == desiredGroups : result == priorGroups);
    }
    assert(attributeFaults != 0U && groupFaults != 0U);
#endif
}

void checkVdbInfo()
{
    using namespace vdb_info_fixture;
    DOpenVDBData data;
    assert(std::string(buildVdbInfo(data, "").asChar()) == "DOpenVDB Info\n\tGrid is empty.");
    auto grid = std::make_shared<openvdb::GridBase>();
    grid->name = std::string(4096U, 'x');
    data.grids.push_back(grid);
    data.grids.push_back(nullptr); // Retain the historical skipped-null entry.
    const int resolution[3] = {std::numeric_limits<int>::max(), 1234, 5};
    const std::string expected = buildVdbInfo(data, "").asChar();
    assert(expected.find(grid->name) != std::string::npos);
    assert(expected.find("grid count: 2") != std::string::npos);
    const Count points{1234U}, primitives{500U};
    const Timer timer;
    const std::string expectedNode = meshNodeInfoText(points, primitives, &data, timer).asChar();
    const std::string expectedResolution = meshResolutionText(resolution).asChar();
    {
        dommy_test::ScopedNumericLocale locale(',', '_');
        assert(std::string(buildVdbInfo(data, "").asChar()).find("Active voxel count: 1_234") != std::string::npos);
        std::ostringstream reference;
        reference << resolution[0] << " x " << resolution[1] << " x " << resolution[2] << " voxels";
        assert(std::string(meshResolutionText(resolution).asChar()) == reference.str());
    }
#ifdef DOMMY_DIAGNOSTIC_ALLOCATION_TESTS
    unsigned int faults = 0U, rejected = 0U;
    for (std::size_t failure = 1U; failure <= 64U; ++failure) {
        for (unsigned int operation = 0U; operation < 3U; ++operation) {
            std::string output("prior");
            bool returned = false;
            fault::arm(failure);
            try {
                if (operation == 0U) output = buildVdbInfo(data, "").asChar();
                if (operation == 1U) output = meshResolutionText(resolution).asChar();
                if (operation == 2U) output = meshNodeInfoText(points, primitives, &data, timer).asChar();
                returned = true;
            } catch (const std::exception&) {}
            fault::armed = false;
            faults += fault::triggered;
            rejected += !returned;
            const std::string& desired = operation == 0U ? expected : operation == 1U ? expectedResolution : expectedNode;
            assert(returned ? output == desired : output == "prior");
        }
    }
    assert(faults != 0U && rejected != 0U);
#endif
}

#ifdef DOMMY_DIAGNOSTIC_ALLOCATION_TESTS
template <typename Operation>
void checkAllocationFailure(const std::string& expected, Operation operation)
{
    std::size_t triggered = 0U, rejected = 0U;
    for (std::size_t failure = 1U; failure <= 64U; ++failure) {
        std::string output("prior");
        bool returned = false;
        fault::arm(failure);
        try { output = operation(); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        triggered += fault::triggered;
        rejected += !returned;
        assert(returned ? output == expected : output == "prior");
    }
    assert(triggered != 0U && rejected != 0U);
}

void checkAllocations(bool report)
{
    const std::string payload(4096U, 'x');
    if (report) {
        const std::string expected = "value:" + payload + "\n";
        checkAllocationFailure(expected, [&] {
            BoundedMetadataReport result;
            assert(result.appendLine("value:", payload));
            return result.finish();
        });
        checkAllocationFailure("structure name:member " + payload + "\n", [&] {
            BoundedMetadataReport result;
            assert(result.appendLine("structure name:", "member", ' ', payload));
            return result.finish();
        });
    } else {
        const std::vector<std::string> names{"first", payload, "last"};
        checkAllocationFailure("first " + payload + " last", [&] { return mergeStringList(names); });
    }
}
#endif
} // namespace

int main(int argc, char** argv)
{
    const bool all = argc != 2 || std::strcmp(argv[1], "all") == 0;
    const bool report = all || std::strcmp(argv[1], "report") == 0;
    const bool merge = all || std::strcmp(argv[1], "merge") == 0;
    const bool descriptions = all || std::strcmp(argv[1], "descriptions") == 0;
    const bool vdb = all || std::strcmp(argv[1], "vdb") == 0;
    if (report) checkReport();
    if (merge) checkMerge();
    if (descriptions) checkDescriptions();
    if (vdb) checkVdbInfo();
#ifdef DOMMY_DIAGNOSTIC_ALLOCATION_TESTS
    if (report) checkAllocations(true);
    if (merge) checkAllocations(false);
#endif
    std::cout << "Diagnostic text: complete-or-error formatting, locale and intentional report limits: OK\n";
}
