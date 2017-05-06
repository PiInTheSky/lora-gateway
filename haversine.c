#include "haversine.h"

double
haversine_distance( double lat1, double lng1, double lat2, double lng2 )
{
    double dx, dy, dz;
    double d_lng = lng1 - lng2;

    d_lng *= TO_RAD, lat1 *= TO_RAD, lat2 *= TO_RAD;

    dz = sin( lat1 ) - sin( lat2 );
    dx = cos( d_lng ) * cos( lat1 ) - cos( lat2 );
    dy = sin( d_lng ) * cos( lat1 );
    return asin( sqrt( dx * dx + dy * dy + dz * dz ) / 2 ) * 2 * R * 1000;
}

double
haversine_bearing( double lat1, double lng1, double lat2, double lng2 )
{
    double bearing_rad;
    double bearing_deg;
    double d_lng = lng2 - lng1;

    d_lng *= TO_RAD, lat1 *= TO_RAD, lat2 *= TO_RAD;

    bearing_rad =
        atan2( ( sin( d_lng ) * cos( lat2 ) ),
               ( ( cos( lat1 ) * sin( lat2 ) ) -
                 ( sin( lat1 ) * cos( lat2 ) * cos( d_lng ) ) ) );

    bearing_deg = bearing_rad * ( 180.0 / M_PI );

    if ( bearing_deg < 0 )
        bearing_deg += 360;

    return bearing_deg;
}

double
haversine_elevation( double lat1, double lng1, double alt1, double lat2,
                     double lng2, double alt2 )
{
    double elevation_rad;
    double elevation_deg;
    double d_lng = lng2 - lng1;

    double radius, sa, sb, aa, ab, angle_at_centre, ta, tb, ea, eb;

    radius = 6371000.0;

    d_lng *= TO_RAD, lat1 *= TO_RAD, lat2 *= TO_RAD;

    sa = cos( lat2 ) * sin( d_lng );
    sb = ( cos( lat1 ) * sin( lat2 ) ) -
        ( sin( lat1 ) * cos( lat2 ) * cos( d_lng ) );

    aa = sqrt( ( sa * sa ) + ( sb * sb ) );
    ab = ( sin( lat1 ) * sin( lat2 ) ) +
        ( cos( lat1 ) * cos( lat2 ) * cos( d_lng ) );

    angle_at_centre = atan2( aa, ab );

    ta = radius + alt1;
    tb = radius + alt2;
    ea = ( cos( angle_at_centre ) * tb ) - ta;
    eb = sin( angle_at_centre ) * tb;
    elevation_rad = atan2( ea, eb );

    elevation_deg = elevation_rad * ( 180.0 / M_PI );

    return elevation_deg;
}
