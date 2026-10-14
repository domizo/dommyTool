// Production text generation and curve batch boundaries; native FreeType/Maya
// geometry is replaced by deterministic, failure-injectable host doubles.
#include "common/utility/DStatusOperation.h"
#include "modules/curve/DCurve/Generate/DTextGenerationPlan.h"
#include "modules/curve/DCurve/Generate/DCurveFromCurvePlan.h"
#include <cassert>
#include <codecvt>
#include <cstdint>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <string>
#include <vector>

static int batches=0, singles=0, constructions=0, replacements=0, groupsAdded=0;
static bool failBatch=false, shortBatch=false, failReplace=false, throwConstruct=false, fileExists=true;
static bool failedInsertObserved=false;
static int failFont=0, libraries=0, faces=0;
struct DPoint {
    float xyz[3];
    DPoint(double x=0,double y=0,double z=0):xyz{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)}{}
    float& operator[](std::size_t i){return xyz[i];}
    float operator[](std::size_t i)const{return xyz[i];}
};
using DPointList=std::vector<DPoint>;
using DPointIter=DPointList::iterator;
using DPIdList=std::vector<bool>;
using DPIndexList=std::vector<std::size_t>;
struct MDoubleArray {std::size_t length=0;};
static void GenerateDCurveKnots(std::size_t count,int degree,MDoubleArray& knots){knots.length=count+degree-1;}
struct DCurveInternalData {
    DPointList points;MDoubleArray knots;int degree;bool open;
    DCurveInternalData(const DPointList& p,const MDoubleArray& k,int d,bool o):points(p),knots(k),degree(d),open(o){
        ++constructions;if(throwConstruct)throw std::runtime_error("construct");
    }
    bool hasValidCurveTopology()const{return points.size()>1 && knots.length==points.size()+degree-1;}
    const DPointList& getPositionConst()const{return points;}
    bool replacePositions(const DPointList& p){++replacements;if(failReplace)return false;points=p;return true;}
};
using DCItem=std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    using DCurveList=std::vector<DCItem>;
    DCurveList curves;std::map<std::string,DPIdList> groups;
    bool insert(const DCurveList& source){
        ++batches;if(failBatch){failedInsertObserved=true;return false;}
        DCurveList candidate;
        for(const auto& curve:source)candidate.push_back(std::make_shared<DCurveInternalData>(*curve));
        if(shortBatch && !candidate.empty())candidate.pop_back();
        curves.swap(candidate);return true;
    }
    bool insert(const DCItem& curve){++singles;curves.push_back(curve);return true;}
    std::size_t numberOfCurves()const{assert(!failedInsertObserved);return curves.size();}
    bool getCheckedTotalCvs(std::size_t& n)const{assert(!failedInsertObserved);n=0;for(const auto& curve:curves)n+=curve->points.size();return true;}
    bool hasValidCurveTopology()const{assert(!failedInsertObserved);for(const auto& curve:curves)if(!curve->hasValidCurveTopology())return false;return true;}
    void updateBBox(){}
    void addGrp(const std::string& name,const DPIdList& ids){++groupsAdded;groups[name]=ids;}
};
struct MString {
    std::string value;MString(const char* text):value(text){}
    MString operator+(int index)const{return MString((value+std::to_string(index)).c_str());}
    const char* asChar()const{return value.c_str();}
};
static bool failPathSet=false;
static std::string expandedFontPath,loadedFontPath;
struct MFileObject {std::string path;
    MStatus setRawFullName(const char* value){path=value;return failPathSet?MS::kFailure:MS::kSuccess;}
    MString resolvedFullName()const{return MString((expandedFontPath.empty()?path:expandedFontPath).c_str());}
    bool exists()const{return fileExists;}};
namespace MGlobal {void displayWarning(const char*){}void displayError(const char*){}}
#define DOMMY_MAYA_ERROR(name, message) do {} while(false)
using FT_UInt=unsigned;
struct FT_Vector {long x,y;};
struct FT_Outline {short n_points=0,n_contours=0;FT_Vector* points=nullptr;char* tags=nullptr;short* contours=nullptr;};
struct Glyph {FT_Outline outline;FT_Vector advance;};
using FT_GlyphSlot=Glyph*;
struct Metrics {long height=2000,max_advance=1000;};
struct Size {Metrics metrics;};
struct Face {Glyph glyphData;Size sizeData;Glyph* glyph=&glyphData;Size* size=&sizeData;};
using FT_Face=Face*;
using FT_Library=int*;
constexpr int FT_LOAD_NO_BITMAP=1;
static int FT_Init_FreeType(FT_Library* library){if(failFont==1)return 1;*library=new int(1);++libraries;return 0;}
static int FT_Done_FreeType(FT_Library library){delete library;--libraries;return 0;}
static int FT_New_Face(FT_Library,const char* path,int,FT_Face* face){loadedFontPath=path;if(failFont==2)return 1;*face=new Face;++faces;return 0;}
static int FT_Done_Face(FT_Face face){delete face;--faces;return 0;}
static int FT_Set_Pixel_Sizes(FT_Face,FT_UInt,FT_UInt){return failFont==3;}
static FT_UInt FT_Get_Char_Index(FT_Face,char32_t c){return static_cast<FT_UInt>(c);}
static int FT_Load_Glyph(FT_Face face,FT_UInt c,int){
    if(failFont==4)return 1;
    static FT_Vector a[]={{0,0},{1000,0},{1000,1000},{0,1000}};
    static FT_Vector b[]={{0,0},{1000,0},{500,1000},{200,200},{800,200},{500,700}};
    static FT_Vector bezier[]={{0,0},{500,1000},{1000,0}};
    static char on[]={1,1,1,1,1,1},off[]={1,0,1};
    static short contourA[]={3},contourB[]={2,5},contourC[]={2};
    auto& o=face->glyph->outline;o=FT_Outline();face->glyph->advance={1000,0};
    if(c=='A'){o.n_points=4;o.n_contours=1;o.points=a;o.tags=on;o.contours=contourA;}
    if(c=='B'){o.n_points=6;o.n_contours=2;o.points=b;o.tags=on;o.contours=contourB;}
    if(c=='C'){o.n_points=3;o.n_contours=1;o.points=bezier;o.tags=off;o.contours=contourC;}
    if(failFont==5)o.contours=nullptr;
    return 0;
}
#include "curve_source_generation.inc"

static void reset(){
    assert(libraries==0 && faces==0);
    batches=singles=constructions=replacements=groupsAdded=0;
    failBatch=shortBatch=failReplace=throwConstruct=false;fileExists=true;failFont=0;
    failPathSet=false;expandedFontPath.clear();loadedFontPath.clear();
    failedInsertObserved=false;
}
static dommy::curve::TextGenerationPlan plan(){
    dommy::curve::TextGenerationPlan p;
    assert(dommy::curve::buildTextGenerationPlan(20,4,5,1,4,0,0,p));return p;
}
static void near(float a,float b){assert(std::fabs(a-b)<1e-5f);}
int main(){
    reset();expandedFontPath="/project/fonts/font.ttf";DCurveData resolved;
    assert(generateTextCurves(resolved,"A","$FONT_ROOT/font.ttf",plan(),false,"piece")==MS::kSuccess);
    assert(loadedFontPath==expandedFontPath);
    reset();failPathSet=true;DCurveData rejected;std::string prior="prior";
    assert(!resolveFontPath("font",prior)&&prior=="prior");
    assert(!generateTextCurves(rejected,"A","font",plan(),false,"piece")&&libraries==0&&faces==0&&batches==0);
    reset();expandedFontPath.assign(dommy::curve::kMaxFontPathBytes+1U,'x');
    assert(!resolveFontPath("font",prior)&&prior=="prior");
    reset();DCurveData text;
    assert(generateTextCurves(text,"AB \nA","font",plan(),true,"piece")==MS::kSuccess);
    assert(batches==1 && singles==0 && constructions==4 && replacements==4 && groupsAdded==3);
    assert(text.curves.size()==4 && text.curves[0]->points.size()==5 && text.curves[1]->points.size()==4);
    assert((text.groups.at("piece0")==DPIdList{true,false,false,false}));
    assert((text.groups.at("piece1")==DPIdList{false,true,true,false}));
    assert((text.groups.at("piece2")==DPIdList{false,false,false,true}));
    near(text.curves[1]->points[0][0],1);near(text.curves[3]->points[0][1],-2);
    reset();DCurveData ungrouped;
    assert(generateTextCurves(ungrouped,"AB \nA","font",plan(),false,"piece")==MS::kSuccess);
    assert(groupsAdded==0 && ungrouped.groups.empty() && batches==1);
    for(std::size_t i=0;i<text.curves.size();++i)
        for(std::size_t j=0;j<text.curves[i]->points.size();++j)
            for(std::size_t k=0;k<3;++k)near(text.curves[i]->points[j][k],ungrouped.curves[i]->points[j][k]);
    reset();DCurveData bezier;
    assert(generateTextCurves(bezier,"C","font",plan(),true,"piece")==MS::kSuccess);
    assert(bezier.curves[0]->points.size()==6);near(bezier.curves[0]->points[1][0],0.25f);near(bezier.curves[0]->points[1][1],0.375f);
    for(int failure=1;failure<=5;++failure){
        reset();failFont=failure;DCurveData output;
        assert(generateTextCurves(output,"A","font",plan(),true,"piece")!=MS::kSuccess);
        assert(batches==0 && groupsAdded==0 && output.curves.empty() && libraries==0 && faces==0);
    }
    reset();failBatch=true;DCurveData failed;
    assert(generateTextCurves(failed,"AB","font",plan(),true,"piece")==MS::kFailure && groupsAdded==0 && failed.curves.empty());
    reset();shortBatch=true;
    assert(generateTextCurves(failed,"AB","font",plan(),true,"piece")==MS::kFailure && groupsAdded==0);
    reset();failReplace=true;
    assert(generateTextCurves(failed,"AB","font",plan(),true,"piece")==MS::kFailure && batches==0);
    reset();throwConstruct=true;
    try {(void)generateTextCurves(failed,"A","font",plan(),true,"piece");assert(false);}catch(const std::runtime_error&){}
    assert(libraries==0 && faces==0 && batches==0);
    reset();DCurveData empty;
    assert(generateTextCurves(empty," \n\t","font",plan(),true,"piece")==MS::kSuccess && empty.curves.empty() && empty.groups.empty());

    dommy::curve::TextOutputBudget budget;
    assert(dommy::curve::initializeTextOutputBudget(0,0,0,false,budget));
    budget.baseBytes=dommy::generation::kMaxCurveGenerationBytes;
    DPointList points;
    assert(!appendPointWithinBudget(points,DPoint(),budget) && points.empty());
    points.push_back(DPoint());
    assert(!appendQuadraticSamples(points,DPoint(1),DPoint(2),4,budget) && points.size()==1);

    reset();dommy::curve::CurveFromCurvePlan curvePlan;
    curvePlan.outputCurveCount=4;curvePlan.outputPointCount=18;
    DCurveData::DCurveList staged=text.curves;DCurveData output;
    assert(commitFromCurve(&output,staged,curvePlan)==MS::kSuccess && batches==1 && singles==0 && staged.empty());
    for(std::size_t i=0;i<text.curves.size();++i)near(output.curves[i]->points[0][0],text.curves[i]->points[0][0]);
    reset();failBatch=true;staged=text.curves;
    assert(commitFromCurve(&output,staged,curvePlan)==MS::kFailure && !staged.empty());
    reset();shortBatch=true;staged=text.curves;
    assert(commitFromCurve(&output,staged,curvePlan)==MS::kFailure);
    reset();
    std::cout<<"Source curve generation: text order/groups/Bezier samples, one batch, failure-before-commit and resource cleanup passed\n";
}
