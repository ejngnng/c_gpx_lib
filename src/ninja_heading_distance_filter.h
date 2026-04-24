/******************************************************************
 *
 *  @file   : ninja_heading_distance_filter.h
 *  @brief  : header file of heading & distance threshold filter
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#ifndef NINJA_HEADING_DISTANCE_FILTER_H
#define NINJA_HEADING_DISTANCE_FILTER_H

#include "ninja_gpx_parser.h"
#include <stddef.h>

/*
 * Linear pre-filter by heading-change and distance threshold.
 * Returns a newly allocated array of kept indices; caller must free.
 */
int *heading_distance_filter_simplify(const gpx_point_t *pts,
                                      size_t n,
                                      double dist_threshold_m,
                                      double heading_threshold_deg,
                                      size_t *out_count);

#endif // NINJA_HEADING_DISTANCE_FILTER_H
