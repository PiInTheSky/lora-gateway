#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>              // Standard input/output definitions
#include <string.h>             // String function definitions
#include <fcntl.h>              // File control definitions
#include <unistd.h>             // UNIX standard function definitions
#include <errno.h>              // Error number definitions
#include <termios.h>            // POSIX terminal control definitions
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <wiringPi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "config.h"
#include "global.h"

extern bool run;
extern bool server_closed;

void ProcessClientLine(int connfd, char *line)
{	
	line[strcspn(line, "\r\n")] = '\0';		// Get rid of CR LF
	
	LogMessage("Received %s from client\n", line);
	
	if (strchr(line, '=') == NULL)
	{
		// Request or command
		
		if (strcasecmp(line, "settings") == 0)
		{
			int Index;
			char SettingName[64], SettingValue[256], packet[4096];
			
			LogMessage("Responding to settings request\n");
			
			Index = 0;
			packet[0] = '\0';
			
			while (SettingAsString(Index, SettingName, sizeof(SettingName), SettingValue, sizeof(SettingValue)))
			{
				char temp[300];
				
				sprintf(temp, "{\"class\":\"SET\",\"set\":\"%s\",\"val\":%s}\r\n", SettingName, SettingValue);
				
				if ((strlen(temp) + strlen(packet)) < sizeof(packet))
				{
					strcat(packet, temp);
				}
				
				Index++;
			}

			send(connfd, packet, strlen(packet), MSG_NOSIGNAL);
		}
		else if (strcasecmp(line, "save") == 0)
		{
			LogMessage("Saving Settings\n");
			SaveConfigFile();
		}
	}
	else
	{
		// Setting
		char *setting, *value, *saveptr;
	
		setting = strtok_r(line, "=", &saveptr);
		value = strtok_r( NULL, "\n", &saveptr);
		
		SetConfigValue(setting, value);
	}
}

void *ServerLoop( void *some_void_ptr )
{
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    char sendBuff[1025];

    sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    memset( &serv_addr, '0', sizeof( serv_addr ) );
    memset( sendBuff, '0', sizeof( sendBuff ) );

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    serv_addr.sin_port = htons( Config.ServerPort );

    LogMessage( "Listening on port %d\n", Config.ServerPort );

    if ( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &( int )
                     {
                     1}, sizeof( int ) ) < 0 )
    {
        LogMessage( "setsockopt(SO_REUSEADDR) failed" );
    }

    if ( bind
         ( sockfd, ( struct sockaddr * ) &serv_addr,
           sizeof( serv_addr ) ) < 0 )
    {
        LogMessage( "Server failed errno %d\n", errno );
        exit( -1 );
    }

    listen( sockfd, 10 );
	
    while ( run )
    {
		int SendEveryMS = 1000;
		int MSPerLoop=100;
        int ms, port_closed, connfd;

		fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) & ~O_NONBLOCK);	// Blocking mode so we wait for a connection

        connfd = accept( sockfd, ( struct sockaddr * ) NULL, NULL );	// Wait for connection

        LogMessage( "Connected to client\n" );

		fcntl(connfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);	// Non-blocking, so we don't block on receiving any commands from client

        for ( port_closed = 0; !port_closed; )
        {
            int Channel;
			char packet[4096];

			// Listen loop
			for (ms=0; ms<SendEveryMS; ms+=MSPerLoop)
			{
				int bytecount;
				
				while ((bytecount = recv(connfd, packet, sizeof(packet), 0)) > 0)
				{
					char *line, *saveptr;
					
					packet[bytecount] = 0;
					
					line = strtok_r(packet, "\n", &saveptr);
					while (line)
					{
						ProcessClientLine(connfd, line);
						line = strtok_r( NULL, "\n", &saveptr);
					}
				}
     
				delay(MSPerLoop);
			}
				
            // Send part
			// Build json

			for (Channel=0; Channel<=1; Channel++)
			{
				if ( Config.EnableDev )
				{
					sprintf(sendBuff, "{\"class\":\"POSN\",\"index\":%d,\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"rate\":%.1lf,\"predlat\":%.5lf,\"predlon\":%.5lf,\"speed\":%d,"
									  "\"head\":%d,\"cda\":%.2lf,\"pls\":%.1lf,\"pt\":%d,\"ca\":%d,\"ct\":%d,\"as\":%.1lf,\"ad\":%d,\"sl\":%d,\"sr\":%d,\"st\":%d,\"gr\":%.2lf,\"fm\":%d}\r\n",
								Channel,
								Config.LoRaDevices[Channel].Payload,
								Config.LoRaDevices[Channel].Time,
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude,
								Config.LoRaDevices[Channel].AscentRate,
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
					sprintf(sendBuff, "{\"class\":\"POSN\",\"index\":%d,\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"rate\":%.1lf}\r\n",
								Channel,
								Config.LoRaDevices[Channel].Payload,
								Config.LoRaDevices[Channel].Time,
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude,
								Config.LoRaDevices[Channel].AscentRate);
				}

				if ( !run )
				{
					port_closed = 1;
				}
				else if ( send(connfd, sendBuff, strlen(sendBuff), MSG_NOSIGNAL ) <= 0 )
				{
					LogMessage( "Disconnected from client\n" );
					port_closed = 1;
				}
			}
        }

        close( connfd );
    }

    return NULL;
}
