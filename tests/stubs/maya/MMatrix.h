#pragma once

#include <cstddef>

class MMatrix
{
public:
    MMatrix()
    {
        for(std::size_t row = 0; row < 4; ++row) {
            for(std::size_t column = 0; column < 4; ++column) {
                values_[row][column] = row == column ? 1.0 : 0.0;
            }
        }
    }

    const double* operator[](std::size_t row) const { return values_[row]; }
    double* operator[](std::size_t row) { return values_[row]; }

private:
    double values_[4][4];
};
