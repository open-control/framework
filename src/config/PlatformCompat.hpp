#pragma once

/**
 * @file PlatformCompat.hpp
 * @brief Platform compatibility layer for Arduino and native builds.
 *
 * Provides no-op definitions for Arduino-specific memory attributes when
 * building for desktop/native targets.
 */

#ifdef ARDUINO
    #include <Arduino.h>
#else
    #ifndef PROGMEM
        #define PROGMEM
    #endif
    #ifndef DMAMEM
        #define DMAMEM
    #endif
    #ifndef EXTMEM
        #define EXTMEM
    #endif
    #ifndef FLASHMEM
        #define FLASHMEM
    #endif

    #include <cstddef>
    #include <cstdint>
#endif

#ifndef OC_ALWAYS_INLINE
    #if defined(_MSC_VER)
        #define OC_ALWAYS_INLINE __forceinline
    #elif defined(__GNUC__) || defined(__clang__)
        #define OC_ALWAYS_INLINE inline __attribute__((always_inline))
    #else
        #define OC_ALWAYS_INLINE inline
    #endif
#endif
