#include "arnold_export_api.h"
#include "arnold_parameter_doubles.h"
#include <cassert>
#include <type_traits>
struct AtNode {};
struct AtArray {};
bool AiNodeDeclare(AtNode* node,const char* name,const char* declaration) {
    return parameterFixture::declare(node,name,declaration);
}
static_assert(std::is_same<decltype(AiNodeDeclare(nullptr,"", "")),bool>::value,"Arnold Boolean declaration result");
#if DOMMY_PARAMETER_HELPER
#include "integrations/arnold/DArnoldUserParameters.h"
#endif
unsigned valueWrites=0U;
#include "parameter_methods.inc"
template<typename Operation> void rejects(Operation operation) {
    bool failed=false;try {operation();} catch(const std::exception&) {failed=true;}
    assert(failed && valueWrites==0U);
}
void reset() {parameterFixture::reset();valueWrites=0U;}
#if DOMMY_PARAMETER_HELPER
void helperBoundaries() {
    using namespace dommy::arnold;
    AtNode node; AtArray array;
    for(const char* name:{"dInstObjTag","dInstMeshTag","dlightTag"}){
        reset();assert(!hasOwnershipTagSchema(nullptr,name)&&parameterFixture::queries==0U);
        assert(!hasOwnershipTagSchema(&node,name)&&parameterFixture::declarations==0U);
        for(int condition=0;condition<6;++condition){reset();auto spec=parameterFixture::descriptor("constant STRING");
            if(condition==0)spec.type=AI_TYPE_INT;if(condition==1)spec.type=AI_TYPE_ARRAY;
            if(condition==2)spec.category=AI_USERDEF_UNIFORM;if(condition==3)spec.category=AI_USERDEF_VARYING;
            if(condition==5)spec.arrayType=AI_TYPE_FLOAT; // Irrelevant for scalar STRING.
            parameterFixture::users[&node][name]=spec;
            assert(hasOwnershipTagSchema(&node,name)==(condition>=4));assert(parameterFixture::declarations==0U);
            const unsigned queries=parameterFixture::queries;
            for(unsigned index=0;index<queries;++index)for(bool badAlloc:{false,true}){
                parameterFixture::queries=0U;parameterFixture::throwQueryAt=int(index);parameterFixture::queryBadAlloc=badAlloc;
                bool threw=false;try{hasOwnershipTagSchema(&node,name);}catch(const std::exception&){threw=true;}
                assert(threw);parameterFixture::throwQueryAt=-1;
            }
        }
    }
    for(const char* name:{static_cast<const char*>(nullptr),"","custom"}){reset();parameterFixture::users[&node]["custom"]=parameterFixture::descriptor("constant STRING");
        assert(!hasOwnershipTagSchema(&node,name)&&parameterFixture::queries==0U&&parameterFixture::declarations==0U);}
    for (int condition : {0,1,2,3,4,5,6,7,8}) {
        reset();
        if(condition==3)parameterFixture::missingEntry=true;
        if(condition==4)parameterFixture::omitDeclaredEntry=true;
        if(condition==5)parameterFixture::wrongDeclaredType=AI_TYPE_BYTE;
        if(condition==8) {parameterFixture::failDeclaration=true;parameterFixture::failedDeclarationWithEntry=true;}
        rejects([&]{requireUserParameter(condition==0?nullptr:&node,
            condition==1?"":condition==6?"dInstObjTag":"custom",
            condition==2?"not a declaration":condition==7?"constant ARRAY FLOAT":"uniform FLOAT");++valueWrites;});
    }
    reset();rejects([&]{requireOwnershipTag(&node,"custom");++valueWrites;});
    for(const char* name:{"radius","matrix","dInstObjTag"}) {
        reset();rejects([&]{requireInstanceUserParameter(nullptr,name,"constant FLOAT");++valueWrites;});
        assert(parameterFixture::declarations==0U);
    }
    reset();requireInstanceUserParameter(nullptr,"custom","constant FLOAT");
    assert(parameterFixture::declarations==0U && parameterFixture::users.empty());
    for(int condition:{0,1,2,3}) {
        reset();parameterFixture::entry.builtins["velocity_grids"]={condition==0?AI_TYPE_FLOAT:AI_TYPE_ARRAY};
        if(condition==2 || condition==3)parameterFixture::builtinArray=&array;
        if(condition==2)parameterFixture::builtinArrayType=AI_TYPE_FLOAT;
        if(condition==0 || condition==2)rejects([&]{requireVelocityGridsParameter(&node);++valueWrites;});
        else requireVelocityGridsParameter(&node); // Unset or existing STRING array.
        assert(parameterFixture::declarations==0U);
    }
    // Metadata-query exceptions stop before the following value sink, and retry.
    for(auto mode:{0,1,2}) {
        reset();
        if(mode==1)requireUserParameter(&node,"custom","uniform FLOAT");
        if(mode==2)parameterFixture::entry.builtins["velocity_grids"]={AI_TYPE_ARRAY};
        parameterFixture::queries=0U;
        auto operation=[&]{if(mode==2)requireVelocityGridsParameter(&node);
            else requireUserParameter(&node,"custom","uniform FLOAT");};
        operation();const unsigned queries=parameterFixture::queries;
        for(unsigned i=0;i<queries;++i) for(bool allocationFailure:{false,true}) {
            reset();
            if(mode==1)requireUserParameter(&node,"custom","uniform FLOAT");
            if(mode==2)parameterFixture::entry.builtins["velocity_grids"]={AI_TYPE_ARRAY};
            parameterFixture::queries=0U;parameterFixture::throwQueryAt=int(i);
            parameterFixture::queryBadAlloc=allocationFailure;
            rejects([&]{operation();++valueWrites;});
            parameterFixture::throwQueryAt=-1;operation();
        }
    }
}
#endif
int main() {
    AtNode node;
    for (unsigned id=0;id<sizeof(stageNames)/sizeof(*stageNames);++id) {
        reset();parameterFixture::failDeclaration=true;
        rejects([&]{parameterStage(id,&node);});
        reset();parameterStage(id,&node);assert(valueWrites==1U&&parameterFixture::declarations==1U);
        valueWrites=0U;parameterStage(id,&node);assert(valueWrites==1U&&parameterFixture::declarations==1U);
        if (stageCustom[id]) {
            reset();rejects([&]{parameterStage(id,&node,"radius");});assert(parameterFixture::declarations==0U);
        }
        for (int mismatch : {0,1,2}) {
            reset();auto expected=parameterFixture::descriptor(stageDeclarations[id]);
            if (mismatch==0)expected.type=AI_TYPE_BYTE;
            if (mismatch==1)expected.category=expected.category==AI_USERDEF_CONSTANT?AI_USERDEF_UNIFORM:AI_USERDEF_CONSTANT;
            if (mismatch==2) {if(expected.type!=AI_TYPE_ARRAY)continue;expected.arrayType=AI_TYPE_FLOAT;}
            parameterFixture::users[&node][stageNames[id]]=expected;
            rejects([&]{parameterStage(id,&node);});assert(parameterFixture::declarations==0U);
        }
    }
#if DOMMY_PARAMETER_HELPER
    helperBoundaries();
#endif
}
