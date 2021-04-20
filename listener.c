#include <stdio.h>
#include <wiringPi.h>
#include <curl/curl.h>

#include "global.h"
#include "gateway.h"

#define LISTENER_UPDATE_INTERVAL    30 // Minutes
#define LISTENER_LOOP_SLEEP         1000 // Milliseconds

size_t write_data( void *buffer, size_t size, size_t nmemb, void *userp )
{
    return size * nmemb;
}

void UploadListenerTelemetry( char *callsign, time_t gps_time, float gps_lat, float gps_lon, char *radio, char *antenna )
{
    char time_string[20];
    struct tm * time_info;

    time_info = localtime (&gps_time);
    strftime(time_string, sizeof(time_string), "%H:%M:%S", time_info);

    if ( Config.EnableHabitat )
    {
        CURL *curl;
        CURLcode res;
        char PostFields[300];
        char JsonData[200];

        /* In windows, this will init the winsock stuff */

        /* get a curl handle */
        curl = curl_easy_init(  );
        if ( curl )
        {
            // So that the response to the curl POST doesn;'t mess up my finely crafted display!
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_data );

            // Set the URL that is about to receive our POST
            curl_easy_setopt( curl, CURLOPT_URL,
                              "http://habitat.habhub.org/transition/listener_telemetry" );

            // Now specify the POST data
            sprintf( JsonData, "{\"latitude\": %f, \"longitude\": %f}",
                     gps_lat, gps_lon );
            sprintf( PostFields, "callsign=%s&time=%d&data=%s", callsign,
                     (int)gps_time, JsonData );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDS, PostFields );

            // Perform the request, res will get the return code
            res = curl_easy_perform( curl );

            // Check for errors
            if ( res == CURLE_OK )
            {
                LogMessage( "Uploaded listener %s %s,%f,%f\n",
                            callsign, time_string, gps_lat, gps_lon );
            }
            else
            {
                LogMessage( "curl_easy_perform() failed: %s\n",
                            curl_easy_strerror( res ) );
            }

            // always cleanup
            curl_easy_cleanup( curl );
        }

        /* In windows, this will init the winsock stuff */

        /* get a curl handle */
        curl = curl_easy_init(  );
        if ( curl )
        {
            // So that the response to the curl POST doesn;'t mess up my finely crafted display!
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_data );

            // Set the URL that is about to receive our POST
            curl_easy_setopt( curl, CURLOPT_URL,
                              "http://habitat.habhub.org/transition/listener_information" );

            // Now specify the POST data
            sprintf( JsonData, "{\"radio\": \"%s\", \"antenna\": \"%s\"}",
                     radio, antenna );
            sprintf( PostFields, "callsign=%s&time=%d&data=%s", callsign, (int)gps_time, JsonData );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDS, PostFields );

            // Perform the request, res will get the return code
            res = curl_easy_perform( curl );

            // Check for errors
            if ( res != CURLE_OK )
            {
                LogMessage( "curl_easy_perform() failed: %s\n",
                            curl_easy_strerror( res ) );
            }

            // always cleanup
            curl_easy_cleanup( curl );
        }

    }
}
void *ListenerLoop(void *ptr)
{
    (void) ptr;

    uint32_t LoopPeriod = LISTENER_UPDATE_INTERVAL*60*1000;		// So we upload listener at start, as well as every LISTENER_UPDATE_INTERVAL minutes thereafter

    while (1)
    {
        if (LoopPeriod > LISTENER_UPDATE_INTERVAL*60*1000)
        {
            UploadListenerTelemetry( Config.Tracker, time(NULL), Config.latitude, Config.longitude, Config.radio, Config.antenna );
            LoopPeriod = 0;
        }
        
        delay(LISTENER_LOOP_SLEEP);
        LoopPeriod += LISTENER_LOOP_SLEEP;
    }
}
