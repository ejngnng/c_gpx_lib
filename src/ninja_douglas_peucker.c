/******************************************************************
 *
 *  @file   : douglas_peucker.c
 *  @brief  : source file of douglas peucker algorithm
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#include "ninja_douglas_peucker.h"
#include "ninja_gpx_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


/*!
    \brief          compute perpendicular distance from point to line (meters)
    \param[in]      xs          : array of X coordinates
    \param[in]      ys          : array of Y coordinates
    \param[in]      s           : start index of line segment
    \param[in]      e           : end index of line segment
    \param[in]      i           : index of point to measure
    \param[out]     none
    \retval         distance in meters
*/
static double point_line_distance_m(const double *xs, 
                                    const double *ys, 
                                    size_t s, 
                                    size_t e, 
                                    size_t i) {
    double x = xs[i], y = ys[i];
    double x1 = xs[s], y1 = ys[s];
    double x2 = xs[e], y2 = ys[e];
    double dx = x2 - x1, dy = y2 - y1;

    if (dx == 0.0 && dy == 0.0) {
        return hypot(x - x1, y - y1);
    }

    double t = ((x - x1) * dx + (y - y1) * dy) / (dx*dx + dy*dy);

    if (t < 0.0) {
        t = 0.0;
    }

    if (t > 1.0) {
        t = 1.0;
    }
    double proj_x = x1 + t*dx; 
    double proj_y = y1 + t*dy;
    return hypot(x - proj_x, y - proj_y);
}

/*!
    \brief          simplify point sequence using Douglas-Peucker
    \param[in]      pts         : input array of points
    \param[in]      n           : number of points
    \param[in]      tol_meters  : tolerance in meters
    \param[out]     out_count   : number of retained indices
    \retval         pointer to array of retained indices, or NULL on error
*/
int *douglas_peucker_simplify(const gpx_point_t *pts, 
                              size_t n, 
                              double tol_meters, 
                              size_t *out_count) {
    if (!pts || n == 0) { 
        if (out_count) {
            *out_count = 0;
         } 
         return NULL; 
    }
    if (n <= 2) {
        int *res = (int*)malloc(n * sizeof(int));
        if (!res) {
            GPX_LOG("[Error] malloc size: %zu failed\n", n * sizeof(int));
            return NULL;
        }
        for (size_t i=0; i<n; i++) {
            res[i] = (int)i;
        }
        if (out_count) {
            *out_count = n;
        }
        return res;
    }

    // build XY arrays using equirectangular with ref_lat = average lat
    double ref_lat = 0.0;
    for (size_t i=0; i<n; i++) {
        ref_lat += pts[i].lat;
    }
    ref_lat /= (double)n;
    double *xs = (double*)malloc(n * sizeof(double));
    double *ys = (double*)malloc(n * sizeof(double));
    if (!xs || !ys) { 
        free(xs); 
        free(ys); 
        return NULL; 
    }

    for (size_t i=0; i<n;i++) {
        double x,y; 
        latlon_to_xy_meters(pts[i].lat, pts[i].lon, ref_lat, &x, &y);
        xs[i] = x; 
        ys[i] = y;
    }

    // keep flags
    unsigned char *keep = (unsigned char*)calloc(n, 1);
    if (!keep) {
        GPX_LOG("[Error] calloc size: %zu failed\n", n);
        free(xs); 
        free(ys); 
        return NULL;
    }
    keep[0] = 1; 
    keep[n-1] = 1;

    // iterative stack of segments
    size_t stack_cap = 64; 
    size_t stack_sz = 0;
    size_t *stack = (size_t*)malloc(stack_cap * sizeof(size_t) * 2);
    if (!stack) { 
        GPX_LOG("[Error] malloc size: %zu failed\n", stack_cap * sizeof(size_t) * 2);
        free(xs); 
        free(ys); 
        free(keep); 
        return NULL; 
    }
    // push (0, n-1)
    stack[stack_sz*2+0] = 0; 
    stack[stack_sz*2+1] = n-1; 
    stack_sz++;

    while (stack_sz) {
        size_t s = stack[(stack_sz-1)*2+0];
        size_t e = stack[(stack_sz-1)*2+1];
        stack_sz--;
        double maxd = -1.0; size_t idx = 0;
        if (e <= s+1) continue;
        for (size_t i=s+1; i<e;i++) {
            double d = point_line_distance_m(xs, ys, s, e, i);
            if (d > maxd) { 
                maxd = d; 
                idx = i; 
            }
        }
        if (maxd > tol_meters) {
            keep[idx] = 1;
            // push (s, idx) and (idx, e)
            if (stack_sz+2 >= stack_cap) {
                stack_cap *= 2; 
                stack = (size_t*)realloc(stack, stack_cap * sizeof(size_t) * 2);
                if (!stack) { 
                    GPX_LOG("[Error] realloc size: %zu failed\n", stack_cap * sizeof(size_t) * 2);
                    free(xs); 
                    free(ys); 
                    free(keep); 
                    return NULL; 
                }
            }
            stack[stack_sz*2+0] = s; 
            stack[stack_sz*2+1] = idx; 
            stack_sz++;

            stack[stack_sz*2+0] = idx; 
            stack[stack_sz*2+1] = e; 
            stack_sz++;
        }
    }

    // collect indices
    size_t keep_count = 0;
    for (size_t i=0; i<n; i++) {
        if (keep[i]) {
            keep_count++;
        }
    }    
    int *out = (int*)malloc(keep_count * sizeof(int));
    if (!out) {
        GPX_LOG("[Error] malloc size: %zu failed\n", keep_count * sizeof(int));
        free(xs); 
        free(ys); 
        free(keep); 
        free(stack); 
        return NULL; 
    }
    size_t k=0;
    for (size_t i=0; i<n; i++) {
        if (keep[i]) {
            out[k++] = (int)i;
        }
    }

    free(xs); 
    free(ys); 
    free(keep); 
    free(stack); 
    if (out_count) {
        *out_count = keep_count;
    }
    return out;
}

