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

#include "ssdv.h"
#include "global.h"

void ConvertFile(char *FileName)
{
	char TargetFile[100], CommandLine[200], *ptr;
	
	// printf("Found file %s to convert\n", FileName);
	
	strcpy(TargetFile, FileName);
	ptr = strchr(TargetFile, '.');
	if (ptr)
	{
		*ptr = '\0';
		strcat(TargetFile, ".JPG");
		
		// Now convert the file
		sprintf(CommandLine, "ssdv -d /tmp/%s %s 2> /dev/null > /dev/null", FileName, TargetFile);
		system(CommandLine);	
		
		if (Config.ftpServer[0] && Config.ftpUser[0] && Config.ftpPassword[0])
		{
			// Upload to ftp server
			sprintf(CommandLine, "curl -T %s %s -Q \"TYPE I\" --user %s:%s 2> /dev/null > /dev/null", TargetFile, Config.ftpServer, Config.ftpUser, Config.ftpPassword);
			system(CommandLine);
		}
	}
}


void *SSDVLoop(void *some_void_ptr)
{
    while (1)
    {
		DIR *dp;
		struct dirent *ep;
		struct stat st;
		char *SSDVFolder;
		char FileName[100];
	
		SSDVFolder = "/tmp";
		
		dp = opendir(SSDVFolder);
		if (dp != NULL)
		{
			while (ep = readdir (dp))
			{
				if (strstr(ep->d_name, ".bin") != NULL)
				{
					sprintf(FileName, "%s/%s", SSDVFolder, ep->d_name);
					stat(FileName, &st);
					// printf ("Age of '%s' is %ld seconds\n", FileName, time(0) - st.st_mtime);
					if ((time(0) - st.st_mtime) < 20)
					{
						ConvertFile(ep->d_name);
					}
				}
			}
		}
		(void) closedir (dp);
		
		sleep(5);
	}
}
