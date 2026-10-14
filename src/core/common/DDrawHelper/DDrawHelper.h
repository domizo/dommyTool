//
//  DDrawHelper.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/24.
//

#pragma once


#include "DType.h"
#include "DMayaHeaders.h"
#include "DDataOp.h"

#include <tbb/tick_count.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/combinable.h>



#pragma mark BBox

class BBoxConvertFloatOp
{
public:
    
    BBoxConvertFloatOp(const MPointArray &pts
                   , const MIntArray &connectArray
                   , std::vector<float> &floatlist
                   , std::vector<unsigned int> &idxlist
                   , const DMatrixArray &matrixArray)
    : m_pts(&pts)
    , m_connectArray(&connectArray)
    , m_floatlist(&floatlist)
    , m_indexlist(&idxlist)
    , m_matlist(&matrixArray)
    { }
    
    void operator()(const tbb::blocked_range<size_t>& r) const
    {
        size_t totalFloat = m_pts->length()*3;
        size_t totalIndex = m_connectArray->length();
        size_t pointCount = m_pts->length();
        for (size_t i = r.begin(); i != r.end(); i++) {
            
            size_t totalVIndex=i*totalFloat;
            
            size_t vIndex = 0;
            for (size_t j=0; j<pointCount; ++j) {
                MPoint pt = (*m_pts)[j]*(*m_matlist)[i];
                
                (*m_floatlist)[totalVIndex+vIndex] = pt[0];vIndex++;
                (*m_floatlist)[totalVIndex+vIndex] = pt[1];vIndex++;
                (*m_floatlist)[totalVIndex+vIndex] = pt[2];vIndex++;
                
            }
            
            //index
            size_t offsetIndex=i*totalIndex;
            size_t totalPIndex=i*pointCount;
            size_t iIndex = 0;
            for (size_t c=0; c<totalIndex; ++c) {
                (*m_indexlist)[offsetIndex+iIndex] = (*m_connectArray)[c]+totalPIndex;
                iIndex++;
            }
            
        }
    }
    
private:
    const MPointArray *m_pts;
    const MIntArray *m_connectArray;
    std::vector<float> *m_floatlist;
    std::vector<unsigned int> *m_indexlist;
    const DMatrixArray *m_matlist;
};//ConvertFloatOp

class BBoxConvGen
{
public:
    BBoxConvGen(const MPointArray &pts, const MIntArray &connectArray, std::vector<float> &floatlist, std::vector<unsigned int> &idxlist,
                const DMatrixArray &matrixArray)
    : m_pts(&pts)
    , m_connectArray(&connectArray)
    , m_floatlist(&floatlist)
    , m_indexlist(&idxlist)
    , m_matlist(&matrixArray)
    {}
    
    void operator()()
    {
        BBoxConvertFloatOp op(*m_pts, *m_connectArray, *m_floatlist, *m_indexlist,*m_matlist);
        size_t count = m_matlist->size();
        size_t m = DGetGlobalProcessNum(count);
        
        tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, count, m);
        tbb::parallel_for( range, op );
    }
    
    const MPointArray *m_pts;
    const MIntArray *m_connectArray;
    std::vector<float> *m_floatlist;
    std::vector<unsigned int> *m_indexlist;
    const DMatrixArray *m_matlist;
}; //BBoxConvGen










