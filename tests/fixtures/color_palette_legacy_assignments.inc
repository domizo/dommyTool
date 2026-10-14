// Historical palette assignment loops used as a compatibility reference.
// Only acquisition is replaced by the checked reader. Compile the reference
// with the same standard library: std::uniform_int_distribution is not a
// cross-library sequence contract. Never exercise the legacy unsafe reader.
MStatus baselineDPColor(MDataBlock &data, int assigneType, ColorGeometry &result) {
    (void)assigneType;
    const int paletteType = static_cast<int>(inputs["mode"].number);
    const int seedVal = static_cast<int>(inputs["seed"].number);
    const std::string colAttrName = "Cd";
    ColorGeometry *outputDPData = &result;
    ColorGeometry *outDCrvs = &result;
    ColorGeometry *outDMeshes = &result;
    MColorArray plColArray;
    DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(data, aPaletteColor, plColArray));
    size_t palletCount = plColArray.length();
    const DPointList *ptsPtr = outputDPData->getPositionPtrConst();
    size_t pointCount = ptsPtr->size();
    DColorList newColArray(pointCount, DColor(0, 0, 0));
    switch (paletteType) {
    case 0: {
        for (size_t i = 0; i < pointCount; ++i) {
            const MColor col = plColArray[static_cast<unsigned int>(i % palletCount)];
            for (int j = 0; j < 3; ++j) {
                newColArray[i][j] = col[j];
            }
        }
    } break;
    case 1: {
        std::vector<int> palletIndexList;
        DGenerateRandomValue(seedVal, int(0), int(plColArray.length() - 1), pointCount,
                             palletIndexList);
        for (size_t i = 0; i < pointCount; ++i) {
            const MColor col = plColArray[static_cast<unsigned int>(palletIndexList[i])];
            for (int j = 0; j < 3; ++j) {
                newColArray[i][j] = col[j];
            }
        }
    } break;
    default:
        break;
    }
    if (!dommy::color::publishValues(*outputDPData, colAttrName, newColArray,
                                     outputDPData->numberOfPoints()))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}
MStatus baselineDCColor(MDataBlock &data, int assigneType, ColorGeometry &result) {
    (void)assigneType;
    const int paletteType = static_cast<int>(inputs["mode"].number);
    const int seedVal = static_cast<int>(inputs["seed"].number);
    const std::string colAttrName = "Cd";
    ColorGeometry *outputDPData = &result;
    ColorGeometry *outDCrvs = &result;
    ColorGeometry *outDMeshes = &result;
    MColorArray plColArray;
    DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(data, aPaletteColor, plColArray));
    if (assigneType == dommy::color::kPrimitive) {
        uint curveCount = outDCrvs->numberOfCurves();
        DVec3DataList newColArray(curveCount, DVec3Data(0, 0, 0));
        if (plColArray.length() > 0) {
            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
            uint stepIndex = 0;
            for (uint i = 0; i < curveCount; i++) {
                MColor col;
                if (paletteType == 0) {
                    col = plColArray[stepIndex];
                    if (stepIndex == (plColArray.length() - 1)) {
                        stepIndex = 0;
                    } else {
                        stepIndex++;
                    }
                } else {
                    int type;
                    if (plColArray.length() > 1) {
                        type = static_cast<int>(randomGenerator.index(plColArray.length()));
                        col = plColArray[type];
                    } else if (plColArray.length() == 1) {
                        col = plColArray[0];
                    }
                }
                newColArray[i] = DVec3Data(col[0], col[1], col[2]);
            }
        }
        if (!dommy::color::publishValues(*outDCrvs, colAttrName, newColArray, curveCount))
            return MS::kInvalidParameter;
    } else {
        dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
        for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {
            uint ptCount = (*cIt)->numberOfPoints();
            DVec3DataList newColArray(ptCount, DVec3Data(0, 0, 0));
            if (plColArray.length() > 0) {
                uint stepIndex = 0;
                for (uint i = 0; i < ptCount; i++) {
                    MColor col;
                    if (paletteType == 0) {
                        col = plColArray[stepIndex];
                        if (stepIndex == (plColArray.length() - 1)) {
                            stepIndex = 0;
                        } else {
                            stepIndex++;
                        }
                    } else {
                        int type;
                        if (plColArray.length() > 1) {
                            type = static_cast<int>(randomGenerator.index(plColArray.length()));
                            col = plColArray[type];
                        } else if (plColArray.length() == 1) {
                            col = plColArray[0];
                        }
                    }
                    newColArray[i] = DVec3Data(col[0], col[1], col[2]);
                }
            }
            if (!dommy::color::publishValues(*(*cIt), colAttrName, newColArray, ptCount))
                return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}
MStatus baselineDMColor(MDataBlock &data, int assigneType, ColorGeometry &result) {
    (void)assigneType;
    const int paletteType = static_cast<int>(inputs["mode"].number);
    const int seedVal = static_cast<int>(inputs["seed"].number);
    const std::string colAttrName = "Cd";
    ColorGeometry *outputDPData = &result;
    ColorGeometry *outDCrvs = &result;
    ColorGeometry *outDMeshes = &result;
    MColorArray plColArray;
    DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(data, aPaletteColor, plColArray));
    if (assigneType == dommy::color::kPrimitive) {
        uint curveCount = outDMeshes->numberOfMesh();
        DVec3DataList newColArray(curveCount, DColor(0, 0, 0));
        if (plColArray.length() > 0) {
            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
            uint stepIndex = 0;
            for (uint i = 0; i < curveCount; i++) {
                MColor col;
                if (paletteType == 0) {
                    col = plColArray[stepIndex];
                    if (stepIndex == (plColArray.length() - 1)) {
                        stepIndex = 0;
                    } else {
                        stepIndex++;
                    }
                } else {
                    int type;
                    if (plColArray.length() > 1) {
                        type = static_cast<int>(randomGenerator.index(plColArray.length()));
                        col = plColArray[type];
                    } else if (plColArray.length() == 1) {
                        col = plColArray[0];
                    }
                }
                newColArray[i] = DVec3Data(col[0], col[1], col[2]);
            }
        }
        if (!dommy::color::publishValues(*outDMeshes, colAttrName, newColArray,
                                         outDMeshes->numberOfMesh()))
            return MS::kInvalidParameter;
    } else {
        dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
        for (auto cIt = outDMeshes->beginMeshes(); cIt != outDMeshes->endMeshes(); ++cIt) {
            uint ptCount = (*cIt)->numberOfPoints();
            DVec3DataList newColArray(ptCount, DVec3Data(0, 0, 0));
            if (plColArray.length() > 0) {
                uint stepIndex = 0;
                for (uint i = 0; i < ptCount; i++) {
                    MColor col;
                    if (paletteType == 0) {
                        col = plColArray[stepIndex];
                        if (stepIndex == (plColArray.length() - 1)) {
                            stepIndex = 0;
                        } else {
                            stepIndex++;
                        }
                    } else {
                        int type;
                        if (plColArray.length() > 1) {
                            type = static_cast<int>(randomGenerator.index(plColArray.length()));
                            col = plColArray[type];
                        } else if (plColArray.length() == 1) {
                            col = plColArray[0];
                        }
                    }
                    newColArray[i] = DVec3Data(col[0], col[1], col[2]);
                }
            }
            if (!dommy::color::publishValues(*(*cIt), colAttrName, newColArray,
                                             (*cIt)->numberOfPoints()))
                return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}
