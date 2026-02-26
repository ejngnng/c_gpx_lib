/******************************************************************
 *
 *  @file   : ninja_gpx_parser.c
 *  @brief  : source file of GPX parser
 *            - Simple streaming GPX parser for embedded systems
 *            - Calls a user callback for each completed <trkseg>
 *            - Memory-efficient: allocates per-segment dynamic array 
 *              and frees after callback
 *  @author : ninja
 *  @date   : 2026-02
 *
 ******************************************************************/
#include "ninja_gpx_parser.h"
#include "ninja_gpx_log.h"
#include "ninja_douglas_peucker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Configuration for streaming processing memory optimization */
#define STREAM_COMPRESS_THRESHOLD   (200)    /* Trigger in-memory compression when point count reaches this value */
#define STREAM_COMPRESS_TOLERANCE   (10.0f)  /* Tolerance for intermediate compression (in meters) */

/* internal dynamic array for points */ 
typedef struct {
    gpx_point_t *data;
    size_t count;
    size_t cap;
} pts_buf_t;

/*!
    \brief          initialize points buffer
    \param[in/out]  b           : buffer to initialize
    \param[out]     none
    \retval         0 on success
*/
static int pts_buf_init(pts_buf_t *b) {
    b->data = NULL;
    b->count = 0;
    b->cap = 0; return 0;
}

/*!
    \brief          free points buffer resources
    \param[in/out]  b           : buffer to free
    \param[out]     none
    \retval         none
*/
static void pts_buf_free(pts_buf_t *b) {
    free(b->data);
    b->data = NULL;
    b->count = b->cap = 0;
}

/*!
    \brief          push a point into points buffer
    \param[in/out]  b           : buffer to push into
    \param[in]      p           : point to push
    \param[out]     none
    \retval         0 on success, -1 on error
*/
static int pts_buf_push(pts_buf_t *b, const gpx_point_t *p) {
    /* When streaming threshold is reached, actively compress to free space */
    if (b->count >= STREAM_COMPRESS_THRESHOLD) {
        size_t out_count = 0;
        int *keep = douglas_peucker_simplify(b->data, b->count, STREAM_COMPRESS_TOLERANCE, &out_count);
        if (keep) {
            if (out_count < b->count) {
                /* Reorganize data in-place, keeping key points */
                for (size_t i = 0; i < out_count; i++) {
                    b->data[i] = b->data[keep[i]];
                }
                b->count = out_count;
            }
            free(keep);
        }
    }

    if (b->count >= b->cap) {
        /* Change from exponential growth (cap * 2) to linear step (+64) to reduce memory allocation pressure */
        size_t ncap = b->cap ? b->cap + 64 : 64;
        gpx_point_t *n = (gpx_point_t*)realloc(b->data, ncap * sizeof(gpx_point_t));
        if (!n) {
            /* Emergency fallback: when realloc fails, try to compress current buffer as recovery */
            if (b->count > 10) {
                GPX_LOG("[Warn] pts_buf_push: realloc failed, trying emergency compression...\n");
                size_t out_count = 0;
                int *keep = douglas_peucker_simplify(b->data, b->count, STREAM_COMPRESS_TOLERANCE, &out_count);
                if (keep) {
                    if (out_count < b->count) {
                        for (size_t i = 0; i < out_count; i++) {
                            b->data[i] = b->data[keep[i]];
                        }
                        b->count = out_count;
                    }
                    free(keep);
                    
                    /* If compression freed up space, store point directly and return success */
                    if (b->count < b->cap) {
                        b->data[b->count++] = *p;
                        return 0;
                    }
                }
            }
            GPX_LOG("[Error] realloc size: %lu failed\n", ncap * sizeof(gpx_point_t));
            return -1;
        }
        b->data = n;
        b->cap = ncap;
    }
    b->data[b->count++] = *p;
    return 0;
}

/*!
    \brief          extract attribute value from tag string
    \param[in]      s           : input string containing tag
    \param[in]      attr        : attribute name to search (e.g. "lat=")
    \param[out]     out         : buffer to store attribute value
    \param[in]      outlen      : length of out buffer
    \retval         1 if found, 0 if not found
*/
static int extract_attr(const char *s, const char *attr, char *out, size_t outlen) {
    char *p = (char*)strstr(s, attr);
    if (!p) {
        GPX_LOG("[Error] %s not found\n", attr);
        return 0;
    }
    p += strlen(attr);
    // skip possible = and quotes
    while (*p && (*p == ' ' || *p == '=' || *p == '\'' || *p == '"')) {
        p++;
    }
    size_t i=0;
    while (*p && *p != '"' && *p != '\'' && *p != ' ' && *p != '>' && i+1<outlen) {
        out[i++] = *p++;
    }
    out[i]=0;
    return 1;
}

/*!
    \brief          extract inner text between <tag>value</tag>
    \param[in]      s           : input string
    \param[in]      open_tag    : opening tag (e.g. "<name>")
    \param[in]      close_tag   : closing tag (e.g. "</name>")
    \param[out]     out         : buffer to store extracted text
    \param[in]      outlen      : length of out buffer
    \retval         1 if success, 0 on failure
*/
static int extract_inner(const char *s,
                         const char *open_tag,
                         const char *close_tag,
                         char *out,
                         size_t outlen) {
    char *p = (char*)strstr(s, open_tag);
    if (!p) {
        GPX_LOG("[Error] %s not found\n", open_tag);
        return 0;
    }
    p += strlen(open_tag);
    char *q = (char*)strstr(p, close_tag);
    if (!q) {
        GPX_LOG("[Error] %s not found\n", close_tag);
        return 0;
    }
    size_t n = (size_t)(q - p);
    if (n >= outlen) {
        n = outlen - 1;
    }
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

/*!
    \brief          parse a GPX file and invoke callback per track segment
    \param[in]      path        : path to GPX file
    \param[in]      cb          : callback invoked per track segment
    \param[in,out]  user_ctx    : user context passed to callback
    \param[out]     none
    \retval         0 on success, non-zero on failure
*/
int gpx_parse_file(const char *path, trk_segment_cb_t cb, void *user_ctx) {
    FILE *f = fopen(path, "r");
    if (!f) {
        GPX_LOG("[Error] gpx_parse_file: fopen %s failed\n", path);
        return -1;
    }
    char line[1024];
    int in_trk = 0, in_trkseg = 0, in_trkpt = 0;
    char track_name[256] = {0};
    pts_buf_t buf;
    pts_buf_init(&buf);

    gpx_point_t cur_pt;
    while (fgets(line, sizeof(line), f)) {
        // strip leading spaces
        char *s = line;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
            s++;
        }
        if (!in_trk && strstr(s, "<trk>") ) {
            in_trk = 1;
            track_name[0]=0;
        }
        if (in_trk && !in_trkseg && strstr(s, "<trkseg>")) {
            in_trkseg = 1;
            pts_buf_free(&buf);
            pts_buf_init(&buf);
        }

        if (in_trk && strstr(s, "<name>") && strstr(s, "</name>")) {
            extract_inner(s, "<name>", "</name>", track_name, sizeof(track_name));
        }

        // start of trackpoint: <trkpt lat="..." lon="...">
        if (in_trkseg && strstr(s, "<trkpt")) {
            char latbuf[64]={0}, lonbuf[64]={0};
            extract_attr(s, "lat=", latbuf, sizeof(latbuf));
            extract_attr(s, "lon=", lonbuf, sizeof(lonbuf));
            cur_pt.lat = latbuf[0] ? atof(latbuf) : 0.0;
            cur_pt.lon = lonbuf[0] ? atof(lonbuf) : 0.0;
            cur_pt.ele = 0.0;
            in_trkpt = 1;
            // if the same line contains <ele>...</ele>
            if (strstr(s, "<ele>")) {
                char elebuf[64]={0};
                extract_inner(s, "<ele>", "</ele>", elebuf, sizeof(elebuf));
                cur_pt.ele = elebuf[0] ? atof(elebuf) : 0.0;
            }
            // if line also contains </trkpt> then push immediately
            if (strstr(s, "</trkpt>")) {
                /* Check if memory allocation failed, exit to prevent null pointer crash */
                if (pts_buf_push(&buf, &cur_pt) < 0) {
                    GPX_LOG("[Error] pts_buf_push failed\n");
                    break;
                }
                in_trkpt = 0;
            }
            continue;
        }

        if (in_trkpt) {
            // look for elevation
            if (strstr(s, "<ele>")) {
                char elebuf[64]={0};
                extract_inner(s, "<ele>", "</ele>", elebuf, sizeof(elebuf));
                cur_pt.ele = elebuf[0] ? atof(elebuf) : cur_pt.ele;
            }
            if (strstr(s, "</trkpt>")) {
                /* Check if memory allocation failed, exit */
                if (pts_buf_push(&buf, &cur_pt) < 0) {
                    GPX_LOG("[Error] pts_buf_push failed\n");
                    break;
                }
                in_trkpt = 0;
            }
            continue;
        }

        if (in_trkseg && strstr(s, "</trkseg>")) {
            // segment finished - invoke callback
            if (cb) {
                cb(track_name[0] ? track_name : "", buf.data, buf.count, user_ctx);
            }
            pts_buf_free(&buf);
            in_trkseg = 0;
        }

        if (in_trk && strstr(s, "</trk>")) { 
            in_trk = 0; 
        }
    }

    pts_buf_free(&buf);
    fclose(f);
    return 0;
}

/*!
    \brief          convert latitude/longitude to local XY meters (equirectangular)
    \param[in]      lat         : latitude in degrees
    \param[in]      lon         : longitude in degrees
    \param[in]      ref_lat     : reference latitude in degrees
    \param[out]     out_x       : resulting X in meters
    \param[out]     out_y       : resulting Y in meters
    \retval         none
*/
void latlon_to_xy_meters(double lat,
                         double lon,
                         double ref_lat,
                         double *out_x,
                         double *out_y) {
    double ref_lat_rad = ref_lat * (3.14159265358979323846/180.0);
    double m_per_deg_lat = 111132.92 - 559.82 * cos(2*ref_lat_rad) + 1.175 * cos(4*ref_lat_rad);
    double m_per_deg_lon = 111412.84 * cos(ref_lat_rad) - 93.5 * cos(3*ref_lat_rad);
    *out_x = (lon - 0.0) * m_per_deg_lon;
    *out_y = (lat - ref_lat) * m_per_deg_lat;
}
