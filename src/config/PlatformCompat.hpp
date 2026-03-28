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
