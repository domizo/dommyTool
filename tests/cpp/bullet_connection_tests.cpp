#include "bullet_connection_doubles.h"
#include "bullet_connection_methods.inc"
using namespace fixture;
void reset(){calls.clear();updates.clear();connections.clear();failAt=-1;kind=published=cleaned=reports=0;
    for(int attr:{10,11,20,30,40,41,50,51,60,61,70,71})connections[attr]=true;}
int main(){
    DBRigidBodyNode rigid;MDataBlock data;
    for(const MPlug output:{MPlug(12),MPlug(13),MPlug(14),MPlug(15,0)}){
        reset();assert(rigid.compute(output,data));const auto trace=calls;
        for(std::size_t i=0;i<trace.size();++i)for(int failure:{1,2,3}){
            for(bool connected:{false,true}){
                reset();
                if(trace[i].find("connection.")==0)connections[std::stoi(trace[i].substr(11))]=connected;
                failAt=int(i);kind=failure;
                const MStatus status=rigid.compute(output,data);
                assert(status==(failure==1?MS::kInvalidParameter:MS::kFailure));
                assert(updates.empty());
                if(trace[i]!="clean")assert(published==0&&cleaned==0);
            }
            reset();assert(rigid.compute(output,data));
        }
    }
    for(const MPlug unsupported:{MPlug(99),MPlug(15),MPlug(15,1),MPlug(99,0)}){
        reset();assert(rigid.compute(unsupported,data)==MS::kUnknownParameter);
        for(const auto& call:calls)assert(call.find("connection.")!=0);
    }
    reset();connections[10]=false;assert(rigid.compute(MPlug(12),data)==MS::kUnknownParameter);
    for(const auto& call:calls)assert(call!="connection.11");
    reset();assert(DBSolverNode::updateAllRigidBodies());assert(updates.size()==7);const auto trace=calls;
    for(std::size_t i=0;i<trace.size();++i)for(bool connected:{false,true})for(int failure:{1,2,3}){
        reset();for(auto& item:connections)item.second=connected;failAt=int(i);kind=failure;
        bool threw=false;MStatus status;
        try{status=DBSolverNode::updateAllRigidBodies();}catch(...){threw=true;}
        assert(failure==1?status==MS::kInvalidParameter:threw);
        assert(calls.size()==i+1); // no query/update after failed admission
        reset();assert(DBSolverNode::updateAllRigidBodies());assert(updates.size()==7);
    }
    reset();for(auto& item:connections)item.second=false;
    assert(DBSolverNode::updateAllRigidBodies()&&updates.empty());
}
