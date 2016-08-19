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
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <curl/curl.h>

#include "urlencode.h"
#include "base64.h"
#include "ssdv.h"
#include "global.h"

size_t write_ssdv_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}


void ConvertStringToHex(unsigned char *Target, unsigned char *Source, int Length)
{
	const char Hex[16] = "0123456789ABCDEF";
	int i;
	
	for (i=0; i<Length; i++)
	{
		*Target++ = Hex[Source[i] >> 4];
		*Target++ = Hex[Source[i] & 0x0F];
	}

	*Target++ = '\0';
}


int UploadImagePackets(void)
{
	CURL *curl;
	CURLcode res;
	char PostFields[1000], base64_data[512], json[32768], packet_json[1000];
	struct curl_slist *headers = NULL;
	int UploadedOK, base64_length;
	char now[32];
	time_t rawtime;
	struct tm *tm;

	UploadedOK = 0;
	
	/* In windows, this will init the winsock stuff */ 
	// curl_global_init(CURL_GLOBAL_ALL); // RJH moved to main in gateway.c not thread safe
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		int PacketIndex;
		
		// So that the response to the curl POST doesn;'t mess up my finely crafted display!
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_ssdv_data);
		
		// Set the timeout
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		
                // RJH capture http errors and report
                curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

		// Avoid curl library bug that happens if above timeout occurs (sigh)
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	
		// Get formatted timestamp
		time(&rawtime);
		tm = gmtime(&rawtime);
		strftime(now, sizeof(now), "%Y-%0m-%0dT%H:%M:%SZ", tm);

		// Create json with the base64 data in hex, the tracker callsign and the current timestamp
		strcpy(json, "{\"type\": \"packets\",\"packets\":[");
		for (PacketIndex = 0; PacketIndex < SSDVPacketArrays[SSDVSendArrayIndex].Count; PacketIndex++)
		{
			base64_encode(SSDVPacketArrays[SSDVSendArrayIndex].Packets[PacketIndex].Packet, 256, &base64_length, base64_data);
			base64_data[base64_length] = '\0';	

			sprintf(packet_json, "{\"type\": \"packet\", \"packet\": \"%s\", \"encoding\": \"base64\", \"received\": \"%s\", \"receiver\": \"%s\"}%s",
					base64_data, now, Config.Tracker, PacketIndex == (SSDVPacketArrays[SSDVSendArrayIndex].Count-1) ? "" : ",");
			strcat(json, packet_json);
		}
		strcat(json, "]}");

		// Set the headers
		headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charsets: utf-8");

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
		curl_easy_setopt(curl, CURLOPT_URL, "http://ssdv.habhub.org/api/v0/packets");  
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

		// Perform the request, res will get the return code
		res = curl_easy_perform(curl);

		/* Check for errors */ 
		if(res == CURLE_OK)
		{
			UploadedOK = 1;
		}
		else
		{
			LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		/* always cleanup */ 
                curl_slist_free_all(headers); // RJH Added this from habitat.c as was missing
		curl_easy_cleanup(curl);
	}
	  
	// curl_global_cleanup();  // RJH moved to main in gateway.c not thread safe
	
	return UploadedOK;
}

void *SSDVLoop(void *arguments)
{
	char HexString[513];

    while (1)
    {
		int ArrayIndex;
		
		pthread_mutex_lock(&ssdv_mutex);

		if (SSDVPacketArrays[0].Count > 0)
		{
			SSDVSendArrayIndex = 0;
		}
		else if (SSDVPacketArrays[1].Count > 0)
		{
			SSDVSendArrayIndex = 1;
		}
		else
		{
			SSDVSendArrayIndex = -1;
		}

		pthread_mutex_unlock(&ssdv_mutex);
		
		if (SSDVSendArrayIndex >= 0)
		{
			if (UploadImagePackets())
			{
				// Mark packets as sent
				SSDVPacketArrays[SSDVSendArrayIndex].Count = 0;						
			}
			
			SSDVSendArrayIndex = -1;
		}
		
		delay(100);
	}
}
