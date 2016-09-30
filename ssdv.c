#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>              // Standard input/output definitions
#include <string.h>             // String function definitions
#include <unistd.h>             // UNIX standard function definitions
#include <fcntl.h>              // File control definitions
#include <errno.h>              // Error number definitions
#include <termios.h>            // POSIX terminal control definitions
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <curl/curl.h>
#include <wiringPi.h>

#include "urlencode.h"
#include "base64.h"
#include "ssdv.h"
#include "gateway.h"
#include "global.h"

extern int ssdv_pipe_fd[2];
extern pthread_mutex_t var;

size_t
write_ssdv_data( void *buffer, size_t size, size_t nmemb, void *userp )
{
    return size * nmemb;
}


void
ConvertStringToHex( unsigned char *Target, unsigned char *Source, int Length )
{
    const char Hex[16] = "0123456789ABCDEF";
    int i;

    for ( i = 0; i < Length; i++ )
    {
        *Target++ = Hex[Source[i] >> 4];
        *Target++ = Hex[Source[i] & 0x0F];
    }

    *Target++ = '\0';
}


void
UploadImagePacket( ssdv_t * s, unsigned int packets )
{
    CURL *curl;
    CURLcode res;
    char curl_error[CURL_ERROR_SIZE];
    char base64_data[512], json[32768], packet_json[1000];
    struct curl_slist *headers = NULL;
    size_t base64_length;
    char now[32];
    time_t rawtime;
    struct tm *tm;
    char url[250];

    /* get a curl handle */
    curl = curl_easy_init(  );
    if ( curl )
    {
        // So that the response to the curl POST doesn;'t mess up my finely crafted display!
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_ssdv_data );

        // Set the timeout
        curl_easy_setopt( curl, CURLOPT_TIMEOUT, 15 );

        // RJH capture http errors and report
        curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1 );
        curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, curl_error );

        // Avoid curl library bug that happens if above timeout occurs (sigh)
        curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );

        // Get formatted timestamp
        time( &rawtime );
        tm = gmtime( &rawtime );
        strftime( now, sizeof( now ), "%Y-%0m-%0dT%H:%M:%SZ", tm );

        int PacketIndex;

        // Create json with the base64 data in hex, the tracker callsign and the current timestamp
        strcpy( json, "{\"type\": \"packets\",\"packets\":[" );

        for (PacketIndex = 0; PacketIndex < packets; PacketIndex++)
        {
            base64_encode(s[PacketIndex].SSDV_Packet, 256, &base64_length, base64_data);
            base64_data[base64_length] = '\0';

            sprintf(packet_json,
                    "{\"type\": \"packet\", \"packet\": \"%s\", \"encoding\": \"base64\", \"received\": \"%s\", \"receiver\": \"%s\"}%s",
                    base64_data, now, Config.Tracker,
                    PacketIndex == ( packets - 1 ) ? "" : ",");
            strcat(json, packet_json);
        }
        strcat(json, "]}");

        // LogTelemetryPacket(json);

        strcpy( url, "http://ssdv.habhub.org/api/v0/packets" );

        // Set the headers
        headers = NULL;
        headers = curl_slist_append( headers, "Accept: application/json" );
        headers =
            curl_slist_append( headers, "Content-Type: application/json" );
        headers = curl_slist_append( headers, "charsets: utf-8" );

        curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
        curl_easy_setopt( curl, CURLOPT_URL, url );

        curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "POST" );
        curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json );

        // Perform the request, res will get the return code
        res = curl_easy_perform( curl );

        /* Check for errors */
        if ( res == CURLE_OK )
        {
        }
        else
        {
            LogMessage( "Failed for URL '%s'\n", url );
            LogMessage( "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror( res ) );
            LogMessage( "error: %s\n", curl_error );
        }

        /* always cleanup */
        curl_slist_free_all( headers ); // RJH Added this from habitat.c as was missing
        curl_easy_cleanup( curl );
    }
}

void *SSDVLoop( void *vars )
{
    if ( Config.EnableSSDV )
    {
        const int max_packets = 51;
        thread_shared_vars_t *stsv;
        stsv = vars;
        ssdv_t s[max_packets];
        unsigned int j = 0;
        unsigned int packets = 0;
        unsigned long total_packets = 0;

        // Keep looping until the parent quits and there are no more packets to
        // send to ssdv.
        while ( ( stsv->parent_status == RUNNING ) || ( packets > 0 ) )
        {

            if ( stsv->packet_count > total_packets )
            {
                packets = read( ssdv_pipe_fd[0], &s[j], sizeof( ssdv_t ) );
            }
            else
            {
                packets = 0;

                // If we have have a rollover after processing 4294967295 packets
                if ( stsv->packet_count < total_packets )
                    total_packets = 0;

            }

            if ( packets )
            {
                j++;
                total_packets++;
            }

            if ( j == 50 || ( ( packets == 0 ) && ( j > 0 ) ) )
            {
				ChannelPrintf(s[0].Channel, 6, 16, "SSDV");
				
                UploadImagePacket( s, j );
				
				ChannelPrintf(s[0].Channel, 6, 16, "    ");

                j = 0;

                packets = 0;
            }
			delay(100);			// Don't eat too much CPU
        }
    }

    close( ssdv_pipe_fd[0] );
    close( ssdv_pipe_fd[1] );

    LogMessage( "SSDV thread closing\n" );

    return NULL;

}
