/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Walt Disney Studios

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Written by: Nicola Candussi <nicola@fluidinteractive.com>
*/

// Modified for dommyTool; not the original upstream version.

// mathUtils.h

#ifndef DYN_MATH_UTILS_H
#define DYN_MATH_UTILS_H

#include "dommyMath.h"

#include "vec.h"
#include "mat.h"
#include "quat.h"

#include <cmath>

typedef mvl::vec<float, 3> vec3f;
typedef mvl::vec<float, 4> vec4f;
typedef mvl::mat<float, 4, 4> mat4x4f;
typedef mvl::mat<float, 3, 3> mat3x3f;
typedef mvl::vec<float, 4> quatf;

template <typename T> inline T deg2rad(T x) {
    return x * T(3.141592654 / 180.0);
}

template <typename T> inline T rad2deg(T x) {
    return x * T(180.0 / 3.141592654);
}

// template<typename T>
// inline T clamp(T x, T min, T max)
//{
//     if(x < min) return min;
//     if(x > max) return max;
//     return x;
// }

// template<typename T>
// inline T sqr(T x) {
//     return x * x;
// }

// A must be a symmetric matrix.
// returns quaternion q such that its corresponding matrix Q
// can be used to Diagonalize A
// Diagonal matrix D = Q^T * A * Q;  and  A = Q*D*Q^T
// The colums of q are the eigenvectors D's diagonal is the eigenvalues
// As per 'column' convention if float3x3 Q = q.getmatrix(); then Q*v = q*v*conj(q)
template <typename T> mvl::vec<T, 4> diagonalizer(const mvl::mat<T, 3, 3>& A) {
    const int maxsteps = 24; // certainly wont need that many.

    typedef mvl::vec<T, 3> vec3_t;
    typedef mvl::vec<T, 4> quat_t;
    typedef mvl::mat<T, 3, 3> mat3x3_t;
    quat_t q(mvl::qidentity<T>());
    for (int i = 0; i < maxsteps; ++i) {
        mat3x3_t Q;
        mvl::q_to_mat(q, Q);                                              // Q*v == q*v*conj(q)
        mat3x3_t D = mvl::prod(mvl::trans(Q), mat3x3_t(mvl::prod(A, Q))); // A = Q^T*D*Q
        vec3_t offdiag(D(2, 1), D(2, 0), D(1, 0)); // elements not on the diagonal
        vec3_t om(std::fabs(offdiag[0]), std::fabs(offdiag[1]),
                  std::fabs(offdiag[2])); // mag of each offdiag elem
        int k = (om[0] > om[1] && om[0] > om[2]) ? 0
                : (om[1] > om[2])                ? 1
                                                 : 2; // index of largest element of offdiag
        int k1 = (k + 1) % 3;
        int k2 = (k + 2) % 3;
        if (offdiag[k] == T())
            break; // diagonal already
        T thet = (D(k2, k2) - D(k1, k1)) / (T(2.0) * offdiag[k]);
        T sgn = (thet > 0.0f) ? T(1.0) : T(-1.0);
        thet *= sgn; // make it positive
        T t = sgn / (thet + ((thet < T(1.E6))
                            ? std::sqrt(dommy::math::sqr(thet) + T(1.0))
                                              : thet)); // sign(T)/(|T|+sqrt(T^2+1))
        T c = T(1.0) /
              std::sqrt(dommy::math::sqr(t) + T(1.0)); // c=1/(t^2+1), t=s/c
        if (c == T(1.0))
            break;             // no room for improvement - reached machine precision.
        quat_t jr(0, 0, 0, 0); // jacobi rotation for this iteration.
        jr[1 + k] =
            sgn * std::sqrt((T(1.0) - c) /
                            T(2.0)); // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)
        jr[1 + k] *= -T(1.0); // since our quat-to-matrix convention was for v*M instead of M*v
        jr[0] = std::sqrt(
            T(1.0) - dommy::math::sqr(jr[1 + k]));
        if (jr[0] == T(1.0))
            break; // reached limits of floating point precision
        q = mvl::qprod(q, jr);
        q = mvl::normalize(q);
    }
    return q;
}

template <typename T>
inline T determinant(mvl::vec<T, 3> const& v0, mvl::vec<T, 3> const& v1, mvl::vec<T, 3> const& v2) {
    return mvl::dot(v0, mvl::cross(v1, v2));
}

#endif
