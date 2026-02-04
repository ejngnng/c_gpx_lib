/******************************************************************
 *
 *  @file   : ninja_gpx_parser.h
 *  @brief  : header file of Minimal GPX types and shared helpers 
 *            for embedded use
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#ifndef NINJA_GPX_PARSER_H
#define NINJA_GPX_PARSER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    double lat;
    double lon;
    double ele;
} gpx_point_t;

/* callback for each track segment */
typedef void (*trk_segment_cb_t)(const char *track_name, 
                                 gpx_point_t *pts, 
                                 size_t count, 
                                 void *user_ctx);

/*
 * parse a GPX file and invoke callback for each track segment
 * returns 0 on success, non-zero on failure
 */
int gpx_parse_file(const char *path, trk_segment_cb_t cb, void *user_ctx);

/* small utility: convert degrees to local XY meters (equirectangular approx) */
void latlon_to_xy_meters(double lat, 
                         double lon, 
                         double ref_lat, 
                         double *out_x, 
                         double *out_y);

#endif // NINJA_GPX_PARSER_H