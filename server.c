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

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

    listen(listenfd, 10); 

    while (1)
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
			sprintf(sendBuff, "{\"class\":\"POSN\",\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"rate\":%.1lf}\r\n",
						Config.LoRaDevices[Channel].Payload,
						Config.LoRaDevices[Channel].Time,
						Config.LoRaDevices[Channel].Latitude,
						Config.LoRaDevices[Channel].Longitude,
						Config.LoRaDevices[Channel].Altitude,
						Config.LoRaDevices[Channel].AscentRate);
			
			
			if (send(connfd, sendBuff, strlen(sendBuff), MSG_NOSIGNAL) <= 0)
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
