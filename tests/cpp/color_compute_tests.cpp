#include "color_compute_doubles.h"
#include "color_compute.inc"

namespace {
enum class Kind { Point, Curve, Mesh };
int evaluations=0, failures=0, shortCases=0;
MDataBlock block;

template<class T>void field(DPAttrList& fields,const std::string& name,const std::vector<T>& values) {
    fields[name]=std::make_shared<TypedDPAttributeData<T>>(values);
}
void reset(int mode=dommy::color::kSolid,int assignment=dommy::color::kPoint,bool uv=true) {
    resetCompute();shortArrayAt=-1;badArrayAccesses=samples=0;
    nonfiniteSample=shortSample=false;expressionValid=true;block=MDataBlock();
    inputPoints=DPData();inputPoints.points={{0,0,0},{1,2,3}};
    if(uv)field(inputPoints.attributes,"uv",std::vector<Imath::V2f>{{0,0},{1,1}});
    field(inputPoints.attributes,"Cd",DPointList{{9,9,9},{9,9,9}});
    inputCurves=DCurveData();inputMeshes=DMeshData();
    for(unsigned i=0;i<2;++i) {
        auto primitive=std::make_shared<Primitive>();
        primitive->points=inputPoints.points;primitive->matrix[3][0]=i*4;
        for(const auto& attr:inputPoints.attributes)primitive->attributes[attr.first]=attr.second->copy();
        inputCurves.primitives.push_back(primitive);
        inputMeshes.primitives.push_back(std::make_shared<Primitive>(*primitive));
    }
    block.inputs["inDP"].payload=&inputPoints;block.inputs["inDP"].type=DPData::id;
    block.inputs["inDC"].payload=&inputCurves;block.inputs["inDC"].type=DCurveData::id;
    block.inputs["inDM"].payload=&inputMeshes;block.inputs["inDM"].type=DMeshData::id;
    block.inputs["aColorType"].number=mode;block.inputs["aAssigneType"].number=assignment;
    block.inputs["aColorName"].text="Cd";block.inputs["aUVAttrName"].text="uv";
    block.inputs["aSolidColor"].vector=MVector(.25,.5,.75);
    block.inputs["aMinRange"].vector=MVector(0,0,0);block.inputs["aMaxRange"].vector=MVector(1,1,1);
    block.inputs["aSeed"].number=3;block.inputs["aExpression"].text="P";
    block.inputs["aIs3DCalc"].number=1;
    block.elements.resize(2);block.elements[0].vector=MVector(1,0,0);block.elements[1].vector=MVector(0,1,0);
    outputPoints=DPData();outputPoints.points={{99,99,99}};
    outputCurves=DCurveData();outputMeshes=DMeshData();
}
MStatus evaluate(Kind kind,bool supported=true) {
    ++evaluations;
    MStatus status;
    if(kind==Kind::Point)status=DPColor().compute(MPlug(supported?DPColor::outDP:MObject{"other"}),block);
    else if(kind==Kind::Curve)status=DCColor().compute(MPlug(supported?DCColor::outDC:MObject{"other"}),block);
    else status=DMColor().compute(MPlug(supported?DMColor::outDM:MObject{"other"}),block);
    assert(badArrayAccesses==0);
    return status;
}
void unpublished() {
    assert(publications==0);
    assert(outputPoints.points.size()==1&&outputPoints.points[0][0]==99);
    assert(outputCurves.primitives.empty()&&outputMeshes.primitives.empty());
    assert(std::find(operations.begin(),operations.end(),"output.clean")==operations.end());
    assert(std::find(operations.begin(),operations.end(),"handle.clean")==operations.end());
}
const DPAttrList& pointFields(Kind kind) {
    if(kind==Kind::Point)return outputPoints.attributes;
    return kind==Kind::Curve?outputCurves.primitives.at(0)->attributes:outputMeshes.primitives.at(0)->attributes;
}
void goldens() {
    for(auto kind:{Kind::Point,Kind::Curve,Kind::Mesh}) {
        for(int mode=0;mode<=6;++mode) {
            reset(mode);
            const auto status=evaluate(kind);
            if(kind==Kind::Curve&&mode==dommy::color::kExpression) {
                assert(status==MS::kInvalidParameter);unpublished();continue;
            }
            assert(status==MS::kSuccess&&publications==1);
            const auto attr=pointFields(kind).at("Cd");
            std::vector<Imath::V3d> colors;
            assert(dommy::attribute::convertVector3Attribute(attr,2,colors));
            // Mesh point-assigned solid color historically stores doubles.
            assert(attr->typeName()==(kind==Kind::Mesh&&mode==dommy::color::kSolid
                ?DPVec3dAttr::staticTypeName():DPVec3fAttr::staticTypeName()));
            if(mode==dommy::color::kSolid)assert(colors[0][0]==.25f&&colors[1][2]==.75f);
            if(mode==dommy::color::kTexture||mode==dommy::color::kExpression)
                assert(colors[1][0]==1&&colors[1][1]==2&&colors[1][2]==3);
            if(mode==dommy::color::kPalette)assert(colors[0][0]==1&&colors[1][1]==1);
            const auto old=std::dynamic_pointer_cast<DPVec3fAttr>(inputPoints.attributes.at("Cd"));
            assert(old->value()[0][0]==9); // Copy/staging must not modify input.
        }
        reset();assert(evaluate(kind,false)==MS::kUnknownParameter&&callCount==0);unpublished();
        reset();
        inputPoints.points.clear();inputPoints.attributes.clear();
        inputCurves.primitives.clear();inputMeshes.primitives.clear();
        assert(evaluate(kind)==MS::kSuccess&&publications==1);
        reset(dommy::color::kTexture);block.inputs["aIs3DCalc"].number=0;
        assert(evaluate(kind)==MS::kSuccess&&samples>0);
        reset(dommy::color::kTexture);shortSample=true;
        assert(evaluate(kind)==MS::kFailure);unpublished();
        reset(dommy::color::kTexture);nonfiniteSample=true;
        assert(evaluate(kind)==MS::kInvalidParameter);unpublished();
    }
    for(auto kind:{Kind::Curve,Kind::Mesh})for(int mode:{0,1,6}) {
        reset(mode,dommy::color::kPrimitive);assert(evaluate(kind)==MS::kSuccess);
        const auto& fields=kind==Kind::Curve?outputCurves.attributes:outputMeshes.attributes;
        assert(fields.at("Cd")->attrSize()==2);
    }
    reset(dommy::color::kTexture,dommy::color::kPoint,false);
    assert(evaluate(Kind::Point)==MS::kSuccess&&samples==1);
}
void admissionFailures() {
    for(auto kind:{Kind::Point,Kind::Curve,Kind::Mesh}) {
        reset();assert(evaluate(kind)==MS::kSuccess);const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at) {
            if(trace[at]!=(kind==Kind::Point?"input inDP":kind==Kind::Curve?"input inDC":"input inDM")&&
               trace[at]!="input.bind"&&trace[at]!="factory.create"&&trace[at]!="factory.bind")continue;
            for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                reset();failAt=at;injectedStatus=code;
                assert(evaluate(kind)==code);unpublished();assert(callCount==static_cast<int>(at+1));
                reset();assert(evaluate(kind)==MS::kSuccess);++failures;
            }
        }
        for(int invalid=0;invalid<4;++invalid) {
            reset();auto& input=block.inputs[kind==Kind::Point?"inDP":kind==Kind::Curve?"inDC":"inDM"];
            if(invalid==0)input.type=0;
            if(invalid==1)input.payload=nullptr;
            if(invalid==2){inputPoints.topology=false;inputCurves.topology=false;inputMeshes.topology=false;}
            if(invalid==3)input.payload=kind==Kind::Point?static_cast<Payload*>(&inputCurves):static_cast<Payload*>(&inputPoints);
            assert(evaluate(kind)==(invalid==2?MS::kInvalidParameter:MS::kFailure));unpublished();
        }
        reset();nullCreated=true;assert(evaluate(kind)==MS::kFailure);unpublished();
        reset();rejectFinal=true;assert(evaluate(kind)==MS::kInvalidParameter);unpublished();
        for(int mode:{-1,7}) {
            reset(mode);assert(evaluate(kind)==MS::kInvalidParameter);unpublished();
        }
        if(kind!=Kind::Point) {
            for(int assignment:{-1,2}) {
                reset(dommy::color::kSolid,assignment);
                assert(evaluate(kind)==MS::kInvalidParameter);unpublished();
            }
            for(int mode:{2,3,4,5}) {
                reset(mode,dommy::color::kPrimitive);
                assert(evaluate(kind)==MS::kInvalidParameter);unpublished();
            }
        }
    }
}
void arrayFailures() {
    for(auto kind:{Kind::Point,Kind::Curve,Kind::Mesh})for(bool uv:{true,false}) {
        if(kind!=Kind::Point&&!uv)continue;
        reset(dommy::color::kTexture,dommy::color::kPoint,uv);
        assert(evaluate(kind)==MS::kSuccess);const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at) {
            if(trace[at].find("array.")!=0)continue;
            reset(dommy::color::kTexture,dommy::color::kPoint,uv);shortArrayAt=at;
            assert(evaluate(kind)==MS::kFailure);unpublished();++shortCases;
            for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                reset(dommy::color::kTexture,dommy::color::kPoint,uv);failAt=at;injectedStatus=code;
                // Legacy UV adapters throw at their API boundary and retain
                // kFailure. Direct coordinate/default builders retain status.
                const auto status=evaluate(kind);
                if(trace[at].find("array.point.")==0||!uv) {
                    assert(status==code&&callCount==static_cast<int>(at+1));
                } else assert(!status);
                const auto priorSamples=std::count(trace.begin(),trace.begin()+at,"shader.sample")+
                    std::count(trace.begin(),trace.begin()+at,"texture.sample");
                assert(samples==priorSamples);unpublished();++failures;
            }
            reset(dommy::color::kTexture,dommy::color::kPoint,uv);assert(evaluate(kind)==MS::kSuccess);
        }
    }
}
void allFailurePaths() {
    for(auto kind:{Kind::Point,Kind::Curve,Kind::Mesh})for(int mode:{0,2,3,6}) {
        reset(mode);assert(evaluate(kind)==MS::kSuccess);const auto trace=operations;
        for(std::size_t at=0;at<trace.size();++at) {
            for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                reset(mode);failAt=at;injectedStatus=code;
                assert(!evaluate(kind));
                if(trace[at]=="output.clean"||trace[at]=="handle.clean")assert(publications==1);
                else unpublished();
                reset(mode);assert(evaluate(kind)==MS::kSuccess);++failures;
            }
            reset(mode);attributeThrowAt=at;
            assert(evaluate(kind)==MS::kFailure);
            if(trace[at]!="output.clean"&&trace[at]!="handle.clean")unpublished();
        }
    }
}
}
int main(int argc,char** argv) {
    const std::string section=argc>1?argv[1]:"all";
    if(section=="all"||section=="goldens")goldens();
    if(section=="all"||section=="admission")admissionFailures();
    if(section=="all"||section=="arrays")arrayFailures();
    if(section=="all")allFailurePaths();
    std::cout<<"Color callbacks: "<<evaluations<<" evaluations, "<<failures
             <<" API failure/retry cases, "<<shortCases<<" short-array cases: OK\n";
}
