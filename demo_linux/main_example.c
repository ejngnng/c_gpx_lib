/******************************************************************
 *
 *  @file   : main_example.c
 *  @brief  : example of GPX parser example application
 *            simplify each segment, write output
 *  @author : ninja
 *  @date   : 2026-02
 *
 ******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ninja_gpx_parser.h"
#include "ninja_douglas_peucker.h"
#include "ninja_output_writer.h"
#include "ninja_gpx_log.h"

typedef struct {
    FILE *out_gpx;
    char out_gpx_path[260];
    int write_gpx;
    int write_cache;
    char cache_path_fmt[260];
    double tol_m;
    int cache_coord_scale;
    int cache_ele_scale;
    size_t seg_index;
} app_ctx_t;

/*!
    \brief          callback invoked per track segment during parsing
    \param[in]      track_name  : name of the track
    \param[in,out]  pts         : points of the segment
    \param[in]      count       : number of points
    \param[in,out]  user_ctx    : application context pointer
    \param[out]     none
    \retval         none
*/
static void segment_cb(const char *track_name, 
                       gpx_point_t *pts, 
                       size_t count, 
                       void *user_ctx) {
    app_ctx_t *ctx = (app_ctx_t*)user_ctx;
    if (!ctx) {
        return;
    }
    if (count == 0) {
        return;
    }
    size_t out_count = 0;
    int *kept = douglas_peucker_simplify(pts, count, ctx->tol_m, &out_count);
    if (!kept) {
        return;
    }
    // build simplified array
    gpx_point_t *simpl = (gpx_point_t*)malloc(out_count * sizeof(gpx_point_t));
    if (!simpl) { 
        free(kept); 
        return; 
    }

    for (size_t i=0;i<out_count;i++) {
        simpl[i] = pts[(size_t)kept[i]];
    }

    if (ctx->write_gpx && ctx->out_gpx) {
        gpx_write_track_segment(ctx->out_gpx, track_name, simpl, out_count);
    }

    if (ctx->write_cache) {
        char cache_path[300];
        snprintf(cache_path, 
                 sizeof(cache_path), 
                 ctx->cache_path_fmt, 
                 (unsigned int)ctx->seg_index);

        write_cache_bin(cache_path, 
                        track_name, 
                        simpl, 
                        out_count, 
                        ctx->cache_coord_scale, 
                        ctx->cache_ele_scale);
    }
    // print stats: before/after and compression ratio
    gpx_print_stats(count, out_count);
    ctx->seg_index++;
    free(simpl); 
    free(kept);
}

/*!
    \brief          program entry point for gpx_simplify example
    \param[in]      argc        : argument count
    \param[in]      argv        : argument vector
    \param[out]     none
    \retval         0 on success, non-zero on error
*/
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input.gpx> [--output out.gpx] [--cache cache_fmt] [--tol meters]\n", argv[0]);
        return 1;
    }
    const char *inpath = argv[1];
    const char *outpath = NULL;
    const char *cache_fmt = NULL;
    double tol = 10.0;
    for (int i=2; i<argc; i++) {
        if (!strcmp(argv[i], "--output") && i+1<argc) { 
            outpath = argv[++i]; 
        } else if (!strcmp(argv[i], "--cache") && i+1<argc) { 
            cache_fmt = argv[++i]; 
        } else if (!strcmp(argv[i], "--tol") && i+1<argc) { 
            tol = atof(argv[++i]); 
        }
    }

    app_ctx_t ctx;
    memset(&ctx,0,sizeof(ctx));
    ctx.tol_m = tol;
    ctx.cache_coord_scale = 10000000; // 1e7 degrees -> int32
    ctx.cache_ele_scale = 1000; // meters * 1000 -> millimeters
    ctx.write_gpx = outpath ? 1 : 0;
    ctx.write_cache = cache_fmt ? 1 : 0;
    if (ctx.write_gpx) {
        strncpy(ctx.out_gpx_path, outpath, sizeof(ctx.out_gpx_path)-1);
        ctx.out_gpx = fopen(outpath, "w");
        if (!ctx.out_gpx) { 
            GPX_LOG("[Error] fopen out: %s\n", outpath); 
            return 2; 
        }
        gpx_write_header(ctx.out_gpx, NULL);
    }
    if (ctx.write_cache) {
        strncpy(ctx.cache_path_fmt, cache_fmt, sizeof(ctx.cache_path_fmt)-1);
    }

    int rc = gpx_parse_file(inpath, segment_cb, &ctx);

    if (ctx.write_gpx && ctx.out_gpx) {
        gpx_write_footer(ctx.out_gpx);
        fclose(ctx.out_gpx);
    }

    if (rc) {
        GPX_LOG("[Error] Failed to parse GPX file: %s\n", inpath);
        return 3;
    }
    GPX_LOG("Processed %s done.\n", inpath);
    return 0;
}
