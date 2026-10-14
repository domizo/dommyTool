//
//  DOpenVDBAttributeTransferUtil.h
//  dommyTool
//
//  Created by yuzo on 12/6/16.
//

#pragma once

#include <string>

/// Describes one DP attribute exported as an OpenVDB grid.
///
/// The source attribute name is intentionally kept separate from the output
/// grid name: renamed grids must still read values from the original DP
/// attribute.
class TransferAttributeData
{
public:
    TransferAttributeData(
        const std::string& attributeName,
        const std::string& gridName,
        int vectorType)
        : mAttributeName(attributeName)
        , mGridName(gridName)
        , mVectorType(vectorType)
    {
    }

    const std::string& attributeName() const { return mAttributeName; }
    const std::string& gridName() const { return mGridName; }
    int vectorType() const { return mVectorType; }

private:
    std::string mAttributeName;
    std::string mGridName;
    int mVectorType;
};
