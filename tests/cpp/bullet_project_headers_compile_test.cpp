#include "DB_BulletSolver_t.h"
#include "db_collision_base_shapes.h"
#include "db_collision_mesh_shapes.h"

namespace {

void compileProjectBulletTypes()
{
    const vec3f dimensions(1.0f, 2.0f, 3.0f);
    db_collision_box box(dimensions);
    (void)box;
}

} // namespace
