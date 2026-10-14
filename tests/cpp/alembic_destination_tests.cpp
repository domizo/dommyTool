#include "alembic_destination_host.h"
#include "destination_methods.inc"

void resetDestination(){admitted=0;failAt=-1;kind=0;nullPlug=false;calls.clear();
    testingPaths=false;directoryExists=true;
    scene={{"OtherNode","other",true},{"AlembicNode","cache",true},{"AlembicNode","absent",false}};}
int main(int argc,char** argv){assert(argc==2);resetDestination();MFileObject destination;destination.path=MString(argv[1]);
    assert(destinationAdmission(destination)&&admitted==1);const std::size_t count=calls.size();
    const std::string path=argv[1];{std::ofstream output(path);output<<"existing";}
    for(std::size_t i=0;i<count;++i)for(int fault=1;fault<=3;++fault){resetDestination();failAt=int(i);kind=fault;
        bool failed=false;std::string pending;
        try{dommy::file::PendingOutputFile output(path,dommy::file::PendingOutputFile::Access::TrustedPath);pending=output.trustedTemporaryPath();
            failed=!destinationAdmission(destination);if(!failed)output.commit();}
        catch(const std::exception&){failed=true;}
        assert(failed&&admitted==0&&!dommy::file::isRegularFile(pending));
        std::ifstream input(path);assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing");
        resetDestination();assert(destinationAdmission(destination));}
    resetDestination();scene[1].path=path;assert(!destinationAdmission(destination)&&admitted==0);
    resetDestination();scene[1].path=path;scene[1].exists=false;assert(destinationAdmission(destination));
    resetDestination();nullPlug=true;assert(!destinationAdmission(destination));
    resetDestination();scene.clear();assert(destinationAdmission(destination));
    resetDestination();testingPaths=true;std::string relative="export.abc";
    assert(pathAdmission(relative,MString("/cache"))&&relative=="/cache/export.abc");
    const auto pathCalls=calls.size();assert(pathCalls==4U);
    for(std::size_t i=0;i<pathCalls;++i)for(int fault=1;fault<=3;++fault){
        resetDestination();testingPaths=true;failAt=int(i);kind=fault;relative="export.abc";bool failed=false;std::string pending;
        try{dommy::file::PendingOutputFile owner(path,dommy::file::PendingOutputFile::Access::TrustedPath);pending=owner.trustedTemporaryPath();failed=!pathAdmission(relative,MString("/cache"));
            if(!failed)owner.commit();}catch(const std::exception&){failed=true;}
        assert(failed&&admitted==0&&!dommy::file::isRegularFile(pending));
        std::ifstream input(path);assert(std::string((std::istreambuf_iterator<char>(input)),{})=="existing");
    }
    resetDestination();testingPaths=true;std::string absolute="/absolute/export.abc";
    assert(pathAdmission(absolute,MString("/cache"))&&absolute=="/absolute/export.abc");
    resetDestination();testingPaths=true;directoryExists=false;relative="export.abc";
    assert(pathAdmission(relative,MString("/cache"))&&directoryExists);
}
