#include "polyBlastFty.h"

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshSelection.h"

#include <maya/MFnMesh.h>

#include <cstddef>
#include <vector>

MStatus polyBlastFty::doIt()
{
    if (fMesh.isNull()) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnMesh meshFunction(fMesh, &status);
    if (!status) return status;

    const int polygonCount = meshFunction.numPolygons(&status);
    if (!status || polygonCount < 0 ||
        static_cast<std::size_t>(polygonCount) >
            dommy::generation::kMaxGeneratedFaceCount ||
        static_cast<std::size_t>(pSelFaces.length()) >
            dommy::generation::kMaxGeneratedFaceCount) {
        return MS::kInvalidParameter;
    }

    std::vector<int> requestedFaces;
    requestedFaces.reserve(static_cast<std::size_t>(pSelFaces.length()));
    for (unsigned int index = 0U; index < pSelFaces.length(); ++index) {
        requestedFaces.push_back(pSelFaces[index]);
    }

    std::vector<int> normalizedFaces;
    if (!dommy::mesh::normalizeFaceSelection(
            requestedFaces,
            static_cast<std::size_t>(polygonCount),
            dommy::generation::kMaxGeneratedFaceCount,
            normalizedFaces)) {
        return MS::kInvalidParameter;
    }

    for (std::size_t remaining = normalizedFaces.size();
         remaining > 0U; --remaining) {
        status = meshFunction.deleteFace(normalizedFaces[remaining - 1U]);
        if (!status) return status;
    }
    return meshFunction.updateSurface();
}
