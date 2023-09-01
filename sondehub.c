#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>
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

pthread_mutex_t crit = PTHREAD_MUTEX_INITIALIZER;		// To protect IncomingSondehubPayloads

struct TPayload IncomingSondehubPayloads[2];
struct TPayload ActiveSondehubPayloads[2];

void SetSondehubSentence(int Channel, char *tmp)
{
    pthread_mutex_lock(&crit); // lock the critical section

	if (sscanf(tmp + 2, "%31[^,],%u,%8[^,],%lf,%lf,%d",
				IncomingSondehubPayloads[Channel].Payload,
				&IncomingSondehubPayloads[Channel].Counter,
				IncomingSondehubPayloads[Channel].Time,
				&IncomingSondehubPayloads[Channel].Latitude,
				&IncomingSondehubPayloads[Channel].Longitude,
				&IncomingSondehubPayloads[Channel].Altitude) == 6)
	{
		IncomingSondehubPayloads[Channel].PacketSNR = Config.LoRaDevices[Channel].PacketSNR;
		IncomingSondehubPayloads[Channel].PacketRSSI = Config.LoRaDevices[Channel].PacketRSSI;
		IncomingSondehubPayloads[Channel].Frequency = Config.LoRaDevices[Channel].Frequency + Config.LoRaDevices[Channel].FrequencyOffset;
	
		strcpy(IncomingSondehubPayloads[Channel].Telemetry, tmp);
	
		IncomingSondehubPayloads[Channel].InUse = (IncomingSondehubPayloads[Channel].Latitude != 0) && (IncomingSondehubPayloads[Channel].Longitude != 0);
	}

    pthread_mutex_unlock(&crit);   // unlock once you are done
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
                    result = true;
				}
                else if (http_resp == 400)
				{
					LogMessage("400 response to %s\n", json);
					LogError(http_resp, "JSON: ", json);
					LogError(http_resp, "RESP: ", curl_error);
                    result = true;		// Don't retry - this failed due to the uploaded JSON containing something that the server rejected
				}
				else
                {
					LogMessage("Unexpected HTTP response %ld for URL '%s'\n", http_resp, url);
					LogError(http_resp, "JSON: ", json);
					LogError(http_resp, "RESP: ", curl_error);
                    result = false;
                }
			}
			else
			{
				http_resp = 0;
				LogMessage("Failed for URL '%s'\n", url);
				LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				LogMessage("error: %s\n", curl_error);

				LogError(res, "JSON: ", json);
				LogError(res, "RESP: ", curl_error);
				
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

void ExtractFields(char *Telemetry, char *ExtractedFields)
{
	char Line[256], FieldList[32];
	char *token;

	ExtractedFields[0] = '\0';
	FieldList[0] = '\0';
	
	strcpy(Line, Telemetry);
	token = strtok(Line, ",*");
	while ((token != NULL) && (FieldList[0] == '\0'))
	{
		if (strncmp(token, "012345", 6) == 0)
		{
			strcpy(FieldList, token);
		}
		token = strtok(NULL, ",*");
	}	
	
	if (FieldList[0])
	{
		// We have a field list field, so go through that and extract each field, adding fields to the JSON for Sondehub
		int i;
		char Value[64];
		
		strcpy(Line, Telemetry);
		token = strtok(Line, ",*");
		for (i=1; FieldList[i]; i++)
		{
			token = strtok(NULL, ",*");
			Value[0] = '\0';
			
			// LogMessage("Field %d type %c is '%s'\n", i, FieldList[i], token);
			
			switch (FieldList[i])
			{
				case '6':
				sprintf(Value, "\"sats\":%s,", token);
				break;

				case '7':
				sprintf(Value, "\"vel_h\":%s,", token);
				break;

				case '8':
				sprintf(Value, "\"heading\":%s,", token);
				break;

				case '9':
					if (strchr(token, '.'))
					{
						// volts
						sprintf(Value, "\"batt\":%s,", token);
					}
					else
					{
						// mV --> V
						sprintf(Value, "\"batt\":%.1lf,", atof(token) / 1000.0);
					}
				break;

				case 'A':	
				sprintf(Value, "\"temp\":%s,", token);
				break;

				case 'B':
				sprintf(Value, "\"ext_temperature\":%s,", token);
				break;

				case 'C':	
				sprintf(Value, "\"pred_lat\":%s,", token);
				break;

				case 'D':	
				sprintf(Value, "\"pred_lon\":%s,", token);
				break;

				case 'R':	
				sprintf(Value, "\"pressure\":%s,", token);
				break;
			}
			
			if (*Value)
			{
				strcat(ExtractedFields, Value);
			}
		}	
	}
	
	// LogMessage("Extracted: '%s'\n", ExtractedFields);
}

void BuildPayloadTime(char *Result, char *TimeInSentence, struct tm *tm)
{
	struct tm tm2;
	
	memcpy(&tm2, tm, sizeof(tm2));
	
    strptime(TimeInSentence, "%H:%M:%S", &tm2);
	
	// Test for payload time being yesterday
	
	if ((tm2.tm_hour == 23) && (tm->tm_hour == 00))
	{
		tm2.tm_mday--;
		timegm(&tm2);
	}

	strftime(Result, 32, "%Y-%0m-%0dT%H:%M:%SZ", &tm2);
}

int UploadSondehubPosition(int Channel)
{
    char json[1000], now[32], payload_time[32], ExtractedFields[256], uploader_position[256];
    time_t rawtime;
    struct tm *tm;

	// Get formatted timestamp for now
	time(&rawtime);
	tm = gmtime(&rawtime);
	strftime(now, sizeof(now), "%Y-%0m-%0dT%H:%M:%SZ", tm);

	BuildPayloadTime(payload_time, ActiveSondehubPayloads[Channel].Time, tm);
	
	// Find field list and extract fields
	ExtractFields(ActiveSondehubPayloads[Channel].Telemetry, ExtractedFields);

    if ((Config.latitude >= -90) && (Config.latitude <= 90) && (Config.longitude >= -180) && (Config.longitude <= 180))
	{
		sprintf(uploader_position,	"\"uploader_position\": ["
									" %.3lf,"												// Listener Latitude
									" %.3lf,"												// Listener Longitude
									" %.0lf"												// Listener Altitude
									"],",
									Config.latitude, Config.longitude, Config.altitude
									);
	}
	else
	{
		uploader_position[0] = '\0';
	}
	
	// Create json as required by sondehub-amateur
	sprintf(json,	"[{\"software_name\": \"LoRa Gateway\","		// Fixed software name
					"\"software_version\": \"%s\","					// Version
					"\"uploader_callsign\": \"%s\","				// User callsign
					"\"time_received\": \"%s\","					// UTC
					"\"payload_callsign\": \"%s\","					// Payload callsign
					"\"datetime\":\"%s\","							// UTC from payload
					"\"lat\": %.5lf,"								// Latitude
					"\"lon\": %.5lf,"								// Longitude
					"\"alt\": %d,"									// Altitude
					"\"frequency\": %.4lf,"							// Frequency
					"\"modulation\": \"LoRa Mode %d\","	 			// Modulation
					"\"snr\": %d,"									// SNR
					"\"rssi\": %d,"									// RSSI
					"\"raw\": \"%s\","								// Sentence
					"%s"
					"%s"
					"\"uploader_antenna\": \"%s\""
					"}]",
					Config.Version, Config.Tracker, now,
					ActiveSondehubPayloads[Channel].Payload, payload_time,
					ActiveSondehubPayloads[Channel].Latitude, ActiveSondehubPayloads[Channel].Longitude, ActiveSondehubPayloads[Channel].Altitude,				
					ActiveSondehubPayloads[Channel].Frequency,
					Config.LoRaDevices[Channel].SpeedMode,
					ActiveSondehubPayloads[Channel].PacketSNR, ActiveSondehubPayloads[Channel].PacketRSSI,
					ActiveSondehubPayloads[Channel].Telemetry,
					ExtractedFields,
					uploader_position,
					Config.antenna);

	return UploadJSONToServer("https://api.v2.sondehub.org/amateur/telemetry", json);
}

char *SanitiseCallsignForMQTT(char *Callsign)
{
	static char Result[32];
	char *ptr;
	
	strcpy(Result, Callsign);
	
    ptr = strchr(Result, '/');
	
    while (ptr)
	{
        *ptr = '-';
        ptr = strchr(ptr, '/');
    }	

	return Result;
}

int UploadListenerToSondehub(void)
{
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
			Config.Version, SanitiseCallsignForMQTT(Config.Tracker),
			Config.latitude, Config.longitude, Config.altitude,
			Config.radio, Config.antenna);
			
	return UploadJSONToServer("https://api.v2.sondehub.org/amateur/listeners", json);
}

	
void *SondehubLoop( void *vars )
{
	while (1)
	{
		static long ListenerCountdown = 0;
		int Channel;
		
		for (Channel=0; Channel<=1; Channel++)
		{
			// Copy new incoming payload, if there is one
			if (IncomingSondehubPayloads[Channel].InUse)
			{
				pthread_mutex_lock(&crit); // lock the critical section
	
				// copy from incoming to active
				memcpy(&ActiveSondehubPayloads[Channel], &IncomingSondehubPayloads[Channel], sizeof(struct TPayload));
				
				pthread_mutex_unlock(&crit);   // unlock once you are done
			}

			// Try to upload active payload, if there is one
			if (ActiveSondehubPayloads[Channel].InUse)
			{
				UploadSondehubPosition(Channel);		// Upload, with limited retries ifd needed
				
				ActiveSondehubPayloads[Channel].InUse = 0;
			}
		}
		
		if ((Config.latitude >= -90) && (Config.latitude <= 90) && (Config.longitude >= -180) && (Config.longitude <= 180))
		{
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
		}
		
        usleep(100000);
	}
}
