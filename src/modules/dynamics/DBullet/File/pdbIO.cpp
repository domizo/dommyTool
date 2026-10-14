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

#include "pdbIO.h"
#include "common/utility/DCheckedSize.h"
#include <cmath>
#include <cstring>
#include <limits>

#define PDB_MAGIC 670

// headers used for reading and writing

struct pdb_channel_data_t {
    int type;
    unsigned int datasize;
    unsigned int blocksize;
    int num_blocks;
    // void                **block;
    int block;
};

struct pdb_channel_t {
    // char                    *name;
    int name;
    int type;
    unsigned int size;
    unsigned int active_start;
    unsigned int active_end;

    char hide;
    char disconnect;
    // pdb_channel_data_t     *data;
    int data;

    // pdb_channel_t          *link;
    int link;
    // pdb_channel_t          *next;
    int next;
};

struct pdb_channel_header_t {
    char magic;
    unsigned short swap;
    char encoding;
    char type;
};

/*
struct pdb_data_t
{
    int         numAttributes;
    int         numParticles;
    float       time;
    // short      *types;
    int          types;
    // char      **names;
    int          names;
    // void      **data;
    int          data;
};
             */

struct pdb_header_t {
    int magic;
    unsigned short swap;
    float version;
    float time;
    unsigned int data_size;
    unsigned int num_data;
    char padding[32];
    // pdb_channel_t   **data;
    int data;
};

/*
struct pdb_channel_data_t
{
    int                 type;
    unsigned            datasize;
    unsigned long       blocksize;
    int                 num_blocks;
    void                **block;
};

struct pdb_channel_t
{
    char                    *name;
    int                     type;
    unsigned long           size;
    unsigned long           active_start;
    unsigned long           active_end;

    char                    hide;
    char                    disconnect;
    pdb_channel_data_t     *data;

    pdb_channel_t          *link;
    pdb_channel_t          *next;
};

struct pdb_channel_header_t
{
    char                magic;
    unsigned short      swap;
    char                encoding;
    char                type;
};

struct pdb_data_t
{
    int         numAttributes;
    int         numParticles;
    float       time;
    short      *types;
    char      **names;
    void      **data;
};



struct pdb_header_t
{
    int			    magic;
    unsigned short	swap;
    float		    version;
    float		    time;
    unsigned		data_size;
    unsigned		num_data;
    char		    padding[32];
    pdb_channel_t   **data;
};

*/

namespace {

const unsigned int kMaxAttributes = 1024U;
const unsigned int kMaxElementsPerAttribute = 1024U;
const std::size_t kMaxAttributeNameBytes = 4096U;
const std::size_t kMaxPayloadBytes = 512U * 1024U * 1024U;

bool readBytes(std::istream& in, void* destination, std::size_t byteCount) {
    if (byteCount == 0) {
        return true;
    }
    in.read(static_cast<char*>(destination), static_cast<std::streamsize>(byteCount));
    return in.good();
}

bool writeBytes(std::ostream& out, const void* source, std::size_t byteCount) {
    if (byteCount == 0) {
        return true;
    }
    out.write(static_cast<const char*>(source), static_cast<std::streamsize>(byteCount));
    return out.good();
}

bool readString(std::istream& in, std::string& value) {
    value.clear();
    for (std::size_t index = 0; index < kMaxAttributeNameBytes; ++index) {
        const int character = in.get();
        if (character == std::char_traits<char>::eof()) {
            value.clear();
            return false;
        }
        if (character == 0) {
            return true;
        }
        value.push_back(static_cast<char>(character));
    }
    value.clear();
    return false;
}

bool writeString(std::ostream& out, const std::string& value) {
    if (value.size() >= kMaxAttributeNameBytes || value.find('\0') != std::string::npos) {
        return false;
    }
    return writeBytes(out, value.data(), value.size()) && writeBytes(out, "\0", 1);
}

bool expectedValueCount(unsigned int particleCount, int elementCount, std::size_t& valueCount) {
    if (elementCount <= 0 || static_cast<unsigned int>(elementCount) > kMaxElementsPerAttribute) {
        return false;
    }
    return dommy::memory::checkedMultiply(static_cast<std::size_t>(particleCount),
                                          static_cast<std::size_t>(elementCount), valueCount);
}

bool isFiniteVector(const pdb_io_t::vec3_t& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool areFinite(const std::vector<float>& values) {
    for (std::vector<float>::const_iterator value = values.begin();
         value != values.end(); ++value) {
        if (!std::isfinite(*value)) return false;
    }
    return true;
}

} // namespace

bool pdb_io_t::read(std::istream& in) {
    pdb_header_t header = {};
    if (!readBytes(in, &header, sizeof(header)) || header.magic != PDB_MAGIC || header.swap != 0 ||
        header.version != 1.0f || !std::isfinite(header.time) ||
        header.data_size > kMaxParticleCount ||
        header.num_data > kMaxAttributes ||
        header.data_size > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        return false;
    }

    pdb_io_t decoded;
    decoded.m_time = header.time;
    decoded.m_num_particles = static_cast<int>(header.data_size);
    decoded.m_attributes.resize(header.num_data);

    std::size_t totalPayloadBytes = 0;
    for (unsigned int index = 0; index < header.num_data; ++index) {
        pdb_channel_header_t channelHeader = {};
        pdb_channel_t channel = {};
        pdb_channel_data_t channelData = {};
        attribute_t& attribute = decoded.m_attributes[index];

        if (!readBytes(in, &channelHeader, sizeof(channelHeader)) ||
            !readBytes(in, &channel, sizeof(channel)) || !readString(in, attribute.m_name) ||
            !readBytes(in, &channelData, sizeof(channelData)) || channelHeader.encoding != 0 ||
            channelHeader.type != static_cast<char>(channel.type) ||
            channelData.type != channel.type || channelData.blocksize != 0 ||
            channelData.num_blocks != 0 || channelData.block != 0) {
            return false;
        }

        attribute.m_type = channel.type;
        std::size_t valueCount = 0;
        std::size_t payloadBytes = 0;

        switch (channel.type) {
        case kVector:
            if (channelData.datasize != sizeof(vec3_t)) {
                return false;
            }
            attribute.m_num_elements = 1;
            valueCount = header.data_size;
            if (!dommy::memory::checkedMultiply(valueCount, sizeof(vec3_t), payloadBytes) ||
                payloadBytes > kMaxPayloadBytes - totalPayloadBytes) {
                return false;
            }
            attribute.m_vector_data.resize(valueCount);
            if (!readBytes(in, attribute.m_vector_data.data(), payloadBytes)) {
                return false;
            }
            for (std::vector<vec3_t>::const_iterator value =
                     attribute.m_vector_data.begin();
                 value != attribute.m_vector_data.end(); ++value) {
                if (!isFiniteVector(*value)) return false;
            }
            break;
        case kReal:
            if (channelData.datasize == 0 || channelData.datasize % sizeof(float) != 0) {
                return false;
            }
            attribute.m_num_elements = static_cast<int>(channelData.datasize / sizeof(float));
            if (!expectedValueCount(header.data_size, attribute.m_num_elements, valueCount) ||
                !dommy::memory::checkedMultiply(valueCount, sizeof(float), payloadBytes) ||
                payloadBytes > kMaxPayloadBytes - totalPayloadBytes) {
                return false;
            }
            attribute.m_real_data.resize(valueCount);
            if (!readBytes(in, attribute.m_real_data.data(), payloadBytes)) {
                return false;
            }
            if (!areFinite(attribute.m_real_data)) return false;
            break;
        case kLong:
            if (channelData.datasize == 0 || channelData.datasize % sizeof(int) != 0) {
                return false;
            }
            attribute.m_num_elements = static_cast<int>(channelData.datasize / sizeof(int));
            if (!expectedValueCount(header.data_size, attribute.m_num_elements, valueCount) ||
                !dommy::memory::checkedMultiply(valueCount, sizeof(int), payloadBytes) ||
                payloadBytes > kMaxPayloadBytes - totalPayloadBytes) {
                return false;
            }
            attribute.m_long_data.resize(valueCount);
            if (!readBytes(in, attribute.m_long_data.data(), payloadBytes)) {
                return false;
            }
            break;
        default:
            return false;
        }

        totalPayloadBytes += payloadBytes;
    }

    *this = decoded;
    return true;
}

bool pdb_io_t::write(std::ostream& out) const {
    if (!out.good() || !std::isfinite(m_time) || m_num_particles < 0 ||
        static_cast<unsigned int>(m_num_particles) > kMaxParticleCount ||
        m_attributes.size() > kMaxAttributes) {
        return false;
    }

    const unsigned int particleCount = static_cast<unsigned int>(m_num_particles);
    pdb_header_t header = {};
    header.magic = PDB_MAGIC;
    header.swap = 0;
    header.version = 1.0f;
    header.time = m_time;
    header.data_size = particleCount;
    header.num_data = static_cast<unsigned int>(m_attributes.size());
    if (!writeBytes(out, &header, sizeof(header))) {
        return false;
    }

    std::size_t totalPayloadBytes = 0;
    for (std::size_t index = 0; index < m_attributes.size(); ++index) {
        const attribute_t& attribute = m_attributes[index];
        std::size_t valueCount = 0;
        if (!expectedValueCount(particleCount, attribute.m_num_elements, valueCount)) {
            return false;
        }

        pdb_channel_data_t channelData = {};
        channelData.type = attribute.m_type;
        const void* payload = nullptr;
        std::size_t payloadBytes = 0;

        switch (attribute.m_type) {
        case kVector:
            if (attribute.m_num_elements != 1 || attribute.m_vector_data.size() != valueCount) {
                return false;
            }
            for (std::vector<vec3_t>::const_iterator value =
                     attribute.m_vector_data.begin();
                 value != attribute.m_vector_data.end(); ++value) {
                if (!isFiniteVector(*value)) return false;
            }
            channelData.datasize = sizeof(vec3_t);
            payload = attribute.m_vector_data.data();
            payloadBytes = valueCount * sizeof(vec3_t);
            break;
        case kReal:
            if (attribute.m_real_data.size() != valueCount ||
                !areFinite(attribute.m_real_data)) {
                return false;
            }
            channelData.datasize = static_cast<unsigned int>(
                sizeof(float) * static_cast<std::size_t>(attribute.m_num_elements));
            payload = attribute.m_real_data.data();
            payloadBytes = valueCount * sizeof(float);
            break;
        case kLong:
            if (attribute.m_long_data.size() != valueCount) {
                return false;
            }
            channelData.datasize = static_cast<unsigned int>(
                sizeof(int) * static_cast<std::size_t>(attribute.m_num_elements));
            payload = attribute.m_long_data.data();
            payloadBytes = valueCount * sizeof(int);
            break;
        default:
            return false;
        }

        if (payloadBytes > kMaxPayloadBytes - totalPayloadBytes) {
            return false;
        }
        totalPayloadBytes += payloadBytes;

        pdb_channel_header_t channelHeader = {};
        channelHeader.type = static_cast<char>(attribute.m_type);
        pdb_channel_t channel = {};
        channel.type = attribute.m_type;

        if (!writeBytes(out, &channelHeader, sizeof(channelHeader)) ||
            !writeBytes(out, &channel, sizeof(channel)) || !writeString(out, attribute.m_name) ||
            !writeBytes(out, &channelData, sizeof(channelData)) ||
            !writeBytes(out, payload, payloadBytes)) {
            return false;
        }
    }
    return out.good();
}
