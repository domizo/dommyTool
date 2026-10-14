#pragma once

// Release and archival builds are quiet by default. Developers can enable
// diagnostic-only branches with -DDOMMY_ENABLE_DEBUG_LOGS=1.
#ifndef DOMMY_ENABLE_DEBUG_LOGS
#define DOMMY_ENABLE_DEBUG_LOGS 0
#endif
