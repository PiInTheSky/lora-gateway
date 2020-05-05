#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#include "hablink.h"
#include "global.h"

char HablinkSentence[256];

void SetHablinkSentence(char *tmp)
{
	strcpy(HablinkSentence, tmp);
}

void UploadSentence(int sockfd, char *Sentence)
{
	char Message[300];
	
	// Create message to upload
	sprintf(Message, "POSITION:CALLSIGN=%s,SENTENCE=%s\n", Config.Tracker, Sentence);

	write(sockfd, Message, strlen(Message));
}

	
int ConnectSocket(char *Host, int Port)
{
	int sockfd;
	struct sockaddr_in serv_addr; 
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		return -1;
	} 

	memset(&serv_addr, '0', sizeof(serv_addr)); 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(Port); 

	if (inet_pton(AF_INET, Host, &serv_addr.sin_addr) <=0)
	{
		close(sockfd);
		return -1;
	} 

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		close(sockfd);
		return -1;
	} 
		
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	
	return sockfd;
}

int StillConnected(int sockfd)
{
	char buf[1];
	int Count;
	
	Count = read(sockfd, buf, 1);
	
	return Count != 0;
}

void *HablinkLoop( void *vars )
{
	int sockfd;
	
	sockfd = -1;

	while (1)
	{
		if (sockfd < 0)
		{
			LogMessage("Connecting to hab.link (%s:%d)...\n", Config.HablinkAddress, 8887);

			while (sockfd < 0)
			{
				sockfd = ConnectSocket(Config.HablinkAddress, 8887);
				
				if (sockfd >= 0)
				{
					LogMessage("Connected to hab.link\n");
				}
				else
				{
					sleep(1);
				}
			}
		}
		
		// Check still connected
		if (!StillConnected(sockfd))
		{
			LogMessage("Disconnected from hab.link\n");
			close(sockfd);
			sockfd = -1;
		}
			
		if ((sockfd >= 0) && HablinkSentence[0])
		{
			UploadSentence(sockfd, HablinkSentence);
			HablinkSentence[0] = '\0';
		}
		
        usleep(100000);
	}
}
