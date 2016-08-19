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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "global.h"

extern bool run;
extern bool server_closed;

void *ServerLoop(void *some_void_ptr)
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr; 

    char sendBuff[1025];

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(Config.ServerPort); 
	
	LogMessage("Listening on port %d\n", Config.ServerPort);

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    {
		LogMessage("setsockopt(SO_REUSEADDR) failed");
	}
	
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		LogMessage("Server failed errno %d\n", errno);
		exit(-1);
	}

    listen(listenfd, 10); 

    while (run)
    {
		int port_closed;
		
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

		LogMessage("Connected to client\n");

		for (port_closed=0; !port_closed; )
		{
			int Channel;
			// Build json
			// sprintf(sendBuff, "{\"class\":\"POSN\",\"time\":\"12:34:56\",\"lat\":54.12345,\"lon\":-2.12345,\"alt\":169}\r\n");
			
			Channel = 1;
			if (Config.EnableDev)
			{
				sprintf(sendBuff, "{\"class\":\"POSN\",\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"predlat\":%.5lf,\"predlon\":%.5lf,\"speed\":%d,"
								  "\"head\":%d,\"cda\":%.2lf,\"pls\":%.1lf,\"pt\":%d,\"ca\":%d,\"ct\":%d,\"as\":%.1lf,\"ad\":%d,\"sl\":%d,\"sr\":%d,\"st\":%d,\"gr\":%.2lf,\"fm\":%d}\r\n",
							Config.LoRaDevices[Channel].Payload,
							Config.LoRaDevices[Channel].Time,
							Config.LoRaDevices[Channel].Latitude,
							Config.LoRaDevices[Channel].Longitude,
							Config.LoRaDevices[Channel].Altitude,
							Config.LoRaDevices[Channel].PredictedLatitude,
							Config.LoRaDevices[Channel].PredictedLongitude,
							Config.LoRaDevices[Channel].Speed,
							
							Config.LoRaDevices[Channel].Heading,
							Config.LoRaDevices[Channel].cda,
							Config.LoRaDevices[Channel].PredictedLandingSpeed,
							Config.LoRaDevices[Channel].PredictedTime,
							Config.LoRaDevices[Channel].CompassActual,
							Config.LoRaDevices[Channel].CompassTarget,
							Config.LoRaDevices[Channel].AirSpeed,
							Config.LoRaDevices[Channel].AirDirection,
							Config.LoRaDevices[Channel].ServoLeft,
							Config.LoRaDevices[Channel].ServoRight,
							Config.LoRaDevices[Channel].ServoTime,
							Config.LoRaDevices[Channel].GlideRatio,
							Config.LoRaDevices[Channel].FlightMode);
				}
				else
				{
					sprintf(sendBuff, "{\"class\":\"POSN\",\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"rate\":%.1lf}\r\n",

								Config.LoRaDevices[Channel].Payload,
								Config.LoRaDevices[Channel].Time,
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude,
								Config.LoRaDevices[Channel].AscentRate);
				}
			
			if (!run)
			{
				port_closed = 1;
			}
			else if (send(connfd, sendBuff, strlen(sendBuff), MSG_NOSIGNAL) <= 0)
			{
				LogMessage("Disconnected from client\n");
				port_closed = 1;
			}
			else
			{
				sleep(1);
			}
		}

        close(connfd);
     }
}
