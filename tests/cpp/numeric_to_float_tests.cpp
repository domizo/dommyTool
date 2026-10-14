#include "array_utility_doubles.h"

// Payload identity belongs to each block handle, independently of the plug store.
struct NumericPayload {
    MFnNumericData::Type type=MFnNumericData::kDouble;
    bool simple=false,generic=false,genericNumeric=false,null=false;
    MInt64 integer=0;
    std::array<double,4> values{{1,2,3,4}};
    MMatrix matrix;
};
std::map<const Input*,NumericPayload> numericPayloads;
struct NumericHandle : MDataHandle {
    NumericHandle(MDataHandle h={}):MDataHandle(h){}
    const NumericPayload& payload()const{check();return numericPayloads.at(input);}
    bool isGeneric(bool& numeric,bool& null)const {
        const auto& p=payload();numeric=p.genericNumeric;null=p.null;return p.generic;
    }
    bool isNumeric()const{return payload().simple;}
    MFnNumericData::Type numericType()const {
        assert(payload().simple);return payload().type;
    }
    double asGenericDouble()const{assert(payload().genericNumeric);return input->number;}
    double asDouble()const{assert(payload().simple);return input->number;}
    bool asBool()const{return asDouble()!=0;}
    unsigned char asUChar()const{return static_cast<unsigned char>(asDouble());}
    char asChar()const{return static_cast<char>(asDouble());}
    short asShort()const{return static_cast<short>(asDouble());}
    int asInt()const{return static_cast<int>(asDouble());}
    MInt64 asInt64()const{assert(payload().simple);return payload().integer;}
    float asFloat()const{return static_cast<float>(asDouble());}
    MMatrix asMatrix()const{return payload().matrix;}
    MObject data() {
        assert(!payload().simple && !payload().genericNumeric);
        return MDataHandle::data();
    }
};
struct NumericBlock : MDataBlock {
    NumericHandle inputValue(const MObject& a,MStatus* s){return MDataBlock::inputValue(a,s);}
};
struct NumericData {
    using Type=MFnNumericData::Type;
    static constexpr Type kInvalid=static_cast<Type>(31);
#define NUMERIC_KIND(name) static constexpr Type name=MFnNumericData::name
    NUMERIC_KIND(kBoolean);NUMERIC_KIND(kByte);NUMERIC_KIND(kChar);NUMERIC_KIND(kShort);
    NUMERIC_KIND(kInt);NUMERIC_KIND(kInt64);NUMERIC_KIND(kFloat);NUMERIC_KIND(kDouble);
    NUMERIC_KIND(k2Short);NUMERIC_KIND(k3Short);NUMERIC_KIND(k2Int);NUMERIC_KIND(k3Int);
    NUMERIC_KIND(k2Float);NUMERIC_KIND(k3Float);NUMERIC_KIND(k2Double);NUMERIC_KIND(k3Double);
    NUMERIC_KIND(k4Double);
#undef NUMERIC_KIND
    MObject object;
    NumericData(MObject o,MStatus* s):object(o){
        *s=api("numeric.bind");if(*s && o.isNull())*s=MS::kInvalidParameter;
    }
    Type numericType(MStatus* s){
        *s=api("numeric.type");return *s?numericPayloads.at(object.inputOwner).type:kInvalid;
    }
    template<class T> MStatus read(T& a,T& b)const {
        const auto s=api("numeric.read");
        if(s){const auto& v=numericPayloads.at(object.inputOwner).values;a=static_cast<T>(v[0]);b=static_cast<T>(v[1]);}
        return s;
    }
    template<class T> MStatus read(T& a,T& b,T& c)const {
        const auto s=api("numeric.read");
        if(s){const auto& v=numericPayloads.at(object.inputOwner).values;
            a=static_cast<T>(v[0]);b=static_cast<T>(v[1]);c=static_cast<T>(v[2]);}
        return s;
    }
    MStatus getData2Short(short& a,short& b)const{return read(a,b);}
    MStatus getData3Short(short& a,short& b,short& c)const{return read(a,b,c);}
    MStatus getData2Int(int& a,int& b)const{return read(a,b);}
    MStatus getData3Int(int& a,int& b,int& c)const{return read(a,b,c);}
    MStatus getData2Float(float& a,float& b)const{return read(a,b);}
    MStatus getData3Float(float& a,float& b,float& c)const{return read(a,b,c);}
    MStatus getData2Double(double& a,double& b)const{return read(a,b);}
    MStatus getData3Double(double& a,double& b,double& c)const{return read(a,b,c);}
    MStatus getData4Double(double& a,double& b,double& c,double& d)const {
        const auto s=api("numeric.read");if(s){const auto& v=numericPayloads.at(object.inputOwner).values;
            a=v[0];b=v[1];c=v[2];d=v[3];}return s;
    }
};
using MIntArray=Array<int>;
template<> const char* arrayName<int>(){return "int";}
MIntArray numericInts;
template<> Array<int>& arrayInput<int>(Input&){return numericInts;}
using MFnIntArrayData=ArrayData<int>;
long long reportedArrayCount=-1;
int numericArrayOwners=0;
template<class T> struct BorrowedNumericArray {
    const Array<T>* source=nullptr;
    explicit BorrowedNumericArray(const Array<T>* value=nullptr):source(value){}
    unsigned int length()const {
        assert(source && numericArrayOwners>0);return source->length();
    }
    const T& operator[](unsigned int index)const {
        assert(source && numericArrayOwners>0);return (*source)[index];
    }
};
template<class T> struct NumericArrayData {
    MObject object;
    bool valid;
    NumericArrayData(MObject value,MStatus* status):object(value) {
        *status=api(std::string(arrayName<T>())+".bind");valid=bool(*status);
        ++numericArrayOwners;
    }
    ~NumericArrayData(){--numericArrayOwners;}
    unsigned int length(MStatus* status)const {
        assert(valid);*status=api(std::string(arrayName<T>())+".length");
        if(!*status)return 0;
        if(reportedArrayCount>=0)return static_cast<unsigned int>(reportedArrayCount);
        return arrayInput<T>(*object.inputOwner).length();
    }
    BorrowedNumericArray<T> array(MStatus* status)const {
        assert(valid);*status=api(std::string(arrayName<T>())+".source");
        return BorrowedNumericArray<T>(*status?&arrayInput<T>(*object.inputOwner):nullptr);
    }
};
MStatus DOutputDataMayaFloatArray(MDataBlock&,const MPlug&,const MObject&,const MFloatArray& values) {
    const auto status=api("publish");
    if(status){++publications;published.clear();for(float value:values.values)published.push_back({{value,0,0,0}});}
    return status;
}
#define MFnNumericData NumericData
#define MDataHandle NumericHandle
#define MDataBlock NumericBlock
#define MFnFloatArrayData NumericArrayData<float>
#define MFnDoubleArrayData NumericArrayData<double>
#define MFnIntArrayData NumericArrayData<int>
#define MFnVectorArrayData NumericArrayData<MVector>
#define MFnPointArrayData NumericArrayData<MPoint>
#define MFnMatrixArrayData NumericArrayData<MMatrix>
#include "numeric_to_float.inc"

NumericBlock context,plugStore;
void setup(MFnData::Type type,NumericData::Type numeric=NumericData::kDouble) {
    assert(numericArrayOwners==0);reportedArrayCount=-1;
    resetRun();numericPayloads.clear();context.inputs.clear();plugStore.inputs.clear();
    numericInts.values.clear();
    activeData=&plugStore;plugStore.inputs["inputData"].connected=true;
    plugStore.inputs["inputData"].number=999;
    auto& input=context.inputs["inputData"];input.type=type;input.number=7.25;
    auto& payload=numericPayloads[&input];payload.type=numeric;
    payload.simple=type==MFnData::kInvalid;
}
MStatus compute();
void arrayAdmission() {
    using namespace dommy;
    for(auto type:{MFnData::kVectorArray,MFnData::kPointArray,MFnData::kFloatArray,
                  MFnData::kDoubleArray,MFnData::kIntArray,MFnData::kMatrixArray}) {
        setup(type);
        const std::size_t components=type==MFnData::kMatrixArray?16U:
            (type==MFnData::kVectorArray||type==MFnData::kPointArray?3U:1U);
        // A logical count, not a huge allocated fixture payload.
        reportedArrayCount=generation::kMaxGeneratedPointCount/components+1U;
        assert(compute()==MS::kInvalidParameter && publications==0 && numericArrayOwners==0);
        for(const auto& op:operations)assert(op.find(".source")==std::string::npos);
        setup(type);reportedArrayCount=1;
        assert(compute()==MS::kFailure && publications==0 && numericArrayOwners==0);
        setup(type);assert(compute()==MS::kSuccess && publications==1 && published.empty());
    }
    setup(MFnData::kDoubleArray);context.inputs["inputData"].doubles.values={1,2};
    MStatus status;NumericHandle handle=context.inputValue(DNumericToFloatArray::inputData,&status);
    assert(status);std::vector<float> values{99};
    assert(maya::readScalarInput(handle,2U,values)==MS::kSuccess && values==std::vector<float>({1,2}));
    for(int index=0;index<3;++index) {
        resetRun();failAt=index;values={99};
        assert(maya::readScalarInput(handle,2U,values)!=MS::kSuccess && values==std::vector<float>({99}));
        assert(callCount==index+1 && numericArrayOwners==0);
    }
    resetRun();reportedArrayCount=1;values={99};
    assert(maya::readScalarInput(handle,2U,values)==MS::kFailure && values==std::vector<float>({99}));
    resetRun();reportedArrayCount=-1;
    assert(maya::readScalarInput(handle,3U,values)==MS::kInvalidParameter && values==std::vector<float>({99}));
    assert(maya::readScalarInput(handle,maya::kInferInputCount,values)==MS::kSuccess);
    resetRun();reportedArrayCount=generation::kMaxGeneratedPointCount+1U;values={99};
    assert(maya::readScalarInput(handle,maya::kInferInputCount,values)==MS::kInvalidParameter);
    assert(values==std::vector<float>({99}) && callCount==2 && numericArrayOwners==0);
    setup(MFnData::kVectorArray);context.inputs["inputData"].vectors.values={{1,2,3}};
    handle=context.inputValue(DNumericToFloatArray::inputData,&status);assert(status);
    using Vector = std::array<float,3>;
    std::vector<Vector> vectors{Vector{{99,99,99}}};
    assert(maya::readVectorInput(handle,1U,vectors)==MS::kSuccess && vectors[0][0]==1);
    for(int index=0;index<3;++index) {
        resetRun();failAt=index;vectors={Vector{{99,99,99}}};
        assert(maya::readVectorInput(handle,1U,vectors)!=MS::kSuccess && vectors[0][0]==99);
        assert(callCount==index+1 && numericArrayOwners==0);
    }
    resetRun();reportedArrayCount=2;vectors={Vector{{99,99,99}}};
    assert(maya::readVectorInput(handle,1U,vectors)==MS::kFailure && vectors[0][0]==99);
    resetRun();reportedArrayCount=-1;
    assert(maya::readVectorInput(handle,maya::kInferInputCount,vectors)==MS::kSuccess);
    resetRun();reportedArrayCount=generation::kMaxGeneratedPointCount+1U;
    vectors={Vector{{99,99,99}}};
    assert(maya::readVectorInput(handle,maya::kInferInputCount,vectors)==MS::kInvalidParameter);
    assert(vectors[0][0]==99 && callCount==2);
    assert(numericArrayOwners==0);
}
MStatus compute(){DNumericToFloatArray node;return node.compute(MPlug(DNumericToFloatArray::outDataArray),context);}
void expect(std::initializer_list<float> values) {
    assert(compute()==MS::kSuccess && publications==1 && published.size()==values.size());
    std::size_t index=0;for(float value:values)assert(published[index++][0]==value);
}
void valuesAndContexts() {
    for(auto type:{NumericData::kBoolean,NumericData::kByte,NumericData::kChar,NumericData::kShort,
                  NumericData::kInt,NumericData::kInt64,NumericData::kFloat,NumericData::kDouble}) {
        setup(MFnData::kInvalid,type);
        auto& input=context.inputs["inputData"];input.number=type==NumericData::kByte?255:-7;
        numericPayloads[&input].integer=-7;
        expect({type==NumericData::kBoolean?1.0f:(type==NumericData::kByte?255.0f:-7.0f)});
    }
    setup(MFnData::kInvalid,NumericData::kInt64);
    const MInt64 wide=(MInt64(1)<<54)+12345;
    numericPayloads[&context.inputs["inputData"]].integer=wide;
    expect({static_cast<float>(static_cast<double>(wide))});
    setup(MFnData::kInvalid);auto& p=numericPayloads[&context.inputs["inputData"]];
    p.simple=false;p.generic=true;p.genericNumeric=true;expect({7.25f});
    for(auto type:{NumericData::k2Short,NumericData::k3Short,NumericData::k2Int,NumericData::k3Int,
                  NumericData::k2Float,NumericData::k3Float,NumericData::k2Double,NumericData::k3Double,
                  NumericData::k4Double}) {
        setup(MFnData::kNumeric,type);
        numericPayloads[&context.inputs["inputData"]].generic=true;
        if(type==NumericData::k4Double)expect({1,2,3,4});
        else if(type==NumericData::k3Short||type==NumericData::k3Int||type==NumericData::k3Float||type==NumericData::k3Double)
            expect({1,2,3});else expect({1,2});
    }
    setup(MFnData::kFloatArray);context.inputs["inputData"].floats.values={1,2};expect({1,2});
    setup(MFnData::kDoubleArray);context.inputs["inputData"].doubles.values={1.25,-2.5};expect({1.25f,-2.5f});
    setup(MFnData::kIntArray);numericInts.values={-7,2147483647};expect({-7,static_cast<float>(2147483647)});
    setup(MFnData::kVectorArray);context.inputs["inputData"].vectors.values={{1,2,3}};expect({1,2,3});
    setup(MFnData::kPointArray);context.inputs["inputData"].points.values={{1,2,3,99}};expect({1,2,3});
    setup(MFnData::kMatrix);expect({1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    setup(MFnData::kMatrixArray);context.inputs["inputData"].matrices.values={MMatrix()};
    expect({1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    setup(MFnData::kMatrix);
    auto& matrix=numericPayloads[&context.inputs["inputData"]].matrix;
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)matrix[row][column]=row*10+column;
    expect({0,1,2,3,10,11,12,13,20,21,22,23,30,31,32,33});
    setup(MFnData::kInvalid);plugStore.inputs["inputData"].connected=false;expect({});
    for(double invalid:std::vector<double>{NAN,INFINITY,-INFINITY,1e100}) {
        setup(MFnData::kInvalid);context.inputs["inputData"].number=invalid;
        assert(compute()==MS::kInvalidParameter && publications==0);
    }
    setup(MFnData::kNumeric,NumericData::k3Double);
    numericPayloads[&context.inputs["inputData"]].values[2]=INFINITY;
    assert(compute()==MS::kInvalidParameter && publications==0);
    setup(MFnData::kInvalid);auto& null=numericPayloads[&context.inputs["inputData"]];
    null.generic=true;null.null=true;
    assert(compute()==MS::kInvalidParameter && publications==0);
    setup(MFnData::kInvalid);numericPayloads[&context.inputs["inputData"]].simple=false;
    assert(compute()==MS::kInvalidParameter && publications==0);
    setup(MFnData::kInvalid);context.inputs["inputData"].number=-0.0;
    expect({-0.0f});assert(std::signbit(published[0][0]));
    setup(MFnData::kNumeric,NumericData::k3Double);context.inputs["inputData"].nullObject=true;
    assert(compute()==MS::kInvalidParameter && publications==0);
    setup(MFnData::kInvalid);DNumericToFloatArray node;
    assert(node.compute(MPlug(MObject("other")),context)==MS::kUnknownParameter && operations.empty());
}
int failures() {
    int tested=0;
    for(auto type:{MFnData::kInvalid,MFnData::kNumeric,MFnData::kVectorArray,MFnData::kPointArray,
                  MFnData::kFloatArray,MFnData::kDoubleArray,MFnData::kIntArray,MFnData::kMatrix,MFnData::kMatrixArray}) {
        const auto fixture=[type](){setup(type,NumericData::k3Double);
            auto& input=context.inputs["inputData"];
            if(type==MFnData::kInvalid)numericPayloads[&input].type=NumericData::kDouble;
            input.vectors.values={{1,2,3}};input.points.values={{1,2,3}};
            input.floats.values={1,2};input.doubles.values={1,2};
            numericInts.values={1,2};input.matrices.values={MMatrix()};};
        fixture();assert(compute()==MS::kSuccess);const auto trace=operations;
        for(std::size_t index=0;index<trace.size();++index) {
            for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
                fixture();failAt=int(index);injectedStatus=status;
                assert(compute()==status && callCount==int(index)+1 && publications==0);++tested;
                resetRun();assert(compute()==MS::kSuccess);
            }
            fixture();throwAt=int(index);assert(compute()!=MS::kSuccess && publications==0);++tested;
        }
    }
    return tested;
}
int main(){valuesAndContexts();arrayAdmission();const int count=failures();
    std::cout<<"Numeric block contexts, count preflight/cardinality/borrowed-owner guards and "
             <<count<<" API failure/recovery cases passed\n";}
