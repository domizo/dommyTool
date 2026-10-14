// Frozen pre-edit smoothing kernel. Maya/container boundaries are doubled;
// hash-grid traversal, float arithmetic and sequential updates are unchanged.
MStatus legacySmoothReference(DPData& output, float radius, float maxEffectRadius,
                              float expandBound, std::size_t iterationCount) {
    DPData* outputDPData = &output;
    const std::size_t ptCount = output.numberOfPoints(), scalarCount = 3U * ptCount;
                DPointList newPtArray;
                newPtArray.resize(ptCount);
                const DPointList *ptsPtr = outputDPData->getPositionPtrConst();
				std::vector<DReal> hPos(scalarCount);
                size_t pIndex = 0;
                size_t ptOffsetIdx = 0;
                while (pIndex<ptCount) {
                    const float *pptr = &(*ptsPtr)[pIndex][0];
                    for (int j=0; j<3; j++) {
						if (!std::isfinite(pptr[j])) {
							MGlobal::displayError("DPSmooth input contains non-finite positions.");
							return MS::kFailure;
						}
						hPos[ptOffsetIdx++] = newPtArray[pIndex][j] = pptr[j];
                    }
                    pIndex++;
                }
                if (iterationCount > 0U) {
					DHashGrid nnGrid(3);
                    DReal expansion = expandBound;
                    MBoundingBox bbox;
                    outputDPData->getBBox(bbox);
                    MPoint minPoint = bbox.min();
                    MPoint maxPoint = bbox.max();
                    DTools::Vec3 m_v3EnvMin = DTools::Vec3(minPoint.x-expansion,minPoint.y-expansion,minPoint.z-expansion);
                    DTools::Vec3 m_v3EnvMax = DTools::Vec3(maxPoint.x+expansion,maxPoint.y+expansion,maxPoint.z+expansion);
                    const uint particleCount = static_cast<uint>(ptCount);
                    DReal pointDistance = radius;
                    DReal h = maxEffectRadius;
                    if(h<D_EPS) h=pointDistance;
                    if (!nnGrid.Setup(m_v3EnvMin, m_v3EnvMax, h,
                                      static_cast<int>(particleCount))) {
                        MGlobal::displayError("DPSmooth: unsupported hash-grid bounds or size.");
                        return MS::kInvalidParameter;
                    }
                    for (size_t it=0; it<iterationCount; it++) {
                        if (!nnGrid.SetObjectToCell(hPos.data(), particleCount)) {
                            return MS::kInvalidParameter;
                        }
                        for (uint i=0; i<particleCount; i++) {
                            DTools::Vec3 pos;
							pos[0] = hPos[3*i+0];
							pos[1] = hPos[3*i+1];
							pos[2] = hPos[3*i+2];
                            std::vector<DNeigh> ne;
                            if (!nnGrid.GetNN(
                                pos, hPos.data(), particleCount, ne,
                                pointDistance)) return MS::kInvalidParameter;
                            if (ne.size()<2) {
                                continue;
                            }
                            DVec3Data vel(0.0,0.0,0.0);
                            uint numNeighbours = 0;
                            for(auto itr=ne.begin(), eit=ne.end(); itr!=eit; ++itr){
                                int j = itr->Idx;
                                if(j < 0) continue;
                                if(j == static_cast<int>(i)) continue;
                                DReal r = (newPtArray[i]-newPtArray[j]).length();
                                if (!std::isfinite(r) || r <= D_EPS || r > pointDistance) {
                                    continue;
                                }
                                DVec3Data targetVec = newPtArray[i]-newPtArray[j];
                                targetVec.normalize();
                                vel += targetVec*(1.0-r/pointDistance);
                                numNeighbours++;
                            }
                            if (numNeighbours>0) {
                                newPtArray[i] += vel/(DReal)numNeighbours;
                            }
                        }
                        if ((it+1)<iterationCount) {
                            size_t ptOffsetIdx = 0;
                            for (size_t i=0,N=newPtArray.size(); i<N; i++) {
                                const float *pptr = &newPtArray[i].x;
                                for(int j=0; j<3; j++) {
									hPos[ptOffsetIdx++] = pptr[j];
                                }
                            }
                        }
                    }
				}

    return output.replacePositions(newPtArray) ? MS::kSuccess : MS::kInvalidParameter;
}
