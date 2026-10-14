"""Embed the complete owned dispatcher against existing bounded host doubles."""
import re


def grid_dispatch_source(root, point_index_in_points=False):
    source = (root / 'src/modules/volume/VDB/Common/DVdbGridDispatch.h').read_text()
    source = re.sub(r'^#include <openvdb/[^\n]+\n', '', source, flags=re.M)
    source = source.replace('#pragma once\n', '')
    # Some historical doubles declared PointIndexGrid beside PointDataGrid.
    # Keep that fixture spelling isolated from the real public OpenVDB API.
    alias = ('namespace tools { using PointIndexGrid = points::PointIndexGrid; }'
             if point_index_in_points else '')
    return ('namespace openvdb {' + alias +
            'template<class G> typename G::Ptr gridPtrCast(const GridBase::Ptr&);'
            'template<class G> typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr&);}\n' + source)
