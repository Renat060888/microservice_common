#ifndef LOGGER_H
#define LOGGER_H

// TODO: ?
//#define SWITCH_LOGGER_SIMPLE

#if SWITCH_LOGGER_DEBIAN_9
#include "logger_normal.h"
#endif

#ifdef SWITCH_LOGGER_ASTRA
#include "logger_astra.h"
#endif

#ifdef SWITCH_LOGGER_SIMPLE
#include "logger_simple.h"
#endif

#endif // LOGGER_H
