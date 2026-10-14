#include "basePolyModifierCmd.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"
#include <maya/MFloatVector.h>
#include <maya/MDGModifier.h>
#include <maya/MDagModifier.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlugArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace {
struct StatusFailure { MStatus status; };
void require(const MStatus& status) { if (!status) throw StatusFailure{status}; }
void invariant(bool value) { if (!value) throw StatusFailure{MS::kFailure}; }
template<class Action> MStatus invoke(Action&& action) noexcept {
    try { action(); return MS::kSuccess; }
    catch (const StatusFailure& failure) { return failure.status; }
    catch (...) { return dommy::maya::reportCommandException("polyBlast", std::current_exception()); }
}
struct Compensation {
    MStatus result = MS::kSuccess;
    template<class Action> void run(Action&& action) noexcept {
        const MStatus status = invoke(std::forward<Action>(action));
        if (result && !status) result = status;
    }
};
struct TemporaryShape {
    MObject root;
    MDGModifier link;
    bool linkAttempted = false, released = false;
    ~TemporaryShape() noexcept { if (!released) (void)close(); }
    MStatus close() noexcept {
        if (released) return MS::kSuccess;
        released = true;
        Compensation recovery;
        if (linkAttempted) recovery.run([&]() { require(link.undoIt()); });
        if (!root.isNull()) recovery.run([&]() {
            const MObjectHandle handle(root);
            if (handle.isValid() && handle.isAlive()) require(MGlobal::deleteNode(root));
        });
        if (!recovery.result) {
            try { MGlobal::displayError("polyBlast: temporary mesh cleanup failed."); } catch (...) {}
        }
        return recovery.result;
    }
};
struct Offset { unsigned int vertex; MFloatVector value; };
MObject vectorData(const MFloatVector& value) {
    MStatus status;
    MFnNumericData fn;
    MObject data = fn.create(MFnNumericData::k3Float, &status);
    require(status); invariant(!data.isNull());
    require(fn.setData(value.x, value.y, value.z));
    return data;
}
std::vector<Offset> readOffsets(MObject shape) {
    MStatus status;
    MFnDependencyNode fn(shape, &status); require(status);
    const MPlug array = fn.findPlug("pnts", true, &status); require(status);
    invariant(array.isArray());
    const unsigned int count = array.numElements(&status); require(status);
    if (count > dommy::generation::kMaxGeneratedPointCount) throw StatusFailure{MS::kInvalidParameter};
    std::vector<Offset> result;
    result.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        const MPlug element = array.elementByPhysicalIndex(i, &status); require(status);
        Offset item;
        item.vertex = element.logicalIndex(&status); require(status);
        MObject data; require(element.getValue(data)); invariant(!data.isNull());
        MFnNumericData numeric(data, &status); require(status);
        require(numeric.getData(item.value.x, item.value.y, item.value.z));
        if (!std::isfinite(item.value.x) || !std::isfinite(item.value.y) || !std::isfinite(item.value.z))
            throw StatusFailure{MS::kInvalidParameter};
        result.push_back(item);
    }
    return result;
}
void writeOffsets(MObject shape, const std::vector<Offset>& offsets) {
    MStatus status;
    MFnDependencyNode fn(shape, &status); require(status);
    const MPlug array = fn.findPlug("pnts", true, &status); require(status);
    invariant(array.isArray());
    for (const Offset& item : offsets) {
        MPlug element = array.elementByLogicalIndex(item.vertex, &status); require(status);
        const MObject data = vectorData(item.value);
        require(element.setValue(data));
    }
}
MPlug temporaryOutput(MObject shape, TemporaryShape& temporary) {
    MStatus status;
    MFnDagNode fn(shape, &status); require(status);
    temporary.root = fn.duplicate(false, false, &status);
    require(status); invariant(!temporary.root.isNull());
    MDagPath path; require(MDagPath::getAPathTo(temporary.root, path)); require(path.extendToShape());
    MFnDependencyNode copy(path.node(), &status); require(status);
    MPlug output = copy.findPlug("outMesh", true, &status); require(status);
    return output;
}
MObject snapshotGeometry(MObject shape) {
    TemporaryShape temporary;
    MObject result;
    const MStatus status = invoke([&]() {
        const MPlug plug = temporaryOutput(shape, temporary);
        MObject evaluated; require(plug.getValue(evaluated)); invariant(!evaluated.isNull());
        MStatus query;
        MFnMeshData data;
        result = data.create(&query); require(query); invariant(!result.isNull());
        MFnMesh mesh;
        const MObject copied = mesh.copy(evaluated, result, &query);
        require(query); invariant(!copied.isNull());
    });
    require(temporary.close()); require(status);
    return result;
}
void restoreGeometry(MObject shape, const MObject& geometry,
                     const std::vector<Offset>& offsets, bool hasOffsets) {
    invariant(!geometry.isNull());
    TemporaryShape temporary;
    const MStatus status = invoke([&]() {
        MStatus query;
        MFnDependencyNode fn(shape, &query); require(query);
        if (!hasOffsets) {
            MPlug output = fn.findPlug("outMesh", true, &query); require(query);
            require(output.setValue(geometry));
        } else {
            const MPlug input = fn.findPlug("inMesh", true, &query); require(query);
            MPlug output = temporaryOutput(shape, temporary);
            require(output.setValue(geometry));
            require(temporary.link.connect(output, input));
            temporary.linkAttempted = true; require(temporary.link.doIt());
            MObject evaluated; require(input.getValue(evaluated));
        }
    });
    require(temporary.close()); require(status);
    writeOffsets(shape, offsets);
}
} // namespace

struct basePolyModifierCmd::State {
    enum class Phase { ready, applied, undone, invalid };
    enum class Mode { direct, existingHistory, newHistory };
    Phase phase = Phase::ready;
    Mode mode = Mode::direct;
    bool started = false, meshSet = false, typeSet = false, nameSet = false;
    bool hasOffsets = false, graphAttempted = false, dagAttempted = false, directAttempted = false;
    MDagPath path;
    MTypeId type;
    MString name;
    MObject originalGeometry;
    std::vector<Offset> offsets;
    MDGModifier graph;
    MDagModifier dag;
    MObjectHandle duplicateRoot, duplicateShape;

    void restoreOriginal() { restoreGeometry(path.node(), originalGeometry, offsets, hasOffsets); }
    MStatus compensateInitial() noexcept {
        Compensation recovery;
        if (directAttempted) recovery.run([&]() { restoreOriginal(); });
        else {
            if (graphAttempted) recovery.run([&]() { require(graph.undoIt()); });
            if (dagAttempted) {
                recovery.run([&]() { restoreOriginal(); });
                recovery.run([&]() { require(dag.undoIt()); });
            }
            if (mode == Mode::existingHistory && graphAttempted)
                recovery.run([&]() { writeOffsets(path.node(), offsets); });
            for (const MObjectHandle& handle : {duplicateRoot, duplicateShape}) recovery.run([&]() {
                if (handle.isValid() && handle.isAlive()) {
                    MObject object = handle.object(); require(MGlobal::deleteNode(object));
                }
            });
        }
        phase = recovery.result ? Phase::ready : Phase::invalid;
        return recovery.result;
    }
    void prepare() {
        require(path.extendToShape());
        if (!path.isValid() || path.apiType() != MFn::kMesh) throw StatusFailure{MS::kInvalidParameter};
        MStatus status;
        MFnDependencyNode node(path.node(), &status); require(status);
        const MPlug input = node.findPlug("inMesh", true, &status); require(status);
        const bool history = input.isConnected(&status); require(status);
        offsets = readOffsets(path.node());
        for (const Offset& item : offsets)
            hasOffsets = hasOffsets || item.value.x != 0 || item.value.y != 0 || item.value.z != 0;
        int recording = 0;
        require(MGlobal::executeCommand("constructionHistory -q -tgl", recording));
        mode = history ? Mode::existingHistory : (recording ? Mode::newHistory : Mode::direct);
        if (!history) originalGeometry = snapshotGeometry(path.node());
    }
    MPlug upstream(const MPlug& destination) {
        MStatus status;
        if (mode == Mode::existingHistory) {
            MPlugArray inputs;
            destination.connectedTo(inputs, true, false, &status); require(status);
            invariant(inputs.length() == 1U);
            require(graph.disconnect(inputs[0], destination));
            return inputs[0];
        }
        MFnDagNode target(path.node(), &status); require(status);
        const unsigned int parents = target.parentCount(&status); require(status); invariant(parents != 0);
        const MObject parent = target.parent(0, &status); require(status);
        TemporaryShape lease;
        lease.root = target.duplicate(false, false, &status);
        duplicateRoot = MObjectHandle(lease.root);
        lease.released = true; // The command now owns the handle, including failed API returns.
        require(status); invariant(!lease.root.isNull());
        require(target.setObject(lease.root));
        const unsigned int children = target.childCount(&status); require(status); invariant(children != 0);
        const MObject shape = target.child(0, &status);
        duplicateShape = MObjectHandle(shape); require(status);
        require(dag.reparentNode(shape, parent));
        dagAttempted = true; require(dag.doIt());
        require(target.setObject(shape)); require(target.setIntermediateObject(true));
        const MPlug source = target.findPlug("outMesh", true, &status); require(status);
        // Reparenting is applied before transform deletion is queued.
        require(dag.deleteNode(lease.root)); require(dag.doIt());
        return source;
    }
    MPlug migrateOffsets(MPlug source) {
        if (!hasOffsets) return source;
        struct Channel { MPlug plug, input; std::vector<MPlug> outputs; };
        struct Vertex { MPlug plug; std::array<Channel, 3> channels; };
        std::vector<Vertex> wiring;
        wiring.reserve(offsets.size());
        MStatus status;
        MFnDependencyNode target(path.node(), &status); require(status);
        const MPlug array = target.findPlug("pnts", true, &status); require(status);
        std::size_t edges = 0U;
        // Stage complete sparse vertex values and wiring before queuing edits.
        for (const Offset& offset : offsets) {
            Vertex vertex;
            vertex.plug = array.elementByLogicalIndex(offset.vertex, &status); require(status);
            const unsigned int width = vertex.plug.numChildren(&status); require(status);
            invariant(vertex.plug.isCompound() && width == 3U);
            for (unsigned int axis = 0; axis != 3; ++axis) {
                Channel& channel = vertex.channels[axis];
                channel.plug = vertex.plug.child(axis, &status); require(status);
                MPlugArray outgoing, incoming;
                channel.plug.connectedTo(outgoing, false, true, &status); require(status);
                channel.plug.connectedTo(incoming, true, false, &status); require(status);
                if (incoming.length() > 1U || !dommy::generation::checkedSum(
                        edges, outgoing.length() + static_cast<std::size_t>(incoming.length()),
                        dommy::generation::kMaxWorkItemCount, edges)) throw StatusFailure{MS::kInvalidParameter};
                for (unsigned int i = 0; i < outgoing.length(); ++i) channel.outputs.push_back(outgoing[i]);
                if (incoming.length()) channel.input = incoming[0];
            }
            wiring.push_back(std::move(vertex));
        }
        const MObject tweakNode = graph.createNode("polyTweak", &status);
        require(status); invariant(!tweakNode.isNull());
        MFnDependencyNode tweak(tweakNode, &status); require(status);
        const MPlug values = tweak.findPlug("tweak", true, &status); require(status);
        const MPlug input = tweak.findPlug("inputPolymesh", true, &status); require(status);
        const MPlug output = tweak.findPlug("output", true, &status); require(status);
        for (std::size_t i = 0; i < offsets.size(); ++i) {
            MPlug value = values.elementByLogicalIndex(offsets[i].vertex, &status); require(status);
            MObject data = vectorData(offsets[i].value); require(value.setValue(data));
            const unsigned int width = value.numChildren(&status); require(status);
            invariant(value.isCompound() && width == 3U);
            for (unsigned int axis = 0; axis < 3; ++axis) {
                const MPlug moved = value.child(axis, &status); require(status);
                const Channel& channel = wiring[i].channels[axis];
                for (const MPlug& destination : channel.outputs) {
                    require(graph.disconnect(channel.plug, destination));
                    require(graph.connect(moved, destination));
                }
                if (!channel.input.isNull()) {
                    require(graph.disconnect(channel.input, channel.plug));
                    require(graph.connect(channel.input, moved));
                }
            }
            MFloatVector zero;
            data = vectorData(zero);
            require(graph.newPlugValue(wiring[i].plug, data));
        }
        require(graph.connect(source, input));
        return output;
    }
    void buildGraph(basePolyModifierCmd& owner) {
        MStatus status;
        graphAttempted = true;
        const MObject node = typeSet ? graph.createNode(type, &status) : graph.createNode(name, &status);
        require(status); invariant(!node.isNull()); require(owner.initModifierNode(node));
        MFnDependencyNode modifier(node, &status); require(status);
        MFnDependencyNode target(path.node(), &status); require(status);
        const MPlug input = modifier.findPlug("inMesh", true, &status); require(status); invariant(!input.isNull());
        const MPlug output = modifier.findPlug("outMesh", true, &status); require(status); invariant(!output.isNull());
        const MPlug destination = target.findPlug("inMesh", true, &status); require(status);
        const MPlug source = migrateOffsets(upstream(destination));
        require(graph.connect(source, input)); require(graph.connect(output, destination)); require(graph.doIt());
    }
};

basePolyModifierCmd::basePolyModifierCmd() : edit_(new State()) {}
basePolyModifierCmd::~basePolyModifierCmd() = default;
void basePolyModifierCmd::setMeshNode(MDagPath path) { edit_->path = path; edit_->meshSet = true; }
MDagPath basePolyModifierCmd::getMeshNode() const { return edit_->path; }
void basePolyModifierCmd::setModifierNodeType(MTypeId type) { edit_->type = type; edit_->typeSet = true; }
void basePolyModifierCmd::setModifierNodeName(MString name) { edit_->name = name; edit_->nameSet = true; }
MTypeId basePolyModifierCmd::getModifierNodeType() const { return edit_->type; }
MString basePolyModifierCmd::getModifierNodeName() const { return edit_->name; }
MStatus basePolyModifierCmd::initModifierNode(MObject) { return MS::kSuccess; }
MStatus basePolyModifierCmd::directModifier(MObject) { return MS::kSuccess; }
bool basePolyModifierCmd::canStartPolyEdit() const { return !edit_->started; }
bool basePolyModifierCmd::isPolyEditApplied() const { return edit_->phase == State::Phase::applied; }
MStatus basePolyModifierCmd::doModifyPoly() {
    State& s = *edit_;
    if (s.started || !s.meshSet || (!s.typeSet && !s.nameSet)) return MS::kInvalidParameter;
    s.started = true;
    const MStatus status = dommy::maya::runCommandTransaction("polyBlast", [&]() {
        return invoke([&]() {
            s.prepare();
            if (s.mode != State::Mode::direct) s.buildGraph(*this);
            else { s.directAttempted = true; require(directModifier(s.path.node())); }
        });
    }, [&]() { return s.compensateInitial(); });
    if (status) s.phase = State::Phase::applied;
    return status;
}
MStatus basePolyModifierCmd::cancelModifyPoly() {
    return isPolyEditApplied() ? edit_->compensateInitial() : MStatus(MS::kFailure);
}
MStatus basePolyModifierCmd::redoModifyPoly() {
    State& s = *edit_;
    if (s.phase == State::Phase::applied) return MS::kSuccess;
    if (s.phase != State::Phase::undone) return MS::kFailure;
    bool graph = false, dag = false, direct = false;
    const MStatus status = dommy::maya::runCommandTransaction("polyBlast redo", [&]() {
        return invoke([&]() {
            if (s.mode == State::Mode::direct) { direct = true; require(directModifier(s.path.node())); }
            else {
                if (s.mode == State::Mode::newHistory) { dag = true; require(s.dag.doIt()); }
                graph = true; require(s.graph.doIt());
            }
        });
    }, [&]() {
        Compensation recovery;
        if (direct) recovery.run([&]() { s.restoreOriginal(); });
        if (graph) recovery.run([&]() { require(s.graph.undoIt()); });
        if (dag) {
            recovery.run([&]() { s.restoreOriginal(); });
            recovery.run([&]() { require(s.dag.undoIt()); });
        }
        if (graph && s.mode == State::Mode::existingHistory)
            recovery.run([&]() { writeOffsets(s.path.node(), s.offsets); });
        if (!recovery.result) s.phase = State::Phase::invalid;
        return recovery.result;
    });
    if (status) s.phase = State::Phase::applied;
    return status;
}
MStatus basePolyModifierCmd::undoModifyPoly() {
    State& s = *edit_;
    if (s.phase == State::Phase::undone) return MS::kSuccess;
    if (s.phase != State::Phase::applied) return MS::kFailure;
    std::vector<Offset> appliedOffsets;
    MObject appliedGeometry;
    MStatus status = invoke([&]() {
        appliedOffsets = readOffsets(s.path.node());
        if (s.mode != State::Mode::existingHistory) appliedGeometry = snapshotGeometry(s.path.node());
    });
    if (!status) return status;
    bool graph = false, dag = false, geometry = false, offsets = false;
    status = dommy::maya::runCommandTransaction("polyBlast undo", [&]() {
        return invoke([&]() {
            if (s.mode == State::Mode::direct) { geometry = true; s.restoreOriginal(); }
            else {
                graph = true; require(s.graph.undoIt());
                if (s.mode == State::Mode::newHistory) {
                    geometry = true; s.restoreOriginal();
                    dag = true; require(s.dag.undoIt());
                }
                offsets = true; writeOffsets(s.path.node(), s.offsets);
            }
        });
    }, [&]() {
        Compensation recovery;
        if (geometry) recovery.run([&]() { restoreGeometry(s.path.node(), appliedGeometry, appliedOffsets, s.hasOffsets); });
        else if (offsets) recovery.run([&]() { writeOffsets(s.path.node(), appliedOffsets); });
        if (dag) recovery.run([&]() { require(s.dag.doIt()); });
        if (graph) recovery.run([&]() { require(s.graph.doIt()); });
        if (!recovery.result) s.phase = State::Phase::invalid;
        return recovery.result;
    });
    if (status) s.phase = State::Phase::undone;
    return status;
}
