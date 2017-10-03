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

void EncryptMessage(char *Code, char *Message)
{
	int i, Len;
	
	Len = strlen(Code);
	
	if (Len > 0)
	{
		i = 0;
		while (*Message)
		{
			*Message = (*Message ^ Code[i]) | 0x80;
			Message++;
			i = (i + 1) % Len;
		}
	}
}

void ProcessJSONClientLine(int connfd, char *line)
{	
	line[strcspn(line, "\r\n")] = '\0';		// Get rid of CR LF
	
	LogMessage("Received '%s' from JSON client\n", line);
	
	if (strchr(line, '=') != NULL)
	{
		// Setting
		char *setting, *value, *saveptr;
	
		setting = strtok_r(line, "=", &saveptr);
		value = strtok_r( NULL, "\n", &saveptr);
		
		SetConfigValue(setting, value);
	}
	else if (strchr(line, ':') != NULL)
	{
		// Command with parameters
		char *command, *value, *saveptr;
	
		command = strtok_r(line, ":", &saveptr);
		value = strtok_r(NULL, "\n", &saveptr);
		
		if (strcasecmp(command, "send") == 0)
		{
			int channel;
			
			channel = *value != '0';
			value++;
			
			LogMessage("LoRa[%d]: To send '%s'\n", channel, value);
	
			EncryptMessage(Config.UplinkCode, value);
			
			strcpy(Config.LoRaDevices[channel].UplinkMessage, value);
		}
	}
	else

	{
		// single-word request

		
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
}


int SendJSON(int connfd)
{
	int PayloadIndex, port_closed;
    char sendBuff[1025];
	
	port_closed = 0;
    memset( sendBuff, '0', sizeof( sendBuff ) );
	
	for (PayloadIndex=0; PayloadIndex<MAX_PAYLOADS; PayloadIndex++)
	{
		if (Config.Payloads[PayloadIndex].InUse)
		{
			sprintf(sendBuff, "{\"class\":\"POSN\",\"index\":%d,\"channel\":%d,\"payload\":\"%s\",\"time\":\"%s\",\"lat\":%.5lf,\"lon\":%.5lf,\"alt\":%d,\"rate\":%.1lf,\"sentence\":\"%s\"}\r\n",
					PayloadIndex,
					Config.Payloads[PayloadIndex].Channel,
					Config.Payloads[PayloadIndex].Payload,
					Config.Payloads[PayloadIndex].Time,
					Config.Payloads[PayloadIndex].Latitude,
					Config.Payloads[PayloadIndex].Longitude,
					Config.Payloads[PayloadIndex].Altitude,
					Config.Payloads[PayloadIndex].AscentRate,
					Config.Payloads[PayloadIndex].Telemetry);

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
	
	return port_closed;
}

void *ServerLoop( void *some_void_ptr )
{
    struct sockaddr_in serv_addr;
	struct TServerInfo *ServerInfo;
	
	ServerInfo = (struct TServerInfo *)some_void_ptr;

    ServerInfo->sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    memset( &serv_addr, '0', sizeof( serv_addr ) );

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    serv_addr.sin_port = htons(ServerInfo->Port);

    LogMessage( "Listening on %s port %d\n", ServerInfo->ServerIndex ? "HAB" : "JSON", ServerInfo->Port);

    if (setsockopt(ServerInfo->sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
	{
        LogMessage( "setsockopt(SO_REUSEADDR) failed" );
    }

    if (bind(ServerInfo->sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        LogMessage( "Server failed errno %d\n", errno );
        exit( -1 );
    }

    listen(ServerInfo->sockfd, 10);
	
    while (run)
    {
		int SendEveryMS = 1000;
		int MSPerLoop=100;
        int ms=0;
		int connfd;
		
		Config.EnableDev=1;

		fcntl(ServerInfo->sockfd, F_SETFL, fcntl(ServerInfo->sockfd, F_GETFL) & ~O_NONBLOCK);	// Blocking mode so we wait for a connection

        connfd = accept(ServerInfo->sockfd, ( struct sockaddr * ) NULL, NULL );	// Wait for connection

        LogMessage( "Connected to %s client\n", ServerInfo->ServerIndex ? "HAB" : "JSON");
		ServerInfo->Connected = 1;

		fcntl(connfd, F_SETFL, fcntl(ServerInfo->sockfd, F_GETFL) | O_NONBLOCK);	// Non-blocking, so we don't block on receiving any commands from client

        while (ServerInfo->Connected)
        {
			int bytecount;

			ms += MSPerLoop;
			
			// Listen part
			bytecount = -1;
			if (ServerInfo->ServerIndex == 0)
			{
				char packet[4096];
				
				while ((bytecount = recv(connfd, packet, sizeof(packet), 0)) > 0)
				{
					char *line, *saveptr;
				
					packet[bytecount] = 0;
					
					// JSON server
					line = strtok_r(packet, "\n", &saveptr);
					while (line)
					{
						ProcessJSONClientLine(connfd, line);
						line = strtok_r( NULL, "\n", &saveptr);
					}
				}
				if (bytecount == 0)
				{
					// -1 is no more data, 0 means port closed
					ServerInfo->Connected = 0;
				}
			}
			else
			{
				char RxByte;
				
				while ((bytecount = recv(connfd, &RxByte, 1, 0)) > 0)
				{
					Config.LoRaDevices[Config.HABChannel].FromTelnetBuffer[Config.LoRaDevices[Config.HABChannel].FromTelnetBufferCount++] = RxByte;
					LogMessage("KEYB BUFFER %d BYTES\n", Config.LoRaDevices[Config.HABChannel].FromTelnetBufferCount);
				}
			}
			
			if (bytecount == 0)
			{
				// -1 is no more data, 0 means port closed
				LogMessage("Disconnected from %s client\n", ServerInfo->ServerIndex ? "HAB" : "JSON");
				ServerInfo->Connected = 0;
			}

			if (ServerInfo->Connected)
			{
				// Send part
				
				if (ServerInfo->ServerIndex == 0)
				{
					// Send to JSON client
					if (ms >= SendEveryMS)
					{
						if (SendJSON(connfd))
						{
							ServerInfo->Connected = 0;
						}
						ms = 0;
					}
				}
				else
				{
					int Channel;
					
					for (Channel=0; Channel<=1; Channel++)

					{
						if (Config.LoRaDevices[Channel].ToTelnetBufferCount > 0)
						{
							send(connfd, Config.LoRaDevices[Channel].ToTelnetBuffer, Config.LoRaDevices[Channel].ToTelnetBufferCount, 0);
							Config.LoRaDevices[Channel].ToTelnetBufferCount = 0;
						}

					}

				}
			}
			
			delay(MSPerLoop);
        }

        close(connfd);
    }

    return NULL;
}
