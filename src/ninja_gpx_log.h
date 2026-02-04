/******************************************************************
 *
 *  @file   : ninja_gpx_log.h
 *  @brief  : header file of gpx logging
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#ifndef NINJA_GPX_LOG_H
#define NINJA_GPX_LOG_H

#include <stdio.h>

#define DEBUG_GPX_LOG                    (1)

#if DEBUG_GPX_LOG
    #define GPX_LOG(fmt, ...)                            \
            do {                                         \
               printf("%s %d: ", __FUNCTION__, __LINE__);\
               printf(fmt, ##__VA_ARGS__);               \
            } while(0)
#else
    #define GPX_LOG(fmt, ...)
#endif




#endif // NINJA_GPX_LOG_H
