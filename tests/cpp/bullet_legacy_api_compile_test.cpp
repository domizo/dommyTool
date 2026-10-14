#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btBox2dShape.h>
#include <BulletCollision/CollisionShapes/btConvex2dShape.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>


void compileBulletLegacyApiProbe()
{
    btBoxShape child(btVector3(1.0, 1.0, 1.0));
    btConvex2dShape shape2d(&child);

    btCompoundShape compound;
    btTransform childTransform;
    childTransform.setIdentity();
    compound.addChildShape(childTransform, &shape2d);

    btScalar masses[1] = {1.0};
    btTransform principal;
    btVector3 inertia;
    compound.calculatePrincipalAxisTransform(masses, principal, inertia);
}
