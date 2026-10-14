#pragma once

// Control-flow doubles, not Maya DG/storage/ownership emulation. In particular,
// native set() failure atomicity and dirty propagation require a real host.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include <cmath>
#include <limits>
#include <map>
#include <memory>

using Weights = std::map<unsigned int, float>;
using Rows = std::map<unsigned int, Weights>;
Rows output;
std::vector<float> input;
unsigned int syntheticInputLength = 0U;
std::vector<bool> statusSites;
std::vector<std::string> cleans;
int publications = 0;

MStatus boundary(const std::string& name, bool hasStatus = true) {
    statusSites.push_back(hasStatus);
    return operation(name);
}

struct MPlug {
    enum Kind { Root, Row, WeightArray, Weight, Unrelated, OtherRow };
    Kind kind;
    unsigned int row, weight;
    bool valid = true;
    bool wrongParent = false, unindexedParent = false;

    explicit MPlug(Kind type, unsigned int r = 0U, unsigned int w = 0U)
        : kind(type), row(r), weight(w) {}
    bool operator==(const MObject& attr) const {
        assert(valid);
        return ((kind == Root || kind == Row) && attr.name == "aWeightsList") ||
               ((kind == WeightArray || kind == Weight) && attr.name == "aWeights");
    }
    bool operator!=(const MObject& attr) const { return !(*this == attr); }
    bool isElement(MStatus* status) const {
        assert(valid);
        *status = boundary("plug.isElement");
        return kind == Row || kind == Weight || kind == OtherRow;
    }
    MPlug array(MStatus* status) const {
        assert(valid && kind == Weight);
        *status = boundary("plug.array");
        MPlug result(*this);
        result.kind = WeightArray;
        result.valid = bool(*status);
        return result;
    }
    MPlug parent(MStatus* status) const {
        assert(valid && kind == WeightArray);
        *status = boundary("plug.parent");
        MPlug result(wrongParent ? OtherRow : (unindexedParent ? Root : Row), row);
        result.valid = bool(*status);
        return result;
    }
    unsigned int logicalIndex(MStatus* status) const {
        assert(valid && (kind == Row || kind == OtherRow));
        *status = boundary("plug.index");
        return row;
    }
};

struct MFloatArray : std::vector<float> {
    unsigned int length() const {
        return syntheticInputLength ? syntheticInputLength : static_cast<unsigned int>(size());
    }
    float operator[](unsigned int index) const { return at(index); }
};

struct MDataBlock;
MStatus getFloatArrayFromData(MDataBlock&, const MObject&, MFloatArray& values) {
    const MStatus status = boundary("input");
    if (status) values.assign(input.begin(), input.end());
    return status;
}

struct MDataHandle {
    Weights* weights = nullptr;
    float* value = nullptr;
    std::shared_ptr<Rows> rowOwner;
    std::shared_ptr<Weights> weightOwner;
    bool valid = false, published = false;

    MDataHandle child(const MObject& attr) const {
        assert(valid && weights && attr.name == "aWeights");
        (void)boundary(published ? "published.child" : "staged.child", false);
        return *this;
    }
    void set(float v) {
        assert(valid && value);
        (void)boundary("value.set", false);
        *value = v;
    }
};

struct MDataBlock;
struct MArrayDataBuilder {
    bool outer = false, valid = false;
    std::shared_ptr<Rows> rows;
    std::shared_ptr<Weights> weights;

    MArrayDataBuilder(MDataBlock*, const MObject& attr, unsigned int count, MStatus* status) {
        outer = attr.name == "aWeightsList";
        assert(outer || attr.name == "aWeights");
        assert(count == (outer ? 1U : input.size()));
        *status = boundary(outer ? "outer.new" : "inner.new");
        valid = bool(*status);
        if (valid) {
            if (outer) rows = std::make_shared<Rows>();
            else weights = std::make_shared<Weights>();
        }
    }
    MDataHandle addElement(unsigned int index, MStatus* status) {
        assert(valid);
        *status = boundary(outer ? "outer.add" : "inner.add");
        MDataHandle handle;
        handle.valid = bool(*status);
        if (*status) {
            if (outer) {
                handle.weights = &(*rows)[index];
                handle.rowOwner = rows;
            } else {
                handle.value = &(*weights)[index];
                handle.weightOwner = weights;
            }
        }
        return handle;
    }
    // Only used by regression mutations of the previous retained-builder path.
    MArrayDataBuilder() = default;
};

struct MArrayDataHandle {
    bool outer, valid, published;
    Weights* weights;
    unsigned int cursor = std::numeric_limits<unsigned int>::max();

    explicit MArrayDataHandle(bool ok) : outer(true), valid(ok), published(true), weights(nullptr) {}
    MArrayDataHandle(const MDataHandle& handle, MStatus* status)
        : outer(false), valid(false), published(handle.published), weights(handle.weights) {
        assert(handle.valid && weights);
        *status = boundary(published ? "published.inner.handle" : "staged.inner.handle");
        valid = bool(*status);
    }
    MStatus set(MArrayDataBuilder& builder) {
        assert(valid && builder.valid && outer == builder.outer);
        const MStatus status = boundary(outer ? "outer.set" : "inner.set");
        if (status) {
            if (outer) {
                output = *builder.rows;
                ++publications;
            } else {
                *weights = *builder.weights;
            }
        }
        return status;
    }
    MStatus jumpToElement(unsigned int index) {
        assert(valid && outer);
        const MStatus status = boundary("output.jump");
        if (status) {
            assert(output.count(index));
            cursor = index;
        }
        return status;
    }
    MDataHandle outputValue(MStatus* status) {
        assert(valid && outer && output.count(cursor));
        *status = boundary("output.row");
        MDataHandle result;
        result.valid = bool(*status);
        result.published = true;
        if (*status) result.weights = &output.at(cursor);
        return result;
    }
    MStatus setAllClean() {
        assert(valid && published && publications == 1);
        const std::string name = outer ? "outer.clean" : "inner.clean";
        const MStatus status = boundary(name);
        if (status) cleans.push_back(name);
        return status;
    }
    MArrayDataBuilder builder(MStatus* status) {
        assert(valid);
        *status = boundary(outer ? "outer.existing" : "inner.existing");
        MArrayDataBuilder result;
        result.outer = outer;
        result.valid = bool(*status);
        if (*status) {
            if (outer) result.rows = std::make_shared<Rows>(output);
            else result.weights = std::make_shared<Weights>(*weights);
        }
        return result;
    }
};

struct MDataBlock {
    MArrayDataHandle outputArrayValue(const MObject& attr, MStatus* status) {
        assert(attr.name == "aWeightsList");
        *status = boundary("output.handle");
        return MArrayDataHandle(bool(*status));
    }
    MStatus setClean(const MPlug&) {
        assert(publications == 1);
        const MStatus status = boundary("block.clean");
        if (status) cleans.push_back("block.clean");
        return status;
    }
};

void resetCompute(int failure = -1) {
    resetCalls(failure);
    syntheticInputLength = 0U;
    statusSites.clear();
    cleans.clear();
    publications = 0;
}
