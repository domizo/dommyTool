#pragma once

#include <memory>

#include "BulletSoftBody/btSoftBody.h"


struct DBulletSoftBodyWorldInfoDeleter
{
    void operator()(btSoftBodyWorldInfo* worldInfo) const noexcept
    {
        if (!worldInfo) {
            return;
        }
        worldInfo->m_sparsesdf.Reset();
        delete worldInfo;
    }
};

typedef std::unique_ptr<
    btSoftBodyWorldInfo,
    DBulletSoftBodyWorldInfoDeleter> DBulletSoftBodyWorldInfoPtr;

// Generation identity, not ownership of Bullet resources. The solver invalidates
// this reference before destroying any world resource. Bodies only keep a weak
// reference, so retaining a facade cannot keep an obsolete world alive.
class DBulletSoftBodyWorldReference
{
public:
    explicit DBulletSoftBodyWorldReference(const btSoftBodyWorldInfo* info) : m_info(info) {}
    DBulletSoftBodyWorldReference(const DBulletSoftBodyWorldReference&) = delete;
    DBulletSoftBodyWorldReference& operator=(const DBulletSoftBodyWorldReference&) = delete;
    const btSoftBodyWorldInfo* get() const { return m_info; }
    void invalidate() noexcept { m_info = nullptr; }
private:
    const btSoftBodyWorldInfo* m_info;
};
typedef std::shared_ptr<DBulletSoftBodyWorldReference> DBulletSoftBodyWorldReferencePtr;
