#pragma once

#include "../RigidBody/DBRigidBodyParameters.h"
#include "common/utility/DGenerationLimits.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>
#include <vector>

namespace dommy { namespace bullet {

// Also keeps Bullet 2.88's cluster seed (node index * 29873) in signed int.
const std::size_t kMaxSoftBodyVertices = 65536U;
const std::size_t kMaxSoftBodyFaces = 250000U;
const std::size_t kMaxSoftBodyLinks = 1000000U;
const std::size_t kMaxSoftBodyWork = 100000000U;
const int kMaxSoftClusterGenerationIterations = 256;

inline bool isSafeSoftBodyCollisionMargin(double margin) {
    return margin >= 0 && isSafeCollisionCoordinate(margin);
}

struct SoftBodyParameters {
    // Native constructor defaults; Maya supplies its own attribute defaults.
    SoftBodyParameters(float m = 100, float f = .5f, float margin = .04f,
                       std::size_t count = 100)
        : mass(m), friction(f), collisionMargin(margin), clusters(count) {}
    float mass, friction, collisionMargin;
    std::size_t clusters;
};

inline bool validSoftBodyParameters(const SoftBodyParameters& p) {
    return isSafeInverseOperand(p.mass) && std::isfinite(p.friction) &&
        p.friction >= 0 && p.friction <= 1 && isSafeSoftBodyCollisionMargin(p.collisionMargin) &&
        p.clusters >= 1 && p.clusters <= 8192;
}

inline bool prepareSoftBodyParameters(double mass, double friction, double margin,
                                     int clusters, SoftBodyParameters& output) {
    SoftBodyParameters candidate;
    if (clusters < 1 || !numeric::checkedFloatCast(mass, candidate.mass) ||
        !numeric::checkedFloatCast(friction, candidate.friction) ||
        !numeric::checkedFloatCast(margin, candidate.collisionMargin) ||
        (mass != 0 && candidate.mass == 0) || mass < 0 || friction < 0 ||
        friction > 1 || margin < 0) return false;
    candidate.clusters = static_cast<std::size_t>(clusters);
    if (!validSoftBodyParameters(candidate)) return false;
    output = candidate;
    return true;
}

typedef std::pair<int, int> SoftBodyEdge;
inline SoftBodyEdge softBodyEdge(int a, int b) {
    return SoftBodyEdge(std::min(a,b), std::max(a,b));
}

// Checks native float cross products/lengths as well as the double area.
inline bool softBodyTriangleArea(const std::vector<float>& xyz, int a, int b, int c,
                                 double& area) {
    if (a<0 || b<0 || c<0 || std::size_t(a)>=xyz.size()/3 ||
        std::size_t(b)>=xyz.size()/3 || std::size_t(c)>=xyz.size()/3 ||
        xyz.size()/3>kMaxSoftBodyVertices) return false;
    double u[3], v[3], cross[3];
    for (int axis=0; axis<3; ++axis) {
        u[axis]=double(xyz[3*b+axis])-xyz[3*a+axis];
        v[axis]=double(xyz[3*c+axis])-xyz[3*a+axis];
    }
    cross[0]=u[1]*v[2]-u[2]*v[1];
    cross[1]=u[2]*v[0]-u[0]*v[2];
    cross[2]=u[0]*v[1]-u[1]*v[0];
    const double squared=cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2];
    if (!std::isfinite(squared) || squared<std::numeric_limits<float>::min() ||
        squared>std::numeric_limits<float>::max()/64.0) return false;
    // Cancellation in btScalar must not collapse a triangle accepted in double.
    float fu[3],fv[3],fc[3];
    for (int i=0;i<3;++i) { fu[i]=float(u[i]); fv[i]=float(v[i]); }
    fc[0]=fu[1]*fv[2]-fu[2]*fv[1];
    fc[1]=fu[2]*fv[0]-fu[0]*fv[2];
    fc[2]=fu[0]*fv[1]-fu[1]*fv[0];
    const float nativeSquared=fc[0]*fc[0]+fc[1]*fc[1]+fc[2]*fc[2];
    if (!std::isnormal(nativeSquared)) return false;
    area=std::sqrt(squared); // Twice area; the common factor cancels in weights.
    return true;
}

inline bool softBodyMassWeights(const std::vector<float>& xyz, const std::vector<int>& faces,
                                std::vector<double>& output) {
    if (xyz.size()<9 || xyz.size()%3 || xyz.size()/3>kMaxSoftBodyVertices ||
        faces.size()<3 || faces.size()%3 || faces.size()/3>kMaxSoftBodyFaces) return false;
    std::vector<double> weights(xyz.size()/3,0);
    double sum=0;
    for (std::size_t i=0;i<faces.size();i+=3) {
        double area;
        if (!softBodyTriangleArea(xyz,faces[i],faces[i+1],faces[i+2],area)) return false;
        for (int j=0;j<3;++j) weights[faces[i+j]]+=area;
        sum+=3*area;
    }
    if (!std::isfinite(sum) || sum<=0) return false;
    for (double& weight:weights) {
        // Orphans had undefined face-based mass in the old native constructor.
        if (!std::isfinite(weight) || weight<=0) return false;
        weight/=sum;
    }
    output.swap(weights);
    return true;
}

inline bool softBodyClusterBudget(std::size_t vertices, std::size_t clusters) {
    if (vertices<3 || vertices>kMaxSoftBodyVertices || clusters==0 || clusters>8192) return false;
    const std::size_t actual=std::min(vertices,clusters)+1; // Native master cluster.
    std::size_t pairs, work;
    return generation::checkedProduct(actual,actual,kMaxSoftBodyWork,pairs) &&
        generation::checkedProduct(vertices,actual,kMaxSoftBodyWork,work);
}

// Shared by the sparse legacy adapter and DM's dense adapter. Bound every
// generation, including mass changes and recovery, without assuming convergence.
inline bool softBodyClusterWorkBudget(std::size_t vertices, std::size_t triangles,
                                     std::size_t clusters, std::size_t& output) {
    if (!softBodyClusterBudget(vertices, clusters) || triangles == 0 ||
        triangles > kMaxSoftBodyFaces) return false;
    const std::size_t count = std::min(vertices, clusters);
    std::size_t work, extra, entries;
    if (!generation::checkedProduct(vertices, count, kMaxSoftBodyWork, work) ||
        !generation::checkedProduct(work, kMaxSoftClusterGenerationIterations,
            kMaxSoftBodyWork, work) ||
        !generation::checkedProduct(triangles, 6, kMaxSoftBodyWork, entries) ||
        !generation::checkedSum(work, entries, kMaxSoftBodyWork, work)) return false;
    if (count > 1) {
        // Cross-cluster face merges scan node lists. Connectivity compares
        // pairs of lists with at most 2V + 6F combined entries (incl. master).
        if (!generation::checkedProduct(entries, vertices, kMaxSoftBodyWork, extra) ||
            !generation::checkedSum(work, extra, kMaxSoftBodyWork, work) ||
            !generation::checkedSum(entries, 2 * vertices, kMaxSoftBodyWork, entries) ||
            !generation::checkedProduct(entries, entries, kMaxSoftBodyWork, extra) ||
            !generation::checkedSum(work, extra, kMaxSoftBodyWork, work)) return false;
    }
    // A single cluster has no cross-cluster merges or master cluster, and its
    // self-connectivity check terminates at the first shared node.
    output = work;
    return true;
}

// DM still uses Bullet 2.88's dense triangle/bending helpers. Account for
// their quadratic storage and (distance > 2) cubic search before calling them.
// This is a conservative admission budget, not a wall-clock estimate.
inline bool softBodyNativeWorkBudget(std::size_t vertices, std::size_t triangles,
    int bendDistance, int clusters, const int (&iterations)[4], std::size_t& output)
{
    if (vertices < 3 || vertices > kMaxSoftBodyVertices || triangles == 0 ||
        triangles > kMaxSoftBodyFaces || bendDistance < 0 || clusters < 0)
        return false;
    std::size_t pairs, links, work, extra, passes = 0;
    if (!generation::checkedProduct(vertices, vertices, kMaxSoftBodyWork, pairs) ||
        !generation::checkedProduct(triangles, 3, kMaxSoftBodyLinks, links)) return false;
    work = pairs;
    if (bendDistance > 1) {
        if (bendDistance == 2) {
            // Sum(degree^2) <= 2 * edges * vertices, including duplicate checks.
            if (!generation::checkedProduct(links, 2 * vertices, kMaxSoftBodyWork, extra)) return false;
        } else if (!generation::checkedProduct(pairs, vertices, kMaxSoftBodyWork, extra)) return false;
        if (!generation::checkedSum(work, extra, kMaxSoftBodyWork, work)) return false;
        links = pairs / 2; // All original and generated links together.
        if (links > kMaxSoftBodyLinks) return false;
    }
    for (int count : iterations) {
        if (count < 0 || count > 1024 ||
            !generation::checkedSum(passes, std::size_t(count), kMaxSoftBodyWork, passes)) return false;
    }
    if (!generation::checkedSum(links, vertices, kMaxSoftBodyWork, extra) ||
        !generation::checkedSum(extra, triangles, kMaxSoftBodyWork, extra) ||
        !generation::checkedProduct(extra, passes, kMaxSoftBodyWork, extra) ||
        !generation::checkedSum(work, extra, kMaxSoftBodyWork, work)) return false;
    if (clusters) {
        if (!softBodyClusterWorkBudget(vertices, triangles, std::size_t(clusters), extra) ||
            !generation::checkedSum(work, extra, kMaxSoftBodyWork, work)) return false;
    }
    output = work;
    return true;
}

struct SoftBodyTopology {
    std::vector<SoftBodyEdge> edges; // Original face traversal/direction order.
    std::vector<SoftBodyEdge> bends; // Sorted (low,high), matching native distance-2 order.
    std::vector<double> weights;
};

// Caller first validates coordinates/indices with hasValidSoftBodyMeshInput.
inline bool prepareSoftBodyTopology(const std::vector<float>& xyz, const std::vector<int>& faces,
                                     SoftBodyTopology& output) {
    const std::size_t count=xyz.size()/3;
    if (count>kMaxSoftBodyVertices || faces.size()/3>kMaxSoftBodyFaces) return false;
    SoftBodyTopology result;
    if (!softBodyMassWeights(xyz,faces,result.weights)) return false;
    std::set<SoftBodyEdge> unique;
    std::vector<std::vector<int> > adjacent(count);
    for (std::size_t face=0;face<faces.size();face+=3) {
        for (int previous=2,current=0;current<3;previous=current++) {
            const int a=faces[face+previous],b=faces[face+current];
            if (unique.insert(softBodyEdge(a,b)).second) {
                if (result.edges.size()>=kMaxSoftBodyLinks) return false;
                result.edges.push_back(SoftBodyEdge(a,b));
                adjacent[a].push_back(b); adjacent[b].push_back(a);
            }
        }
    }
    std::size_t work=0;
    for (const auto& neighbors:adjacent) {
        std::size_t visits;
        if (!generation::checkedProduct(neighbors.size(),neighbors.size(),kMaxSoftBodyWork,visits) ||
            !generation::checkedSum(work,visits,kMaxSoftBodyWork,work)) return false;
    }
    std::set<SoftBodyEdge> bends;
    for (std::size_t a=0;a<count;++a) {
        for (int middle:adjacent[a]) for (int b:adjacent[middle]) {
            if (b<=int(a)) continue;
            const SoftBodyEdge edge(int(a),b);
            if (unique.count(edge)) continue;
            bends.insert(edge);
            if (bends.size()>kMaxSoftBodyLinks-result.edges.size()) return false;
        }
    }
    result.bends.assign(bends.begin(),bends.end());
    output=std::move(result);
    return true;
}

inline bool prepareSoftBodyMass(float mass, const std::vector<double>& weights,
                                std::vector<float>& output) {
    if (!isSafeInverseOperand(mass) || weights.empty()) return false;
    std::vector<float> values(weights.size());
    for (std::size_t i=0;i<weights.size();++i) {
        if (!std::isfinite(weights[i]) || weights[i]<=0 || weights[i]>1) return false;
        const double value=double(mass)*weights[i];
        if (value>std::numeric_limits<float>::max()) return false;
        const float nodeMass=float(value);
        if ((mass>0 && nodeMass==0) || !isSafeInverseOperand(nodeMass)) return false;
        values[i]=nodeMass;
    }
    output.swap(values);
    return true;
}

// Native clusters invert float inertia matrices. Bound their determinant scale
// before native mutation; scalar reciprocal admission alone is insufficient.
// Every generated cluster contains a complete incident face. Adding positive
// point masses cannot reduce that face's smallest inertia eigenvalue.
inline bool softBodyClusterMassRange(const std::vector<float>& xyz,
                                     const std::vector<int>& faces,
                                     const std::vector<float>& masses) {
    if (masses.empty() || masses.size()>kMaxSoftBodyVertices || xyz.size()!=3*masses.size() ||
        faces.empty() || faces.size()%3 || faces.size()/3>kMaxSoftBodyFaces) return false;
    double minimumMass=std::numeric_limits<double>::max(),total=0;
    double lo[3]={xyz[0],xyz[1],xyz[2]},hi[3]={xyz[0],xyz[1],xyz[2]};
    for (std::size_t i=0;i<masses.size();++i) {
        if (masses[i]<=0 || !isSafeInverseOperand(masses[i])) return false;
        minimumMass=std::min(minimumMass,double(masses[i])); total+=masses[i];
        for (int a=0;a<3;++a) {
            if (!std::isfinite(xyz[3*i+a])) return false;
            lo[a]=std::min(lo[a],double(xyz[3*i+a])); hi[a]=std::max(hi[a],double(xyz[3*i+a]));
        }
    }
    double minimumGeometry=std::numeric_limits<double>::max();
    for (std::size_t i=0;i<faces.size();i+=3) {
        double area;
        if (!softBodyTriangleArea(xyz,faces[i],faces[i+1],faces[i+2],area)) return false;
        double edgeSquared=0;
        for (int j=0;j<3;++j) for (int a=0;a<3;++a) {
            const double d=double(xyz[3*faces[i+j]+a])-xyz[3*faces[i+(j+1)%3]+a];
            edgeSquared+=d*d;
        }
        minimumGeometry=std::min(minimumGeometry,area*area/edgeSquared);
    }
    double diameterSquared=0;
    for (int a=0;a<3;++a) diameterSquared+=(hi[a]-lo[a])*(hi[a]-lo[a]);
    const double lower=minimumMass*minimumGeometry,upper=total*diameterSquared;
    return std::isfinite(lower) && std::isfinite(upper) &&
        lower*lower*lower>=double(std::numeric_limits<float>::min())*4096 &&
        upper*upper*upper<=double(std::numeric_limits<float>::max())/4096;
}

// Shared by both native adapters; keep this header usable without Bullet.
template <class Body>
inline bool validSoftBodyClusters(const Body& body)
{
    for (int i=0;i<body.m_clusters.size();++i) {
        const auto& cluster=*body.m_clusters[i];
        if (!isSafeInverseOperand(cluster.m_imass) || cluster.m_imass==0) return false;
        for (int axis=0;axis<3;++axis) {
            if (!std::isfinite(cluster.m_com[axis])) return false;
            for (int j=0;j<3;++j) {
                if (!std::isfinite(cluster.m_locii[axis][j]) ||
                    !std::isfinite(cluster.m_invwi[axis][j])) return false;
            }
        }
    }
    return true;
}

} } // namespace dommy::bullet
