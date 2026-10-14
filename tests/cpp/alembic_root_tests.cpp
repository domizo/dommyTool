#include "alembic_root_host.h"
#include "root_methods.inc"
#include <iostream>

template<class Callback> void rejected(Callback callback) {
    bool failed = false;
    try { callback(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
void reset() { queries = 0U; failAt = -1; faultKind = 0; }
MSelectionList selection() {
    MSelectionList active;
    active.paths = {MDagPath("|root|child"), MDagPath("|root|other"), MDagPath("|utility")};
    active.dagItems = {true, true, false};
    return active;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    using dommy::exporting::RootAdmission;
    using namespace ddata_export_util;
    reset(); RootAdmission many; ShapeSet paths;
    for (unsigned int index = 0U; index < 15000U; ++index) {
        const std::string name = "|root" + std::to_string(index) + "|child";
        insertExportRoot(paths, MDagPath(name.c_str()), many);
    }
    many.validate();
    assert(paths.size() == 15000U && many.size() == 15000U);
    assert(many.work().used() < 30000000U && queries < 2000000U);
    const auto before = many.nameBytes();
    insertExportRoot(paths, MDagPath("|root0|child"), many);
    assert(paths.size() == 15000U && many.nameBytes() == before);
    std::cout << "15000 roots: bounded indexed validation, " << many.work().used() << " work units\n";
    for (bool reverse : {false, true}) {
        RootAdmission roots;
        roots.admit(reverse ? "|a|c|child" : "|a|c", sizeof(MDagPath));
        roots.admit("|a|c_sibling", sizeof(MDagPath));
        roots.admit(reverse ? "|a|c" : "|a|c|child", sizeof(MDagPath));
        rejected([&] { roots.validate(); });
    }
    RootAdmission unrelated;
    for (const char* name : {"|a", "|ab", "|ns:root", "|other:root"}) unrelated.admit(name, 0U);
    unrelated.validate();
    for (const std::string& name : {std::string(""), std::string("|"), std::string("root"),
         std::string("|root|"), std::string("|root||child"), std::string("|root\nchild"),
         std::string("|bad\0node", 9U), "|" + std::string(32768U, 'a')})
        rejected([&] { RootAdmission invalid; invalid.admit(name, 0U); });
    std::string deep;
    for (unsigned int depth = 0U; depth < 256U; ++depth) deep += "|n";
    RootAdmission maximum; maximum.admit(deep, 0U); maximum.validate();
    rejected([&] { RootAdmission invalid; invalid.admit(deep + "|n", 0U); });
    rejected([&] { RootAdmission invalid; invalid.admit("|a", 64U * 1024U * 1024U); });
    rejected([&] { RootAdmission invalid; invalid.admit("|a", std::numeric_limits<std::size_t>::max()); });
    rejected([&] { RootAdmission invalid; invalid.admitSelection(1000001U); });
    auto work = std::make_shared<dommy::generation::WorkBudget>(100U);
    RootAdmission first(work), second(work);
    first.admitSelection(60U);
    rejected([&] { second.admitSelection(41U); });
    assert(work->exceeded());
    rejected([&] { first.admitSelection(0U); });
    rejected([&] { RootAdmission invalid(std::shared_ptr<dommy::generation::WorkBudget>{}); });
    auto tiny = std::make_shared<dommy::generation::WorkBudget>(3U);
    rejected([&] { RootAdmission invalid(tiny); invalid.admit("|a", 0U); });

    reset(); MSelectionList expanded; ShapeSet inferred; RootAdmission admission;
    prepareExportSelection(expanded, inferred, selection(), admission);
    assert(inferred.size() == 1U && expanded.length() == 5U);
    const auto callCount = queries;
    const std::string destination = argv[1];
    { std::ofstream output(destination); output << "existing"; }
    for (std::size_t index = 0U; index < callCount; ++index) for (int kind : {1, 2, 3}) {
        // getDagPath normally reports failure for this non-DAG item. A returned
        // failure at that same boundary is still a valid omission; classification
        // and exceptions must not be omitted.
        if (kind == 1 && index == callCount - 2U) continue;
        reset(); failAt = int(index); faultKind = kind;
        std::string temporary;
        rejected([&] {
            dommy::file::PendingOutputFile file(destination, dommy::file::PendingOutputFile::Access::TrustedPath); temporary = file.trustedTemporaryPath();
            MSelectionList stage; ShapeSet roots; RootAdmission budget;
            prepareExportSelection(stage, roots, selection(), budget);
            file.commit();
        });
        assert(!dommy::file::isRegularFile(temporary));
        std::ifstream input(destination);
        assert(std::string((std::istreambuf_iterator<char>(input)), {}) == "existing");
    }
    reset(); MSelectionList oversized; oversized.countOverride = 1000001U;
    rejected([&] { ShapeSet roots; RootAdmission budget; prepareExportSelection(expanded, roots, oversized, budget); });
    assert(queries == 0U);
    reset(); MSelectionList empty;
    rejected([&] { ShapeSet roots; RootAdmission budget; prepareExportSelection(expanded, roots, empty, budget); });
    reset(); MSelectionList nonDag; nonDag.paths = {MDagPath("|utility")}; nonDag.dagItems = {false};
    rejected([&] { ShapeSet roots; RootAdmission budget; prepareExportSelection(expanded, roots, nonDag, budget); });
    reset(); MSelectionList overDepth; overDepth.paths = {MDagPath((deep + "|n").c_str())}; overDepth.dagItems = {true};
    rejected([&] { ShapeSet roots; RootAdmission budget; prepareExportSelection(expanded, roots, overDepth, budget); });
    reset(); RootAdmission explicitBudget; ShapeSet explicitRoots;
    insertExportRoot(explicitRoots, MDagPath("|explicit"), explicitBudget);
    prepareExportSelection(expanded, explicitRoots, selection(), explicitBudget);
    assert(explicitRoots.size() == 1U && explicitRoots.begin()->name == "|explicit");
    reset(); assert(isAncestorDescendentRelationship(MDagPath("|a"), MDagPath("|a|child")));
    assert(!isAncestorDescendentRelationship(MDagPath("|a"), MDagPath("|ab")));
    for (int kind : {1, 2, 3}) for (int index : {0, 1}) {
        reset(); failAt = index; faultKind = kind;
        rejected([&] { isAncestorDescendentRelationship(MDagPath("|a"), MDagPath("|b")); });
    }
    reset();
    std::cout << "Root count/name/depth/storage/work guards, path statuses, selection expansion and file preservation: OK\n";
    MSelectionList explicitSelection;
    explicitSelection.paths = {MDagPath("|groupA|first"), MDagPath("|groupB|second"), MDagPath("|utility")};
    explicitSelection.dagItems = {true, true, false};
    reset(); RootJobArgs explicitArgs;
    assert(collectExplicitRoots(explicitArgs, explicitSelection));
    assert(explicitArgs.dagPaths.size() == 2U);
    const auto explicitCalls = queries;
    for (std::size_t index = 0U; index < explicitCalls; ++index) for (int kind : {1, 2, 3}) {
        if (kind == 1 && index == explicitCalls - 2U) continue;
        reset(); failAt = int(index); faultKind = kind;
        std::string temporary;
        rejected([&] {
            dommy::file::PendingOutputFile owner(destination, dommy::file::PendingOutputFile::Access::TrustedPath); temporary = owner.trustedTemporaryPath();
            RootJobArgs stage; collectExplicitRoots(stage, explicitSelection);
            { std::ofstream output(owner.trustedTemporaryPath()); output << "partial"; }
            owner.commit();
        });
        assert(!dommy::file::isRegularFile(temporary));
        std::ifstream input(destination);
        assert(std::string((std::istreambuf_iterator<char>(input)), {}) == "existing");
    }
    reset(); MDagPath retained("|retained"); failAt = 0; faultKind = 1;
    rejected([&] { readExportDagSelectionItem(explicitSelection, 0U, retained); });
    assert(retained.name == "|retained");
    reset(); assert(!readExportDagSelectionItem(explicitSelection, 2U, retained));
    assert(retained.name == "|retained");
    for (unsigned int depth : {0U, 1U, 2U, std::numeric_limits<unsigned int>::max()}) {
        reset(); RootAdmission names; ShapeSet duplicate;
        const char* first = depth == 0U ? "|groupA|mesh" : "|nsA:group|alpha:shared:mesh";
        const char* second = depth == 0U ? "|groupB|mesh" : depth == 1U ?
            "|nsB:other|beta:shared:mesh" : "|nsB:other|gamma:other:mesh";
        insertExportRoot(duplicate, MDagPath(first), names);
        insertExportRoot(duplicate, MDagPath(second), names);
        if (depth != 0U) requireUniqueExportRootNames(duplicate, 0U, names);
        std::string temporary;
        rejected([&] {
            dommy::file::PendingOutputFile owner(destination, dommy::file::PendingOutputFile::Access::TrustedPath); temporary = owner.trustedTemporaryPath();
            requireUniqueExportRootNames(duplicate, depth, names); owner.commit();
        });
        assert(!dommy::file::isRegularFile(temporary));
        std::ifstream input(destination);
        assert(std::string((std::istreambuf_iterator<char>(input)), {}) == "existing");
    }
    reset(); RootAdmission distinct; ShapeSet distinctPaths;
    insertExportRoot(distinctPaths, MDagPath("|nsA:group|alpha:beta:mesh"), distinct);
    insertExportRoot(distinctPaths, MDagPath("|nsB:group|gamma:delta:mesh"), distinct);
    queries = 0U;
    requireUniqueExportRootNames(distinctPaths, 1U, distinct);
    const auto nameCalls = queries;
    for (std::size_t index = 0U; index < nameCalls; ++index) for (int kind : {1, 2, 3}) {
        reset(); RootAdmission budget; ShapeSet stage;
        insertExportRoot(stage, MDagPath("|nsA:group|alpha:beta:mesh"), budget);
        insertExportRoot(stage, MDagPath("|nsB:group|gamma:delta:mesh"), budget);
        queries = 0U; failAt = int(index); faultKind = kind;
        rejected([&] { requireUniqueExportRootNames(stage, 1U, budget); });
    }
    reset(); MObject namedObject; namedObject.name = "alpha:beta:leaf";
    MStatus bindStatus; MFnDependencyNode namedNode(namedObject, &bindStatus);
    assert(exportObjectName(namedNode, 0U).text == "alpha:beta:leaf");
    assert(exportObjectName(namedNode, 1U).text == "beta:leaf");
    assert(exportObjectName(namedNode, 2U).text == "leaf");
    assert(exportObjectName(namedNode, std::numeric_limits<unsigned int>::max()).text == "leaf");
    for (const std::string& invalid : {std::string(""), std::string(32769U, 'n')}) {
        reset(); namedObject.name = invalid; MFnDependencyNode invalidNode(namedObject, &bindStatus);
        rejected([&] { exportObjectName(invalidNode, 0U); });
    }
    reset(); RootAdmission fullStorage;
    const std::size_t fixed = sizeof(std::string) + 6U * sizeof(void*);
    fullStorage.admit("|a", 64U * 1024U * 1024U - fixed - 2U);
    rejected([&] { fullStorage.requireTemporaryStorage(1U); });
    reset(); RootAdmission workLimited; ShapeSet workPaths;
    insertExportRoot(workPaths, MDagPath("|unique"), workLimited);
    assert(workLimited.work().consume(workLimited.work().remaining()));
    queries = 0U;
    rejected([&] { requireUniqueExportRootNames(workPaths, 0U, workLimited); });
    assert(queries == 0U);
    std::cout << "Actual explicit-root loop, common naming, namespace/status failures, duplicate preflight and combined budgets: OK\n";
}
