#include "export_input_doubles.h"
#include "export_input_methods.inc"

void resetExportInput() {
    static AtNode oldA,oldB;
    assert(liveHandles==0);
    operations.clear();failStep=-1;failureKind=errors=reports=destroyed=0;
    reachedPayload=false;connected=validUuid=validData=true;nullPlug=false;
    points.count=1;existingLights={&oldA,&oldB};
}

int main() {
    DPLightTranslatorAi translator;AtNode parent;
    resetExportInput();translator.Update(&parent);
    assert(reachedPayload && existingLights.size()==2 && destroyed==0 && liveHandles==0);
    const auto trace=operations;
    for(std::size_t i=0;i<trace.size();++i) for(int kind:{1,2,3}) {
        if(kind==1 && (trace[i]=="plug.find" || trace[i]=="plug.null" ||
                       trace[i]=="arnold.matrix" || trace[i]=="arnold.visibility"))continue;
        resetExportInput();failStep=int(i);failureKind=kind;
        translator.Update(&parent);
        assert(!reachedPayload && existingLights.size()==2 && destroyed==0 && liveHandles==0);
        resetExportInput();translator.Update(&parent);
        assert(reachedPayload && existingLights.size()==2 && destroyed==0 && liveHandles==0);
    }
    const auto query=std::find(trace.begin(),trace.end(),"plug.connected");
    assert(query!=trace.end());
    for(bool connectionValue:{false,true}) {
        resetExportInput();connected=connectionValue;failStep=int(query-trace.begin());failureKind=1;
        translator.Update(&parent);
        assert(destroyed==0 && existingLights.size()==2 && !reachedPayload && errors==1);
    }
    resetExportInput();nullPlug=true;translator.Update(&parent);
    assert(destroyed==0 && existingLights.size()==2 && errors==1);
    resetExportInput();validUuid=false;translator.Update(&parent);
    assert(destroyed==0 && existingLights.size()==2 && errors==1);
    resetExportInput();validData=false;translator.Update(&parent);
    assert(destroyed==0 && existingLights.size()==2 && errors==1 && liveHandles==0);
    resetExportInput();connected=false;translator.Update(&parent);
    assert(destroyed==2 && existingLights.empty() && !reachedPayload && errors==0);
    resetExportInput();points.count=0;translator.Update(&parent);
    assert(destroyed==2 && existingLights.empty() && !reachedPayload && errors==0 && liveHandles==0);
}
