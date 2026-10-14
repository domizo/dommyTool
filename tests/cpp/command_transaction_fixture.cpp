#include "command_transaction_host.h"
#include "command_transaction_methods.inc"

#include <cstdio>

namespace host = command_host;
static const host::Failure failures[] = {host::returnedFailure, host::standardException, host::unknownException};

static void checkShapeFill() {
    host::reset();
    shapeFillCmd command;
    assert(command.doIt(MArgList()));
    assert(host::aliveParticles() == 2 && command.createdParticleShapes.length() == 2);
    assert(command.resultCount == 2);
    assert(command.undoIt() && host::aliveParticles() == 0);
    assert(command.redoIt() && host::aliveParticles() == 2);
    assert(!command.doIt(MArgList()) && host::aliveParticles() == 2);

    for (host::Failure failure : failures) {
        for (const char* operation : {"tracking.append", "particle.emit", "particle.initial", "node.name", "names.append", "selection.next"}) {
            host::reset(); host::fail(operation, failure);
            shapeFillCmd rejected;
            assert(!rejected.doIt(MArgList()));
            assert(host::aliveParticles() == 0);
            if (std::string(operation) == "tracking.append") assert(host::createdCount == 0);
        }
        host::reset(); host::fail("mesh.bounds", failure, 2);
        shapeFillCmd laterMeshFailure;
        assert(!laterMeshFailure.doIt(MArgList()));
        assert(host::createdCount == 1 && host::aliveParticles() == 0);

        host::reset(); host::fail("tracking.append", failure, 2);
        shapeFillCmd laterTrackingFailure;
        assert(!laterTrackingFailure.doIt(MArgList()));
        assert(host::createdCount == 1 && host::aliveParticles() == 0);

        host::reset();
        shapeFillCmd undoFailure;
        assert(undoFailure.doIt(MArgList()));
        host::fail("delete.do", failure);
        assert(!undoFailure.undoIt());
        assert(host::aliveParticles() == 2 && !undoFailure.particlesDeleted);
        assert(undoFailure.undoIt() && host::aliveParticles() == 0);
        host::fail("delete.undo", failure, host::calls["delete.undo"] + 1);
        assert(!undoFailure.redoIt());
        assert(host::aliveParticles() == 0 && undoFailure.particlesDeleted);
        assert(undoFailure.redoIt() && host::aliveParticles() == 2);
    }
    // Maya may return a handle alongside a failure status: it still needs cleanup.
    host::reset(); host::fail("particle.create", host::returnedFailure);
    shapeFillCmd creationFailure;
    assert(!creationFailure.doIt(MArgList()) && host::aliveParticles() == 0);

    host::reset(); host::fail("result", host::standardException);
    shapeFillCmd resultFailure;
    assert(!resultFailure.doIt(MArgList()) && host::aliveParticles() == 0);

    host::reset();
    shapeFillCmd queueRetry;
    assert(queueRetry.doIt(MArgList()));
    host::fail("delete.queue", host::returnedFailure, 2);
    assert(!queueRetry.undoIt() && host::aliveParticles() == 2);
    assert(queueRetry.queuedDeleteCount == 1 && queueRetry.deleteModifier.deletions.size() == 1);
    assert(queueRetry.undoIt() && host::aliveParticles() == 0);
    assert(queueRetry.deleteModifier.deletions.size() == 2);

    host::reset(); host::fail("particle.emit", host::returnedFailure);
    host::fail("delete.do", host::invalidParameter);
    shapeFillCmd rollbackFailure;
    assert(rollbackFailure.doIt(MArgList()) == MS::kInvalidParameter);
    assert(host::reports > 0 && rollbackFailure.createdParticleShapes.length() == 1);
}

template <typename Command>
static void checkNodeCommand(unsigned int resultCount) {
    host::reset();
    Command command;
    assert(command.doIt(MArgList()));
    assert(command.resultCount == resultCount);
    const auto constructionCalls = host::calls;
    assert(host::dagPresent && host::membership && command.hasShadingMembership);
    assert(command.redoIt() && host::calls["dag.do"] == 1);
    assert(command.undoIt() && !host::dagPresent && !host::membership);
    assert(command.redoIt() && host::dagPresent && host::membership);

    for (host::Failure failure : failures) {
        for (const char* operation : {"selection.active", "selection.path", "node.queue", "node.rename",
                                      "dependency.bind", "plug.find", "plug.element", "connection.queue",
                                      "selection.name", "selection.node", "node.name", "names.append"}) {
            const auto found = constructionCalls.find(operation);
            if (found == constructionCalls.end()) continue;
            for (unsigned int occurrence = 1; occurrence <= found->second; ++occurrence) {
                host::reset(); host::fail(operation, failure, occurrence);
                Command rejected;
                assert(!rejected.doIt(MArgList()));
                assert(!host::dagPresent && !host::membership);
            }
        }
        for (const char* operation : {"dag.do", "set.bind", "set.add"}) {
            host::reset(); host::fail(operation, failure);
            Command rejected;
            assert(!rejected.doIt(MArgList()));
            assert(!host::dagPresent && !host::membership && !rejected.hasShadingMembership);
        }
        for (const char* operation : {"set.remove", "dag.undo"}) {
            host::reset();
            Command rejected;
            assert(rejected.doIt(MArgList()));
            host::fail(operation, failure);
            assert(!rejected.undoIt());
            assert(host::dagPresent && host::membership && rejected.hasShadingMembership);
            assert(rejected.undoIt() && !host::dagPresent && !host::membership);
        }
        host::reset(); host::fail("result", failure == host::returnedFailure ? host::standardException : failure);
        Command resultFailure;
        assert(!resultFailure.doIt(MArgList()) && !host::dagPresent && !host::membership);
    }
    host::reset(); host::fail("set.add", host::returnedFailure);
    host::fail("dag.undo", host::invalidParameter);
    Command rollbackFailure;
    assert(rollbackFailure.doIt(MArgList()) == MS::kInvalidParameter);
    assert(host::reports > 0);
}

static void checkWeights() {
    for (host::Failure failure : failures) {
        host::reset(); host::fail("weight.target", failure);
        DClusterWeightCmd rejected;
        assert(!rejected.redoIt() && !rejected.applied);
        assert(host::weights == rejected.originalWeights.values);
        assert(rejected.redoIt() && rejected.applied);
        host::fail("weight.original", failure, host::calls["weight.original"] + 1);
        assert(!rejected.undoIt() && rejected.applied);
        assert(host::weights == rejected.targetWeights.values);
        assert(rejected.undoIt() && !rejected.applied);
    }
    host::reset(); host::fail("weight.target", host::returnedFailure);
    host::fail("weight.original", host::invalidParameter);
    DClusterWeightCmd rollbackFailure;
    assert(rollbackFailure.redoIt() == MS::kInvalidParameter && host::reports > 0);

    host::reset();
    DClusterWeightCmd plan;
    assert(plan.buildWeightPlan(MObject()));
    assert(plan.targetWeights.length() == 2 && plan.targetWeights[0] == 1.0F && plan.targetWeights[1] == 0.5F);
    for (host::Failure failure : failures) {
        host::reset(); host::fail("geometry.next", failure);
        DClusterWeightCmd rejected;
        const MStatus status = dommy::maya::invokeCommandOperation("test", "plan", [&]() { return rejected.buildWeightPlan(MObject()); });
        assert(!status && rejected.targetWeights.length() == 1 && host::calls["geometry.next"] == 1);
    }
    for (bool stalled : {false, true}) {
        host::reset(); host::stalledGeometry = stalled; host::geometryLength = 3;
        DClusterWeightCmd rejected;
        const MStatus status = dommy::maya::invokeCommandOperation("test", "plan", [&]() { return rejected.buildWeightPlan(MObject()); });
        assert(!status && rejected.targetWeights.length() <= 2 && host::calls["geometry.next"] == 2);
    }
    host::reset(); host::geometryLength = 1;
    DClusterWeightCmd shortPlan;
    assert(!shortPlan.buildWeightPlan(MObject()));
    host::reset(); host::geometryCount = 10000001;
    DClusterWeightCmd oversizedPlan;
    assert(!oversizedPlan.buildWeightPlan(MObject()) && host::calls["geometry.next"] == 0);
}

static void prepareColors(bool existing) {
    host::reset(); host::attributes = host::originalAttributes = existing;
}
static void checkColors() {
    for (bool existing : {false, true}) {
        prepareColors(existing);
        setParticleRGBCmd command;
        command.hadRgbAttribute = existing;
        assert(command.redoIt() && command.applied && host::attributes && host::colors == 1);
        assert(command.undoIt() && !command.applied && host::attributes == existing);
        if (existing) assert(host::colors == 0);
        assert(command.redoIt() && command.applied && host::colors == 1);
        for (host::Failure failure : failures) {
            for (const char* operation : {"attribute.do", "color.target"}) {
                prepareColors(existing); host::fail(operation, failure);
                setParticleRGBCmd rejected;
                rejected.hadRgbAttribute = existing;
                assert(!rejected.redoIt() && !rejected.applied && !rejected.attributesApplied);
                assert(host::attributes == existing);
                if (existing) assert(host::colors == 0);
            }
            prepareColors(existing);
            setParticleRGBCmd undoFailure;
            undoFailure.hadRgbAttribute = existing;
            assert(undoFailure.redoIt());
            host::fail("attribute.undo", failure);
            assert(!undoFailure.undoIt() && undoFailure.applied && undoFailure.attributesApplied);
            assert(host::attributes && host::colors == 1);
            assert(undoFailure.undoIt());
        }
    }
    for (host::Failure failure : failures) {
        prepareColors(true);
        setParticleRGBCmd undoFailure;
        undoFailure.hadRgbAttribute = true;
        assert(undoFailure.redoIt());
        host::fail("color.original", failure);
        assert(!undoFailure.undoIt() && undoFailure.applied && host::colors == 1);

        prepareColors(true); host::fail("color.target", host::returnedFailure);
        host::fail("color.original", failure);
        setParticleRGBCmd cleanupFailure;
        cleanupFailure.hadRgbAttribute = true;
        assert(!cleanupFailure.redoIt());
        assert(host::calls["attribute.undo"] == 1 && !cleanupFailure.attributesApplied);
        assert(host::reports > 0);
    }
}

static void checkWoolCurve() {
    for (host::Failure failure : failures) {
        for (const char* operation : {"dag.do", "dg.do"}) {
            host::reset(); host::fail(operation, failure);
            woolCurveCmd rejected;
            assert(!rejected.doIt(MArgList()) && !host::dagPresent && !host::dgPresent);
        }
        for (const char* operation : {"dg.undo", "dag.undo"}) {
            host::reset();
            woolCurveCmd rejected;
            assert(rejected.doIt(MArgList()));
            host::fail(operation, failure);
            assert(!rejected.undoIt() && host::dagPresent && host::dgPresent);
            assert(rejected.undoIt() && !host::dagPresent && !host::dgPresent);
        }
        host::reset(); host::fail("dg.do", host::returnedFailure);
        host::fail("dg.undo", failure);
        woolCurveCmd cleanupFailure;
        assert(!cleanupFailure.redoIt() && host::calls["dag.undo"] == 1);
    }
}

static void checkCompleteWeightCommand() {
    host::reset();
    DClusterWeightCmd healthy;
    assert(healthy.doIt(MArgList())==MS::kSuccess && healthy.planReady && healthy.applied);
    assert(host::weights==std::vector<float>({1.0F,.5F}));
    const auto baseline=host::calls;
    assert(healthy.undoIt()==MS::kSuccess && host::weights==std::vector<float>({0,0}));
    assert(healthy.redoIt()==MS::kSuccess && host::weights==std::vector<float>({1.0F,.5F}));
    for (host::Failure failure : {host::returnedFailure,host::invalidParameter,
                                 host::standardException,host::unknownException}) {
        for (const auto& entry : baseline) {
            for (unsigned int point=1;point<=entry.second;++point) {
                host::reset();host::fail(entry.first.c_str(),failure,point);
                DClusterWeightCmd rejected;
                const MStatus status=rejected.doIt(MArgList());
                assert(!status && !rejected.applied);
                assert(host::weights==std::vector<float>({0,0}));
                if (entry.first=="weight.original.clear" || entry.first=="weight.target.clear") {
                    assert(!rejected.planReady && host::calls["weight.target"]==0);
                    if (point==1) assert(host::calls["parse"]==0);
                    assert(host::calls[entry.first]==point);
                    if (failure==host::invalidParameter) assert(status==MS::kInvalidParameter);
                }
                host::reset();
                assert(rejected.doIt(MArgList())==MS::kSuccess && rejected.applied);
                assert(host::weights==std::vector<float>({1.0F,.5F}));
                assert(rejected.undoIt()==MS::kSuccess && host::weights==std::vector<float>({0,0}));
            }
        }
    }
    host::reset();host::fail("selection.clear",host::invalidParameter);
    DClusterWeightCmd resetFailure;
    assert(resetFailure.doIt(MArgList())==MS::kInvalidParameter);
    assert(host::calls["selection.add"]==1 && host::calls["weight.target"]==0);
}

static void checkCompleteColorCommand() {
    for (bool existing : {false,true}) {
        prepareColors(existing);
        setParticleRGBCmd healthy;
        assert(healthy.doIt(MArgList())==MS::kSuccess && healthy.planReady && healthy.applied);
        assert(healthy.targetColors.length()==2 && healthy.targetColors[0].x==1);
        const auto baseline=host::calls;
        assert(healthy.undoIt()==MS::kSuccess && host::attributes==existing);
        assert(healthy.redoIt()==MS::kSuccess && host::attributes && host::colors==1);
        for (host::Failure failure : {host::returnedFailure,host::invalidParameter,
                                     host::standardException,host::unknownException}) {
            for (const auto& entry : baseline) {
                for (unsigned int point=1;point<=entry.second;++point) {
                    prepareColors(existing);host::fail(entry.first.c_str(),failure,point);
                    setParticleRGBCmd rejected;
                    const MStatus status=rejected.doIt(MArgList());
                    assert(!status && !rejected.applied && host::attributes==existing);
                    if (existing) assert(host::colors==0);
                    if (entry.first=="color.original.clear" || entry.first=="color.target.clear") {
                        assert(!rejected.planReady && host::calls["color.target"]==0);
                        assert(host::calls[entry.first]==point);
                        if (point==1) assert(host::calls["parse"]==0);
                        if (failure==host::invalidParameter) assert(status==MS::kInvalidParameter);
                    }
                    prepareColors(existing);
                    assert(rejected.doIt(MArgList())==MS::kSuccess && rejected.applied);
                    assert(rejected.targetColors.length()==2 && rejected.targetColors[0].x==1);
                    assert(rejected.undoIt()==MS::kSuccess && host::attributes==existing);
                }
            }
        }
    }
    for (bool wrongOriginal : {false,true}) {
        prepareColors(true);
        if (wrongOriginal) host::originalColorCount=1;
        else host::sampledColorCount=1;
        setParticleRGBCmd rejected;
        assert(!rejected.doIt(MArgList()) && !rejected.planReady && !rejected.applied);
        assert(host::calls["color.target"]==0 && host::attributes);
    }
    prepareColors(false);host::particleCount=0;
    setParticleRGBCmd empty;
    assert(empty.doIt(MArgList())==MS::kSuccess && !empty.planReady && !empty.applied);
    assert(host::calls["shading"]==0 && host::calls["attribute.queue"]==0);
    prepareColors(false);
    setParticleRGBCmd camera;
    camera.cameraName="camera";
    assert(camera.doIt(MArgList())==MS::kSuccess && camera.applied);
    assert(host::calls["camera"]==0 && host::calls["selection.add"]==1);
}

int main() {
    checkShapeFill();
    checkNodeCommand<spaceDuplicaterCmd>(1);
    checkNodeCommand<surfaceDuplicaterCmd>(2);
    checkNodeCommand<icosahedronCmd>(1);
    checkWeights(); checkColors(); checkWoolCurve();
    checkCompleteWeightCommand();checkCompleteColorCommand();
    host::reset(); host::fail("report", host::standardException);
    const MStatus status = dommy::maya::runCommandTransaction("test", []() -> MStatus { throw 42; }, []() { return MS::kSuccess; });
    assert(!status);
    std::puts("Seven commands: complete doIt/Undo/Redo transaction bodies; RGB/weight reset, admission, failure recovery and compensation passed (Maya/geometry/shading/queue seams).");
}
