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

int FindSSDVPacketToSend(int ThreadIndex)
{
	int i;
	
	for (i=0; i<16; i++)
	{
		if (SSDVPacketArrays.Packets[ThreadIndex].Packets[i].InUse)
		{
			return i;
		}
	}
	
	return -1;
}

size_t write_ssdv_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}


int UploadImagePacket(char *EncodedCallsign, char *EncodedEncoding, char *EncodedData)
{
	CURL *curl;
	CURLcode res;
	char PostFields[1000];
	int UploadedOK;
 
	UploadedOK = 0;
	
	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		// So that the response to the curl POST doesn;'t mess up my finely crafted display!
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_ssdv_data);
		
		// Set the timeout
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		
		// Avoid curl library bug that happens if above timeout occurs (sigh)
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

		/* Set the URL that is about to receive our POST. This URL can
		   just as well be a https:// URL if that is what should receive the
		   data. */ 
		curl_easy_setopt(curl, CURLOPT_URL, "http://ssdv.habhub.org/data.php");
	
		/* Now specify the POST data */ 
		sprintf(PostFields, "callsign=%s&encoding=%s&packet=%s", Config.Tracker, EncodedEncoding, EncodedData);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
 
		/* Perform the request, res will get the return code */ 
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
		curl_easy_cleanup(curl);
	}
	  
	curl_global_cleanup();
	
	return UploadedOK;
}

void *SSDVLoop(void *arguments)
{
	struct TThreadArguments *ThreadArguments;
	int ThreadIndex;
	char HexString[513];

	ThreadArguments = (struct TThreadArguments *)arguments;
	
	ThreadIndex = ThreadArguments->Index;
	
    while (1)
    {
		int PacketIndex;
		
		PacketIndex = FindSSDVPacketToSend(ThreadIndex);
		
		if (PacketIndex >= 0)
		{
			char *EncodedCallsign, *EncodedEncoding, *EncodedData;
			
			LogMessage("Found packet %d for thread %d from payload %s\n", PacketIndex, ThreadIndex, SSDVPacketArrays.Packets[ThreadIndex].Packets[PacketIndex].Callsign);

			EncodedCallsign = url_encode(SSDVPacketArrays.Packets[ThreadIndex].Packets[PacketIndex].Callsign); 
			EncodedEncoding = url_encode("hex"); 

			ConvertStringToHex(HexString, SSDVPacketArrays.Packets[ThreadIndex].Packets[PacketIndex].Packet, 256);
			EncodedData = url_encode(HexString); 
			
			if (UploadImagePacket(EncodedCallsign, EncodedEncoding, EncodedData))
			{
				// Mark packet as sent
				SSDVPacketArrays.Packets[ThreadIndex].Packets[PacketIndex].InUse = 0;
			}
			
			free(EncodedCallsign);
			free(EncodedEncoding);
			free(EncodedData);			
		}
		
		sleep(0.1);
	}
}
