#include "polygon_generation_doubles.h"
#include "polygon_generation.inc"
#include <functional>

namespace {
void reset() {
    calls.clear();failAt=throwAt=shortAt=-1;
    invalidAccesses=bulkReads=creates=0;
    normalWrites=pointWrites=matrixReads=0;
    for(auto& input:normalInputs)input=nullptr;
    shortened=nullData=nullMesh=false;geometry=Geometry();
}
MStatus sphere(int detail,MObject& output) {
    try {
        DMeshSphere generated(detail);
        if(generated.meshObj.isNull())return MS::kFailure;
        output=generated.meshObj;
        return MS::kSuccess;
    } catch(const std::exception&) {return MS::kFailure;}
}
MStatus read(meshPrimData& output) {
    try {
        MStatus status;MFnMesh source(MObject(1),&status);
        if(!status)return status;
        return getPrimDataFromMesh(source,output);
    } catch(const std::exception&) {return MS::kFailure;}
}
void checkSphere() {
    assert(geometry.connections.size()==geometry.faces.size()*3U);
    for(const auto& p:geometry.points)assert(std::abs(p.x*p.x+p.y*p.y+p.z*p.z-1.0)<1.0e-6);
    for(int index:geometry.connections)assert(index>=0 && std::size_t(index)<geometry.points.size());
    for(std::size_t i=0;i<geometry.faces.size();++i) {
        assert(geometry.faces[i]==3);
        const auto& a=geometry.points[geometry.connections[i*3]];
        const auto& b=geometry.points[geometry.connections[i*3+1]];
        const auto& c=geometry.points[geometry.connections[i*3+2]];
        const double x=(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y);
        const double y=(b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z);
        const double z=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        assert(x*a.x+y*a.y+z*a.z>0.0); // Outward, nondegenerate winding.
    }
}
std::size_t injected=0,shortenedCases=0;
void failures(const std::function<MStatus()>& action) {
    reset();assert(action());const auto trace=calls;
    for(int mode=0;mode<2;++mode)for(std::size_t i=0;i<trace.size();++i) {
        reset();(mode==0?failAt:throwAt)=int(i);
        if(action()){std::cerr<<"Missed failure: "<<trace[i]<<" "<<i<<"\n";assert(false);}
        assert(invalidAccesses==0);
        reset();assert(action());assert(invalidAccesses==0);++injected;
    }
}
meshPrimData primitive() {
    meshPrimData result;
    result.m_pts=geometry.points;result.m_faces=geometry.faces;result.m_connects=geometry.connections;
    result.m_obj=MObject(11);result.hasExplicitNormals=true;
    result.surfaceGroup=result.interiorGroup=result.seamGroup={true,false};
    result.seamPointGroup={true,false,true,false};return result;
}
void unchanged(const meshPrimData& data) {
    assert(data.m_obj.id==11 && data.hasExplicitNormals);
    assert(data.m_pts.size()==geometry.points.size() && data.m_faces==geometry.faces && data.m_connects==geometry.connections);
    for(unsigned i=0;i<data.m_pts.size();++i) {
        const auto& a=data.m_pts[i];const auto& b=geometry.points[i];
        assert(a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w);
    }
    assert((data.surfaceGroup==std::vector<bool>{true,false}));
    assert(data.interiorGroup==data.surfaceGroup && data.seamGroup==data.surfaceGroup);
    assert((data.seamPointGroup==std::vector<bool>{true,false,true,false}));
}
MStatus transform(meshPrimData& source,const MTransformationMatrix& matrix,meshPrimData& output) {
    try {return source.duplicateTransform(matrix,output);}
    catch(const std::exception&) {return MS::kFailure;}
}
MStatus normals(meshPrimData& source,MVectorArray& values,MIntArray& faces,MIntArray& vertices) {
    try {return source.setNormalPerVtxPerFace(values,faces,vertices);}
    catch(const std::exception&) {return MS::kFailure;}
}
void testTransform() {
    for(int detail=1;detail<=5;++detail)for(int kind=0;kind<4;++kind) {
        reset();MObject object;assert(sphere(detail,object));
        auto source=primitive();auto output=primitive();MTransformationMatrix matrix;
        if(kind==1){matrix.matrix[0][0]=-2;matrix.matrix[1][1]=3;matrix.matrix[2][2]=0.5;}
        if(kind==2){matrix.matrix[0][0]=0;matrix.matrix[0][1]=1;matrix.matrix[1][0]=-1;matrix.matrix[1][1]=0;}
        if(kind==3){matrix.matrix[0][1]=0.25;matrix.matrix[2][2]=0;}
        matrix.matrix[3][0]=7;matrix.matrix[3][1]=-5;matrix.matrix[3][2]=2;
        assert(transform(source,matrix,output));unchanged(source);
        assert(matrixReads==1 && output.m_obj.isNull() && !output.hasExplicitNormals);
        assert(output.surfaceGroup.empty() && output.interiorGroup.empty() && output.seamGroup.empty() && output.seamPointGroup.empty());
        assert(output.m_faces==source.m_faces && output.m_connects==source.m_connects && output.m_pts.size()==source.m_pts.size());
        for(unsigned i=0;i<source.m_pts.size();++i) {
            const auto& p=source.m_pts[i];const auto& q=output.m_pts[i];
            assert(q.x==p.x*matrix.matrix[0][0]+p.y*matrix.matrix[1][0]+p.z*matrix.matrix[2][0]+7);
            assert(q.y==p.x*matrix.matrix[0][1]+p.y*matrix.matrix[1][1]+p.z*matrix.matrix[2][1]-5);
            assert(q.z==p.z*matrix.matrix[2][2]+2 && q.w==1);
        }
    }
    auto action=[] {
        auto source=primitive();auto output=primitive();MTransformationMatrix matrix;matrix.matrix[3][0]=2;
        const auto status=transform(source,matrix,output);unchanged(source);
        if(!status)assert(output.m_obj.id==11 && output.hasExplicitNormals);
        return status;
    };
    failures(action);
    reset();assert(action());const auto trace=calls;
    for(unsigned i=0;i<trace.size();++i) {
        reset();shortAt=int(i);assert(!action());assert(shortened && !invalidAccesses);++shortenedCases;
        reset();assert(action());
    }
    for(int bad=0;bad<7;++bad) {
        reset();auto source=primitive();auto output=primitive();MTransformationMatrix matrix;
        if(bad==0)matrix.matrix[1][1]=NAN;
        if(bad==1)source.m_pts[0].x=INFINITY;
        if(bad==2)source.m_pts[0].w=NAN;
        if(bad==3)matrix.matrix[3][0]=std::numeric_limits<double>::max();
        if(bad==4)source.m_pts.reportedLength=2100000;
        if(bad==5)source.m_faces.reportedLength=2000001;
        if(bad==6)source.m_connects.reportedLength=100000001;
        assert(!transform(source,matrix,output));unchanged(output);
        assert(!invalidAccesses);if(bad==0 || bad>=4)assert(calls.empty());
    }
    reset();auto source=primitive();MTransformationMatrix matrix;
    assert(!transform(source,matrix,source));unchanged(source);assert(calls.empty());
    meshPrimData empty,output;assert(transform(empty,matrix,output));assert(output.m_pts.empty());
    source.m_pts[0].w=2;matrix.matrix[3][0]=7;
    assert(transform(source,matrix,output));assert(output.m_pts[0].x==14 && output.m_pts[0].w==2);
}
void testNormalsAndPointUpdate() {
    auto action=[] {
        meshPrimData source;source.m_obj=MObject(1);
        MVectorArray values{{0,0,1},{0,1,0}};MIntArray faces{0,1},vertices{0,3};
        const auto status=normals(source,values,faces,vertices);
        assert(source.hasExplicitNormals==bool(status));
        if(status)assert(normalInputs[0]==&values && normalInputs[1]==&faces && normalInputs[2]==&vertices);
        else assert(!normalWrites);
        return status;
    };
    failures(action);
    for(int bad=0;bad<11;++bad) {
        reset();meshPrimData source;MVectorArray values{{0,0,1},{0,1,0}};
        MIntArray faces{0,1},vertices{0,3};
        if(bad==0)values.pop_back();
        if(bad==1)faces.pop_back();
        if(bad==2)vertices.pop_back();
        if(bad==3)values[1].x=NAN;
        if(bad==4)values[1].y=std::numeric_limits<double>::max();
        if(bad==5)faces[1]=-1;
        if(bad==6)vertices[1]=-1;
        if(bad==7)faces[1]=2;
        if(bad==8)vertices[1]=4;
        if(bad==9)values.reportedLength=faces.reportedLength=vertices.reportedLength=100000001;
        if(bad==10)geometry.reportedFaces=-2;
        assert(!normals(source,values,faces,vertices));assert(!source.hasExplicitNormals && !normalWrites && !invalidAccesses);
        if(bad<7 || bad==9)assert(calls.empty());
        reset();assert(action());
    }
    reset();meshPrimData source;MVectorArray values;MIntArray faces,vertices;
    assert(normals(source,values,faces,vertices) && source.hasExplicitNormals); // Legacy empty call.
    reset();failAt=0;assert(!normals(source,values,faces,vertices) && source.hasExplicitNormals);
    failures([] {
        auto source=primitive();MStatus status;
        try {status=source.updatePoints();}catch(const std::exception&){status=MS::kFailure;}
        unchanged(source);return status;
    });
}
}
int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream fixture(argv[1]);assert(fixture);
    for(int detail=1;detail<=5;++detail) {
        reset();MObject result(99);assert(sphere(detail,result));assert(result.id==1);
        checkSphere();int recorded=0;std::size_t points=0,faces=0,connections=0;std::uint64_t fingerprint=0;
        fixture>>recorded>>points>>faces>>connections>>fingerprint;
        assert(fixture && recorded==detail && points==geometry.points.size() &&
            faces==geometry.faces.size() && connections==geometry.connections.size() &&
            fingerprint==geometryFingerprint());
        meshPrimData data;assert(read(data));
        assert(data.m_pts.size()==points && data.m_faces.size()==faces && data.m_connects.size()==connections);
    }
    for(int detail:{1,2}) {
        failures([=]{MObject result(99);const auto s=sphere(detail,result);if(!s)assert(result.id==99);return s;});
        reset();MObject result;assert(sphere(detail,result));const auto trace=calls;
        // Check successful-but-short append at seed, midpoint, face and connection boundaries.
        std::vector<std::size_t> selected;
        for(std::size_t i=0;i<trace.size();++i) {
            if(trace[i]!="point.append" && trace[i]!="int.append")continue;
            if(i==0 || trace[i]!=trace[i-1] || i==12 || i+1==trace.size() || trace[i]!=trace[i+1])selected.push_back(i);
        }
        for(auto i:selected) {
            reset();shortAt=int(i);result=MObject(99);
            assert(!sphere(detail,result));assert(shortened && result.id==99 && invalidAccesses==0);++shortenedCases;
            reset();assert(sphere(detail,result));
        }
    }
    failures([]{meshPrimData data;return read(data);});
    reset();meshPrimData data;assert(read(data));const auto trace=calls;
    for(std::size_t i=0;i<trace.size();++i) {
        if(trace[i]!="mesh.points" && trace[i]!="mesh.topology" && trace[i]!="point.copy" && trace[i]!="int.copy")continue;
        reset();shortAt=int(i);meshPrimData staged;
        assert(!read(staged));assert(shortened && invalidAccesses==0);++shortenedCases;
        reset();assert(read(staged));
    }
    for(int detail:{-1,0,6,1000000000}){reset();MObject result(99);assert(!sphere(detail,result));assert(calls.empty()&&result.id==99);}
    for(int which=0;which<2;++which){
        reset();(which?nullMesh:nullData)=true;MObject result(99);
        assert(!sphere(2,result));assert(result.id==99 && creates==(which?1:0));
    }
    for(int which=0;which<7;++which) {
        reset();
        if(which==0)geometry.reportedPoints=10000001;
        if(which==1)geometry.reportedFaces=2000001;
        if(which==2)geometry.reportedConnections=100000001;
        if(which==3)geometry.reportedPoints=2100000;
        if(which==4)geometry.reportedUVs=-2;
        if(which==5)geometry.reportedPoints=0;
        if(which==6)geometry.reportedFaces=-2;
        meshPrimData output;output.m_pts={{42,42,42}};
        assert(!read(output));assert(bulkReads==0 && output.m_pts.size()==1 && output.m_pts[0].x==42);
    }
    for(int which=0;which<9;++which) {
        reset();
        if(which==0)geometry.reportedPoints=5;
        if(which==1)geometry.reportedFaces=3;
        if(which==2)geometry.points[0].x=std::numeric_limits<double>::infinity();
        if(which==3)geometry.points[0].x=std::numeric_limits<double>::max();
        if(which==4)geometry.points[0].w=std::numeric_limits<double>::quiet_NaN();
        if(which==5)geometry.connections[0]=-1;
        if(which==6)geometry.connections[0]=4;
        if(which==7)geometry.faces[0]=2;
        if(which==8)geometry.faces[0]=4;
        meshPrimData output;output.m_pts={{42,42,42}};
        assert(!read(output));assert(output.m_pts.size()==1 && output.m_pts[0].x==42);
    }
    reset();meshPrimData reused;reused.hasExplicitNormals=true;reused.m_obj=MObject(99);
    reused.surfaceGroup=reused.interiorGroup=reused.seamGroup=reused.seamPointGroup={true};
    assert(read(reused));assert(!reused.hasExplicitNormals && reused.m_obj.isNull());
    assert(reused.surfaceGroup.empty() && reused.interiorGroup.empty() && reused.seamGroup.empty() && reused.seamPointGroup.empty());
    geometry.points.resize(3);geometry.faces={3};geometry.connections={0,1,2};
    assert(read(reused));assert(reused.m_pts.size()==3 && reused.m_faces.size()==1 && reused.m_connects.size()==3);
    testTransform();
    testNormalsAndPointUpdate();
    std::cout<<"Polygon generation: "<<injected<<" status/exception cases with recovery, "
             <<shortenedCases<<" short-array cases; all five historical geometry fixtures passed\n";
    std::cout<<"Primitive transform/normal write/update: actual declarations and bodies, 20 transform fixtures, borrowed normal arrays and invalid-input gates passed\n";
}
