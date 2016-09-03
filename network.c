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
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <wiringPi.h>           // Include WiringPi library!
#include "network.h"
#include "global.h"

int
HaveAnIPAddress( void )
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *addr;
    int FoundAddress;

    FoundAddress = 0;

    if ( getifaddrs( &ifap ) == 0 )
    {
        // Success
        for ( ifa = ifap; ifa; ifa = ifa->ifa_next )
        {
            if ( ifa->ifa_addr != NULL )
            {
                // Family is known (which it isn't for a VPN)
                if ( ifa->ifa_addr->sa_family == AF_INET )
                {
                    sa = ( struct sockaddr_in * ) ifa->ifa_addr;
                    addr = inet_ntoa( sa->sin_addr );
                    if ( strcmp( addr, "127.0.0.1" ) != 0 )
                    {
                        FoundAddress = 1;
                    }
                }
            }
        }
    }

    freeifaddrs( ifap );

    return FoundAddress;
}

int
CanSeeTheInternet( void )
{
    struct addrinfo hints, *res;
    int status, sockfd, FoundInternet;

    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_UNSPEC;    // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ( ( status = getaddrinfo( "google.com", "80", &hints, &res ) ) != 0 )
    {
        return 0;
    }

    sockfd = socket( res->ai_family, res->ai_socktype, res->ai_protocol );

    FoundInternet = 1;
    if ( connect( sockfd, res->ai_addr, res->ai_addrlen ) == -1 )
    {
        FoundInternet = 0;
    }

    close( sockfd );

    freeaddrinfo( res );        // free the linked list

    return FoundInternet;
}

void *
NetworkLoop( void *some_void_ptr )
{
    while ( 1 )
    {
        if ( HaveAnIPAddress(  ) )
        {
            digitalWrite( Config.NetworkLED, 1 );
//          LogMessage("On network :-)\n");

            if ( CanSeeTheInternet(  ) )
            {
                digitalWrite( Config.InternetLED, 1 );
//              LogMessage("On the internet :-)\n");
            }
            else
            {
                digitalWrite( Config.InternetLED, 0 );
//              LogMessage("Not on internet :-(\n");
            }
        }
        else
        {
            digitalWrite( Config.NetworkLED, 0 );
//          LogMessage("No network :-(\n");
        }

        sleep( 5 );
    }
}
