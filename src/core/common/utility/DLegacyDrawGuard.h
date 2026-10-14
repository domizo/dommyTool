#ifndef DOMMY_DLEGACY_DRAW_GUARD_H
#define DOMMY_DLEGACY_DRAW_GUARD_H

#include "DMayaEntryPointGuard.h"

#include <maya/M3dView.h>
#include <maya/MGL.h>

namespace dommy {
namespace maya {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

class ScopedLegacyGL final
{
public:
    explicit ScopedLegacyGL(M3dView& view)
        : view_(view)
        , attributesPushed_(false)
    {
        view_.beginGL();
    }

    ScopedLegacyGL(M3dView& view, GLbitfield attributeMask)
        : view_(view)
        , attributesPushed_(false)
    {
        view_.beginGL();
        glPushAttrib(attributeMask);
        attributesPushed_ = true;
    }

    ~ScopedLegacyGL() noexcept
    {
        if (attributesPushed_) {
            try {
                glPopAttrib();
            } catch (...) {
            }
        }
        try {
            view_.endGL();
        } catch (...) {
        }
    }

private:
    ScopedLegacyGL(const ScopedLegacyGL&);
    ScopedLegacyGL& operator=(const ScopedLegacyGL&);

    M3dView& view_;
    bool attributesPushed_;
};


class ScopedLegacyGLMatrix final
{
public:
    ScopedLegacyGLMatrix()
    {
        glPushMatrix();
    }

    ~ScopedLegacyGLMatrix() noexcept
    {
        try {
            glPopMatrix();
        } catch (...) {
        }
    }

private:
    ScopedLegacyGLMatrix(const ScopedLegacyGLMatrix&);
    ScopedLegacyGLMatrix& operator=(const ScopedLegacyGLMatrix&);
};

class ScopedLegacyGLPrimitive final
{
public:
    explicit ScopedLegacyGLPrimitive(GLenum mode) { glBegin(mode); }
    ~ScopedLegacyGLPrimitive() noexcept
    {
        try { glEnd(); } catch (...) {}
    }
private:
    ScopedLegacyGLPrimitive(const ScopedLegacyGLPrimitive&) = delete;
    ScopedLegacyGLPrimitive& operator=(const ScopedLegacyGLPrimitive&) = delete;
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


inline void reportDrawException(
    const char* ownerName, const std::exception_ptr& exception) noexcept
{
    (void)reportEntryPointException(ownerName, "draw", exception);
}

} // namespace maya
} // namespace dommy

#define DOMMY_DRAW_CATCH(ownerName) \
    catch (...) { \
        ::dommy::maya::reportDrawException( \
            ownerName, std::current_exception()); \
    }

#endif // DOMMY_DLEGACY_DRAW_GUARD_H
