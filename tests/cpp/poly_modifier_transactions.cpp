#include "poly_modifier_host.h"
#include "poly_modifier_methods.inc"

#include <cstdio>
#include <limits>

namespace host = poly_host;
static const host::Failure failures[] = {host::returnedFailure, host::standardException, host::unknownException};

static void originalScene() {
    assert(host::nodes.at(1).alive && host::nodes.at(2).alive);
    assert(host::nodes.at(2).mesh == 100);
    assert(host::nodes.at(2).tweaks.at(7).x == (host::withTweaks ? 1.0F : 0.0F));
    assert(temporaryInputs.empty());
}

static void checkConstruction(bool history, bool recording, bool tweaks) {
    host::reset(history, recording, tweaks);
    polyBlast successful;
    assert(successful.doIt(MArgList()));
    assert(host::nodes.at(2).mesh == 50 && temporaryInputs.empty());
    if (history || recording) assert(host::nodes.at(2).tweaks.at(7).x == 0.0F);
    if (!history && recording) {
        unsigned int upstreamShapes = 0;
        for (const auto& node : host::nodes) {
            if (node.first == 2 || !node.second.alive || !node.second.shape) continue;
            assert(node.second.parent == 1 && node.second.intermediate);
            ++upstreamShapes;
        }
        assert(upstreamShapes == 1);
    }
    const auto constructionCalls = host::calls;
    assert(!successful.doIt(MArgList())); // Do not overwrite the retained plan.
    assert(successful.undoIt()); originalScene();
    assert(successful.redoIt() && host::nodes.at(2).mesh == 50);
    const unsigned int directCalls = host::calls["direct.edit"];
    assert(successful.redoIt() && host::calls["direct.edit"] == directCalls);

    for (host::Failure failure : failures) {
        for (const auto& call : constructionCalls) {
            for (unsigned int occurrence = 1; occurrence <= call.second; ++occurrence) {
                host::reset(history, recording, tweaks);
                host::fail(call.first, failure, occurrence);
                polyBlast rejected;
                const MStatus status = rejected.doIt(MArgList());
                if (status) std::fprintf(stderr, "Unhandled construction fault: %s/%u mode %d/%d/%d\n", call.first.c_str(), occurrence, history, recording, tweaks);
                assert(!status);
                originalScene();
                assert(host::aliveDuplicates() == 0);
            }
        }
    }
}

static void checkUndoRedo(bool history, bool recording, bool tweaks) {
    host::reset(history, recording, tweaks);
    polyBlast good;
    assert(good.doIt(MArgList()));
    host::calls.clear();
    assert(good.undoIt());
    const auto undoCalls = host::calls;
    host::calls.clear();
    assert(good.redoIt());
    const auto redoCalls = host::calls;
    for (host::Failure failure : failures) {
        for (const auto& call : undoCalls) {
            for (unsigned int occurrence = 1; occurrence <= call.second; ++occurrence) {
                host::reset(history, recording, tweaks);
                polyBlast rejected;
                assert(rejected.doIt(MArgList()));
                const unsigned int beforeDuplicates = host::aliveDuplicates();
                host::calls.clear(); host::fail(call.first, failure, occurrence);
                assert(!rejected.undoIt());
                assert(host::nodes.at(2).mesh == 50 && temporaryInputs.empty());
                assert(host::aliveDuplicates() == beforeDuplicates);
                assert(rejected.undoIt()); originalScene();
            }
        }
        for (const auto& call : redoCalls) {
            for (unsigned int occurrence = 1; occurrence <= call.second; ++occurrence) {
                host::reset(history, recording, tweaks);
                polyBlast rejected;
                assert(rejected.doIt(MArgList()) && rejected.undoIt());
                const unsigned int beforeDuplicates = host::aliveDuplicates();
                host::calls.clear(); host::fail(call.first, failure, occurrence);
                assert(!rejected.redoIt()); originalScene();
                assert(host::aliveDuplicates() == beforeDuplicates);
                assert(rejected.redoIt() && host::nodes.at(2).mesh == 50);
            }
        }
    }
}

static void checkCleanupFailures() {
    for (host::Failure failure : failures) {
        host::reset(false, true, true);
        host::fail("dg.do", failure);
        host::fail("dg.undo", host::invalidParameter);
        polyBlast command;
        assert(command.doIt(MArgList()) == MS::kInvalidParameter);
        assert(host::calls["dag.undo"] == 1 && host::aliveDuplicates() == 0);
        assert(host::reports > 0 && !command.redoIt());

        host::reset(false, false, true);
        polyBlast undoFailure;
        assert(undoFailure.doIt(MArgList()));
        host::calls.clear();
        host::fail("temp.undo", failure);
        assert(!undoFailure.undoIt());
        assert(host::calls["node.delete"] >= 3 && temporaryInputs.empty());
        assert(host::aliveDuplicates() == 0 && host::nodes.at(2).mesh == 50);

        host::reset(false, true, false);
        polyBlast redoFailure;
        assert(redoFailure.doIt(MArgList()) && redoFailure.undoIt());
        host::calls.clear();
        host::fail("dg.do", failure);
        host::fail("dg.undo", host::invalidParameter);
        assert(redoFailure.redoIt() == MS::kInvalidParameter);
        assert(host::calls["dag.undo"] == 1);
        assert(!redoFailure.redoIt());
    }
    host::reset(); host::fail("node.delete", host::returnedFailure);
    polyBlast snapshotDeletionFailure;
    assert(!snapshotDeletionFailure.doIt(MArgList()));
    assert(host::reports > 0 && host::calls["direct.edit"] == 0);
}

static void checkInputs() {
    host::reset(false, true, true); host::skippedSelection = 3;
    polyBlast polygonSelection;
    assert(polygonSelection.doIt(MArgList()) && host::nodes.at(2).mesh == 50);
    assert(polygonSelection.undoIt()); originalScene();
    for (int skipped : {1, 2}) {
        host::reset(); host::selectionCount = 2; host::skippedSelection = skipped;
        polyBlast command;
        assert(command.doIt(MArgList()));
        assert(host::calls["selection.next"] == 2 && host::nodes.at(2).mesh == 50);
        for (host::Failure failure : failures) {
            host::reset(); host::selectionCount = 2; host::skippedSelection = skipped;
            host::fail("selection.next", failure);
            polyBlast rejected;
            assert(!rejected.doIt(MArgList())); originalScene();
        }
    }
    host::reset(); host::stalledSelection = true; host::skippedSelection = 1;
    polyBlast stalled;
    assert(!stalled.doIt(MArgList()));
    assert(host::calls["selection.next"] == 1 && host::duplicateCount == 0);
    host::reset(); host::reportedTweakCount = 10000001;
    polyBlast huge;
    assert(huge.doIt(MArgList()) == MS::kInvalidParameter && host::duplicateCount == 0);
    host::reset(false, true, true); host::childCount = 2;
    polyBlast malformed;
    assert(!malformed.doIt(MArgList())); originalScene();
    assert(host::aliveDuplicates() == 0 && host::calls["disconnect.queue"] == 0);
    host::reset(); host::nodes[2].tweaks[7].x = std::numeric_limits<float>::quiet_NaN();
    polyBlast nonfinite;
    assert(nonfinite.doIt(MArgList()) == MS::kInvalidParameter && host::duplicateCount == 0);
}

int main() {
    for (bool history : {false, true}) for (bool recording : {false, true}) for (bool tweaks : {false, true}) {
        checkConstruction(history, recording, tweaks);
        checkUndoRedo(history, recording, tweaks);
    }
    checkCleanupFailures();
    checkInputs();
    std::puts("Complete polyBlast/base modifier implementations: snapshots, cleanup, rollback, retries, result publication, and bounded iteration passed.");
}
