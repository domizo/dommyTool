#pragma once

template <typename Particle>
void assertNucleusTypedResult(const Particle& particle, const DPData& input)
{
    if(input.attributes.count("vel"))
        assert(particle.velocities==MFloatPointArray(2,MFloatPoint(2,3,4,0)));
    if(input.attributes.count("mass"))assert(particle.masses==DScalarDataList(2,2));
    if(input.attributes.count("pscale"))assert(particle.radii==DScalarDataList(2,2));
}

// Shared cases run through both complete setup bodies with real typed data.
template <typename Configure>
int testNucleusAttributeAdmission(Configure configure)
{
    int cases = 0;
    const auto check = [&](const char* name, const DPAttributeData::Ptr& attribute, bool accepted) {
        resetCalls(); shortArrayAt=-1; numericInputs.clear(); stringInputs.clear();
        DPData input;
        input.scalars["pscale"]={.25f,.5f}; input.scalars["mass"]={1,2};
        input.attributes[name]=attribute;
        const MStatus result=configure(input);
        assert(accepted ? result==MS::kSuccess : result==MS::kInvalidParameter);
        assert(std::count(operations.begin(),operations.end(),"native.topology")==int(accepted));
        assert(input.attributes.at(name)==attribute); // Borrowed data was not replaced.
        ++cases;
    };
    for(const auto& value : std::vector<DPAttributeData::Ptr>{
            std::make_shared<DPVec3iAttr>(std::vector<Imath::V3i>(2,Imath::V3i(2,3,4))),
            std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>(2,Imath::V3f(2,3,4))),
            std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>(2,Imath::V3d(2,3,4)))}) {
        check("vel",value,true);
        check("mass",value,false); check("pscale",value,false);
    }
    for(const auto& value : std::vector<DPAttributeData::Ptr>{
            std::make_shared<DPIntAttr>(std::vector<int32_t>(2,2)),
            std::make_shared<DPUInt32Attr>(std::vector<uint32_t>(2,2)),
            std::make_shared<DPInt64Attr>(std::vector<int64_t>(2,2)),
            std::make_shared<DPFloatAttr>(std::vector<float>(2,2)),
            std::make_shared<DPDoubleAttr>(std::vector<double>(2,2))}) {
        check("vel",value,false);
        check("mass",value,true); check("pscale",value,true);
    }
    for(const auto& value : std::vector<DPAttributeData::Ptr>{nullptr,
            std::make_shared<DPBoolAttr>(std::vector<bool>(2,true)),
            std::make_shared<DPStringAttr>(std::vector<std::string>(2,"2")),
            std::make_shared<DPVec2fAttr>(std::vector<Imath::V2f>(2,Imath::V2f(2,3)))}) {
        for(const char* name:{"vel","mass","pscale"})check(name,value,false);
    }
    for(std::size_t count:{0U,1U,3U}) {
        check("vel",std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>(count,Imath::V3f(1,2,3))),false);
        for(const char* name:{"mass","pscale"})
            check(name,std::make_shared<DPDoubleAttr>(std::vector<double>(count,2)),false);
    }
    for(double value:{std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
                      double(std::numeric_limits<float>::max())*2}) {
        for(unsigned axis=0;axis<3;++axis) {
            Imath::V3d vector(1,2,3);vector[axis]=value;
            check("vel",std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>(2,vector)),false);
        }
        for(const char* name:{"mass","pscale"})
            check(name,std::make_shared<DPDoubleAttr>(std::vector<double>(2,value)),false);
    }
    return cases;
}
