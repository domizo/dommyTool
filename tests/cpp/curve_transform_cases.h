#pragma once

// Deliberately independent primitive matrices; Geometry::matrix stays local.
inline void assertSameMatrix(const MMatrix& a, const MMatrix& b) {
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            assert(a[r][c] == b[r][c]);
}
inline std::vector<MMatrix> primitiveTransformCases() {
    std::vector<MMatrix> result(1);
    MMatrix translation;
    translation[3][0] = 100;
    translation[3][1] = -20;
    result.push_back(translation);
    MMatrix rotationScale;
    rotationScale[0][0] = 0;
    rotationScale[0][1] = 2;
    rotationScale[1][0] = -3;
    rotationScale[1][1] = 0;
    rotationScale[2][2] = -4;
    rotationScale[3][0] = 100;
    rotationScale[3][1] = -20;
    rotationScale[3][2] = 7;
    result.push_back(rotationScale);
    MMatrix shear;
    shear[0][1] = .5;
    result.push_back(shear);
    MMatrix reflection;
    reflection[0][0] = -2;
    result.push_back(reflection);
    MMatrix singular;
    singular[1][1] = 0;
    result.push_back(singular);
    return result;
}
