#pragma once

// Configure-time values used by the official IlmBase 2.2.1 headers during
// source-only compatibility checks. No IlmBase implementation is built here.
#define HAVE_PTHREAD 1
#define HAVE_POSIX_SEMAPHORES 1

#define IMATH_NAMESPACE Imath
#define IMATH_INTERNAL_NAMESPACE Imath_2_2
#define IEX_NAMESPACE Iex
#define IEX_INTERNAL_NAMESPACE Iex_2_2
#define ILMTHREAD_NAMESPACE IlmThread
#define ILMTHREAD_INTERNAL_NAMESPACE IlmThread_2_2

#define ILMBASE_VERSION_STRING "2.2.1"
#define ILMBASE_PACKAGE_STRING "IlmBase 2.2.1"
#define ILMBASE_VERSION_MAJOR 2
#define ILMBASE_VERSION_MINOR 2
#define ILMBASE_VERSION_PATCH 1
#define ILMBASE_VERSION_HEX \
    ((ILMBASE_VERSION_MAJOR << 24) | \
     (ILMBASE_VERSION_MINOR << 16) | \
     (ILMBASE_VERSION_PATCH << 8))
