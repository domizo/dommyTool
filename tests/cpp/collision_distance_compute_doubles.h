#pragma once
// Production compute, arithmetic, conversions and budgets; host queries and
// publication boundaries are explicit failure-injectable doubles, not Maya.
#define DOMMY_COLLISION_DISTANCE_TESTS
#include "point_force_compute_doubles.h"
using DIntArray=std::vector<int>;
struct DPointSearchKDTreeRadiusGen {
    const DPointList& points;double radius;IDsListArray& ids;
    DPointSearchKDTreeRadiusGen(const DPointList& p,int maximum,double r,IDsListArray& i)
        :points(p),radius(r),ids(i){assert(maximum==-1);}
    void operator()(){
        voidApi("collision.search");
        for(std::size_t i=0;i<points.size();++i)for(std::size_t j=0;j<points.size();++j){
            dommy::direction::Delta delta;
            assert(dommy::direction::difference(points[i],points[j],delta));
            if(i!=j&&delta.length<radius)ids[i].push_back(badNeighbor?UINT32_MAX:static_cast<uint32_t>(j));
        }
    }
};
struct NativeDistanceOp {
    const DPointList& points;DScalarDataList& values;
    template<typename Target>NativeDistanceOp(const Target&,const DPointList& p,DScalarDataList& v)
        :points(p),values(v){}
    MStatus operator()(const tbb::blocked_range<std::size_t>& range){
        const auto status=operation("native.distance.query");if(!status)return status;
        if(rejectNative)return MS::kFailure;++nativeQueries;
        for(std::size_t i=range.begin();i<range.end();++i)values[i]=std::abs(points[i][0]);
        return MS::kSuccess;
    }
};
using DistanceCurveOp=NativeDistanceOp;
using DistanceSurfaceOp=NativeDistanceOp;
using DistanceMeshOp=NativeDistanceOp;
