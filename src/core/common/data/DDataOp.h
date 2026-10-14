//
//  DDataOp.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/08.
//

#pragma once


#include <iostream>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <functional>

#include "DColorOperation.h"
#include "common/utility/DParallelWork.h"
#include "common/utility/DViewportAttributeConversion.h"


#include <tbb/parallel_for.h>
#include <time.h>


inline size_t DGetGlobalProcessNum(const size_t &count) {
    return dommy::parallel::grainSize(
        count, static_cast<size_t>(std::thread::hardware_concurrency()));
}


template <typename T>
bool isContainDData(const std::vector<T>& array, const T& val) {
    if(std::find(array.begin(), array.end(), val) == array.end()) {
        //not containd yet
        return false;
    } else {
        return true;
    }
}

template <typename T>
void checkMinMaxRange(T &min, T &max) {
    if(min>=0.0 && max<0.0) {
        T tmp = max;
        max = min;
        min = tmp;
    }
}

template <typename T>
void checkMinMax(T &min, T &max) {
    if(min>max) {
        T tmp = max;
        max = min;
        min = tmp;
    }
}


#pragma mark vector convert method


/*Scalar Maya array to std::vector ||  MIntArray->std::vector<float> etc..*/
template <typename S, typename T>
void DScalarMayaArrayToVectorList(const S &srclist, std::vector<T> &targetlist)
{
    targetlist.resize(srclist.length());
    
    for (size_t i=0,N=srclist.length(); i<N; i++) {
        targetlist[i]= static_cast<T>(srclist[i]);
    }
}


/*std::vector Array toVector Maya array ||  MPointArray or MVectorArray -> std::vector<Vec3d> etc..*/
template <typename S, typename T>
void DMayaVecArrayToVecVectorList(const S &srclist, std::vector<T> &targetlist)
{
    
    size_t resultCount = srclist.length();
    size_t m = DGetGlobalProcessNum(resultCount);
    
    targetlist.resize(resultCount);
    
    tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, resultCount, m);
    tbb::parallel_for(range, [=, &targetlist](const tbb::blocked_range<size_t>& r){
        for (size_t i = r.begin(); i != r.end(); i++) {
            for (int j=0; j<3; j++) {
                targetlist[i][j] = srclist[i][j];
            }
        }
    });
    
//    for (size_t i=0,N=srclist.length(); i<N; i++) {
//        for (int j=0; j<3; j++) {
//            targetlist[i][j] = srclist[i][j];
//        }
//    }
}


#pragma mark For attribute


template <typename T>
void SingleToColorFloat(const std::vector<T> &srcList, const bool &isBlackTrans, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::ScalarColor, isBlackTrans);
    dommy::viewport::appendAttributeValues(values, candidate);
}

template <typename T>
void SingleToVectorFloat(const std::vector<T> &srcList, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::ScalarVector);
    values.swap(candidate);
}


template <typename T>
void VectorToVectorFloat(const std::vector<T> &srcList, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Vector3);
    values.swap(candidate);
}

template <typename T>
void TwoVectorToValueFloat(const std::vector<T> &srcList, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Vector2);
    values.swap(candidate);
}

template <typename T>
void TwoVectorTo3DVectorFloat(const std::vector<T> &srcList, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Vector2XZ);
    values.swap(candidate);
}

template <typename T>
void TwoVectorToColorFloat(const std::vector<T> &srcList, const bool &isBlackTrans, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Vector2Color, isBlackTrans);
    values.swap(candidate);
}


template <typename T>
void VectorToColorFloat(const std::vector<T> &srcList, const bool &isBlackTrans, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Vector3Color, isBlackTrans);
    values.swap(candidate);
}

template <typename T>
void ValueToValueFloat(const std::vector<T> &srcList, std::vector<float> &values)
{
    auto candidate = dommy::viewport::makeAttributeValues(
        srcList, dommy::viewport::AttributeDisplayMode::Scalar);
    values.swap(candidate);
}
