#!/usr/bin/env python3
import argparse
import os
import math
import gpxpy
import gpxpy.gpx
import matplotlib.pyplot as plt
import numpy as np

# Convert lat/lon to local meters using equirectangular approximation
def latlon_to_xy(lat, lon, ref_lat):
    """Convert latitude/longitude to approximate local x,y in meters.

    Uses an equirectangular-like approximation (scaled degrees to meters)
    based on the reference latitude to compute meters-per-degree.

    Args:
        lat (float): Latitude in degrees.
        lon (float): Longitude in degrees.
        ref_lat (float): Reference latitude in degrees used to scale
            longitudinal distances.

    Returns:
        (float, float): Tuple (x, y) in meters. x is longitudinal offset
        (meters) and y is latitudinal offset (meters) relative to
        the provided reference latitude.
    """
    lat_rad = math.radians(lat)
    lon_rad = math.radians(lon)
    ref_lat_rad = math.radians(ref_lat)
    m_per_deg_lat = 111132.92 - 559.82 * math.cos(2 * ref_lat_rad) + 1.175 * math.cos(4 * ref_lat_rad)
    m_per_deg_lon = 111412.84 * math.cos(ref_lat_rad) - 93.5 * math.cos(3 * ref_lat_rad)
    x = (lon - 0.0) * m_per_deg_lon
    y = (lat - ref_lat) * m_per_deg_lat
    return x, y

def point_line_distance(pt, start, end):
    """Compute distance from a point to a line segment.

    Args:
        pt (tuple): (x, y) point coordinates.
        start (tuple): (x1, y1) start of the segment.
        end (tuple): (x2, y2) end of the segment.

    Returns:
        float: Shortest distance from the point to the segment.
    """
    (x, y) = pt
    (x1, y1) = start
    (x2, y2) = end
    dx = x2 - x1
    dy = y2 - y1
    if dx == 0 and dy == 0:
        return math.hypot(x - x1, y - y1)
    t = ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    proj_x = x1 + t * dx
    proj_y = y1 + t * dy
    return math.hypot(x - proj_x, y - proj_y)

def douglas_peucker(points_xy, tol):
    """Perform Douglas-Peucker simplification on a polyline.

    This implementation returns the indices of points to keep from the
    original list rather than constructing the simplified list directly.

    Args:
        points_xy (list): List of (x, y) tuples representing the polyline.
        tol (float): Tolerance (in same units as points) — maximum allowed
            perpendicular distance for intermediate points to be discarded.

    Returns:
        list: Sorted list of indices of points_xy that should be kept.
    """
    if len(points_xy) <= 2:
        return list(range(len(points_xy)))
    stack = [(0, len(points_xy) - 1)]
    keep = set([0, len(points_xy) - 1])
    while stack:
        s, e = stack.pop()
        max_dist = -1.0
        index = None
        for i in range(s + 1, e):
            d = point_line_distance(points_xy[i], points_xy[s], points_xy[e])
            if d > max_dist:
                max_dist = d
                index = i
        if max_dist is not None and max_dist > tol:
            keep.add(index)
            stack.append((s, index))
            stack.append((index, e))
    kept_indices = sorted(list(keep))
    return kept_indices


def simplify_trackpoints(trackpoints, tol_meters):
    """Simplify a list of GPX trackpoints using Douglas-Peucker.

    Converts latitude/longitude to local meters using a mean reference
    latitude, applies Douglas-Peucker with the provided tolerance, and
    returns the simplified list of original trackpoint objects (preserving
    elevation/time attributes).

    Args:
        trackpoints (list): Iterable of GPXTrackPoint-like objects with
            `latitude` and `longitude` attributes.
        tol_meters (float): Simplification tolerance in meters.

    Returns:
        list: Subset of the original `trackpoints` that are kept after
        simplification, in the original order.
    """
    if not trackpoints:
        return []
    lats = [p.latitude for p in trackpoints]
    lons = [p.longitude for p in trackpoints]
    ref_lat = sum(lats) / len(lats)
    pts_xy = [latlon_to_xy(lat, lon, ref_lat) for lat, lon in zip(lats, lons)]
    kept_idx = douglas_peucker(pts_xy, tol_meters)
    simplified = [trackpoints[i] for i in kept_idx]
    return simplified


def process_gpx(infile, outfile, tol):
    """Read a GPX file, simplify tracks, write simplified GPX and PNG.

    The function parses `infile`, simplifies each track segment using the
    provided tolerance (meters), writes a new GPX file to `outfile`, and
    also produces a comparison PNG plotting original vs simplified points.

    Args:
        infile (str): Path to input GPX file.
        outfile (str): Path where simplified GPX will be written.
        tol (float): Simplification tolerance in meters.
    """
    with open(infile, 'r', encoding='utf-8') as f:
        gpx = gpxpy.parse(f)
    new_gpx = gpxpy.gpx.GPX()
    # copy metadata where available
    new_gpx.name = gpx.name
    new_gpx.description = gpx.description
    # Build robust track name list: prefer track-level <name>, then GPX metadata name,
    # then any segment/point-level name found, otherwise default to "Track N".
    track_names = []
    for i, t in enumerate(gpx.tracks):
        name = None
        if getattr(t, 'name', None):
            name = t.name
        elif getattr(gpx, 'name', None):
            name = gpx.name
        else:
            # try to find a name in segments or points
            for seg in t.segments:
                if getattr(seg, 'name', None):
                    name = seg.name
                    break
                for p in getattr(seg, 'points', []):
                    if getattr(p, 'name', None):
                        name = p.name
                        break
                if name:
                    break
        if not name:
            name = f'Track {i+1}'
        track_names.append(name)

    all_original_lats = []
    all_original_lons = []
    all_simpl_lats = []
    all_simpl_lons = []

    for track in gpx.tracks:
        new_track = gpxpy.gpx.GPXTrack(name=track.name)
        for segment in track.segments:
            new_segment = gpxpy.gpx.GPXTrackSegment()
            # simplify this segment
            pts = segment.points
            simplified_pts = simplify_trackpoints(pts, tol)
            for p in simplified_pts:
                new_segment.points.append(gpxpy.gpx.GPXTrackPoint(p.latitude, p.longitude, elevation=p.elevation, time=p.time))
            new_track.segments.append(new_segment)
            # for plotting, collect combined lists
            all_original_lats.extend([p.latitude for p in pts])
            all_original_lons.extend([p.longitude for p in pts])
            all_simpl_lats.extend([p.latitude for p in simplified_pts])
            all_simpl_lons.extend([p.longitude for p in simplified_pts])
        new_gpx.tracks.append(new_track)

    # write output GPX
    with open(outfile, 'w', encoding='utf-8') as f:
        f.write(new_gpx.to_xml())

    # compute counts and compression percentage
    orig_count = len(all_original_lats)
    simpl_count = len(all_simpl_lats)
    compression_pct = (1.0 - (simpl_count / orig_count)) * 100.0 if orig_count > 0 else 0.0

    # plotting (improved colors and track name annotation)
    plt.figure(figsize=(10, 8))
    # original: subtle light slate, thin and semi-transparent
    plt.plot(all_original_lons, all_original_lats, '-', color="#67CC90", linewidth=8.0, alpha=0.6, label='original')
    # simplified: vivid orange-red with black edge on markers for contrast
    plt.plot(all_simpl_lons, all_simpl_lats, '-o', color='#D84315', linewidth=2.0, markersize=5, label='simplified', markeredgecolor='k', markeredgewidth=0.6)
    plt.xlabel('Longitude')
    plt.ylabel('Latitude')
    track_names_str = ', '.join(track_names) if track_names else 'Track'
    plt.title(f'GPX Simplify: tol={tol} m | original pts={orig_count} simplified pts={simpl_count} | reduced {compression_pct:.1f}%')
    # add track name(s) in the upper-left corner of the plot
    plt.gca().text(0.01, 0.99, track_names_str, transform=plt.gca().transAxes,
                   verticalalignment='top', horizontalalignment='left', fontsize=10,
                   bbox=dict(facecolor='white', alpha=0.8, edgecolor='none'))
    # add a prominent compression percentage in the upper-right
    plt.gca().text(0.99, 0.99, f'Compressed\n{compression_pct:.1f}%', transform=plt.gca().transAxes,
                   verticalalignment='top', horizontalalignment='right', fontsize=14, fontweight='bold',
                   bbox=dict(facecolor='white', alpha=0.9, edgecolor='none'))
    plt.legend()
    plt.grid(True)
    outdir = os.path.dirname(outfile) or '.'
    base = os.path.splitext(os.path.basename(outfile))[0]
    pngpath = os.path.join(outdir, f'comparison_{base}.png')
    plt.savefig(pngpath, dpi=150, bbox_inches='tight')
    plt.close()

    print(f'Wrote simplified GPX: {outfile}')
    print(f'Wrote comparison PNG: {pngpath}')


def main():
    # Usage:
    #   python compress_gpx.py <input.gpx> [--output <out.gpx>] [--tolerance <meters>]
    #
    # Arguments:
    #   input            Path to input GPX file.
    #
    # Options:
    #   -o, --output     Output simplified GPX path (defaults to <input>_simplified.gpx).
    #   -t, --tolerance  Simplification tolerance in meters (default 10.0).
    #
    # Output:
    #   - simplified GPX file at the output path
    #   - comparison PNG 'comparison_<output_basename>.png' showing original vs simplified tracks
    #
    # Example usage:
    #   python3 ./compress_gpx.py ./input.gpx" --output "out.gpx" --t 10
    parser = argparse.ArgumentParser(description='Simplify GPX tracks using Douglas-Peucker (tolerance in meters).')
    parser.add_argument('input', help='Input GPX file path')
    parser.add_argument('--output', '-o', help='Output simplified GPX file path', default=None)
    parser.add_argument('--tolerance', '-t', type=float, default=10.0, help='Tolerance in meters (default 10)')
    args = parser.parse_args()
    infile = args.input
    outfile = args.output or (os.path.splitext(infile)[0] + '_simplified.gpx')
    process_gpx(infile, outfile, args.tolerance)

if __name__ == '__main__':
    main()
