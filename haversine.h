#ifndef _HAVERSINE_H
#define _HAVERSINE_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define R 6371
#define TO_RAD (3.1415926536 / 180)

double haversine_distance( double lat1, double lng1, double lat2,
                           double lng2 );
double haversine_bearing( double lat1, double lng1, double lat2,
                          double lng2 );
double haversine_elevation( double lat1, double lng1, double alt1,
                            double lat2, double lng2, double alt2 );

#endif
