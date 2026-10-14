// Frozen Poisson branch: positive-distance compatibility only.
// Run with the same C++ standard library; random distributions are not portable.
MStatus referencePoisson(size_t countVal, DPointList& newPtArray) try {EmitterBlock data; MStatus stat; const MObject aPoissonMinDistance{"distance"}; const unsigned seedVal=1; const float sizeXVal=10,sizeYVal=10,offsetX=5,offsetY=5; switch(0){case 0:{

                const float mindist = data.inputValue(aPoissonMinDistance, &stat).asFloat();
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                if (!std::isfinite(mindist) || mindist <= 0.0f) {
                    MGlobal::displayError("DPointPlaneEmit: Poisson distance must be positive.");
                    return MS::kFailure;
                }

                PoissonGenerator::DefaultPRNG PRNG(seedVal);
                const auto Points = PoissonGenerator::GeneratePoissonPoints(
                    countVal, PRNG, countVal, false, mindist);

                newPtArray.resize(Points.size());
                size_t index = 0;
                for (const auto& p : Points) {
                    newPtArray[index][0] = p.x * sizeXVal - offsetX;
                    newPtArray[index][1] = 0.0;
                    newPtArray[index][2] = p.y * sizeYVal - offsetY;

                    index++;
                }

            }break;}return MS::kSuccess;} DOMMY_COMPUTE_CATCH("baseline")
