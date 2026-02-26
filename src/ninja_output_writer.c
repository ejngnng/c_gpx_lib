/******************************************************************
 *
 *  @file   : output_writer.c
 *  @brief  : source file of output writer
 *  @author : ninja
 *  @date   : 2026-02
 *
******************************************************************/
#include "ninja_output_writer.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*!
    \brief          write GPX header to file
    \param[in]      f           : output FILE pointer
    \param[in]      orig_name   : original track name (optional)
    \param[out]     none
    \retval         0 on success, -1 on error
*/
int gpx_write_header(FILE *f, const char *orig_name) {
    if (!f) {
        return -1;
    }
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<gpx version=\"1.1\" creator=\"gpx_lib\">\n");
    if (orig_name && orig_name[0]) {
        fprintf(f, "  <metadata><name>%s</name></metadata>\n", orig_name);
    }
    return 0;
}

/*!
    \brief          write GPX footer to file
    \param[in]      f           : output FILE pointer
    \param[out]     none
    \retval         0 on success, -1 on error
*/
int gpx_write_footer(FILE *f) {
    if (!f) {
        return -1;
    }
    fprintf(f, "</gpx>\n");
    return 0;
}

/*!
    \brief          write a track segment (trkseg) to GPX file
    \param[in]      f           : output FILE pointer
    \param[in]      track_name  : track name (optional)
    \param[in]      pts         : array of points
    \param[in]      n           : number of points
    \param[out]     none
    \retval         0 on success, -1 on error
*/
int gpx_write_track_segment(FILE *f, 
                            const char *track_name, 
                            const gpx_point_t *pts, 
                            size_t n) {
    if (!f || !pts) return -1;
    fprintf(f, "  <trk>\n");
    if (track_name && track_name[0]) {
        fprintf(f, "    <name>%s</name>\n", track_name);
    }
    fprintf(f, "    <trkseg>\n");
    for (size_t i=0;i<n;i++) {
        fprintf(f, "      <trkpt lat=\"%.8f\" lon=\"%.8f\">\n", pts[i].lat, pts[i].lon);
        fprintf(f, "        <ele>%.3f</ele>\n", pts[i].ele);
        fprintf(f, "      </trkpt>\n");
    }
    fprintf(f, "    </trkseg>\n");
    fprintf(f, "  </trk>\n");
    return 0;
}

/*!
    \brief          write 32-bit little-endian value to file
    \param[in]      f           : output FILE pointer
    \param[in]      v           : value to write
    \param[out]     none
    \retval         none
*/
static void put_le32(FILE *f, uint32_t v) {
    uint8_t b[4];
    b[0] = v & 0xFF; 
    b[1] = (v>>8)&0xFF; 
    b[2] = (v>>16)&0xFF; 
    b[3] = (v>>24)&0xFF;
    fwrite(b,1,4,f);
}

/*!
    \brief          write a binary cache file for a track segment
    \param[in]      path        : output file path
    \param[in]      track_name  : track name (optional)
    \param[in]      pts         : array of points
    \param[in]      n           : number of points
    \param[in]      coord_scale : multiplier for coord scaling
    \param[in]      ele_scale   : multiplier for elevation scaling
    \param[out]     none
    \retval         0 on success, -1 on error
*/
int write_cache_bin(const char *path, 
                    const char *track_name, 
                    const gpx_point_t *pts, 
                    size_t n, 
                    int32_t coord_scale, 
                    int32_t ele_scale) {
    if (!path || !pts) {
        return -1;
    }
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t name_len = (uint32_t)(track_name ? strlen(track_name) : 0);
    put_le32(f, name_len);
    if (name_len) {
        fwrite(track_name,1,name_len,f);
    }
    put_le32(f, (uint32_t)n);
    for (size_t i=0;i<n;i++) {
        int32_t lon_i = (int32_t)llround(pts[i].lon * coord_scale);
        int32_t lat_i = (int32_t)llround(pts[i].lat * coord_scale);
        int32_t ele_i = (int32_t)llround(pts[i].ele * ele_scale);
        put_le32(f, (uint32_t)lon_i);
        put_le32(f, (uint32_t)lat_i);
        put_le32(f, (uint32_t)ele_i);
    }
    fclose(f);
    return 0;
}

/*!
    \brief          print point count stats before/after simplification
    \param[in]      before      : number of points before simplification
    \param[in]      after       : number of points after simplification
    \param[out]     none
    \retval         none
*/
void gpx_print_stats(size_t before, size_t after) {
    printf("===============================\n");
    if (before == 0) {
        printf("Points before      : %lu \n", before);
        printf("Points after       : %lu \n", after);
        printf("compression ratio  : N/A\n");
        return;
    }
    double ratio = (double)after / (double)before;  
    double percent = ratio * 100.0;
    double reduction = (1.0 - ratio) * 100.0;
    printf("Points before      : %lu \n", before);
    printf("Points after       : %lu \n", after);
    printf("compression ratio  : %.2f%% \n", percent);
    printf("reduction          : %.2f%% \n", reduction);
}
