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
#include <stddef.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <curl/curl.h>
#include <wiringPi.h>

#include "base64.h"
#include "habitat.h"
#include "global.h"
#include "sha256.h"
#include "wiringPi.h"
#include "gateway.h"
#include "lifo_buffer.h"

extern lifo_buffer_t Habitat_Upload_Buffer;

extern int telem_pipe_fd[2];
extern pthread_mutex_t var;
extern void ChannelPrintf( int Channel, int row, int column,
                           const char *format, ... );

size_t habitat_write_data( void *buffer, size_t size, size_t nmemb, void *userp )
{
    // LogMessage("%s\n", (char *)buffer);
    return size * nmemb;
}

void hash_to_hex( unsigned char *hash, char *line )
{
    int idx;

    for ( idx = 0; idx < 32; idx++ )
    {
        sprintf( &( line[idx * 2] ), "%02x", hash[idx] );
    }
    line[64] = '\0';
}

bool UploadTelemetryPacket( received_t * t )
{
    CURL *curl;
    CURLcode res;
    char curl_error[CURL_ERROR_SIZE];

    /* get a curl handle */
    curl = curl_easy_init(  );
    if ( curl )
    {
        bool result;
        char url[200];
        char base64_data[1000];
        size_t base64_length;
        SHA256_CTX ctx;
        unsigned char hash[32];
        char doc_id[100];
        char json[1000], now[32], doc_time[32];
        char Sentence[512];
        struct curl_slist *headers = NULL;
        time_t rawtime;
        struct tm *tm, *doc_tm;
		int retries;
		long int http_resp;

        // Get formatted timestamp for now
        time( &rawtime );
        tm = gmtime( &rawtime );
        strftime( now, sizeof( now ), "%Y-%0m-%0dT%H:%M:%SZ", tm );

        // Get formatted timestamp for doc timestamp
        doc_tm = gmtime( &t->Metadata.Timestamp );
        strftime( doc_time, sizeof( doc_time ), "%Y-%0m-%0dT%H:%M:%SZ", doc_tm );

        // Grab current telemetry string and append a linefeed
        sprintf(Sentence, "%s\n", t->HabitatString);

        // Convert sentence to base64
        base64_encode( Sentence, strlen( Sentence ), &base64_length,
                       base64_data );
        base64_data[base64_length] = '\0';

        // Take SHA256 hash of the base64 version and express as hex.  This will be the document ID
        sha256_init( &ctx );
        sha256_update( &ctx, base64_data, base64_length );
        sha256_final( &ctx, hash );
        hash_to_hex( hash, doc_id );

        // Create json with the base64 data in hex, the tracker callsign and the current timestamp
        sprintf( json,
                 "{\"data\": {\"_raw\": \"%s\"},\"receivers\": {\"%s\": {\"time_created\": \"%s\",\"time_uploaded\": \"%s\",\"rig_info\": {\"frequency\":%.0f}}}}",
                 base64_data, Config.Tracker, doc_time, now, (t->Metadata.Frequency + t->Metadata.FrequencyError) * 1000000 );

        // Set the URL that is about to receive our PUT
        sprintf( url, "http://habitat.habhub.org/habitat/_design/payload_telemetry/_update/add_listener/%s", doc_id);

        // So that the response to the curl PUT doesn't mess up my finely crafted display!
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, habitat_write_data );

        // Set the timeout
        curl_easy_setopt( curl, CURLOPT_TIMEOUT, 15 );

        // RJH capture http errors and report
        // curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1 );
        curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, curl_error );

        // Avoid curl library bug that happens if above timeout occurs (sigh)
        curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );

        // Set the headers
        headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "charsets: utf-8" );

        // PUT to http://habitat.habhub.org/habitat/_design/payload_telemetry/_update/add_listener/<doc_id> with content-type application/json
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

		retries = 0;
		do
		{
			// Perform the request, res will get the return code
			res = curl_easy_perform( curl );

			// Check for errors
			if ( res == CURLE_OK )
			{
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_resp);
				if (http_resp != 201 && http_resp != 403 && http_resp != 409)
				{
					LogMessage("Unexpected HTTP response %ld for URL '%s'\n", http_resp, url);
                    result = false;
				}
                else
                {
                    /* Everything performing nominally (even if we didn't successfully insert this time) */
                    result = true;
                }
			}
			else
			{
				http_resp = 0;
				LogMessage("Failed for URL '%s'\n", url);
				LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				LogMessage("error: %s\n", curl_error);
                /* Likely a network error, so return false to requeue */
                result = false;
			}
			
			if (http_resp == 409)
			{
				// conflict between us and another uploader at the same time
				// wait for a random period before trying again
				delay(random() & 255);		// 0-255 ms
			}
		} while ((http_resp == 409) && (++retries < 5));

        // always cleanup
        curl_slist_free_all( headers );
        curl_easy_cleanup( curl );

        return result;
    }
    else
    {
        /* CURL error, return false so we requeue */
        return false;
    }
}


void *HabitatLoop( void *vars )
{
    if ( Config.EnableHabitat )
    {
        received_t *dequeued_telemetry_ptr;

        // Keep looping until the parent quits
        while ( true )
        {
            dequeued_telemetry_ptr = lifo_buffer_waitpop(&Habitat_Upload_Buffer);

            if(dequeued_telemetry_ptr != NULL)
            {
                ChannelPrintf( dequeued_telemetry_ptr->Metadata.Channel, 6, 1, "Habitat (%d queued)", lifo_buffer_queued(&Habitat_Upload_Buffer) );

                if(UploadTelemetryPacket( dequeued_telemetry_ptr ))
                {
                    ChannelPrintf( dequeued_telemetry_ptr->Metadata.Channel, 6, 1, "                   " );
                    free(dequeued_telemetry_ptr);
                }
                else
                {
                    /* Network / CURL Error, requeue packet */
                    ChannelPrintf( dequeued_telemetry_ptr->Metadata.Channel, 6, 1, "Habitat Net Error! " );

                    if(!lifo_buffer_requeue(&Habitat_Upload_Buffer, dequeued_telemetry_ptr))
                    {
                        /* Requeue failed, drop packet */
                        free(dequeued_telemetry_ptr);
                    }
                }
            }
            else
            {
                /* NULL returned: We've been asked to quit */
                /* Don't bother free()ing stuff, as application is quitting */
                break;
            }
        }
    }

    return NULL;
}
