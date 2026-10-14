// Compile the production builder and batch against real Maya declarations.
// This is a syntax/type boundary, not a linked or running Maya plug-in.
#include "DCurve/Common/DConnectionCurveBuilder.h"

bool compileConnectionCurveBatch(DCurveData& output,
                                 const MPoint& first, const MPoint& second)
{
    DCurveOutputBatch batch;
    return dommy::curve::appendConnectionCurve(batch, first, second, 0, 1) &&
           batch.publishTo(output);
}
