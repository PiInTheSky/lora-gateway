#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>              // Standard input/output definitions
#include <string.h>             // String function definitions
#include <unistd.h>             // UNIX standard function definitions
#include <fcntl.h>              // File control definitions
#include <errno.h>              // Error number definitions
#include <termios.h>            // POSIX terminal control definitions
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>

#include "ftp.h"
#include "global.h"

void
ConvertFile( char *FileName )
{
    char TargetFile[100], CommandLine[200], *ptr;

    strcpy( TargetFile, FileName );
    ptr = strchr( TargetFile, '.' );
    if ( ptr && Config.SSDVJpegFolder[0] )
    {
        *ptr = '\0';
        strcat( TargetFile, ".JPG" );

        // Now convert the file
        // LogMessage("Converting %s to %s\n", FileName, TargetFile);

        sprintf( CommandLine,
                 "ssdv -d /tmp/%s %s/%s 2> /dev/null > /dev/null", FileName,
                 Config.SSDVJpegFolder, TargetFile );
        // LogMessage("COMMAND %s\n", CommandLine);
        system( CommandLine );

        if ( Config.ftpServer[0] && Config.ftpUser[0]
             && Config.ftpPassword[0] )
        {
            // Upload to ftp server
            sprintf( CommandLine,
                     "curl -T %s %s -Q \"TYPE I\" --user %s:%s 2> /dev/null > /dev/null",
                     TargetFile, Config.ftpServer, Config.ftpUser,
                     Config.ftpPassword );
            system( CommandLine );
        }
    }
}


void *
FTPLoop( void *some_void_ptr )
{
    while ( 1 )
    {
        DIR *dp;
        struct dirent *ep;
        struct stat st;
        char *SSDVFolder;
        char FileName[100], TempName[100];

        SSDVFolder = "/tmp";

        dp = opendir( SSDVFolder );
        if ( dp != NULL )
        {
            while ( ( ep = readdir( dp ) ) )
            {
                if ( strstr( ep->d_name, ".bin" ) != NULL )
                {
                    sprintf( FileName, "%s/%s", SSDVFolder, ep->d_name );
                    stat( FileName, &st );
                    // LogMessage("Age of '%s' is %ld seconds\n", FileName, time(0) - st.st_mtime);
                    if ( ( time( 0 ) - st.st_mtime ) < 20 )
                    {
                        ConvertFile( ep->d_name );
                    }
                    else if ( ( time( 0 ) - st.st_mtime ) > 120 )
                    {
                        sprintf( TempName, "%s/%s", SSDVFolder, ep->d_name );
                        // LogMessage("Removing %s\n", TempName);
                        remove( TempName );
                    }
                }
            }
        }
        ( void ) closedir( dp );

        sleep( 5 );
    }
}
