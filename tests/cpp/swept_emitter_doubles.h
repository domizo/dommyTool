#pragma once

#include "particle_output_doubles.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DPointMean.h"
#include "perlin.h"

// Reuse the particle API boundary. Only swept geometry queries are added here;
// no Maya simulation, ownership or swept-geometry construction is emulated.
using TriangleVertices=std::array<MVector,3>;
struct SweptFixture {
    std::vector<TriangleVertices> current={{{MVector(1,2,3),MVector(4,5,6),MVector(7,8,9)}}};
    std::vector<TriangleVertices> previous={{{MVector(-2,-1,0),MVector(1,2,3),MVector(4,5,6)}}};
    std::vector<MVector> lines={MVector(0,0,0),MVector(6,12,18),MVector(12,24,36)};
    bool overrideCount=false;
    int reportedCount=0;
};
static SweptFixture swept;
const MObject mSweptGeometry{"mSweptGeometry"};

struct MDynSweptTriangle {
    int index;
    bool valid;
    MVector vertex(int i,double time=1.0) {
        assert(valid && (time==0 || time==1));
        return (time==1 ? swept.current : swept.previous).at(index).at(i);
    }
};
struct MDynSweptLine {
    int index;
    bool valid;
    MVector vertex(int i,double time=1.0) {
        assert(valid && i==0 && time==1);
        return swept.lines.at(index);
    }
};
struct MFnDynSweptGeometryData {
    bool valid;
    MFnDynSweptGeometryData(const MObject&,MStatus* s) {
        *s=operation("swept.bind"); valid=bool(*s);
    }
    int triangleCount(MStatus* s) {
        assert(valid); *s=operation("swept.triangleCount");
        return swept.overrideCount ? swept.reportedCount : static_cast<int>(swept.current.size());
    }
    int lineCount(MStatus* s) {
        assert(valid); *s=operation("swept.lineCount");
        return swept.overrideCount ? swept.reportedCount : static_cast<int>(swept.lines.size());
    }
    MDynSweptTriangle sweptTriangle(int i,MStatus* s) {
        assert(valid); *s=operation("swept.triangle"); return {i,bool(*s)};
    }
    MDynSweptLine sweptLine(int i,MStatus* s) {
        assert(valid); *s=operation("swept.line"); return {i,bool(*s)};
    }
};
