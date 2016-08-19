#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>   	// Standard input/output definitions
#include <string.h>  	// String function definitions
#include <unistd.h>  	// UNIX standard function definitions
#include <fcntl.h>   	// File control definitions
#include <errno.h>   	// Error number definitions
#include <termios.h> 	// POSIX terminal control definitions
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <dirent.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>

#include "base64.h"
#include "habitat.h"
#include "global.h"
#include "sha256.h"


extern void ChannelPrintf(int Channel, int row, int column, const char *format, ...);

size_t habitat_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	// LogMessage("%s\n", (char *)buffer);
	return size * nmemb;
}

void hash_to_hex(unsigned char *hash, char *line)
{
	int idx;

	for (idx=0; idx < 32; idx++)
	{
		sprintf(&(line[idx*2]), "%02x", hash[idx]);
	}
	line[64] = '\0';
	
	// LogMessage(line);
}

void UploadTelemetryPacket(int Channel)
{
	CURL *curl;
	CURLcode res;
 
	/* In windows, this will init the winsock stuff */ 
	// curl_global_init(CURL_GLOBAL_ALL); // RJH moved to main in gateway.c not thread safe
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		char url[200];
		char base64_data[1000];
		size_t base64_length;
		SHA256_CTX ctx;
		unsigned char hash[32];
		char doc_id[100];
		char json[1000], now[32];
		char PostFields[400], Sentence[512];
		struct curl_slist *headers = NULL;
		time_t rawtime;
		struct tm *tm;

		// Get formatted timestamp
		time(&rawtime);
		tm = gmtime(&rawtime);
		strftime(now, sizeof(now), "%Y-%0m-%0dT%H:%M:%SZ", tm);
		
		// So that the response to the curl PUT doesn't mess up my finely crafted display!
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, habitat_write_data);
		
		// Set the timeout
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

                // RJH capture http errors and report
                curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
		
		// Avoid curl library bug that happens if above timeout occurs (sigh)
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

		// Grab current telemetry string and append a linefeed
		sprintf(Sentence, "%s\n", Config.LoRaDevices[Channel].Telemetry);
		
		// Convert sentence to base64
		base64_encode(Sentence, strlen(Sentence), &base64_length, base64_data);
		base64_data[base64_length] = '\0';	
		
		// Take SHA256 hash of the base64 version and express as hex.  This will be the document ID
		sha256_init(&ctx);
		sha256_update(&ctx, base64_data, base64_length);
		sha256_final(&ctx, hash);
		hash_to_hex(hash, doc_id);
				
		// Create json with the base64 data in hex, the tracker callsign and the current timestamp
		sprintf(json,
				"{\"data\": {\"_raw\": \"%s\"},\"receivers\": {\"%s\": {\"time_created\": \"%s\",\"time_uploaded\": \"%s\"}}}",
				base64_data,
				Config.Tracker,
				now,
				now);
		
		// Set the URL that is about to receive our PUT
		sprintf(url, "http://habitat.habhub.org/habitat/_design/payload_telemetry/_update/add_listener/%s", doc_id);
		
		// Set the headers
		headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charsets: utf-8");

		// PUT to http://habitat.habhub.org/habitat/_design/payload_telemetry/_update/add_listener/<doc_id> with content-type application/json
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
		curl_easy_setopt(curl, CURLOPT_URL, url);  
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
		
		// LogMessage("%s\n", Config.LoRaDevices[Channel].Telemetry);
		
		// Perform the request, res will get the return code
		res = curl_easy_perform(curl);
	
		// Check for errors
		if (res == CURLE_OK)
		{
			// LogMessage("OK\n");
		}
		else
		{
			LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		// always cleanup
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		// free(base64_data);
	}
  
	// curl_global_cleanup(); // RJH moved to main in gateway.c not thread safe
}


void *HabitatLoop(void *some_void_ptr)
{
    while (1)
    {
		if (Config.EnableHabitat)
		{
			int Channel;
			
			for (Channel=0; Channel<=1; Channel++)
			{
				if (Config.LoRaDevices[Channel].Counter != Config.LoRaDevices[Channel].LastCounter)
				{
					ChannelPrintf(Channel, 6, 1, "Habitat");
					
					UploadTelemetryPacket(Channel);
					
					Config.LoRaDevices[Channel].LastCounter = Config.LoRaDevices[Channel].Counter;

					delay(100);

					ChannelPrintf(Channel, 6, 1, "       ");
				}
			}
		}
		
		delay(100);
	}
}
