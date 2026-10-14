#include "timeshift_doubles.h"
#include <functional>
#include <fstream>

template<class T> const std::vector<T>& fieldValues(const ShiftData& data,const std::string& name) {
    const auto field=std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(data.getAttr(name));
    assert(field);return field->value();
}

void fixture(bool geometry = false, MFnData::Type type = MFnData::kMesh) {
    reset(); inputs.clear(); timePoints.clear(); timeObjects.clear(); queriedTimes.clear();
    geometries.clear(); emitterMeshes.clear(); scalarPayloads.clear();
    activeTime=MTime(2,MTime::kNTSCFrame); shortPointStorage=false; scalarReportedCount=0;
    rejectFloatField=rejectIdField=false;
    for(int i=0;i<3;++i){
        timePoints[i].points={{double(i),0,0},{double(i),1,0}};
        timeObjects[i]={"sample"+std::to_string(i)};
        geometries[timeObjects[i].name].points.assign(timePoints[i].points.begin(),timePoints[i].points.end());
        geometries[timeObjects[i].name].reportedCvCount=2;
        emitterMeshes[timeObjects[i].name].points.assign(timePoints[i].points.begin(),timePoints[i].points.end());
    }
    inputs["inDP"].plugin=&timePoints[2]; inputs["inDP"].connected=!geometry;
    inputs["inObject"].connected=geometry; inputs["inObject"].type=type;
    inputs["inObject"].object=timeObjects[2];
    inputs["inTimeOffsetData"].connected=false;inputs["inTimeLengthData"].connected=false;
    inputs["TimeSampling"].number=3;inputs["TimeLength"].number=2;
    inputs["CurrentTime"].number=2;inputs["TimeLengthType"].number=DP_TIME_LEN;
    inputs["PointOrderType"].number=DP_TIME_BIRTH_SAME_TIME;
    inputs["TimeOffsetAttrName"].string="offset";inputs["TimeLengthAttrName"].string="length";
    inputs["TimeAttrName"].string="time";inputs["ParentIdAttrName"].string="pid";
    inputs["IsAddTimeAttr"].number=1;inputs["IsAddParentIdAttr"].number=1;
    inputs["TimeAttrValueType"].number=DP_TIME_ATTR_RAW;
    shiftOutput=ShiftData(); shiftOutput.points={{99,99,99}};
}
MStatus compute() { DPTimeShift node;ShiftBlock block;return node.compute(ShiftPlug({},DPTimeShift::outDP),block); }
void numeric() {
    DScalarDataList values;
    getFrameValueArray(values,1,-2,0,0); assert(values==DScalarDataList{-1});
    getFrameValueArray(values,3,-2,0,0); assert(values==DScalarDataList({-2,-1,0}));
    getFrameValueArray(values,2,3,0,0); assert(values==DScalarDataList({3,5}));
    for(float random:{-1.0f,std::numeric_limits<float>::max(),INFINITY,NAN}) {
        bool rejected=false;values={42};
        try{getTimeValueArray(values,3,5,0,0,random);}catch(const std::exception&){rejected=true;}
        assert(rejected&&values==DScalarDataList{42});
    }
    for(uint count:{0U,static_cast<uint>(dommy::sampling::kMaxTimeSamples+1)}) {
        bool rejected=false;
        try{getFrameValueArray(values,count,0,0,0);}catch(const std::exception&){rejected=true;}
        assert(rejected);
    }
    assert(!dommy::sampling::validTimePointProduct(100000,1000));
    assert(dommy::sampling::validTimePointProduct(2,3));
    dommy::sampling::TimeSamplingBudget budget;
    assert(budget.readPoints(dommy::generation::kMaxWorkItemCount-1));
    assert(!budget.readPoints(0));
}
void setup(int source, int mode = DP_TIME_LEN) {
    const MFnData::Type types[]{MFnData::kAny,MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface};
    fixture(source != 0, types[source]);
    inputs["TimeLengthType"].number=mode;
    if (mode == DP_TIME_LEN_ATTRIBUTE) {
        timePoints[2].fields["offset"]=std::make_shared<DPFloatAttr>(DScalarDataList{0,0});
        timePoints[2].fields["length"]=std::make_shared<DPFloatAttr>(DScalarDataList{2,2});
    }
    if (mode == DP_TIME_LEN_DATA) {
        inputs["inTimeOffsetData"].connected=true;
        inputs["inTimeOffsetData"].type=MFnData::kDoubleArray;
        inputs["inTimeOffsetData"].object={"offsets"};
        scalarPayloads["offsets"]={0,0};
    }
}
void resizeSnapshot(int time, unsigned count) {
    timePoints[time].points.resize(count);
    geometries[timeObjects[time].name].points.assign(timePoints[time].points.begin(),timePoints[time].points.end());
    geometries[timeObjects[time].name].reportedCvCount=count;
    emitterMeshes[timeObjects[time].name].points.assign(timePoints[time].points.begin(),timePoints[time].points.end());
}
void unchangedOutput() {
    assert(!published && shiftOutput.points.size()==1 && shiftOutput.points[0].x==99);
}
void scalarInput(MFnData::Type type, const std::vector<double>& values) {
    setup(0,DP_TIME_LEN_DATA);
    inputs["inTimeOffsetData"].type=type;
    scalarPayloads["offsets"]=values;
}
MStatus readOffsets(DScalarDataList& values) {
    return getTimeValueArrayFromData(values,ShiftPlug({},DPTimeShift::inTimeOffsetData));
}
void scalarReaderBoundaries() {
    const double maximum=std::numeric_limits<float>::max();
    const double denormal=std::numeric_limits<float>::denorm_min();
    const DScalarDataList sentinel{42,-17};
    for(auto type:{MFnData::kFloatArray,MFnData::kDoubleArray,MFnData::kIntArray}) {
        std::vector<double> payload;
        if(type==MFnData::kIntArray)
            payload={0,1,-1,16777217,-16777217,
                     double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max())};
        else {
            payload={0.0,-0.0,1.25,-2.5,maximum,-maximum,denormal,-denormal};
            if(type==MFnData::kDoubleArray) {
                payload.insert(payload.end(),{.1,-.1,16777217,-16777217,
                    std::nextafter(maximum,0.0),std::nextafter(-maximum,0.0),
                    denormal/2,-denormal/2});
            }
        }
        scalarInput(type,payload);DScalarDataList values=sentinel;
        assert(readOffsets(values)==MS::kSuccess && values.size()==payload.size());
        for(std::size_t i=0;i<payload.size();++i) {
            const float expected=static_cast<float>(payload[i]);
            assert(values[i]==expected);
            if(expected==0)assert(std::signbit(values[i])==std::signbit(expected));
        }
        scalarInput(type,{});values=sentinel;
        assert(readOffsets(values)==MS::kSuccess && values.empty());
        scalarInput(type,{1,2,3});scalarReportedCount=4;values=sentinel;
        assert(readOffsets(values)==MS::kFailure && values==sentinel);
        scalarInput(type,{1,2,3});scalarReportedCount=3000000;values=sentinel;
        assert(readOffsets(values)==MS::kInvalidParameter && values==sentinel);
        assert(std::find(operations.begin(),operations.end(),"scalar.array")==operations.end());
    }
    for(auto type:{MFnData::kFloatArray,MFnData::kDoubleArray}) {
        std::vector<double> invalid{NAN,INFINITY,-INFINITY};
        if(type==MFnData::kDoubleArray)
            invalid.insert(invalid.end(),{std::nextafter(maximum,INFINITY),
                std::nextafter(-maximum,-INFINITY),1e39,-1e39,
                std::numeric_limits<double>::max(),-std::numeric_limits<double>::max()});
        for(double value:invalid)for(std::size_t index=0;index<3;++index) {
            std::vector<double> payload{1,2,3};payload[index]=value;
            scalarInput(type,payload);DScalarDataList values=sentinel;
            assert(readOffsets(values)==MS::kInvalidParameter && values==sentinel);
            scalarPayloads["offsets"]={1,2,3};
            assert(readOffsets(values)==MS::kSuccess && values==DScalarDataList({1,2,3}));
        }
    }
    scalarInput(MFnData::kMesh,{1});DScalarDataList values=sentinel;
    assert(readOffsets(values)==MS::kInvalidParameter && values==sentinel);
}
int scalarReaderFailures() {
    int tested=0;
    const DScalarDataList sentinel{42,-17};
    for(auto type:{MFnData::kFloatArray,MFnData::kDoubleArray,MFnData::kIntArray}) {
        scalarInput(type,{1,2,3});DScalarDataList values=sentinel;
        assert(readOffsets(values)==MS::kSuccess);
        const auto trace=operations;
        assert(trace==std::vector<std::string>({"plug.handle","scalar.bind","scalar.length","scalar.array"}));
        for(unsigned i=0;i<trace.size();++i) {
            for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
                scalarInput(type,{1,2,3});values=sentinel;failAt=i;injectedStatus=status;
                assert(readOffsets(values)==status && callCount==int(i)+1 && values==sentinel);
                reset();
                assert(readOffsets(values)==MS::kSuccess && values==DScalarDataList({1,2,3}));
                ++tested;
            }
            scalarInput(type,{1,2,3});values=sentinel;throwAt=i;bool threw=false;
            try{(void)readOffsets(values);}catch(const std::runtime_error&){threw=true;}
            assert(threw && callCount==int(i)+1 && values==sentinel);
            reset();
            assert(readOffsets(values)==MS::kSuccess && values==DScalarDataList({1,2,3}));
            ++tested;
        }
        scalarInput(type,{1,2,3});values=sentinel;shortAt=3;
        assert(readOffsets(values)==MS::kFailure && shortApplied && values==sentinel);
        reset();
        assert(readOffsets(values)==MS::kSuccess && values==DScalarDataList({1,2,3}));
        ++tested;
    }
    return tested;
}
int scalarComputeAdmission() {
    int tested=0;
    const double maximum=std::numeric_limits<float>::max();
    for(int source=0;source<4;++source)for(const std::string& input:{"offsets","lengths"})
        for(double invalid:{std::nextafter(maximum,INFINITY),std::nextafter(-maximum,-INFINITY),
                            1e39,-1e39,double(NAN),double(INFINITY)}) {
            setup(source,DP_TIME_LEN_DATA);
            inputs["inTimeLengthData"].connected=true;
            inputs["inTimeLengthData"].type=MFnData::kDoubleArray;
            inputs["inTimeLengthData"].object={"lengths"};
            scalarPayloads["lengths"]={2,2};
            scalarPayloads[input][1]=invalid;
            assert(compute()==MS::kInvalidParameter);unchangedOutput();
            for(const auto& time:queriedTimes)assert(time.value()==2 && time.unit()==MTime::kNTSCFrame);
            assert(std::find(operations.begin(),operations.end(),"plugin.create")==operations.end());
            scalarPayloads[input]=input=="offsets" ? std::vector<double>{0,0}:std::vector<double>{2,2};
            reset();queriedTimes.clear();
            assert(compute()==MS::kSuccess && shiftOutput.points.size()==6);
            assert(activeTime.value()==2 && activeTime.unit()==MTime::kNTSCFrame);
            ++tested;
        }
    // Existing point-count fallback is intentional, including empty arrays.
    for(const auto& payload:std::vector<std::vector<double>>{{},{0}}) {
        scalarInput(MFnData::kDoubleArray,payload);
        assert(compute()==MS::kSuccess && shiftOutput.points.size()==6);
    }
    return tested;
}
void aligned() {
    for(int source=0;source<4;++source) {
        for(int order:{DP_TIME_BIRTH_SAME_TIME,DP_TIME_BIRTH_SAME_INDEX}) {
            setup(source,source?DP_TIME_LEN_DATA:DP_TIME_LEN_ATTRIBUTE);
            resizeSnapshot(1,1);
            inputs["PointOrderType"].number=order;
            const auto status=compute();
            if(status!=MS::kSuccess || shiftOutput.points.size()!=5) {
                std::cerr<<"alignment source="<<source<<" order="<<order<<" points="<<shiftOutput.points.size()<<'\n';
                for(const auto& operation:operations)std::cerr<<operation<<'\n';
            }
            assert(status==MS::kSuccess && shiftOutput.points.size()==5);
            const DScalarDataList expectedTimes=order==DP_TIME_BIRTH_SAME_TIME ?
                DScalarDataList{0,1,2,0,2}:DScalarDataList{0,0,1,2,2};
            const DAUIntList expectedIds=order==DP_TIME_BIRTH_SAME_TIME ?
                DAUIntList{0,0,0,1,1}:DAUIntList{0,1,0,1,0};
            assert(fieldValues<float>(shiftOutput,"time")==expectedTimes);
            assert(fieldValues<uint32_t>(shiftOutput,"pid")==expectedIds);
            for(unsigned i=0;i<5;++i) {
                assert(shiftOutput.points[i].x==2-expectedTimes[i]);
                assert(shiftOutput.points[i].y==expectedIds[i]);
            }
            for(const auto& t:queriedTimes)assert(t.unit()==MTime::kNTSCFrame);
            assert(activeTime.value()==2 && activeTime.unit()==MTime::kNTSCFrame);
        }
    }
    setup(0);
    MPointArray points;DScalarDataList times;
    assert(DGetPlugTimeDPPP(activeTime,points,ShiftPlug({},DPTimeShift::inDP),{.5f,1.5f},1,&times,nullptr));
    assert(points.size()==2&&points[0].x==1.5&&points[1].x==.5&&times==DScalarDataList({.5f,1.5f}));
    setup(0);resizeSnapshot(1,1);points.clear();times.clear();
    assert(DGetPlugTimeDPPP(activeTime,points,ShiftPlug({},DPTimeShift::inDP),{.5f,1.5f},1,&times,nullptr));
    assert(points.empty()&&times.empty()&&activeTime.value()==2);
}

void outputOrdering() {
    std::vector<std::vector<unsigned>> cases{{}, {0}, {0,0,0}, {1}, {2,0,3},
        {4,4,4}, {5,1,2,0}, {0,1,2,5}, {3,0,1,4,2}};
    std::vector<unsigned> skewed(256U, 0U);
    skewed[127U] = 1024U;
    for (unsigned group = 0U; group < skewed.size(); group += 3U) skewed[group] = 1U;
    cases.push_back(skewed);
    for (const auto& lengths : cases) {
        for (int mode : {DP_TIME_BIRTH_SAME_INDEX, DP_TIME_BIRTH_SAME_TIME}) {
            fixture();
            DPTimeShiftParams params;
            params.m_birth_type = mode;
            params.m_is_time_attr = params.m_is_parent_id_attr = true;
            params.m_time_attr_name = "time"; params.m_parent_id_attr_name = "pid";
            std::vector<MPointArray> groups(lengths.size());
            DScalarDataList times;
            DAUIntList ids;
            unsigned longest = 0U;
            for (unsigned group = 0U; group < lengths.size(); ++group) {
                longest = std::max(longest, lengths[group]);
                for (unsigned item = 0U; item < lengths[group]; ++item) {
                    groups[group].push_back(MPoint(group, item, times.size()));
                    times.push_back(float(times.size())); ids.push_back(group);
                }
            }
            ShiftPointList reference;
            if (mode == DP_TIME_BIRTH_SAME_INDEX) {
                for (unsigned item = 0U; item < longest; ++item)
                    for (unsigned group = 0U; group < groups.size(); ++group)
                        if (item < lengths[group]) reference.push_back(groups[group][item]);
            } else {
                for (const auto& group : groups) reference.insert(reference.end(), group.begin(), group.end());
            }
            ShiftBlock block;
            MStatus status = MS::kFailure;
            try {
                status = finishTimeShiftOutput(block, ShiftPlug({},DPTimeShift::outDP),
                    DPTimeShift::outDP, params, groups, &times, &ids);
            } catch (const std::exception& error) {
                std::cerr << "TimeShift output ordering: " << error.what() << '\n';
                assert(false && "TimeShift output ordering threw");
            }
            assert(status == MS::kSuccess);
            assert(published == 1 && shiftOutput.points.size() == reference.size());
            if (groups.empty()) assert(shiftOutput.fields.empty());
            else {
                const auto& outputTimes = fieldValues<float>(shiftOutput,"time");
                const auto& outputIds = fieldValues<uint32_t>(shiftOutput,"pid");
                for (unsigned index = 0U; index < reference.size(); ++index) {
                    const auto& expected = reference[index];
                    const auto& actual = shiftOutput.points[index];
                    assert(actual.x == expected.x && actual.y == expected.y && actual.z == expected.z);
                    assert(outputTimes[index] == expected.z && outputIds[index] == expected.x);
                }
            }
        }
    }
    // Keep the historical conservative work rejection, even though the new
    // traversal need not visit empty slots. Storage alone would admit this.
    fixture();
    DPTimeShiftParams params;
    std::vector<MPointArray> groups(1001U);
    groups[500U].resize(100000U);
    assert(dommy::sampling::validTimeStorage(100000U, groups.size()));
    ShiftPointList points;
    bool rejected = false;
    try { generateFinalPointList(params, groups, points); }
    catch (const std::length_error&) { rejected = true; }
    assert(rejected && points.empty());
    ShiftData data; data.points.resize(100000U);
    rejected = false;
    try { generateOutputAttributeList(params, data, groups, nullptr, nullptr); }
    catch (const std::length_error&) { rejected = true; }
    assert(rejected && data.fields.empty());
    params.m_birth_type = DP_TIME_BIRTH_SAME_TIME;
    generateFinalPointList(params, groups, points);
    assert(points.size() == 100000U);

    // Invalid coordinates may leave scratch partially filled, never published.
    fixture(); params.m_birth_type = DP_TIME_BIRTH_SAME_INDEX;
    groups = {{{0,0,0},{1,0,0}}, {}, {{2,0,0},{NAN,0,0}}};
    ShiftBlock block; rejected = false;
    try { (void)finishTimeShiftOutput(block, ShiftPlug({},DPTimeShift::outDP),
        DPTimeShift::outDP, params, groups, nullptr, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected); unchangedOutput();
}
void blockTimingContexts() {
    for(int source=0;source<4;++source) {
        setup(source,DP_TIME_LEN_DATA);
        inputs["inTimeLengthData"].connected=true;
        inputs["inTimeLengthData"].type=MFnData::kIntArray;
        inputs["inTimeLengthData"].object={"lengths"};
        scalarPayloads["lengths"]={2,2};
        assert(compute()==MS::kSuccess);
        const auto expectedTimes=fieldValues<float>(shiftOutput,"time");
        const auto expectedPoints=shiftOutput.points;
        reset();queriedTimes.clear();
        ShiftBlock block;
        for(const auto& entry:std::vector<std::pair<std::string,std::string>>{
                {"inTimeOffsetData","contextOffsets"},{"inTimeLengthData","contextLengths"}}) {
            auto input=inputs.at(entry.first);
            input.connected=true;input.type=MFnData::kIntArray;input.object={entry.second};
            inputs[entry.first].connected=true;
            block.contextInputs[entry.first]=input;
        }
        scalarPayloads["contextOffsets"]={0,0};scalarPayloads["contextLengths"]={2,2};
        // The global plug payload deliberately disagrees with the compute block.
        scalarPayloads["offsets"]={99,99};
        scalarPayloads["lengths"]={99,99};
        DPTimeShift node;
        assert(node.compute(ShiftPlug({},DPTimeShift::outDP),block)==MS::kSuccess);
        assert(fieldValues<float>(shiftOutput,"time")==expectedTimes);
        assert(shiftOutput.points.size()==expectedPoints.size());
        for(std::size_t i=0;i<expectedPoints.size();++i)
            assert(shiftOutput.points[i].x==expectedPoints[i].x &&
                   shiftOutput.points[i].y==expectedPoints[i].y);
    }
}
void pointAttributeAdmission() {
    const DScalarDataList expected{0,1,2,0,1,2};
    for (const std::string& name : {"offset", "length"}) {
        std::vector<DPAttributeData::Ptr> invalid{
            std::make_shared<DPStringAttr>(std::vector<std::string>{"x","y"}),
            std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{{1,2,3},{4,5,6}}),
            std::make_shared<DPBoolAttr>(std::vector<bool>{true,false}),
            std::make_shared<DPMat4Attr>(std::vector<MMatrix>(2))};
        for (const auto& field : invalid) {
            setup(0,DP_TIME_LEN_ATTRIBUTE); timePoints[2].fields[name]=field;
            assert(compute()==MS::kInvalidParameter); unchangedOutput();
            assert(queriedTimes.empty());
            timePoints[2].fields.erase(name); reset();
            assert(compute()==MS::kSuccess && fieldValues<float>(shiftOutput,"time")==expected);
        }
        std::vector<DPAttributeData::Ptr> valid{
            std::make_shared<DPIntAttr>(std::vector<int32_t>(2,name=="length"?2:0)),
            std::make_shared<DPUInt32Attr>(std::vector<uint32_t>(2,name=="length"?2:0)),
            std::make_shared<DPInt64Attr>(std::vector<int64_t>(2,name=="length"?2:0)),
            std::make_shared<DPFloatAttr>(std::vector<float>(2,name=="length"?2:0)),
            std::make_shared<DPDoubleAttr>(std::vector<double>(2,name=="length"?2:0))};
        for (const auto& field : valid) {
            setup(0,DP_TIME_LEN_ATTRIBUTE); timePoints[2].fields[name]=field;
            // Both present fields differ from the scalar defaults, so accepting
            // the type must actually read it rather than silently keep defaults.
            inputs["TimeOffset"].number=1; inputs["TimeLength"].number=1;
            assert(compute()==MS::kSuccess && fieldValues<float>(shiftOutput,"time")==expected);
            assert(timePoints[2].fields.at(name)==field);
        }
    }
    setup(0,DP_TIME_LEN_ATTRIBUTE); resizeSnapshot(2,0); timePoints[2].fields.clear();
    timePoints[2].fields["offset"]=std::make_shared<DPStringAttr>(std::vector<std::string>{});
    assert(compute()==MS::kSuccess && shiftOutput.points.empty());
    setup(0); assert(compute()==MS::kSuccess); const auto baseline=operations;
    for (const std::string& name : {"input inDP","plugin.read"}) {
        const auto found=std::find(baseline.begin(),baseline.end(),name); assert(found!=baseline.end());
        const int index=std::distance(baseline.begin(),found);
        for (auto code : {MS::kFailure,MS::kInvalidParameter}) {
            setup(0);failAt=index;injectedStatus=code;
            assert(compute()==code && callCount==index+1);unchangedOutput();
            reset();assert(compute()==MS::kSuccess);
        }
    }
    setup(0); inputs["inDP"].plugin=nullptr;
    assert(compute()==MS::kInvalidParameter);unchangedOutput();
    setup(0); timePoints[2].points[0].x=NAN;
    assert(compute()==MS::kInvalidParameter);unchangedOutput();
}
int failures() {
    int tested=0;
    for(int source=0;source<4;++source)
        for(int mode:{DP_TIME_LEN,DP_TIME_LEN_DATA,DP_TIME_LEN_RAMP,DP_TIME_LEN_FRAME,DP_TIME_LEN_ATTRIBUTE}) {
            setup(source,mode);assert(compute()==MS::kSuccess);const auto trace=operations;
            for(unsigned i=0;i<trace.size();++i) {
                for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
                    setup(source,mode);failAt=i;injectedStatus=status;
                    const auto result=compute();
                    if(result==MS::kSuccess||callCount!=int(i)+1)
                        std::cerr<<"failure source="<<source<<" mode="<<mode<<" operation="<<trace[i]<<" index="<<i<<" calls="<<callCount<<'\n';
                    assert(result!=MS::kSuccess && callCount==int(i)+1);
                    if(trace[i]=="clean")assert(published==1);
                    else unchangedOutput();
                    assert(activeTime.value()==2&&activeTime.unit()==MTime::kNTSCFrame);
                    ++tested;
                }
                setup(source,mode);throwAt=i;
                assert(compute()!=MS::kSuccess);
                if(trace[i]=="clean")assert(published==1);
                else unchangedOutput();
                assert(activeTime.value()==2);
                ++tested;
                if(trace[i]=="mesh.points"||trace[i]=="curve.cvs"||trace[i]=="surface.cvs"||
                   trace[i]=="array.copy"||trace[i]=="array.resize"||trace[i]=="scalar.array") {
                    setup(source,mode);shortAt=i;
                    assert(compute()!=MS::kSuccess && shortApplied);unchangedOutput();++tested;
                }
            }
        }
    return tested;
}
void boundaries() {
    for(const auto& invalid:std::vector<std::pair<std::string,double>>{
            {"TimeSampling",0},{"TimeSampling",1000001},{"TimeOffset",INFINITY},
            {"TimeLength",NAN},{"TimeLengthRandom",std::numeric_limits<float>::max()},{"Seed",-1}}) {
        setup(0);inputs[invalid.first].number=invalid.second;
        assert(compute()!=MS::kSuccess);unchangedOutput();assert(queriedTimes.empty());
    }
    for(int source=0;source<4;++source) {
        setup(source);shortPointStorage=true;assert(compute()!=MS::kSuccess);unchangedOutput();
        setup(source);for(int t=0;t<3;++t)resizeSnapshot(t,0);
        assert(compute()==MS::kSuccess&&shiftOutput.points.empty());
        setup(source,source?DP_TIME_LEN_DATA:DP_TIME_LEN_ATTRIBUTE);resizeSnapshot(2,0);
        if(!source)timePoints[2].fields.clear();
        assert(compute()==MS::kSuccess&&shiftOutput.points.empty()&&shiftOutput.fields.empty());
    }
    for(int source=1;source<4;++source) {
        setup(source);
        geometries["sample2"].reportedCvCount=3000000;
        emitterMeshes["sample2"].pointCount=3000000;
        assert(compute()!=MS::kSuccess);unchangedOutput();
        assert(std::find(operations.begin(),operations.end(),"mesh.points")==operations.end());
        assert(std::find(operations.begin(),operations.end(),"curve.cvs")==operations.end());
        assert(std::find(operations.begin(),operations.end(),"surface.cvs")==operations.end());
    }
    setup(0,DP_TIME_LEN_DATA);scalarReportedCount=3000000;
    assert(compute()!=MS::kSuccess);unchangedOutput();
    assert(std::find(operations.begin(),operations.end(),"scalar.array")==operations.end());
    setup(0,DP_TIME_LEN_DATA);scalarPayloads["offsets"]={std::numeric_limits<double>::max()};
    assert(compute()!=MS::kSuccess);unchangedOutput();
    setup(0,DP_TIME_LEN_DATA);inputs["inTimeOffsetData"].type=MFnData::kMesh;
    assert(compute()!=MS::kSuccess);unchangedOutput();
    for(auto type:{MFnData::kFloatArray,MFnData::kDoubleArray,MFnData::kIntArray}) {
        setup(0,DP_TIME_LEN_DATA);inputs["inTimeOffsetData"].type=type;
        assert(compute()==MS::kSuccess&&shiftOutput.points.size()==6);
    }
    for(int source=1;source<4;++source) {
        setup(source,DP_TIME_LEN_DATA);
        geometries["sample2"].reportedCvCount=800000;
        emitterMeshes["sample2"].pointCount=800000;
        assert(compute()!=MS::kSuccess);unchangedOutput();
        assert(queriedTimes.size()==1); // Current geometry only; scalar inputs use the block.
    }
    using Reader=MStatus(*)(const MTime&,MPointArray&,const ShiftPlug&,
        const DScalarDataList&,uint,DScalarDataList*,dommy::sampling::TimeSamplingBudget*);
    const Reader readers[]{DGetPlugTimeDPPP,DGetPlugTimeMeshPP,DGetPlugTimeCurvePP,DGetPlugTimeNurbsPP};
    for(int source=0;source<4;++source) {
        setup(source);dommy::sampling::TimeSamplingBudget work;
        assert(work.readPoints(dommy::generation::kMaxWorkItemCount-2));
        MPointArray points;DScalarDataList times;
        assert(!readers[source](activeTime,points,ShiftPlug({},source?DPTimeShift::inObject:DPTimeShift::inDP),
                               {0},0,&times,&work));
        assert(points.empty()&&times.empty()&&queriedTimes.size()==1);
        assert(activeTime.value()==2);
    }
    DPTimeShiftParams params;params.m_is_time_attr=true;
    ShiftData data;data.points={{0,0,0}};DScalarDataList values;
    bool rejected=false;
    try{generateOutputAttributeList(params,data,{{{0,0,0}}},&values,nullptr);}catch(const std::exception&){rejected=true;}
    assert(rejected);
    dommy::sampling::TimeSamplingBudget storage;
    assert(storage.addPoints(2097151,1));assert(!storage.addPoints(1,0));assert(storage.addPoints(0,0));
    setup(0);std::vector<MMatrix> matrices;
    assert(DGetPlugTimeMatrix(activeTime,matrices,ShiftPlug({},DPTimeShift::inObject),{0,1,2}));
    assert(matrices.size()==3&&activeTime.value()==2);
}
std::string numericContract() {
    std::ostringstream out;out<<std::setprecision(9);
    for(uint count:{1U,2U,3U,7U})for(float offset:{0.0f,2.5f})for(float random:{0.0f,.25f})
        for(int mode=0;mode<3;++mode) {
            DScalarDataList values;
            if(mode==0)getTimeValueArray(values,count,5,offset,7,random);
            if(mode==1)getFrameValueArray(values,count,offset,7,random);
            if(mode==2)assert(getTimeRampValueArray(values,ShiftRamp(),count,5,offset,7,random));
            // Compare the historical float RNG on this runtime; standard-library
            // distribution sequences are not a cross-platform contract.
            std::mt19937 engine(7);
            std::uniform_real_distribution<float> distribution(-random,random);
            const float step=count>1?(mode==2?1.0f:5.0f)/(count-1):0.0f;
            for(uint i=0;i<count;++i) {
                const float jitter=random==0?0:distribution(engine);
                float expected=0;
                if(mode==0)expected=(count==1?0.0f:step*i)+offset+jitter;
                if(mode==1)expected=(count==2&&i==0)?std::trunc(offset)+jitter:
                    (count<=2?count:i)+std::trunc(offset)+std::trunc(jitter);
                if(mode==2)expected=5.0f*(count==1?1.0f:step*i)+offset+jitter;
                assert(values[i]==expected);
            }
            if(random!=0.0f)continue;
            out<<mode<<' '<<count<<' '<<offset<<' '<<random;
            for(float value:values)out<<' '<<value;out<<'\n';
        }
    return out.str();
}
int main(int argc,char** argv) {
    const auto schema=schemaTrace("DPTimeShift",DPTimeShift::initialize);
    const int initCalls=callCount;
    if(argc==2&&std::string(argv[1])=="--print-schema"){std::cout<<schema;return 0;}
    if(argc==2&&std::string(argv[1])=="--print-numeric"){std::cout<<numericContract();return 0;}
    assert(argc==3);
    for(int i=1;i<3;++i){std::ifstream file(argv[i]);assert(file.good());
        const std::string expected((std::istreambuf_iterator<char>(file)),{});
        assert(expected==(i==1?schema:numericContract()));}
    const int initCount=testFailures("DPTimeShift",DPTimeShift::initialize,initCalls);
    numeric();
    fixture();assert(compute()==MS::kSuccess);
    assert(shiftOutput.points.size()==6&&fieldValues<float>(shiftOutput,"time")==DScalarDataList({0,0,1,1,2,2}));
    fixture();inputs["ParentIdAttrName"].string="time";
    assert(compute()==MS::kInvalidParameter);unchangedOutput();
    fixture();inputs["ParentIdAttrName"].string="time";inputs["IsAddParentIdAttr"].number=0;
    assert(compute()==MS::kSuccess&&shiftOutput.fields.size()==1);
    fixture();inputs["TimeAttrName"].string="";
    assert(compute()==MS::kSuccess&&shiftOutput.fields.size()==1&&shiftOutput.getAttr("pid"));
    fixture();inputs["TimeAttrName"].string.assign(dommy::generation::kMaxGeneratedStringByteCount+1U,'x');
    assert(compute()==MS::kInvalidParameter);unchangedOutput();
    for(int order:{DP_TIME_BIRTH_SAME_INDEX,DP_TIME_BIRTH_SAME_TIME})for(bool ids:{false,true}) {
        fixture();inputs["PointOrderType"].number=order;
        rejectFloatField=!ids;rejectIdField=ids;
        assert(compute()==MS::kFailure);unchangedOutput();
    }
    setup(0,DP_TIME_LEN_ATTRIBUTE);resizeSnapshot(2,0);timePoints[2].fields.clear();
    inputs["ParentIdAttrName"].string="time";
    assert(compute()==MS::kSuccess&&shiftOutput.fields.empty());
    for(auto type:{MFnData::kMesh,MFnData::kNurbsCurve,MFnData::kNurbsSurface}) {
        fixture(true,type);assert(compute()==MS::kSuccess);assert(shiftOutput.points.size()==6);
    }
    scalarReaderBoundaries();
    pointAttributeAdmission();
    const int scalarFailureCount=scalarReaderFailures();
    const int admissionCount=scalarComputeAdmission();
    aligned();outputOrdering();blockTimingContexts();boundaries();const int failureCount=failures();
    std::cout<<"DPTimeShift contracts, scalar conversion, bounds, alignment and "
             <<failureCount+initCount+scalarFailureCount<<" injected failures passed; "
             <<admissionCount<<" invalid-input rejection/retry cases passed\n";
}
