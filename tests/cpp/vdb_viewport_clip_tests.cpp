// These affine doubles cover helper control flow, not native sparse VDB trees.
#include <array>
#include <cassert>
#include <cmath>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include "math/DBounds.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include "DVdbViewportMath.h"

#if defined(__clang__)
#pragma STDC FENV_ACCESS ON
#endif

namespace openvdb {
using Int32=std::int32_t;
struct Vec3d {
    std::array<double,3> values;
    explicit Vec3d(double value=0) : values{{value,value,value}} {}
    Vec3d(double x,double y,double z) : values{{x,y,z}} {}
    double& operator[](int axis) {return values.at(static_cast<std::size_t>(axis));}
    double operator[](int axis) const {return values.at(static_cast<std::size_t>(axis));}
};
Vec3d operator+(const Vec3d& a,const Vec3d& b) {return Vec3d(a[0]+b[0],a[1]+b[1],a[2]+b[2]);}
Vec3d operator-(const Vec3d& a,const Vec3d& b) {return Vec3d(a[0]-b[0],a[1]-b[1],a[2]-b[2]);}
struct Coord {
    std::array<Int32,3> values;
    Coord(Int32 x=0,Int32 y=0,Int32 z=0) : values{{x,y,z}} {}
    Int32 operator[](int axis) const {return values.at(static_cast<std::size_t>(axis));}
    Vec3d asVec3d() const {return Vec3d(values[0],values[1],values[2]);}
};
struct CoordBBox {
    Coord low,high;
    bool absent=true;
    CoordBBox()=default;
    CoordBBox(Coord a,Coord b) : low(a),high(b),absent(false) {}
    bool empty() const {return absent;}
    const Coord& min() const {return low;}
    const Coord& max() const {return high;}
    void reset(Coord a,Coord b) {low=a;high=b;absent=false;}
};
struct BBoxd {
    Vec3d low,high;
    bool absent=true;
    BBoxd()=default;
    BBoxd(Vec3d a,Vec3d b) : low(a),high(b),absent(false) {}
    bool empty() const {return absent || low[0]>high[0] || low[1]>high[1] || low[2]>high[2];}
    const Vec3d& min() const {return low;}
    const Vec3d& max() const {return high;}
};
struct Transform {
    double matrix[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    Vec3d translation;
    Vec3d indexToWorld(const Vec3d& p) const {
        Vec3d result=translation;
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) result[i]+=matrix[i][j]*p[j];
        return result;
    }
    Vec3d worldToIndex(const Vec3d& p) const {
        double cofactors[3][3];
        for(int i=0;i<3;++i) for(int j=0;j<3;++j)
            cofactors[i][j]=matrix[(i+1)%3][(j+1)%3]*matrix[(i+2)%3][(j+2)%3]-
                             matrix[(i+1)%3][(j+2)%3]*matrix[(i+2)%3][(j+1)%3];
        const double determinant=matrix[0][0]*cofactors[0][0]+matrix[0][1]*cofactors[0][1]+matrix[0][2]*cofactors[0][2];
        Vec3d result;
        for(int i=0;i<3;++i) for(int j=0;j<3;++j)
            result[i]+=cofactors[j][i]*(p[j]-translation[j])/determinant;
        return result;
    }
    BBoxd indexToWorld(const BBoxd& box) const {
        Vec3d low(std::numeric_limits<double>::max()),high(-std::numeric_limits<double>::max());
        for(int corner=0;corner<8;++corner) {
            Vec3d point;
            for(int axis=0;axis<3;++axis) point[axis]=(corner&(1<<axis)) ? box.max()[axis] : box.min()[axis];
            point=indexToWorld(point);
            for(int axis=0;axis<3;++axis) {
                if(!std::isfinite(point[axis])) return BBoxd(point,point);
                low[axis]=std::min(low[axis],point[axis]);high[axis]=std::max(high[axis],point[axis]);
            }
        }
        return BBoxd(low,high);
    }
};
namespace math {
void calculateBounds(const Transform& transform,const Vec3d& first,const Vec3d& second,
                     Vec3d& low,Vec3d& high) {
    low=Vec3d(std::numeric_limits<double>::max());high=Vec3d(-std::numeric_limits<double>::max());
    for(int corner=0;corner<8;++corner) {
        Vec3d p;
        for(int axis=0;axis<3;++axis) p[axis]=(corner&(1<<axis)) ? second[axis] : first[axis];
        p=transform.worldToIndex(p);
        for(int axis=0;axis<3;++axis) {
            if(!std::isfinite(p[axis])) {low=high=p;return;}
            low[axis]=std::min(low[axis],p[axis]);high[axis]=std::max(high[axis],p[axis]);
        }
    }
}
}
struct GridBase {
    CoordBBox bounds;
    Transform map;
    bool throwBounds=false;
    bool empty() const {return bounds.empty();}
    CoordBBox evalActiveVoxelBoundingBox() const {
        if(throwBounds) throw std::runtime_error("bounds");
        return bounds;
    }
    const Transform& transform() const {return map;}
};
}
using GridCPtrVec=std::vector<std::shared_ptr<const openvdb::GridBase>>;
using GridCPtrVecCIter=GridCPtrVec::const_iterator;
#include "vdb_viewport_clip.inc"

bool near(double a,double b) {return std::fabs(a-b)<1e-6;}
std::shared_ptr<openvdb::GridBase> makeGrid() {
    auto grid=std::make_shared<openvdb::GridBase>();
    grid->bounds=openvdb::CoordBBox(openvdb::Coord(0,0,0),openvdb::Coord(10,20,30));
    return grid;
}
void clipTests() {
    auto grid=makeGrid();openvdb::BBoxd box;bool hasBounds=false;
    for(int axis=0;axis<3;++axis) for(float position : {0.0f,.5f,1.0f}) {
        assert(buildVdbViewportClipBox({grid},axis,position,.2f,box,hasBounds) && hasBounds);
        const float length=static_cast<float>(grid->bounds.max()[axis])+.8f;
        const double center=grid->bounds.max()[axis]*.5;
        assert(near(box.min()[axis],-length*.5+length*position+center-.1f));
        assert(near(box.max()[axis],-length*.5+length*position+center+.1f));
        for(int component=0;component<3;++component) if(component!=axis) {
            assert(near(box.min()[component],grid->bounds.max()[component]*.5-32));
            assert(near(box.max()[component],grid->bounds.max()[component]*.5+32));
        }
    }
    openvdb::BBoxd complete,centers;
    assert(getActiveVoxelWorldBounds(*grid,complete));
    assert(getActiveVoxelWorldBounds(*grid,centers,false));
    assert(complete.min()[0]==-.5 && complete.max()[0]==10.5);
    assert(centers.min()[0]==0 && centers.max()[0]==10);

    grid->bounds=openvdb::CoordBBox(openvdb::Coord(0,0,0),openvdb::Coord(1,1,1));
    const double c=std::sqrt(.5);
    grid->map.matrix[0][0]=grid->map.matrix[1][1]=c;
    grid->map.matrix[0][1]=-c;grid->map.matrix[1][0]=c;
    assert(getActiveVoxelWorldBounds(*grid,centers,false));
    assert(near(centers.min()[0],-c) && near(centers.max()[0],c));
    assert(buildVdbViewportClipBox({grid},0,0,.2f,box,hasBounds));
    assert(near(box.min()[0],-double(float(2*c)+.8f)*.5-.1f));
    grid->map=openvdb::Transform();grid->map.matrix[0][1]=-2;
    assert(getActiveVoxelWorldBounds(*grid,centers,false));
    assert(centers.min()[0]==-2 && centers.max()[0]==1);
    grid->map.matrix[0][0]=-3;
    assert(getActiveVoxelWorldBounds(*grid,centers,false));
    assert(centers.min()[0]==-5 && centers.max()[0]==0);
    grid->map.translation=openvdb::Vec3d(10,-20,30);
    auto other=makeGrid();other->map.translation=openvdb::Vec3d(-50,0,0);
    assert(buildVdbViewportClipBox({grid,other},0,.5f,.2f,box,hasBounds));
    assert(near((box.min()[0]+box.max()[0])*.5,-20));

    auto rejects=[&](const GridCPtrVec& grids,int axis=0,float position=.5f,float width=.2f) {
        box=openvdb::BBoxd(openvdb::Vec3d(91),openvdb::Vec3d(92));hasBounds=true;
        assert(!buildVdbViewportClipBox(grids,axis,position,width,box,hasBounds));
        assert(box.min()[0]==91 && box.max()[2]==92 && hasBounds);
    };
    rejects({grid},-1);rejects({grid},3);rejects({grid},0,-.1f);rejects({grid},0,1.1f);
    for(float value : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity()}) {
        rejects({grid},0,value);rejects({grid},0,.5f,value);
    }
    rejects({grid},0,.5f,0);rejects({nullptr});
    rejects(GridCPtrVec(dommy::vdb::kMaxVdbGridCount+1U,other));
    other->map.matrix[0][0]=1e40;rejects({grid,other});
    other->map.matrix[0][0]=std::numeric_limits<double>::infinity();rejects({other});
    other->map=openvdb::Transform();other->map.matrix[0][0]=1e-12;rejects({other});
    other->map=openvdb::Transform();other->map.translation[0]=std::numeric_limits<double>::quiet_NaN();rejects({other});
    other->map=openvdb::Transform();other->map.matrix[0][0]=0;rejects({other});
    other->bounds=openvdb::CoordBBox(openvdb::Coord(std::numeric_limits<int>::max()-100,0,0),
                                    openvdb::Coord(std::numeric_limits<int>::max()-10,1,1));
    rejects({other});
    other->bounds=openvdb::CoordBBox();box=openvdb::BBoxd(openvdb::Vec3d(91),openvdb::Vec3d(92));
    assert(buildVdbViewportClipBox({other},0,.5f,.2f,box,hasBounds) && !hasBounds);
    assert(box.min()[0]==91 && box.max()[0]==92);
    grid->throwBounds=true;
    try {buildVdbViewportClipBox({grid},0,.5f,.2f,box,hasBounds);assert(false);} catch(const std::runtime_error&) {}
    assert(box.min()[0]==91 && box.max()[0]==92);
}
void numericTests() {
    using dommy::vdb::viewportClipBounds;
    std::array<double,3> low={{0,0,0}},high={{10,20,30}},a={{91,91,91}},b={{92,92,92}};
    high[0]=1e40;assert(!viewportClipBounds(low,high,0,.5f,.2f,a,b));assert(a[0]==91 && b[0]==92);
    const double maximum=std::numeric_limits<float>::max();
    low[0]=-maximum;high[0]=maximum;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(!viewportClipBounds(low,high,0,.5f,.2f,a,b));
    assert(std::fetestexcept(FE_OVERFLOW)==0); // Reject before a narrowing can raise overflow.
    low={{0,0,0}};high={{10,20,30}};
    assert(viewportClipBounds(low,high,2,.5f,.2f,a,b));
    high[2]=std::numeric_limits<double>::quiet_NaN();const auto oldA=a,oldB=b;
    assert(!viewportClipBounds(low,high,2,.5f,.2f,a,b));assert(a==oldA && b==oldB);
    high[2]=-1;assert(!viewportClipBounds(low,high,2,.5f,.2f,a,b));
}
int main() {clipTests();numericTests();std::cout<<"Viewport clip: affine bounds, legacy padding, numeric/index admission and staged failure passed (API-shaped OpenVDB boundaries)\n";}
