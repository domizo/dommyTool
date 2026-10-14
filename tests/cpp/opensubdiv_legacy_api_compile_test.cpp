#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/stencilTable.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>


void compileOpenSubdivLegacyApiProbe()
{
    typedef OpenSubdiv::Far::TopologyDescriptor Descriptor;

    Descriptor descriptor;
    descriptor.numVertices = 0;
    descriptor.numFaces = 0;
    descriptor.numVertsPerFace = 0;
    descriptor.vertIndicesPerFace = 0;

    OpenSubdiv::Far::TopologyRefinerFactory<Descriptor>::Options options(
        OpenSubdiv::Sdc::SCHEME_CATMARK,
        OpenSubdiv::Sdc::Options());

    OpenSubdiv::Far::TopologyRefiner* refiner =
        OpenSubdiv::Far::TopologyRefinerFactory<Descriptor>::Create(descriptor, options);
    delete refiner;
}
