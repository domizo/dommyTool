// DOMMY_THIRD_PARTY_NOTICE_BEGIN
// Software License Agreement (BSD License)
// 
// Copyright 2011-2016 Jose Luis Blanco (joseluisblancoc@gmail.com).
//   All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/jlblancoc/nanoflann/blob/f2efec0110eb68edf8cae90fce394a5044bf3db8/examples/pointcloud_example.cpp
// Adapted portion: the PointCloud dataset adaptor; other routines are project-owned.

//
//  DPointOp.hpp
//  dommyTool
//
//  Created by yuzo on 3/11/17.
//
//

#pragma once

#include <stdio.h>
#include <vector>

#include "DType.h"
#include "DMayaHeaders.h"


#include <thread>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <time.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>

#include "DPData.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "DDataOp.h"
#include "DPointGeneration.h"
#include "common/utility/DBoundedRadiusSearch.h"



#include <nanoflann.hpp>


namespace {
    typedef std::vector<uint32_t> IDs;
    typedef std::vector<IDs> IDsListArray;
    typedef std::vector<double> DScalarList;
    typedef std::vector<DScalarList > DScalarListArray;

#pragma mark kd-tree
	template <typename T>
	struct PointCloud
	{
		struct Point
		{
			T  x,y,z;
		};
		
		std::vector<Point>  pts;
        
        inline void addPoint(const MPoint p)
        {
            Point pt;
            pt.x=p.x;
            pt.y=p.y;
            pt.z=p.z;
            pts.push_back(pt);
        }
		
		// Must return the number of data points
		inline size_t kdtree_get_point_count() const { return pts.size(); }
		
		// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
		inline T kdtree_distance(const T *p1, const size_t idx_p2,size_t /*size*/) const
		{
			const T d0=p1[0]-pts[idx_p2].x;
			const T d1=p1[1]-pts[idx_p2].y;
			const T d2=p1[2]-pts[idx_p2].z;
			return d0*d0+d1*d1+d2*d2;
		}
		
		// Returns the dim'th component of the idx'th point in the class:
		// Since this is inlined and the "dim" argument is typically an immediate value, the
		//  "if/else's" are actually solved at compile time.
		inline T kdtree_get_pt(const size_t idx, int dim) const
		{
			if (dim==0) return pts[idx].x;
			else if (dim==1) return pts[idx].y;
			else return pts[idx].z;
		}
		
		// Optional bounding-box computation: return false to default to a standard bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
		//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX>
		bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }
		
	};
	
	
    class DPkdtree : public nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<double, PointCloud<double>>,
        PointCloud<double>, 3> {
        typedef nanoflann::KDTreeSingleIndexAdaptor<
            nanoflann::L2_Simple_Adaptor<double, PointCloud<double>>,
            PointCloud<double>, 3> Base;
    public:
        DPkdtree(int dimensions, const PointCloud<double>& cloud,
                 const nanoflann::KDTreeSingleIndexAdaptorParams& params)
            : Base(dimensions, cloud, params), leafSize_(params.leaf_max_size) {
            if (leafSize_ == 0U ||
                cloud.pts.size() > dommy::generation::kMaxGeneratedPointCount) {
                throw std::invalid_argument("Invalid point search index size");
            }
        }
        std::size_t leafSize() const { return leafSize_; }
        dommy::spatial::RadiusSearchBudget& radiusBudget() const { return budget_; }
    private:
        std::size_t leafSize_;
        mutable dommy::spatial::RadiusSearchBudget budget_;
    };

    inline std::size_t radiusSamplingCapacity(
        const DPkdtree& tree, std::size_t queries, int maximumSampling) {
        if (maximumSampling < -1 ||
            queries > dommy::generation::kMaxGeneratedPointCount) {
            throw std::invalid_argument("Invalid radius sampling count");
        }
        const std::size_t capacity = maximumSampling < 0 ? tree.size() :
            std::min(tree.size(), static_cast<std::size_t>(maximumSampling));
        std::size_t storedNeighbors = 0U;
        if (!dommy::generation::checkedProduct(queries, capacity,
                dommy::generation::kMaxWorkItemCount, storedNeighbors)) {
            throw std::length_error("Radius neighbor storage exceeded its budget");
        }
        return capacity;
    }

    template <typename Point>
    inline std::vector<dommy::spatial::BoundedRadiusResultSet::Match>
    boundedRadiusNeighbors(const DPkdtree& tree, const Point& point,
                           std::size_t capacity, double radius,
                           std::size_t excludedId = std::numeric_limits<std::size_t>::max()) {
        const double query[3] = {point[0], point[1], point[2]};
        if (!std::isfinite(query[0]) || !std::isfinite(query[1]) ||
            !std::isfinite(query[2]) || !std::isfinite(radius) || radius <= 0.0) {
            throw std::invalid_argument("Invalid radius search coordinates or radius");
        }
        dommy::spatial::BoundedRadiusResultSet result(
            radius * radius, capacity, tree.radiusBudget(), tree.leafSize(), excludedId);
        if (capacity != 0U) {
            tree.findNeighbors(result, query, nanoflann::SearchParams(32, 0, false));
        }
        return result.takeSortedMatches();
    }
	
	
    class PointSearchKDTreeRadiusOp
    {
    public:
        PointSearchKDTreeRadiusOp(const DPkdtree& tree, const DPointList& points,
                                 int maximumSampling, double radius,
                                 IDsListArray& ids)
            : tree_(tree), points_(points), radius_(radius), ids_(ids),
              capacity_(radiusSamplingCapacity(tree, points.size(), maximumSampling)) {
            if (ids.size() != points.size()) {
                throw std::invalid_argument("Radius output cardinality mismatch");
            }
        }

        void operator()(const tbb::blocked_range<size_t>& range) const {
            for (std::size_t index = range.begin(); index != range.end(); ++index) {
                const auto matches = boundedRadiusNeighbors(
                    tree_, points_[index], capacity_, radius_, index);
                IDs ids;
                ids.reserve(matches.size());
                for (const auto& match : matches) {
                    if (match.second > std::numeric_limits<uint32_t>::max()) {
                        throw std::overflow_error("Radius result ID exceeds storage");
                    }
                    ids.push_back(static_cast<uint32_t>(match.second));
                }
                std::sort(ids.begin(), ids.end());
                ids_[index].swap(ids);
            }
        }

    private:
        const DPkdtree& tree_;
        const DPointList& points_;
        double radius_;
        IDsListArray& ids_;
        std::size_t capacity_;
    };

    template <typename Points>
    class PointSearchKDTreeRadiusWithDistance
    {
    public:
        PointSearchKDTreeRadiusWithDistance(
            const DPkdtree& tree, const Points& points,
            int maximumSampling, double radius,
            IDsListArray& ids, DScalarListArray& distances)
            : tree_(tree), points_(points), radius_(radius),
              ids_(ids), distances_(distances),
              capacity_(radiusSamplingCapacity(tree, points.size(), maximumSampling)) {
            if (ids.size() != points.size() || distances.size() != points.size()) {
                throw std::invalid_argument("Radius output cardinality mismatch");
            }
        }

        void operator()(const tbb::blocked_range<size_t>& range) const {
            for (std::size_t index = range.begin(); index != range.end(); ++index) {
                const auto matches = boundedRadiusNeighbors(
                    tree_, points_[index], capacity_, radius_);
                IDs ids;
                DScalarList distances;
                ids.reserve(matches.size());
                distances.reserve(matches.size());
                for (const auto& match : matches) {
                    if (match.second > std::numeric_limits<uint32_t>::max()) {
                        throw std::overflow_error("Radius result ID exceeds storage");
                    }
                    ids.push_back(static_cast<uint32_t>(match.second));
                    distances.push_back(std::sqrt(match.first));
                }
                ids_[index].swap(ids);
                distances_[index].swap(distances);
            }
        }

    private:
        const DPkdtree& tree_;
        const Points& points_;
        double radius_;
        IDsListArray& ids_;
        DScalarListArray& distances_;
        std::size_t capacity_;
    };

    using WorldPointList = std::vector<Imath::V3d>;
    using PointSearchKDTreeRadiusWithDistanceOp = PointSearchKDTreeRadiusWithDistance<DPointList>;
    using WorldPointSearchKDTreeRadiusWithDistanceOp = PointSearchKDTreeRadiusWithDistance<WorldPointList>;

    class PointSearchKDTreeKNNOp
    {
    public:
        PointSearchKDTreeKNNOp(const DPkdtree &kdtree, const DPointList &dpoint, int maxSampling, double searchRadius, IDsListArray &idListArray)
        : m_kdtree(&kdtree),m_dpoints(&dpoint)
        , mMaxSampling(maxSampling)
        , mSearchRadius(searchRadius)
        , mIdListArray(&idListArray)
        
        
        { }
        
        void operator()(const tbb::blocked_range<size_t>& r) const
        {
            if (mMaxSampling <= 0) return;

            const size_t num_results =
                static_cast<size_t>(mMaxSampling);
            nanoflann::SearchParams params(10);
            
            for (size_t i = r.begin(); i != r.end(); i++)
            {
                double query_pt[3] = { (*m_dpoints)[i][0], (*m_dpoints)[i][1], (*m_dpoints)[i][2]};
                
                
                std::vector<size_t> ret_indices(num_results);
                std::vector<double> out_distances_sqr(num_results);
                nanoflann::KNNResultSet<double> resultSet(num_results);
                resultSet.init(
                    ret_indices.data(), out_distances_sqr.data());
                m_kdtree->findNeighbors(resultSet, &query_pt[0], params);

                IDs indices;
                const size_t resultCount = resultSet.size();
                indices.reserve(resultCount);
                const double maximumSquaredDistance =
                    mSearchRadius > 0.0
                        ? mSearchRadius * mSearchRadius
                        : std::numeric_limits<double>::infinity();
                for (size_t result = 0U;
                     result < resultCount; ++result) {
                    if (ret_indices[result] >
                            static_cast<size_t>(
                                std::numeric_limits<uint32_t>::max()) ||
                        !std::isfinite(out_distances_sqr[result]) ||
                        out_distances_sqr[result] < 0.0 ||
                        out_distances_sqr[result] >
                            maximumSquaredDistance) {
                        continue;
                    }
                    indices.push_back(
                        static_cast<uint32_t>(ret_indices[result]));
                }
                (*mIdListArray)[i].swap(indices);
            }
            
        }
        
    private:
        
        const DPkdtree *m_kdtree;
        const DPointList *m_dpoints;
        int mMaxSampling;
        double mSearchRadius;
        IDsListArray *mIdListArray;
        
    };//PointSearchKDTreeKNNOp

    class PointSearchKDTreeNearestWithDistanceOp
    {
    public:
        PointSearchKDTreeNearestWithDistanceOp(
            const DPkdtree& kdtree,
            const DPointList& points,
            IDs& nearestIds,
            DScalarList& nearestDistances)
            : mKdtree(&kdtree),
              mPoints(&points),
              mNearestIds(&nearestIds),
              mNearestDistances(&nearestDistances) {}

        void operator()(const tbb::blocked_range<size_t>& range) const
        {
            nanoflann::SearchParams params(10);
            for (size_t index = range.begin();
                 index != range.end(); ++index) {
                const double query[3] = {
                    (*mPoints)[index][0],
                    (*mPoints)[index][1],
                    (*mPoints)[index][2]
                };
                size_t nearestId = std::numeric_limits<size_t>::max();
                double squaredDistance =
                    std::numeric_limits<double>::infinity();
                nanoflann::KNNResultSet<double> resultSet(1U);
                resultSet.init(&nearestId, &squaredDistance);
                mKdtree->findNeighbors(resultSet, query, params);

                if (nearestId >
                        static_cast<size_t>(
                            std::numeric_limits<uint32_t>::max()) ||
                    !std::isfinite(squaredDistance) ||
                    squaredDistance < 0.0) {
                    (*mNearestIds)[index] =
                        std::numeric_limits<uint32_t>::max();
                    (*mNearestDistances)[index] =
                        std::numeric_limits<double>::infinity();
                } else {
                    (*mNearestIds)[index] =
                        static_cast<uint32_t>(nearestId);
                    (*mNearestDistances)[index] =
                        std::sqrt(squaredDistance);
                }
            }
        }

    private:
        const DPkdtree* mKdtree;
        const DPointList* mPoints;
        IDs* mNearestIds;
        DScalarList* mNearestDistances;
    };
    
    
	
    class DPointSearchKDTreeRadiusGen
    {
    public:
        DPointSearchKDTreeRadiusGen(const DPointList &dpoint, int maxSampling, double searchRadius, IDsListArray &idListArray)
        : m_dpoints(&dpoint)
        , mMaxSampling(maxSampling)
        , mSearchRadius(searchRadius)
        , mIdListArray(&idListArray)
        {
            
        }
        
        
        void operator()()
        {
            
            //radius search
            PointCloud<double> cloud;
            
            cloud.pts.resize(m_dpoints->size());
            
            size_t i=0;
            for (auto pIt=m_dpoints->begin();pIt!=m_dpoints->end();++pIt)
            {
                cloud.pts[i].x = (*pIt)[0];
                cloud.pts[i].y = (*pIt)[1];
                cloud.pts[i].z = (*pIt)[2];
                i++;
            }
            
            // construct a kd-tree index:
            DPkdtree index(3 /*dim*/, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10 /* max leaf */) );
            index.buildIndex();
            
            PointSearchKDTreeRadiusOp kdOp(index, *m_dpoints, mMaxSampling, mSearchRadius, *mIdListArray);
            
            tbb::blocked_range<size_t> kdrange = tbb::blocked_range<size_t>(0, m_dpoints->size(), 10000);
            tbb::parallel_for( kdrange, kdOp );
                        
        }
        
        const DPointList *m_dpoints;
        int mMaxSampling;
        double mSearchRadius;
        IDsListArray *mIdListArray;
        
    }; //DPointSearchKDTreeRadiusGen
    
	
	
	
}//utility

#pragma mark create search data

inline void CreateSearchDataMFP(const MFloatPointArray &pts, PointCloud<double> &cloudPts) {
    
    size_t resultCount = pts.length();
    
    cloudPts.pts.resize(resultCount);
    size_t m = DGetGlobalProcessNum(resultCount);
    
    tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, resultCount, m);
    tbb::parallel_for(range, [=, &cloudPts](const tbb::blocked_range<size_t>& r){
        for (size_t i = r.begin(); i != r.end(); i++) {
            
            cloudPts.pts[i].x = pts[i][0];
            cloudPts.pts[i].y = pts[i][1];
            cloudPts.pts[i].z = pts[i][2];
            
        }
    });
}

inline void CreateSearchDataMP(const MPointArray &pts, PointCloud<double> &cloudPts) {
    
    size_t resultCount = pts.length();
    
    cloudPts.pts.resize(resultCount);
    size_t m = DGetGlobalProcessNum(resultCount);
    
    tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, resultCount, m);
    tbb::parallel_for(range, [=, &cloudPts](const tbb::blocked_range<size_t>& r){
        for (size_t i = r.begin(); i != r.end(); i++) {
            
            cloudPts.pts[i].x = pts[i][0];
            cloudPts.pts[i].y = pts[i][1];
            cloudPts.pts[i].z = pts[i][2];
            
        }
    });
}

template <typename T>
inline void CreateSearchDataPoint(const DPointList &pts, PointCloud<T> &cloudPts) {
    
    size_t resultCount = pts.size();
    
    cloudPts.pts.resize(resultCount);
    size_t m = DGetGlobalProcessNum(resultCount);
    
    
    tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, resultCount, m);
    tbb::parallel_for(range, [=, &cloudPts](const tbb::blocked_range<size_t>& r){
        for (size_t i = r.begin(); i != r.end(); i++) {
            
            cloudPts.pts[i].x = pts[i][0];
            cloudPts.pts[i].y = pts[i][1];
            cloudPts.pts[i].z = pts[i][2];
            
        }
    });
}

inline void CreateSearchDataDC_PT(const DCurveData &dcurve, PointCloud<double> &cloudPts, DPIndexList &itemIdList, DPIndexList &vtxIdList) {
    
    size_t cvCoount = dcurve.getTotalCvs();
    cloudPts.pts.resize(cvCoount);
    itemIdList.resize(cvCoount);
    vtxIdList.resize(cvCoount);
    
    size_t primindex=0;
    size_t totalIndex=0;
    for(auto cit=dcurve.beginPrims(),ecit=dcurve.endPrims(); cit!=ecit; ++cit){
        
        
        size_t cvindex=0;
        for (auto it=(*cit)->beginPt(),eIt=(*cit)->endPt(); it!=eIt; ++it) {
            cloudPts.pts[totalIndex].x = (*it)[0];
            cloudPts.pts[totalIndex].y = (*it)[1];
            cloudPts.pts[totalIndex].z = (*it)[2];
            
            itemIdList[totalIndex] = primindex;
            vtxIdList[totalIndex] = cvindex;
            cvindex++;
            totalIndex++;
        }
        
        primindex++;
    }
}

inline void CreateSearchDataDC_PRIM(const DCurveData &dcurve, PointCloud<double> &cloudPts) {
    
    MStatus stat;
    cloudPts.pts.resize(dcurve.numberOfCurves());
    
    size_t primIndex=0;
    for(auto cit=dcurve.beginPrims(); cit!=dcurve.endPrims(); ++cit){
        
        //read curve
        MFnNurbsCurve crvFn;
        MObject crvObj = MFnNurbsCurveData().create();
        
        MFnNurbsCurve::Form isOpen = (*cit)->isCurveOpen()?MFnNurbsCurve::Form::kOpen:MFnNurbsCurve::Form::kClosed;
        crvFn.create((*cit)->getCvs(), (*cit)->getKnots(), (*cit)->getDegree(), isOpen, false, false, crvObj, &stat);
        CHECK_MSTATUS(stat);
        
        double length = crvFn.length();
        double halfLen = length*.5;
        
        double param = crvFn.findParamFromLength(halfLen);
        MPoint pt;
        crvFn.getPointAtParam(param, pt);
        
        cloudPts.pts[primIndex].x = pt[0];
        cloudPts.pts[primIndex].y = pt[1];
        cloudPts.pts[primIndex].z = pt[2];
        
        primIndex++;
    }
    
}

inline void CreateSearchDataDM_PT(const DMeshData &dmesh, PointCloud<double> &cloudPts, DPIndexList &itemIdList, DPIndexList &vtxIdList) {
    
    
    size_t vrxCoount;
    dmesh.getTotalVertex(vrxCoount);
    cloudPts.pts.resize(vrxCoount);
    itemIdList.resize(vrxCoount);
    vtxIdList.resize(vrxCoount);
    
    size_t primindex=0;
    size_t totalIndex=0;
    for(auto cit=dmesh.beginPrims(); cit!=dmesh.endPrims(); ++cit){
        
        size_t cvindex=0;
        for (auto it=(*cit)->beginPt(); it!=(*cit)->endPt(); ++it) {
            cloudPts.pts[totalIndex].x = (*it)[0];
            cloudPts.pts[totalIndex].y = (*it)[1];
            cloudPts.pts[totalIndex].z = (*it)[2];
            
            itemIdList[totalIndex] = primindex;
            vtxIdList[totalIndex] = cvindex;
            cvindex++;
            totalIndex++;
        }
        
        primindex++;
    }
    
}

inline void CreateSearchDataDM_PRIM(const DMeshData &dmesh, PointCloud<double> &cloudPts) {
    
    cloudPts.pts.resize(dmesh.numberOfMesh());
    
    size_t primIndex=0;
    for(auto mIt=dmesh.beginPrims(); mIt!=dmesh.endPrims(); ++mIt){
        
        DPoint center = (*mIt)->getCenter();
        
        cloudPts.pts[primIndex].x = center[0];
        cloudPts.pts[primIndex].y = center[1];
        cloudPts.pts[primIndex].z = center[2];
        
        primIndex++;
    }
    
}



DPointList CreateConvex2DList(const DPointList &pts);

