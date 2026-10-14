#pragma once
// Maya APIs and production attribute/color kernels are compiled natively.
// Custom payload storage/transport and SeExpr are declaration-only seams;
// TBB uses a serial compile adapter, not its native backend.
// Imath uses existing test declarations. No native rendering/link is implied.
#include "procedural_api_sdk.h"
#include "Debug/DMayaDebug.h"
#include "common/utility/DColorPalette.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DExpressionResult.h"
#include "common/utility/DRandomValueGeneration.h"
#include "math/DRandomGenerator.h"
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MTime.h>
#include <atomic>

using DCItem=std::shared_ptr<ApiPrimitive>;
using DMItem=DCItem;
namespace tbb {
template<class T>class blocked_range {
    T first_,last_;
public:
    blocked_range(T first,T last,std::size_t):first_(first),last_(last){}
    T begin()const{return first_;}
    T end()const{return last_;}
};
template<class Range,class Body>void parallel_for(const Range& range,Body body){body(range);}
}
std::size_t DGetGlobalProcessNum(std::size_t);
class DPointSeExpr {
public:
    explicit DPointSeExpr(const std::string&);
    bool isValid()const;
    bool usesPointDataFunctions()const;
    const char* parseError()const;
    void setDPData(DPData*);
    void setId(std::size_t);
    void setPos(const DPoint&);
    Imath::V3d evaluate();
    bool evaluationSucceeded()const;
};
namespace dommy { namespace maya {
MStatus getEvaluationTime(MDataBlock&,MTime&);
MStatus setExpressionTime(DPointSeExpr&,const MTime&);
}}
MStatus getColorFromTex(const MObject&,const MObject&,const MDoubleArray&,
                       const MDoubleArray&,MVectorArray&,MDoubleArray&);
MStatus getColorFromShaderNetwork(const MObject&,const MObject&,const MFloatPointArray&,
                                 const MDoubleArray&,const MDoubleArray&,
                                 MFloatVectorArray&,MFloatVectorArray&);
