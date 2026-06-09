/* Great-circle distance and bearing on a sphere. Portable, host-tested. */
#ifndef UTIL_GEO_H
#define UTIL_GEO_H

/* Distance in metres between two lat/lon points (haversine). */
double geo_distance_m(double lat1, double lon1, double lat2, double lon2);

/* Initial bearing from point 1 to point 2, in degrees clockwise from true
 * north (0..360). */
double geo_bearing_deg(double lat1, double lon1, double lat2, double lon2);

#endif /* UTIL_GEO_H */
