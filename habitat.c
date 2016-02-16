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

#include "habitat.h"
#include "global.h"

extern void ChannelPrintf(int Channel, int row, int column, const char *format, ...);

size_t habitat_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}

void UploadTelemetryPacket(int Channel)
{
	CURL *curl;
	CURLcode res;
	char PostFields[400];
 
	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		// So that the response to the curl POST doesn;'t mess up my finely crafted display!
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, habitat_write_data);
		
		// Set the timeout
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		
		// Avoid curl library bug that happens if above timeout occurs (sigh)
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

		// Set the URL that is about to receive our POST
		curl_easy_setopt(curl, CURLOPT_URL, "http://habitat.habhub.org/transition/payload_telemetry");
	
		// Now specify the POST data
		sprintf(PostFields, "callsign=%s&string=%s\n&string_type=ascii&metadata={}", Config.Tracker, Config.LoRaDevices[Channel].Telemetry);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
 
		// Perform the request, res will get the return code
		res = curl_easy_perform(curl);
	
		// Check for errors
		if(res != CURLE_OK)
		{
			LogMessage("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		// always cleanup
		curl_easy_cleanup(curl);
	}
  
	curl_global_cleanup();
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
					ChannelPrintf(Channel, 5, 1, "Habitat");
					
					UploadTelemetryPacket(Channel);
					
					Config.LoRaDevices[Channel].LastCounter = Config.LoRaDevices[Channel].Counter;

					delay(100);

					ChannelPrintf(Channel, 5, 1, "       ");
				}
			}
		}
		
		delay(100);
	}
}
