#include "rfmxx.h"

TLoRaMode LoRaModes[] = {
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_20K8, SPREADING_11, 1, 60,
     "Telemetry"}
    ,                           // 0: Normal mode for telemetry
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_20K8, SPREADING_6, 0, 1400,
     "SSDV"}
    ,                           // 1: Normal mode for SSDV
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_62K5, SPREADING_8, 0, 2000,
     "Repeater"}
    ,                           // 2: Normal mode for repeater network  
    {EXPLICIT_MODE, ERROR_CODING_4_6, BANDWIDTH_250K, SPREADING_7, 0, 8000,
     "Turbo"}
    ,                           // 3: Normal mode for high speed images in 868MHz band
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_250K, SPREADING_6, 0, 16828,
     "TurboX"}
    ,                           // 4: Fastest mode within IR2030 in 868MHz band
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_41K7, SPREADING_11, 0, 200,
     "Calling"}
    ,                           // 5: Calling mode
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_41K7, SPREADING_6, 0, 2800,
     "Uplink"}                  // 6: Uplink mode for 868
};

TBandwidth Bandwidths[] = {
    {BANDWIDTH_7K8, 7.8, "7K8"}
    ,
    {BANDWIDTH_10K4, 10.4, "10K4"}
    ,
    {BANDWIDTH_15K6, 15.6, "15K6"}
    ,
    {BANDWIDTH_20K8, 20.8, "20K8"}
    ,
    {BANDWIDTH_31K25, 31.25, "31K25"}
    ,
    {BANDWIDTH_41K7, 41.7, "41K7"}
    ,
    {BANDWIDTH_62K5, 62.5, "62K5"}
    ,
    {BANDWIDTH_125K, 125.0, "125K"}
    ,
    {BANDWIDTH_250K, 250.0, "250K"}
    ,
    {BANDWIDTH_500K, 500.0, "500K"}
};

int
rfmxx_NumberOfLoRaModes( void )
{
    return ( sizeof( LoRaModes ) / sizeof( TLoRaMode ) );
}
