/******************************************************************
 *
 *  @file   : ninja_output_writer.h
 *  @brief  : header file of output writer functions
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#ifndef NINJA_OUTPUT_WRITER_H
#define NINJA_OUTPUT_WRITER_H

#include "ninja_gpx_parser.h"
#include <stdio.h>

typedef enum { 
    OUT_TYPE_GPX, 
    OUT_TYPE_CACHE_BIN 
} out_type_t;

/* write GPX header (open file) and footer functions */
int gpx_write_header(FILE *f, const char *orig_name);
int gpx_write_footer(FILE *f);

/* write a simplified track segment in GPX text form (appends to FILE) */
int gpx_write_track_segment(FILE *f, 
                            const char *track_name, 
                            const gpx_point_t *pts, 
                            size_t n);

/*
 * write cache binary file: simple format: uint32_t name_len, name bytes, uint32_t point_count,
 * then repeated int32_t lon_scaled, int32_t lat_scaled, int32_t ele_scaled (little-endian)
 */ 
int write_cache_bin(const char *path, 
                    const char *track_name, 
                    const gpx_point_t *pts, 
                    size_t n, 
                    int32_t coord_scale, 
                    int32_t ele_scale);

/* print statistics: points before simplification, after simplification, and compression ratio */
void gpx_print_stats(size_t before, size_t after);

#endif // NINJA_OUTPUT_WRITER_H
