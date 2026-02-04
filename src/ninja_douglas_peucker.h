/******************************************************************
 *
 *  @file   : ninja_douglas_peucker.h
 *  @brief  : header file of douglas peucker algorithm
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#ifndef NINJA_DOUGLAS_PEUCKER_H
#define NINJA_DOUGLAS_PEUCKER_H

#include "ninja_gpx_parser.h"
#include <stddef.h>

/*
 * Simplify an array of points (lat/lon/ele). The elevation is preserved but not used
 * for geometry. The function returns a newly allocated array of indices that should
 * be kept; caller must free the returned pointer. The out_count is set to number
 * of indices.
 * tol_meters: tolerance in meters
 */ 
int *douglas_peucker_simplify(const gpx_point_t *pts, 
                              size_t n, 
                              double tol_meters, 
                              size_t *out_count);

#endif // NINJA_DOUGLAS_PEUCKER_H
