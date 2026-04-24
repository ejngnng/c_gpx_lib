/******************************************************************
 *
 *  @file   : ninja_heading_distance_filter.c
 *  @brief  : source file of heading & distance threshold filter
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#include "ninja_heading_distance_filter.h"
#include "ninja_gpx_log.h"
#include <stdlib.h>
#include <math.h>

/*!
    \brief          normalize heading difference to [0, 180]
    \param[in]      deg_diff    : heading difference in degrees
    \param[out]     none
    \retval         normalized heading difference
*/
static double normalize_heading_diff(double deg_diff) {
    while (deg_diff < 0.0) {
        deg_diff += 360.0;
    }
    while (deg_diff >= 360.0) {
        deg_diff -= 360.0;
    }
    if (deg_diff > 180.0) {
        deg_diff = 360.0 - deg_diff;
    }
    return deg_diff;
}

/*!
    \brief          calculate distance between two points in meters
    \param[in]      p1          : start point
    \param[in]      p2          : end point
    \param[out]     none
    \retval         planar distance in meters
*/
static double point_distance_m(const gpx_point_t *p1, const gpx_point_t *p2) {
    double ref_lat = (p1->lat + p2->lat) * 0.5;
    double x1, y1, x2, y2;
    latlon_to_xy_meters(p1->lat, p1->lon, ref_lat, &x1, &y1);
    latlon_to_xy_meters(p2->lat, p2->lon, ref_lat, &x2, &y2);
    return hypot(x2 - x1, y2 - y1);
}

/*!
    \brief          calculate heading angle from p1 to p2
    \param[in]      p1          : start point
    \param[in]      p2          : end point
    \param[out]     none
    \retval         heading angle in degrees [0, 360)
*/
static double point_heading_deg(const gpx_point_t *p1, const gpx_point_t *p2) {
    double ref_lat = (p1->lat + p2->lat) * 0.5;
    double x1, y1, x2, y2;
    double heading;

    latlon_to_xy_meters(p1->lat, p1->lon, ref_lat, &x1, &y1);
    latlon_to_xy_meters(p2->lat, p2->lon, ref_lat, &x2, &y2);

    /* heading is clockwise from north: atan2(East, North) */
    heading = atan2(x2 - x1, y2 - y1) * (180.0 / 3.14159265358979323846);
    if (heading < 0.0) {
        heading += 360.0;
    }
    return heading;
}

/*!
    \brief          pre-filter point sequence by heading & distance threshold
    \param[in]      pts                 : input point array
    \param[in]      n                   : number of points
    \param[in]      dist_threshold_m    : minimum distance threshold in meters
    \param[in]      heading_threshold_deg: heading change threshold in degrees
    \param[out]     out_count           : kept index count
    \retval         pointer to kept index array, NULL on failure
*/
int *heading_distance_filter_simplify(const gpx_point_t *pts,
                                      size_t n,
                                      double dist_threshold_m,
                                      double heading_threshold_deg,
                                      size_t *out_count) {
    if (!pts || n == 0) {
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    if (n <= 2) {
        int *res = (int *)malloc(n * sizeof(int));
        if (!res) {
            GPX_LOG("[Error] malloc size: %d failed\n", n * sizeof(int));
            if (out_count) {
                *out_count = 0;
            }
            return NULL;
        }
        for (size_t i = 0; i < n; i++) {
            res[i] = (int)i;
        }
        if (out_count) {
            *out_count = n;
        }
        return res;
    }

    int *indices = (int *)malloc(n * sizeof(int));
    if (!indices) {
        GPX_LOG("[Error] malloc size: %d failed\n", n * sizeof(int));
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    size_t kept = 0;
    size_t last_kept = 0;
    indices[kept++] = 0;

    for (size_t i = 1; i < (n - 1); i++) {
        double dist = point_distance_m(&pts[last_kept], &pts[i]);
        if (dist >= dist_threshold_m) {
            indices[kept++] = (int)i;
            last_kept = i;
            continue;
        }

        double h1 = point_heading_deg(&pts[last_kept], &pts[i]);
        double h2 = point_heading_deg(&pts[i], &pts[i + 1]);
        double delta_h = normalize_heading_diff(h1 - h2);
        if (delta_h >= heading_threshold_deg) {
            indices[kept++] = (int)i;
            last_kept = i;
        }
    }

    if (indices[kept - 1] != (int)(n - 1)) {
        indices[kept++] = (int)(n - 1);
    }

    int *shrink = (int *)realloc(indices, kept * sizeof(int));
    if (shrink) {
        indices = shrink;
    }

    if (out_count) {
        *out_count = kept;
    }
    return indices;
}
