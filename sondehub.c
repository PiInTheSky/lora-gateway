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

#include "global.h"
#include "gateway.h"
#include "sondehub.h"

struct TPayload SondehubPayloads[2];

void SetSondehubSentence(int Channel, char *tmp)
{
	sscanf(tmp + 2, "%31[^,],%u,%8[^,],%lf,%lf,%d",
			SondehubPayloads[Channel].Payload,
			&SondehubPayloads[Channel].Counter,
			SondehubPayloads[Channel].Time,
			&SondehubPayloads[Channel].Latitude,
			&SondehubPayloads[Channel].Longitude,
			&SondehubPayloads[Channel].Altitude);

	// LogMessage("Sondehub: %s,%d,%s,%.5lf,%.5lf,%d\n", 
				// SondehubPayloads[Channel].Payload,
				// SondehubPayloads[Channel].Counter,
				// SondehubPayloads[Channel].Time,
				// SondehubPayloads[Channel].Latitude,
				// SondehubPayloads[Channel].Longitude,
				// SondehubPayloads[Channel].Altitude);
				
	SondehubPayloads[Channel].InUse = (SondehubPayloads[Channel].Latitude != 0) && (SondehubPayloads[Channel].Longitude != 0);
}

size_t sondehub_write_data( void *buffer, size_t size, size_t nmemb, void *userp )
{
    // LogMessage("%s\n", (char *)buffer);
    return size * nmemb;
}

int UploadJSONToServer(char *url, char *json)
{
    CURL *curl;
    CURLcode res;
    char curl_error[CURL_ERROR_SIZE];

    /* get a curl handle */
    curl = curl_easy_init(  );
    if ( curl )
    {
        bool result;
        struct curl_slist *headers = NULL;
		int retries;
		long int http_resp;

        // So that the response to the curl PUT doesn't mess up my finely crafted display!
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, sondehub_write_data );

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

        // PUT https://api.v2.sondehub.org/amateur/telemetry with content-type application/json
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
				if (http_resp == 200)
				{
					// LogMessage("Saved OK to sondehub\n");
                    // Everything performing nominally (even if we didn't successfully insert this time)
                    result = true;
				}
                else
                {
					LogMessage("Unexpected HTTP response %ld for URL '%s'\n", http_resp, url);
                    result = false;
                }
			}
			else
			{
				http_resp = 0;
				LogMessage("Failed for URL '%s'\n", url);
				LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				LogMessage("error: %s\n", curl_error);
                // Likely a network error, so return false to requeue
                result = false;
			}
		} while ((!result) && (++retries < 5));

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

int UploadSondehubPosition(int Channel)
{
    char url[200];
    char json[1000], now[32], doc_time[32];
    time_t rawtime;
    struct tm *tm, *doc_tm;

	// Get formatted timestamp for now
	time( &rawtime );
	tm = gmtime( &rawtime );
	strftime( now, sizeof( now ), "%Y-%0m-%0dT%H:%M:%SZ", tm );

	// Get formatted timestamp for doc timestamp
	doc_tm = gmtime( &rawtime );
	strftime(doc_time, sizeof( doc_time ), "%Y-%0m-%0dT%H:%M:%SZ", doc_tm);

	// Create json as required by sondehub-amateur
	sprintf(json,	"[{\"software_name\": \"LoRa Gateway\","		// Fixed software name
					"\"software_version\": \"%s\","					// Version
					"\"uploader_callsign\": \"%s\","				// User callsign
					"\"time_received\": \"%s\","					// UTC
					"\"payload_callsign\": \"%s\","					// Payload callsign
					"\"datetime\":\"%s\","							// UTC from payload
					"\"lat\": %.5lf,"								// Latitude
					"\"lon\": %.5lf,"								// Longitude
					"\"alt\": %.d,"								// Altitude
					"\"frequency\": %.4lf,"							// Frequency
					"\"modulation\": \"LoRa\","						// Modulation
			// DoubleToString('snr', SNR, HasSNR) +
			// DoubleToString('rssi', PacketRSSI, HasPacketRSSI) +
			"\"uploader_position\": ["
			" %.3lf,"												// Listener Latitude
			" %.3lf,"												// Listener Longitude
			" %.0lf"												// Listener Altitude
			"],"
			"\"uploader_antenna\": \"%s\""
			"}]",
			Config.Version, Config.Tracker, now,
			SondehubPayloads[Channel].Payload, doc_time,
			SondehubPayloads[Channel].Latitude, SondehubPayloads[Channel].Longitude, SondehubPayloads[Channel].Altitude,
			Config.LoRaDevices[Channel].Frequency + Config.LoRaDevices[Channel].FrequencyOffset,
			Config.latitude, Config.longitude, Config.altitude, Config.antenna);

	// Set the URL that is about to receive our PUT
	strcpy(url, "https://api.v2.sondehub.org/amateur/telemetry");

	return UploadJSONToServer(url, json);
}

int UploadListenerToSondehub(void)
{
    char url[200];
    char json[1000], now[32], doc_time[32];
    time_t rawtime;
    struct tm *tm, *doc_tm;

	// Get formatted timestamp for now
	time( &rawtime );
	tm = gmtime( &rawtime );
	strftime( now, sizeof( now ), "%Y-%0m-%0dT%H:%M:%SZ", tm );

	// Get formatted timestamp for doc timestamp
	doc_tm = gmtime( &rawtime );
	strftime(doc_time, sizeof( doc_time ), "%Y-%0m-%0dT%H:%M:%SZ", doc_tm);

	// Create json as required by sondehub-amateur
	sprintf(json,	"{\"software_name\": \"LoRa Gateway\","		// Fixed software name
					"\"software_version\": \"%s\","					// Version
					"\"uploader_callsign\": \"%s\","				// User callsign
			"\"uploader_position\": ["
			" %.3lf,"												// Listener Latitude
			" %.3lf,"												// Listener Longitude
			" %.0lf"												// Listener Altitude
			"],"
			"\"uploader_radio\": \"%s\","
			"\"uploader_antenna\": \"%s\""
			"}",
			Config.Version, Config.Tracker,
			Config.latitude, Config.longitude, Config.altitude,
			Config.radio, Config.antenna);
			
	strcpy(url, "https://api.v2.sondehub.org/amateur/listeners");

	return UploadJSONToServer(url, json);
}

	
void *SondehubLoop( void *vars )
{
	while (1)
	{
		static long ListenerCountdown = 0;
		int Channel;
		
		for (Channel=0; Channel<=1; Channel++)
		{
			if (SondehubPayloads[Channel].InUse)
			{
				if (UploadSondehubPosition(Channel))
				{
					SondehubPayloads[Channel].InUse = 0;
				}
			}
		}
		
		if (--ListenerCountdown <= 0)
		{
			if (UploadListenerToSondehub())
			{
				LogMessage("Uploaded listener info to Sondehub/amateur\n");
				ListenerCountdown = 216000;		// Every 6 hours
			}
			else
			{
				LogMessage("Failed to upload listener info to Sondehub/amateur\n");
				ListenerCountdown = 600;		// Try again in 1 minute
			}
		}
		
        usleep(100000);
	}
}
