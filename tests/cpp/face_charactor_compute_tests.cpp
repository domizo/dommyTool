#include "face_charactor_doubles.h"
#include "face_charactor_compute.inc"

MStatus evaluate(int kind,MDataBlock& block,bool rotate=false) {
    if(kind==0){DPolyFaceInfo node;return node.compute(MPlug(node.thisMObject(),
        rotate?DPolyFaceInfo::outRotate:DPolyFaceInfo::outTranslate),block);}
    DMCharactor node;return node.compute(MPlug(node.thisMObject(),DMCharactor::outDM),block);
}
void unchanged() {
    assert(translationWrites==0 && rotationWrites==0 && published==0 && cleaned==0);
    assert(translation[0]==99 && rotation[0]==99);
    const auto old=std::dynamic_pointer_cast<DPDoubleAttr>(inputData.meshes[0]->getAttr("curvature"));
    assert(old && old->values[0]==9);
}
void fails(int kind,MDataBlock& block) { assert(!evaluate(kind,block));unchanged(); }

void boundaryCases(int kind,int mode) {
    auto block=setup(kind,mode);assert(evaluate(kind,block));const auto baseline=operations;
    for(auto failure:{MS::kFailure,MS::kInvalidParameter}) {
        injectedStatus=failure;
        for(int at=0;at<int(baseline.size());++at) {
            block=setup(kind,mode);failAt=at;
            const auto status=evaluate(kind,block);
            assert(!status && callCount==at+1);
            assert(status==failure);
            if(baseline[at]!="clean")unchanged();
        }
    }
    for(int at=0;at<int(baseline.size());++at) {
        block=setup(kind,mode);attributeThrowAt=at;
        assert(!evaluate(kind,block) && callCount==at+1);
        if(baseline[at]!="clean")unchanged();
        if(baseline[at]=="array.length" || baseline[at]=="mesh.points" || baseline[at]=="mesh.vertices") {
            block=setup(kind,mode);shortAt=at;fails(kind,block);
            assert(callCount==at+1);
        }
    }
    std::cout<<"Face/Charactor compute "<<kind<<"/"<<mode<<": "<<baseline.size()
             <<" failure/exception sites passed\n";
}
void faceCases() {
    auto block=setup(0);assert(evaluate(0,block));
    assert(translationWrites==1 && rotationWrites==1 && translation[0]==0 && translation[2]==1.0/3.0);
    for(unsigned int i=0;i<3;++i)assert(rotation[i]==dommy::math::radiansToDegrees(hostRotation[i]));
    block=setup(0);assert(evaluate(0,block,true));
    assert(translationWrites==1 && rotationWrites==1);
    // Every permutation has the same arithmetic mean despite cancellation.
    MIntArray order{0,1,2};
    do {
        block=setup(0);meshPoints={{1e16,0,0},{1,0,1},{-1e16,0,0}};polygonIds=order;
        assert(evaluate(0,block));assert(translation[0]==1.0/3.0 && translation[2]==1.0/3.0);
    }while(std::next_permutation(order.begin(),order.end()));
    for(int count:{-2,10000001}) {
        block=setup(0);pointCount=count;fails(0,block);
        assert(std::find(operations.begin(),operations.end(),"mesh.points")==operations.end());
        block=setup(0);cornerCount=count;fails(0,block);
        assert(std::find(operations.begin(),operations.end(),"mesh.vertices")==operations.end());
    }
    block=setup(0);pointCount=3000000;fails(0,block);
    assert(std::find(operations.begin(),operations.end(),"mesh.points")==operations.end());
    block=setup(0);polygonIds={0,1,3};fails(0,block);
    block=setup(0);polygonIds={0,1,-1};fails(0,block);
    block=setup(0);cornerCount=2;fails(0,block);
    block=setup(0);block.controls["faceIndex"]=-1;fails(0,block);
    block=setup(0);block.controls["faceIndex"]=1;fails(0,block);
    block=setup(0);faceCount=0;fails(0,block);
    block=setup(0);nullMode=1;fails(0,block);
    block=setup(0);meshPoints[1].x=NAN;fails(0,block);
    block=setup(0);meshPoints.assign(3,MPoint());fails(0,block);
    block=setup(0);badNormal=true;fails(0,block);
    block=setup(0);zeroNormal=true;fails(0,block);
    for(double value:{double(NAN),double(INFINITY),std::numeric_limits<double>::max()}) {
        block=setup(0);hostRotation[0]=value;fails(0,block);
    }
}

void expectCurvature(int iterations,bool color,const DDoubleArray& expected) {
    auto block=setup(1,color?0:1);block.controls["aSmoothIterate"]=iterations;
    assert(evaluate(1,block) && published==1);
    const auto field=stagedData.meshes[0]->getAttr("curvature");assert(field);
    if(color){
        const auto values=std::dynamic_pointer_cast<DPVec3fAttr>(field);assert(values && samples.size()==5);
        for(unsigned int i=0;i<5;++i)assert(std::fabs(values->values[i][0]-(expected[i]+1)*.5)<1e-6);
    }else{
        const auto values=std::dynamic_pointer_cast<DPDoubleAttr>(field);assert(values);
        for(unsigned int i=0;i<5;++i)assert(std::fabs(values->values[i]-expected[i])<1e-12);
        assert(std::find(operations.begin(),operations.end(),"ramp.bind")==operations.end());
    }
    const auto center=std::dynamic_pointer_cast<DPVec3fAttr>(stagedData.getAttr("center"));
    assert(center && center->values[0][0]==10 && center->values[0][2]==30);
    const auto old=std::dynamic_pointer_cast<DPDoubleAttr>(inputData.meshes[0]->getAttr("curvature"));
    assert(old && old->values[0]==9);
}
void charactorCases() {
    for(bool color:{false,true}) {
        expectCurvature(0,color,{-1,0,1,0,0});
        expectCurvature(1,color,{.5,-1,-1,0,0});
        expectCurvature(2,color,{-1,.5,.5,0,0});
    }
    for(int pass=0;pass<3;++pass)for(bool stuck:{false,true}) {
        auto block=setup(1);if(stuck)stuckPass=pass;else earlyPass=pass;
        fails(1,block);assert(callCount<250);
    }
    auto block=setup(1);badIndex=true;fails(1,block);
    block=setup(1);pointCount=4;fails(1,block);
    block=setup(1);pointCount=-2;fails(1,block);
    block=setup(1);badNormal=true;fails(1,block);
    block=setup(1);badColor=true;fails(1,block);
    block=setup(1);faces[0]={-1};fails(1,block);
    block=setup(1);neighbours[0]={5};fails(1,block);
    block=setup(1);faceCenter.x=NAN;fails(1,block);
    block=setup(1);block.controls["aSmoothIterate"]=-1;fails(1,block);
    block=setup(1);block.controls["aSmoothIterate"]=20000001;fails(1,block);
    block=setup(1);inputData.totalOverride=10000001;fails(1,block);
    block=setup(1);inputData.valid=false;fails(1,block);
    for(int mode:{1,2,3}) {block=setup(1);nullMode=mode;fails(1,block);}
    block=setup(1);zeroNormal=true;assert(evaluate(1,block));
    for(double sample:samples)assert(sample==.5);
    block=setup(1);block.controls["aAddCurvature"]=0;assert(evaluate(1,block));
    assert(std::find(operations.begin(),operations.end(),"vertex.bind")==operations.end());
    block=setup(1);inputData.meshes.clear();assert(evaluate(1,block));
    block=setup(1);meshPoints.clear();inputData.meshes[0]->points.clear();assert(evaluate(1,block));
}
int main() {
    boundaryCases(0,0);boundaryCases(1,0);boundaryCases(1,1);
    faceCases();charactorCases();
    auto block=setup(0);DPolyFaceInfo face;
    assert(face.compute(MPlug(face.thisMObject(),{"unrelated"}),block)==MS::kUnknownParameter && callCount==0);
    block=setup(1);DMCharactor character;
    assert(character.compute(MPlug(character.thisMObject(),{"unrelated"}),block)==MS::kUnknownParameter && callCount==0);
    std::cout<<"Face publication/mean and Charactor bounded traversal/legacy values passed\n";
}
