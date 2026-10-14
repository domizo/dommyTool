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

#ifndef PDBIO_H
#define PDBIO_H

#include <iostream>
#include <string>
#include <vector>

struct pdb_io_t {
    static const unsigned int kMaxParticleCount = 10000000U;

    enum Type { kVector = 1, kReal = 2, kLong = 3, kChar = 4, kPointer = 5 };

    struct vec3_t {
        float x;
        float y;
        float z;
    };

    struct attribute_t {
        std::string m_name;
        int m_type;
        int m_num_elements;

        //
        std::vector<vec3_t> m_vector_data;
        std::vector<float> m_real_data;
        std::vector<int> m_long_data;

        attribute_t() : m_type(0), m_num_elements(0) {}
    };

    // data
    float m_time;
    int m_num_particles;
    std::vector<attribute_t> m_attributes;

    // methods

    bool read(std::istream& in);
    bool write(std::ostream& out) const;

    pdb_io_t() : m_time(0.0f), m_num_particles(0) {}
};

#endif
