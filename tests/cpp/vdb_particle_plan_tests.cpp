#include "vdb_distribution_doubles.h"
#include "modules/volume/VDB/Common/DVdbParticleRasterPlan.h"
#include "vdb_particle_plan.inc"

static void checkTrailProgress() {
    using namespace dommy::vdb;
    ParticleRasterParameters p;
    p.voxelSize=1;p.halfBandVoxels=3;p.radiusScale=1;
    p.velocityMultiplier=1;p.rasterizeTrails=true;
    int checked=0;
    for(double start:{-999000.,0.,999000.})
    for(float radius:{2.f,8.f,32.f})
    for(float minimum:{.01f,1.f,2.f})
    for(float spacing:{.05f,.2f,1.f,2.f,16.f})
    for(float speed:{.001f,.1f,1.f,5.f}) {
        p.minimumRadiusVoxels=minimum;p.velocitySpacing=spacing;
        std::size_t work=0;
        if(!testedParticleRasterWork(1,radius,speed,p,100000000,work))continue;
        const std::size_t side=static_cast<std::size_t>(2*std::ceil(radius+3+speed)+1);
        const std::size_t admittedSteps=work/(side*side*side);
        double position=start;float distance=0,currentRadius=radius;
        const float inverseSpeed=1.f/speed;
        std::size_t steps=0;
        // Scalar arithmetic of a 5.2 FloatGrid trail, not the raster kernel.
        while(distance<=speed) {
            assert(++steps<=admittedSteps);
            const double next=position-.5*spacing*currentRadius*double(speed)*inverseSpeed;
            assert(std::isfinite(next) && next<position);
            position=next;distance=static_cast<float>(std::fabs(position-start));
            currentRadius=radius-(radius-minimum)*distance*inverseSpeed;
        }
        ++checked;
    }
    assert(checked>100);
    std::cout<<"Bounded native trail arithmetic: "<<checked<<" admitted cases passed\n";
}

int main() {
    checkTrailProgress();
    using namespace dommy::vdb;
    ParticleRasterParameters p;
    p.voxelSize=1;p.halfBandVoxels=3;p.radiusScale=1;p.minimumRadiusVoxels=1.5;
    p.velocitySpacing=1;p.velocityMultiplier=1;
    ParticleInputExtrema input;
    assert(input.addPosition(openvdb::Vec3d(32,32,32)) && input.addRadius(1));
    TreeEditBudget budget;float background=-1;
    assert(planParticleConversion(1,input,p,1,0,budget,background));
    assert(background==3 && budget.bytes>0 && budget.work>0);
    auto twice=input;twice.addPosition(openvdb::Vec3d(32,32,32));TreeEditBudget doubleBudget;
    assert(planParticleConversion(2,twice,p,1,0,doubleBudget,background));
    assert(doubleBudget.bytes==budget.bytes+160 && doubleBudget.work>budget.work);
    for(double x:{1e7,1e10,2147483520.0,-2147483648.0}) {
        ParticleInputExtrema bad;bad.addPosition(openvdb::Vec3d(x,0,0));bad.addRadius(1);
        TreeEditBudget unchanged;float before=-1;
        assert(!planParticleConversion(1,bad,p,1,0,unchanged,before));
        assert(unchanged.bytes==0 && unchanged.work==0 && before==-1);
    }
    for(double voxel:{0.0,1e-10,1e7,double(std::numeric_limits<float>::max()),double(INFINITY),double(NAN)}) {
        auto invalid=p;invalid.voxelSize=voxel;TreeEditBudget b;
        assert(!planParticleConversion(1,input,invalid,1,0,b,background));
    }
    for(double band:{0.0,65.0,double(INFINITY),double(NAN)}) {
        auto invalid=p;invalid.halfBandVoxels=band;TreeEditBudget b;
        assert(!planParticleConversion(1,input,invalid,1,0,b,background));
    }
    ParticleInputExtrema spread;spread.addPosition(openvdb::Vec3d(-10000,0,0));
    spread.addPosition(openvdb::Vec3d(10000,10000,10000));spread.addRadius(1);
    TreeEditBudget b;
    b.bytes=kMaxVdbPayloadBytes/4-budget.bytes+1;
    assert(!planParticleConversion(1,input,p,1,0,b,background));
    b=TreeEditBudget();b.work=dommy::generation::kMaxWorkItemCount-budget.work+1;
    assert(!planParticleConversion(1,input,p,1,0,b,background));
    b=TreeEditBudget();
    assert(!planParticleConversion(2,spread,p,1,0,b,background));
    assert(!planParticleConversion(1,input,p,1,1024,b,background));
    b.bytes=kMaxVdbPayloadBytes/4;
    assert(!planParticleConversion(1,input,p,1,0,b,background));
    b=TreeEditBudget();b.work=dommy::generation::kMaxWorkItemCount;
    assert(!planParticleConversion(1,input,p,1,0,b,background));
    // The sampling coordinate contract intentionally remains full Int32.
    assert(isValidIndexCoordinate(INT_MAX));
    // The old admission accepted this stalled native trail step.
    ParticleInputExtrema moving;
    moving.addPosition(openvdb::Vec3d(32,32,32));
    moving.addRadius(2);moving.addVelocity(openvdb::Vec3d(1,0,0));
    auto trail=p;trail.rasterizeTrails=true;trail.minimumRadiusVoxels=2;
    TreeEditBudget normalTrail;
    assert(planParticleConversion(1,moving,trail,1,0,normalTrail,background));
    std::size_t normalWork=0,closerWork=0;
    assert(testedParticleRasterWork(1,2,1,trail,100000000,normalWork));
    trail.velocitySpacing=.5;
    assert(testedParticleRasterWork(1,2,1,trail,100000000,closerWork));
    assert(closerWork>normalWork);
    for(double spacing:{1e-20,1e-9,1e-6}) {
        trail.velocitySpacing=spacing;TreeEditBudget unchanged;float sentinel=-1;
        assert(!planParticleConversion(1,moving,trail,1,0,unchanged,sentinel));
        assert(unchanged.bytes==0 && unchanged.work==0 && sentinel==-1);
    }
    trail.velocitySpacing=1;
    for(double minimum:{0.,1e-9}) {
        trail.minimumRadiusVoxels=minimum;std::size_t sentinel=99;
        assert(!testedParticleRasterWork(1,2,1,trail,100000000,sentinel));
        assert(sentinel==99);
    }
    // Stationary particles and ordinary sphere mode remain valid.
    trail.minimumRadiusVoxels=2;
    assert(testedParticleRasterWork(1,2,0,trail,100000000,normalWork));
    // Isolate the precision guard from the iteration budget: stationary input
    // still must not admit a spacing that cannot advance an indexed position.
    trail.velocitySpacing=1e-20;
    assert(!testedParticleRasterWork(1,2,0,trail,100000000,normalWork));
    trail.velocitySpacing=1e9;trail.minimumRadiusVoxels=1e-9;
    assert(!testedParticleRasterWork(1,2,0,trail,100000000,normalWork));
    trail.rasterizeTrails=false;trail.minimumRadiusVoxels=0;trail.velocitySpacing=1e-20;
    assert(testedParticleRasterWork(1,2,1,trail,100000000,normalWork));
    std::cout<<"Particle admission: large coordinates, scale, bands, domain, copies and aggregate budgets passed\n";
}
