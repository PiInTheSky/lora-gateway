#include <features.h>
#define __USE_XOPEN
#include <stdio.h>
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h> 
#include <pthread.h>
#include <curses.h>
#include <math.h>
#include <dirent.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <time.h>

#include "urlencode.h"
#include "base64.h"
#include "ssdv.h"
#include "ftp.h"
#include "habitat.h"
#include "network.h"
#include "global.h"
#include "server.h"
#include "gateway.h"
#include "config.h"
#include "gui.h"
#include "listener.h"

#define VERSION	"V1.8.14"
bool run = TRUE;

// RFM98
uint8_t currentMode = 0x81;

#define REG_FIFO                    0x00
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F
#define REG_RX_NB_BYTES             0x13
#define REG_OPMODE                  0x01
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS               0x12
#define REG_PACKET_SNR				0x19
#define REG_PACKET_RSSI				0x1A
#define REG_CURRENT_RSSI			0x1B
#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_MODEM_CONFIG            0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_MODEM_CONFIG3           0x26
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_HOP_PERIOD              0x24
#define REG_FREQ_ERROR				0x28
#define REG_DETECT_OPT				0x31
#define	REG_DETECTION_THRESHOLD		0x37

// MODES
#define RF98_MODE_RX_CONTINUOUS     0x85
#define RF98_MODE_TX                0x83
#define RF98_MODE_SLEEP             0x80
#define RF98_MODE_STANDBY           0x81

#define PAYLOAD_LENGTH              255

// Modem Config 1
#define EXPLICIT_MODE               0x00
#define IMPLICIT_MODE               0x01

#define ERROR_CODING_4_5            0x02
#define ERROR_CODING_4_6            0x04
#define ERROR_CODING_4_7            0x06
#define ERROR_CODING_4_8            0x08

#define BANDWIDTH_7K8               0x00
#define BANDWIDTH_10K4              0x10
#define BANDWIDTH_15K6              0x20
#define BANDWIDTH_20K8              0x30
#define BANDWIDTH_31K25             0x40
#define BANDWIDTH_41K7              0x50
#define BANDWIDTH_62K5              0x60
#define BANDWIDTH_125K              0x70
#define BANDWIDTH_250K              0x80
#define BANDWIDTH_500K              0x90

// Modem Config 2

#define SPREADING_6                 0x60
#define SPREADING_7                 0x70
#define SPREADING_8                 0x80
#define SPREADING_9                 0x90
#define SPREADING_10                0xA0
#define SPREADING_11                0xB0
#define SPREADING_12                0xC0

#define CRC_OFF                     0x00
#define CRC_ON                      0x04

// POWER AMPLIFIER CONFIG
#define REG_PA_CONFIG               0x09
#define PA_MAX_BOOST                0x8F
#define PA_LOW_BOOST                0x81
#define PA_MED_BOOST                0x8A
#define PA_MAX_UK                   0x88
#define PA_OFF_BOOST                0x00
#define RFO_MIN                     0x00

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23    // 0010 0011
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN                0xC0    // 1100 0000

struct TLoRaMode 
{
	int	ImplicitOrExplicit;
	int ErrorCoding;
	int Bandwidth;
	int SpreadingFactor;
	int LowDataRateOptimize;
	int BaudRate;
	char *Description;
} LoRaModes[] =
{
	{EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_20K8, SPREADING_11, 1,    60, "Telemetry"},			// 0: Normal mode for telemetry
	{IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_20K8, SPREADING_6,  0,  1400, "SSDV"},				// 1: Normal mode for SSDV
	{EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_62K5, SPREADING_8,  0,  2000, "Repeater"},			// 2: Normal mode for repeater network	
	{EXPLICIT_MODE, ERROR_CODING_4_6, BANDWIDTH_250K, SPREADING_7,  0,  8000, "Turbo"},				// 3: Normal mode for high speed images in 868MHz band
	{IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_250K, SPREADING_6,  0, 16828, "TurboX"},			// 4: Fastest mode within IR2030 in 868MHz band
	{EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_41K7, SPREADING_11, 0,   200, "Calling"},			// 5: Calling mode
	{IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_41K7, SPREADING_6,  0,  2800, "Uplink"},			// 6: Uplink mode for 868
	{EXPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_20K8, SPREADING_7,  0,  2800, "Telnet"},			// 7: Telnet-style comms with HAB on 434
	{IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_62K5, SPREADING_6,  0,  4500, "SSDV Repeater"}		// 8: Fast (SSDV) repeater network	
};

struct TConfig Config;

struct TBandwidth
{
	int LoRaValue;
	double Bandwidth;
	char *ConfigString;
} Bandwidths[] =
{
	{BANDWIDTH_7K8,     7.8,	"7K8"},
	{BANDWIDTH_10K4,   10.4,	"10K4"},
	{BANDWIDTH_15K6,   15.6,	"15K6"},
	{BANDWIDTH_20K8,   20.8,	"20K8"},
	{BANDWIDTH_31K25,  31.25,	"31K25"},
	{BANDWIDTH_41K7,   41.7,	"41K7"},
	{BANDWIDTH_62K5,   62.5,	"62K5"},
	{BANDWIDTH_125K,  125.0,	"125K"},
	{BANDWIDTH_250K,  250.0,	"250K"},
	{BANDWIDTH_500K,  500.0,	"500K"}
};

int LEDCounts[2];

int help_win_displayed = 0;

pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER;

#pragma pack(1)

struct TBinaryPacket {
    uint8_t PayloadIDs;
    uint16_t Counter;
    uint16_t BiSeconds;
    float Latitude;
    float Longitude;
    uint16_t Altitude;
};

// Create pipes for inter proces communication 
// GLOBAL AS CALLED FROM INTERRRUPT
int telem_pipe_fd[2];
int ssdv_pipe_fd[2];

// Create a structure to share some variables with the habitat child process
// GLOBAL AS CALLED FROM INTERRRUPT
thread_shared_vars_t htsv;

// Create a structure to share some variables with the ssdv child process
// GLOBAL AS CALLED FROM INTERRRUPT
thread_shared_vars_t stsv;

WINDOW *mainwin=NULL;		// Curses window

void CloseDisplay( WINDOW * mainwin )
{
    /*  Clean up after ourselves  */
    delwin( mainwin );
    endwin(  );
    refresh(  );
}


void bye(void)
{
	if (mainwin != NULL)
	{
		CloseDisplay( mainwin);
		mainwin = NULL;
	}
}

void exit_error(char *msg)
{
	bye();		// Close ncurses window, plus any future tidy-ups
	
	fprintf(stderr, msg);
    exit(1);	
}


void
hexdump_buffer( const char *title, const char *buffer, const int len_buffer )
{
    int i, j = 0;
    char message[200];
    FILE *fp;

    fp = fopen( "pkt.txt", "a" );

    fprintf( fp, "Title = %s\n", title );

    for ( i = 0; i < len_buffer; i++ )
    {
        sprintf( &message[3 * j], "%02x ", buffer[i] );
        j++;
        if ( i % 16 == 15 )
        {
            j = 0;
            fprintf( fp, "%s\n", message );
            message[0] = '\0';
        }
    }
    fprintf( fp, "%s\n", message );
    fclose( fp );

}

void
writeRegister( int Channel, uint8_t reg, uint8_t val )
{
    unsigned char data[2];

    data[0] = reg | 0x80;
    data[1] = val;
    wiringPiSPIDataRW( Channel, data, 2 );
}

uint8_t
readRegister( int Channel, uint8_t reg )
{
    unsigned char data[2];
    uint8_t val;

    data[0] = reg & 0x7F;
    data[1] = 0;
    wiringPiSPIDataRW( Channel, data, 2 );
    val = data[1];

    return val;
}

void LogPacket( rx_metadata_t *Metadata, int Bytes, unsigned char MessageType )
{
    if ( Config.EnablePacketLogging )
    {
        FILE *fp;

        if ( ( fp = fopen( "packets.txt", "at" ) ) != NULL )
        {
            struct tm *tm;
            tm = localtime( &Metadata->Timestamp );

            fprintf( fp,
                     "%04d-%02d-%02d"
                     " %02d:%02d:%02d"
                     " - Ch %d"
                     ", SNR %d"
                     ", RSSI %d"
                     ", Freq %.1lf"
                     ", FreqErr %.1lf"
                     ", BW %.2lf"
                     ", EC 4:%d"
                     ", SF %d"
                     ", LDRO %d"
                     ", Impl %d"
                     ", Bytes %d"
                     ", Type %02Xh\n",
                     (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec,
                     Metadata->Channel,
                     Metadata->SNR,
                     Metadata->RSSI,
                     Metadata->Frequency*1000, /* NB: in KHz */
                     Metadata->FrequencyError*1000, /* NB: in KHz */
                     Metadata->Bandwidth,
                     Metadata->ErrorCoding,
                     Metadata->SpreadingFactor,
                     Metadata->LowDataRateOptimize,
                     Metadata->ImplicitOrExplicit,
                     Bytes,
                     MessageType );

            fclose( fp );
        }
    }
}

void LogTelemetryPacket(char *Telemetry)
{
    // if (Config.EnableTelemetryLogging)
    {
        FILE *fp;

        if ( ( fp = fopen( "telemetry.txt", "at" ) ) != NULL )
        {
            time_t now;
            struct tm *tm;

            now = time( 0 );
            tm = localtime( &now );

            fprintf( fp, "%02d:%02d:%02d - %s\n", tm->tm_hour, tm->tm_min,
                     tm->tm_sec, Telemetry );

            fclose( fp );
        }
    }
}

void LogMessage( const char *format, ... )
{
    static WINDOW *Window = NULL;
    char Buffer[512];

    pthread_mutex_lock( &var ); // lock the critical section

    if ( Window == NULL )
    {
        // Window = newwin(25, 30, 0, 50);
        Window = newwin( LINES - 16, COLS, 16, 0 );
        scrollok( Window, TRUE );
    }

    va_list args;
    va_start( args, format );

    vsprintf( Buffer, format, args );

    va_end( args );

    if ( strlen( Buffer ) > COLS - 1 )
    {
        Buffer[COLS - 3] = '.';
        Buffer[COLS - 2] = '.';
        Buffer[COLS - 1] = '\n';
        Buffer[COLS] = 0;
    }

    waddstr( Window, Buffer );

    wrefresh( Window );

    pthread_mutex_unlock( &var );   // unlock once you are done

}

void ChannelPrintf(int Channel, int row, int column, const char *format, ... )
{
    char Buffer[80];
    va_list args;

    pthread_mutex_lock( &var ); // lock the critical section

    va_start( args, format );

    vsprintf( Buffer, format, args );

    va_end( args );

    mvwaddstr( Config.LoRaDevices[Channel].Window, row, column, Buffer );

    if (! help_win_displayed)
    {
        wrefresh( Config.LoRaDevices[Channel].Window );
    }

    pthread_mutex_unlock( &var );   // unlock once you are done

}

void
setMode( int Channel, uint8_t newMode )
{
    if ( newMode == currentMode )
        return;

    switch ( newMode )
    {
        case RF98_MODE_TX:
            writeRegister( Channel, REG_LNA, LNA_OFF_GAIN );    // TURN LNA OFF FOR TRANSMITT
            writeRegister( Channel, REG_PA_CONFIG, Config.LoRaDevices[Channel].Power ); // PA_MAX_UK
            writeRegister( Channel, REG_OPMODE, newMode );
            currentMode = newMode;
            break;
        case RF98_MODE_RX_CONTINUOUS:
            writeRegister( Channel, REG_PA_CONFIG, PA_OFF_BOOST );  // TURN PA OFF FOR RECIEVE??
            writeRegister( Channel, REG_LNA, LNA_MAX_GAIN );    // MAX GAIN FOR RECEIVE
            writeRegister( Channel, REG_OPMODE, newMode );
            currentMode = newMode;
            // LogMessage("Changing to Receive Continuous Mode\n");
            break;
        case RF98_MODE_SLEEP:
            writeRegister( Channel, REG_OPMODE, newMode );
            currentMode = newMode;
            // LogMessage("Changing to Sleep Mode\n"); 
            break;
        case RF98_MODE_STANDBY:
            writeRegister( Channel, REG_OPMODE, newMode );
            currentMode = newMode;
            // LogMessage("Changing to Standby Mode\n");
            break;
        default:
            return;
    }

    if ( newMode != RF98_MODE_SLEEP )
    {
        while ( digitalRead( Config.LoRaDevices[Channel].DIO5 ) == 0 )
        {
        }
        // delay(1);
    }

    // LogMessage("Mode Change Done\n");
    return;
}

void setFrequency( int Channel, double Frequency )
{
    unsigned long FrequencyValue;
    char FrequencyString[10];

    // Format frequency as xxx.xxx.x Mhz
    sprintf( FrequencyString, "%8.4lf ", Frequency );
    FrequencyString[8] = FrequencyString[7];
    FrequencyString[7] = '.';

    FrequencyValue = ( unsigned long ) ( Frequency * 7110656 / 434 );

    writeRegister( Channel, 0x06, ( FrequencyValue >> 16 ) & 0xFF );    // Set frequency
    writeRegister( Channel, 0x07, ( FrequencyValue >> 8 ) & 0xFF );
    writeRegister( Channel, 0x08, FrequencyValue & 0xFF );

    Config.LoRaDevices[Channel].activeFreq = Frequency;

    // LogMessage("Set Frequency to %lf\n", Frequency);

    ChannelPrintf( Channel, 1, 1, "Channel %d %s MHz ", Channel, FrequencyString );
}

void displayFrequency ( int Channel, double Frequency )
{
    char FrequencyString[10];

    // Format frequency as xxx.xxx.x Mhz
    sprintf( FrequencyString, "%8.4lf ", Frequency );
    FrequencyString[8] = FrequencyString[7];
    FrequencyString[7] = '.';

    ChannelPrintf( Channel, 1, 1, "Channel %d %s MHz ", Channel, FrequencyString );
}

void setLoRaMode( int Channel )
{
    // LogMessage("Setting LoRa Mode\n");
    setMode( Channel, RF98_MODE_SLEEP );
    writeRegister( Channel, REG_OPMODE, 0x80 );

    setMode( Channel, RF98_MODE_SLEEP );

    // LogMessage("Set Default Frequency\n");
    setFrequency( Channel, Config.LoRaDevices[Channel].Frequency);
}

int IntToSF(int Value)
{
	return Value << 4;
}

int SFToInt(int SpreadingFactor)
{
	return SpreadingFactor >> 4;
}

int IntToEC(int Value)
{
	return (Value - 4) << 1;
}

int ECToInt(int ErrorCoding)
{
	return (ErrorCoding >> 1) + 4;
}

int DoubleToBandwidth(double Bandwidth)
{
	int i;
	
	for (i=0; i<10; i++)
	{
		if (abs(Bandwidth - Bandwidths[i].Bandwidth) < (Bandwidths[i].Bandwidth/10))
		{
			return Bandwidths[i].LoRaValue;
		}
	}

	return BANDWIDTH_20K8;
}

double BandwidthToDouble(int LoRaValue)
{
	int i;
	
	for (i=0; i<10; i++)
	{
		if (LoRaValue == Bandwidths[i].LoRaValue)
		{
			return Bandwidths[i].Bandwidth;
		}
	}

	return 20.8;
}

int IntToLowOpt(int Value)
{
	return Value ? 0x08 : 0;
}

int LowOptToInt(int LowOpt)
{
	return LowOpt ? 1 : 0;
}

void SetLoRaParameters( int Channel, int ImplicitOrExplicit, int ErrorCoding, double Bandwidth, int SpreadingFactor, int LowDataRateOptimize )
{
    writeRegister( Channel, REG_MODEM_CONFIG, ImplicitOrExplicit | IntToEC(ErrorCoding) | DoubleToBandwidth(Bandwidth));
    writeRegister( Channel, REG_MODEM_CONFIG2, IntToSF(SpreadingFactor) | CRC_ON );
    writeRegister( Channel, REG_MODEM_CONFIG3, 0x04 | IntToLowOpt(LowDataRateOptimize));    // 0x04: AGC sets LNA gain
    writeRegister( Channel, REG_DETECT_OPT, ( readRegister( Channel, REG_DETECT_OPT ) & 0xF8 ) | ( ( SpreadingFactor == 6 ) ? 0x05 : 0x03 ) );    // 0x05 For SF6; 0x03 otherwise
    writeRegister( Channel, REG_DETECTION_THRESHOLD, ( SpreadingFactor == 6 ) ? 0x0C : 0x0A );    // 0x0C for SF6, 0x0A otherwise

    Config.LoRaDevices[Channel].CurrentBandwidth = Bandwidth;			// Used for AFC - current bandwidth may be different to that configured (i.e. because we're using calling mode)

    ChannelPrintf( Channel, 2, 1, "%s, %.2lf, SF%d, EC4:%d %s",
                   ImplicitOrExplicit ? "Implicit" : "Explicit",
                   Bandwidth,
				   SpreadingFactor,
                   ErrorCoding,
                   LowDataRateOptimize ? "LDRO" : "" );
}

void displayLoRaParameters( int Channel, int ImplicitOrExplicit, int ErrorCoding, double Bandwidth, int SpreadingFactor, int LowDataRateOptimize )
{

    ChannelPrintf( Channel, 2, 1, "%s, %.2lf, SF%d, EC4:%d %s",
                   ImplicitOrExplicit ? "Implicit" : "Explicit",
                   Bandwidth,
				   SpreadingFactor,
                   ErrorCoding,
                   LowDataRateOptimize ? "LDRO" : "" );
}

void SetDefaultLoRaParameters( int Channel )
{
    // LogMessage("Set Default Parameters\n");

    SetLoRaParameters( Channel,
                       Config.LoRaDevices[Channel].ImplicitOrExplicit,
                       Config.LoRaDevices[Channel].ErrorCoding,
                       Config.LoRaDevices[Channel].Bandwidth,
                       Config.LoRaDevices[Channel].SpreadingFactor,
                       Config.LoRaDevices[Channel].LowDataRateOptimize );
}

/////////////////////////////////////
//    Method:   Setup to receive continuously
//////////////////////////////////////
void
startReceiving( int Channel )
{
    writeRegister( Channel, REG_DIO_MAPPING_1, 0x00 );  // 00 00 00 00 maps DIO0 to RxDone

    writeRegister( Channel, REG_PAYLOAD_LENGTH, 255 );
    writeRegister( Channel, REG_RX_NB_BYTES, 255 );

    writeRegister( Channel, REG_FIFO_RX_BASE_AD, 0 );
    writeRegister( Channel, REG_FIFO_ADDR_PTR, 0 );

    // Setup Receive Continous Mode
    setMode( Channel, RF98_MODE_RX_CONTINUOUS );
}

void ReTune( int Channel, double FreqShift )
{
    setMode( Channel, RF98_MODE_SLEEP );
    LogMessage( "Retune by %.1lfkHz\n", FreqShift * 1000 );
    setFrequency( Channel, Config.LoRaDevices[Channel].activeFreq + FreqShift );
    startReceiving( Channel );
}

void SendLoRaData(int Channel, char *buffer, int Length)
{
    unsigned char data[257];
    int i;

	// Change frequency for the uplink ?
	if (Config.LoRaDevices[Channel].UplinkFrequency > 0)
	{
		LogMessage("Change frequency to %.3lfMHz\n", Config.LoRaDevices[Channel].UplinkFrequency);
        setFrequency(Channel, Config.LoRaDevices[Channel].UplinkFrequency);
	}
	
	// Change mode for the uplink ?
	if (Config.LoRaDevices[Channel].UplinkMode >= 0)
	{
		int UplinkMode;
		
		UplinkMode = Config.LoRaDevices[Channel].UplinkMode;
		
		LogMessage("Change LoRa mode to %d\n", Config.LoRaDevices[Channel].UplinkMode);
		
        SetLoRaParameters(Channel,
						  LoRaModes[UplinkMode].ImplicitOrExplicit,
						  ECToInt(LoRaModes[UplinkMode].ErrorCoding),
						  BandwidthToDouble(LoRaModes[UplinkMode].Bandwidth),
						  SFToInt(LoRaModes[UplinkMode].SpreadingFactor),
						  0);
	}
	
    LogMessage( "LoRa Channel %d Sending %d bytes\n", Channel, Length );
    Config.LoRaDevices[Channel].Sending = 1;

    setMode( Channel, RF98_MODE_STANDBY );

    writeRegister( Channel, REG_DIO_MAPPING_1, 0x40 );  // 01 00 00 00 maps DIO0 to TxDone

    writeRegister( Channel, REG_FIFO_TX_BASE_AD, 0x00 );    // Update the address ptr to the current tx base address
    writeRegister( Channel, REG_FIFO_ADDR_PTR, 0x00 );

    data[0] = REG_FIFO | 0x80;
    for ( i = 0; i < Length; i++ )
    {
        data[i + 1] = buffer[i];
    }
    wiringPiSPIDataRW( Channel, data, Length + 1 );

    // Set the length. For implicit mode, since the length needs to match what the receiver expects, we have to set a value which is 255 for an SSDV packet
    writeRegister( Channel, REG_PAYLOAD_LENGTH, Length );

    // go into transmit mode
    setMode( Channel, RF98_MODE_TX );
}

void ShowPacketCounts(int Channel)
{
    if (Config.LoRaDevices[Channel].InUse)
    {
        ChannelPrintf( Channel, 7, 1, "Telem Packets = %d (%us)     ",
                       Config.LoRaDevices[Channel].TelemetryCount,
                       Config.LoRaDevices[Channel].LastTelemetryPacketAt ? (unsigned int) (time(NULL) - Config.LoRaDevices[Channel].LastTelemetryPacketAt) : 0);
        ChannelPrintf( Channel, 8, 1, "Image Packets = %d (%us)     ",
                       Config.LoRaDevices[Channel].SSDVCount,
                       Config.LoRaDevices[Channel].
                       LastSSDVPacketAt ? ( unsigned int ) ( time( NULL ) -
                                                             Config.
                                                             LoRaDevices
                                                             [Channel].
                                                             LastSSDVPacketAt )
                       : 0 );

        ChannelPrintf( Channel, 9, 1, "Bad CRC = %d Bad Type = %d",
                       Config.LoRaDevices[Channel].BadCRCCount,
                       Config.LoRaDevices[Channel].UnknownCount );
    }
}

void ProcessUploadMessage(int Channel, char *Message)
{
    // LogMessage("Ch %d: Uploaded message %s\n", Channel, Message);
}

void ProcessCallingMessage(int Channel, char *Message)
{
    char Payload[16];
    double Frequency;
    int ImplicitOrExplicit, ErrorCoding, Bandwidth, SpreadingFactor, LowDataRateOptimize;

    ChannelPrintf( Channel, 3, 1, "Calling message %d bytes ",
                   strlen( Message ) );

    if ( sscanf( Message + 2, "%15[^,],%lf,%d,%d,%d,%d,%d",
                 Payload,
                 &Frequency,
                 &ImplicitOrExplicit,
                 &ErrorCoding,
                 &Bandwidth, &SpreadingFactor, &LowDataRateOptimize ) == 7 )
    {
        if (Config.LoRaDevices[Channel].AFC)
        {
            Frequency += (Config.LoRaDevices[Channel].activeFreq - Config.LoRaDevices[Channel].Frequency);
        }

        LogMessage( "Ch %d: Calling message, new frequency %7.3lf\n", Channel,
                    Frequency );

        // Decoded OK
        setMode( Channel, RF98_MODE_SLEEP );

        // setFrequency(Channel, Config.LoRaDevices[Channel].activeFreq + );
        setFrequency( Channel, Frequency );

        SetLoRaParameters( Channel, ImplicitOrExplicit, ECToInt(ErrorCoding), BandwidthToDouble(Bandwidth), SFToInt(SpreadingFactor), LowOptToInt(LowDataRateOptimize));

        setMode( Channel, RF98_MODE_RX_CONTINUOUS );

        Config.LoRaDevices[Channel].InCallingMode = 1;

        // ChannelPrintf(Channel, 1, 1, "Channel %d %7.3lfMHz              ", Channel, Frequency);
    }
}

void RemoveOldPayloads(void)
{
	int i;
	
	for (i=0; i<MAX_PAYLOADS; i++)
	{
		if (Config.Payloads[i].InUse)
		{
            if ((time(NULL) - Config.Payloads[i].LastPacketAt) > 10800)
			{
				// More than 3 hours old, so remove it
			}
			
			Config.Payloads[i].InUse = 0;
		}
	}
}

int FindFreePayload(char *Payload)
{
	int i, Oldest;
	
	// First pass - find match for payload
	for (i=0; i<MAX_PAYLOADS; i++)
	{
		if (Config.Payloads[i].InUse)
		{
			if (strcmp(Payload, Config.Payloads[i].Payload) == 0)
			{
				return i;
			}
		}
	}
	
	// Second pass - just find a free position
	for (i=0; i<MAX_PAYLOADS; i++)
	{
		if (!Config.Payloads[i].InUse)
		{
			Config.Payloads[i].InUse = 1;
			strcpy(Config.Payloads[i].Payload, Payload);
            return i;
		}
	}
	
	// Third pass - find oldest payload
	Oldest = 0;
	for (i=1; i<MAX_PAYLOADS; i++)
	{
		if (Config.Payloads[i].LastPositionAt < Config.Payloads[Oldest].LastPositionAt)
		{
			Oldest = i;
		}
	}
	
	strcpy(Config.Payloads[Oldest].Payload, Payload);
	
	return i;
}

void DoPositionCalcs(int PayloadIndex)
{
    unsigned long Now;
    struct tm tm;
    float Climb, Period;

    strptime(Config.Payloads[PayloadIndex].Time, "%H:%M:%S", &tm);
    Now = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

    if ((Config.Payloads[PayloadIndex].LastPositionAt > 0 )
         && ( Now > Config.Payloads[PayloadIndex].LastPositionAt ) )
    {
        Climb = (float)Config.Payloads[PayloadIndex].Altitude - (float)Config.Payloads[PayloadIndex].PreviousAltitude;
        Period = (float)Now - (float)Config.Payloads[PayloadIndex].LastPositionAt;
        Config.Payloads[PayloadIndex].AscentRate = Climb / Period;
    }
    else
    {
        Config.Payloads[PayloadIndex].AscentRate = 0;
    }

    Config.Payloads[PayloadIndex].LastPositionAt = Now;
    Config.Payloads[PayloadIndex].PreviousAltitude = Config.Payloads[PayloadIndex].Altitude;
}

void ProcessLine(int Channel, char *Line)
{
	int PayloadIndex;
	char Payload[32];

	// Find free position for this payload
	sscanf(Line + 2, "%31[^,]", Payload);
	PayloadIndex = FindFreePayload(Payload);

	// Store sentence against this payload
    strcpy(Config.Payloads[PayloadIndex].Telemetry, Line);
	
	// Fill in source channel
	Config.Payloads[PayloadIndex].Channel = Channel;
	
	// Parse key fields from sentence
	sscanf( Line + 2, "%15[^,],%u,%8[^,],%lf,%lf,%u",
			(Config.Payloads[PayloadIndex].Payload),
			&(Config.Payloads[PayloadIndex].Counter),
			(Config.Payloads[PayloadIndex].Time),
			&(Config.Payloads[PayloadIndex].Latitude),
			&(Config.Payloads[PayloadIndex].Longitude),
			&(Config.Payloads[PayloadIndex].Altitude));

	// Mark when this was received, so we can time-out old payloads
	Config.Payloads[PayloadIndex].LastPacketAt = time(NULL);

	// Ascent rate
    DoPositionCalcs(PayloadIndex);
	
	// Update display
    ChannelPrintf(Channel, 4, 1, "%8.5lf, %8.5lf, %05u   ",
                  Config.Payloads[PayloadIndex].Latitude,
                  Config.Payloads[PayloadIndex].Longitude,
                  Config.Payloads[PayloadIndex].Altitude);	
}


void ProcessTelemetryMessage(int Channel, char *Message, rx_metadata_t *Metadata)
{
    if (strlen(Message + 1) < 250)
    {
        char *startmessage, *endmessage;
        char telem[40];
        char buffer[40];

        sprintf(telem, "Telemetry %d bytes", strlen( Message + 1 ));

        // Pad the string with spaces to the size of the window
        sprintf(buffer,"%-37s", telem );
        ChannelPrintf( Channel, 3, 1, buffer);

        startmessage = Message + strspn(Message, "$%") - 2;
        endmessage = strchr( startmessage, '\n' );
		if (endmessage == NULL)	endmessage = strchr(startmessage, 0);

        while ( endmessage != NULL )
        {
			int Repeated;
            struct tm *tm;

            *endmessage = '\0';

			LogTelemetryPacket(startmessage);
			
			if ((Repeated = (*startmessage == '%')))
			{
				*startmessage = '$';
			}

            strcpy( Config.LoRaDevices[Channel].Telemetry, startmessage );

            ProcessLine(Channel, startmessage);

            tm = localtime( &Metadata->Timestamp );


            if ( Config.EnableHabitat )
            {

                // Create a telemetry packet
                telemetry_t t;
                memcpy( t.Telemetry, startmessage, strlen( startmessage ) + 1 );
                memcpy( &t.Metadata, Metadata, sizeof( rx_metadata_t ) );

                // Add the telemetry packet to the pipe
                int result = write( telem_pipe_fd[1], &t, sizeof( t ) );
                if ( result == -1 )
                {
                    exit_error("Error writing to the telemetry pipe\n");
                }
                if ( result == 0 )
                {
                    LogMessage( "Nothing written to telemetry pipe \n" );
                }
                if ( result > 1 )
                {
                    htsv.packet_count++;
                }
            }

			Config.LoRaDevices[Channel].TelemetryCount++;
			
            LogMessage("%02d:%02d:%02d Ch%d: %s%s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Channel, startmessage, Repeated ? " (repeated)" : "");

			startmessage = endmessage + 1;
			endmessage = strchr( startmessage, '\n' );
        }

        Config.LoRaDevices[Channel].LastTelemetryPacketAt = Metadata->Timestamp;
    }
}

void ProcessTelnetMessage(int Channel, char *Message, int Bytes)
{
	char FirstByte;
	
	FirstByte = Message[1];
	
	Config.LoRaDevices[Channel].GotHABReply = 1;

    LogMessage( "Telnet Downlink message channel %d %c[%02X]'%s' bytes = %d\n", Channel, Message[0], FirstByte, Message+2, Bytes);

	// Store header details which are used in next Tx
	Config.LoRaDevices[Channel].HABConnected = (FirstByte & 0x80) ? 1 : 0;
	Config.LoRaDevices[Channel].HABAck = (FirstByte & 0x40) ? 1 : 0;

	if (Bytes > 2)
	{
		strcpy(Config.LoRaDevices[Channel].ToTelnetBuffer, Message+2);
		Config.LoRaDevices[Channel].ToTelnetBufferCount = Bytes-2;
	}
}

static char *decode_callsign( char *callsign, uint32_t code )
{
    char *c, s;

    *callsign = '\0';

    /* Is callsign valid? */
    if ( code > 0xF423FFFF )
        return ( callsign );

    for ( c = callsign; code; c++ )
    {
        s = code % 40;
        if ( s == 0 )
            *c = '-';
        else if ( s < 11 )
            *c = '0' + s - 1;
        else if ( s < 14 )
            *c = '-';
        else
            *c = 'A' + s - 14;
        code /= 40;
    }
    *c = '\0';

    return ( callsign );
}

int
FileExists( char *filename )
{
    struct stat st;

    return stat( filename, &st ) == 0;
}

void ProcessSSDVMessage( int Channel, char *Message, int Repeated)
{
    // SSDV packet
    uint32_t CallsignCode;
    char Callsign[7], *FileMode;
    int ImageNumber, PacketNumber;
    char filename[100];
    FILE *fp;
	time_t now;
	struct tm *tm;

	now = time( 0 );
	tm = localtime( &now );

    Message[0] = 0x55;

    CallsignCode = Message[2];
    CallsignCode <<= 8;
    CallsignCode |= Message[3];
    CallsignCode <<= 8;
    CallsignCode |= Message[4];
    CallsignCode <<= 8;
    CallsignCode |= Message[5];

    decode_callsign( Callsign, CallsignCode );

    ImageNumber = Message[6];
    PacketNumber = Message[7] * 256 + Message[8];

    LogMessage("%02d:%02d:%02d Ch%d: SSDV Packet, Callsign %s, Image %d, Packet %d%s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Channel, Callsign, Message[6], PacketNumber, Repeated ? " (repeated)" : "");
    ChannelPrintf( Channel, 3, 1, "SSDV Packet                     " );
    ChannelPrintf( Channel, 5, 1, "SSDV %s: Image %d, Packet %d", Callsign,
                   Message[6], PacketNumber );

    // Create new file ?
    sprintf( filename, "/tmp/%s_%d.bin", Callsign, ImageNumber );
    if ( !FileExists( filename ) )
    {
        // New image so new file
        LogMessage( "Started image %d\n", ImageNumber );

        // AddImageNumberToLog(Channel, ImageNumber);

        FileMode = "wb";
    }
    else
    {
        FileMode = "r+b";
    }

    // Save to file
    if ( ( fp = fopen( filename, FileMode ) ) )
    {
        fseek( fp, PacketNumber * 256, SEEK_SET );
        if ( fwrite( Message, 1, 256, fp ) == 256 )
        {
            // AddImagePacketToLog(Channel, ImageNumber, PacketNumber);
        }
        else
        {
            LogMessage( "** FAILED TO WRITE TO SSDV FILE\n" );
        }

        fclose( fp );
    }

    // ShowMissingPackets(Channel);

    if ( Config.EnableSSDV )
    {
        // Create a SSDV packet
        ssdv_t s;
        s.Channel = Channel;
        s.Packet_Number = Config.LoRaDevices[Channel].SSDVCount;
        memcpy( s.SSDV_Packet, Message, 256 );

        // Add the SSDV packet to the pipe
        int result = write( ssdv_pipe_fd[1], &s, sizeof( s ) );
        if ( result == -1 )
        {
            exit_error("Error writing to the issdv pipe\n");
        }
        if ( result == 0 )
        {
            LogMessage( "Nothing written to ssdv pipe \n" );
        }
        if ( result > 1 )
        {
            stsv.packet_count++;
        }

    }

    Config.LoRaDevices[Channel].SSDVCount++;
    Config.LoRaDevices[Channel].LastSSDVPacketAt = time( NULL );
}

void
TestMessageForSMSAcknowledgement( int Channel, char *Message )
{
    if ( Config.SMSFolder[0] )
    {
        if ( strlen( Message ) < 250 )
        {
            char Line[256];
            char *token, *value1, *value2;
            int FileNumber;

            value1 = NULL;
            value2 = NULL;

            strcpy( Line, Message );
            token = strtok( Line, "," );
            while ( token != NULL )
            {
                value1 = value2;
                value2 = token;
                token = strtok( NULL, "," );
            }

            // Rename the file matching this parameter
            if ( value1 )
            {
                char OldFileName[256], NewFileName[256];
                FileNumber = atoi( value1 );
                if ( FileNumber > 0 )
                {
                    sprintf( OldFileName, "%s/%d.sms", Config.SMSFolder, FileNumber );
                    if ( FileExists( OldFileName ) )
                    {
                        sprintf( NewFileName, "%s/%d.ack", Config.SMSFolder, FileNumber );
                        if ( FileExists( NewFileName ) )
                        {
                            remove( NewFileName );
                        }
                        rename( OldFileName, NewFileName );
                        LogMessage( "Renamed %s as %s\n", OldFileName,
                                    NewFileName );
                    }
                }
            }
        }
    }
}

int FixRSSI(int Channel, int RawRSSI, int SNR)
{
	int RSSI;
	
	if (Config.LoRaDevices[Channel].Frequency > 525)
	{
		// HF port (band 1)
		RSSI = RawRSSI - 157;
	}
	else
	{
		// LF port (Bands 2/3)
		RSSI = RawRSSI - 164;
	}
	
	if (SNR < 0)
	{
		RSSI += SNR/4;
	}
	
	return RSSI;
}

int CurrentRSSI(int Channel)
{
	return FixRSSI(Channel, readRegister(Channel, REG_CURRENT_RSSI), 0);
}
		
int PacketSNR(int Channel)
{
	int8_t SNR;

	SNR = readRegister(Channel, REG_PACKET_SNR);
	SNR /= 4;
	
	return (int)SNR;
}

int PacketRSSI(int Channel)
{
	int SNR;
	
	SNR = PacketSNR(Channel);
	
	return FixRSSI(Channel, readRegister(Channel, REG_PACKET_RSSI), SNR);
}
		
void DIO0_Interrupt( int Channel )
{
    if ( Config.LoRaDevices[Channel].Sending )
    {
        Config.LoRaDevices[Channel].Sending = 0;
        // LogMessage( "Ch%d: End of Tx\n", Channel );

        setLoRaMode( Channel );
        SetDefaultLoRaParameters( Channel );
        startReceiving( Channel );
    }
    else
    {
        int Bytes;
        char Message[257];
        rx_metadata_t Metadata;

        Metadata.Channel = Channel;

        Bytes = receiveMessage( Channel, Message + 1, &Metadata );
		
		Config.LoRaDevices[Channel].GotReply = 1;		

        if ( Bytes > 0 )
        {			
            if ( Config.LoRaDevices[Channel].ActivityLED >= 0 )
            {
                digitalWrite( Config.LoRaDevices[Channel].ActivityLED, 1 );
                LEDCounts[Channel] = 5;
            }

            if ( Message[1] == '!' )
            {
                ProcessUploadMessage( Channel, Message + 1 );
            }
            else if ( Message[1] == '^' )
            {
                ProcessCallingMessage( Channel, Message + 1 );
            }
            else if ((Message[1] == '$') || (Message[1] == '%'))
            {
                ProcessTelemetryMessage(Channel, Message + 1, &Metadata);
                TestMessageForSMSAcknowledgement( Channel, Message + 1);
            }
            else if ( Message[1] == '>' )
            {
                LogMessage( "Flight Controller message %d bytes = %s\n", Bytes, Message + 1 );
            }
            else if ( Message[1] == '*' )
            {
                LogMessage( "Uplink Command message %d bytes = %s\n", Bytes, Message + 1 );
            }
            else if (Message[1] == '+')
            {
                LogMessage( "Telnet Uplink message %d packet ID %d bytes = '%s'\n", Bytes, Message[2], Message + 3);
            }
            else if ( Message[1] == '-' )
            {
				ProcessTelnetMessage(Channel, Message+1, Bytes);
            }
            else if (((Message[1] & 0x7F) == 0x66) ||		// SSDV JPG format
					 ((Message[1] & 0x7F) == 0x67) ||		// SSDV other formats
					 ((Message[1] & 0x7F) == 0x68) ||
					 ((Message[1] & 0x7F) == 0x69))
            {
				int Repeated;
				
				// Handle repeater bit
				Repeated = Message[1] & 0x80;
				Message[1] &= 0x7F;			
				
                ProcessSSDVMessage( Channel, Message, Repeated);
            }
            else if ( Message[1] == 0x00)
            {
				time_t now;
				struct tm *tm;

				now = time( 0 );
				tm = localtime( &now );
				
				LogMessage("%02d:%02d:%02d Ch%d: Null uplink packet\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Channel);
            }
            else
            {
                LogMessage("Unknown packet type is %02Xh, RSSI %d\n", Message[1], PacketRSSI(Channel));
                ChannelPrintf( Channel, 3, 1, "Unknown Packet %d, %d bytes", Message[0], Bytes);
                Config.LoRaDevices[Channel].UnknownCount++;
            }

            // Config.LoRaDevices[Channel].LastPacketAt = time( NULL );

            if (Config.LoRaDevices[Channel].InCallingMode && (Config.CallingTimeout > 0))
            {
                Config.LoRaDevices[Channel].ReturnToCallingModeAt = time( NULL ) + Config.CallingTimeout;
            }

            if (!Config.LoRaDevices[Channel].InCallingMode && (Config.LoRaDevices[Channel].AFCTimeout > 0))
            {
                Config.LoRaDevices[Channel].ReturnToOriginalFrequencyAt = time(NULL) + Config.LoRaDevices[Channel].AFCTimeout;
            }
			
            ShowPacketCounts( Channel );
        }
    }
}

void DIO_Ignore_Interrupt_0( void )
{
    // nothing, obviously!
}

void
DIO0_Interrupt_0( void )
{
    DIO0_Interrupt( 0 );
}

void
DIO0_Interrupt_1( void )
{
    DIO0_Interrupt( 1 );
}

void setupRFM98( int Channel )
{
    if ( Config.LoRaDevices[Channel].InUse )
    {
        // initialize the pins
        pinMode( Config.LoRaDevices[Channel].DIO0, INPUT );
        pinMode( Config.LoRaDevices[Channel].DIO5, INPUT );

        wiringPiISR( Config.LoRaDevices[Channel].DIO0, INT_EDGE_RISING,
                     Channel > 0 ? &DIO0_Interrupt_1 : &DIO0_Interrupt_0 );

        if ( wiringPiSPISetup( Channel, 500000 ) < 0 )
        {
            exit_error("Failed to open SPI port.  Try loading spi library with 'gpio load spi'" );
        }

        // LoRa mode 
        setLoRaMode( Channel );

        SetDefaultLoRaParameters( Channel );

        startReceiving( Channel );
    }
}

double FrequencyError( int Channel )
{
    int32_t Temp;

    Temp = ( int32_t ) readRegister( Channel, REG_FREQ_ERROR ) & 7;
    Temp <<= 8L;
    Temp += ( int32_t ) readRegister( Channel, REG_FREQ_ERROR + 1 );
    Temp <<= 8L;
    Temp += ( int32_t ) readRegister( Channel, REG_FREQ_ERROR + 2 );

    if ( readRegister( Channel, REG_FREQ_ERROR ) & 8 )
    {
        Temp = Temp - 524288;
    }

    return -( ( double ) Temp * ( 1 << 24 ) / 32000000.0 ) * (Config.LoRaDevices[Channel].CurrentBandwidth / 500.0);
}

int
receiveMessage( int Channel, char *message, rx_metadata_t *Metadata )
{
    int i, Bytes, currentAddr, x;
    unsigned char data[257];
    double FreqError;

    Bytes = 0;

    x = readRegister( Channel, REG_IRQ_FLAGS );
    // LogMessage("Message status = %02Xh\n", x);

    // clear the rxDone flag
    writeRegister( Channel, REG_IRQ_FLAGS, 0x40 );

    // check for payload crc issues (0x20 is the bit we are looking for
    if ( ( x & 0x20 ) == 0x20 )
    {
        LogMessage( "Ch%d: CRC Failure, RSSI %d\n", Channel, PacketRSSI(Channel));
		
        // reset the crc flags
        writeRegister( Channel, REG_IRQ_FLAGS, 0x20 );
        ChannelPrintf( Channel, 3, 1, "CRC Failure %02Xh!!\n", x );
        Config.LoRaDevices[Channel].BadCRCCount++;
        ShowPacketCounts( Channel );
    }
    else
    {
        currentAddr = readRegister( Channel, REG_FIFO_RX_CURRENT_ADDR );
        Bytes = readRegister( Channel, REG_RX_NB_BYTES );

        Metadata->SNR = PacketSNR(Channel);
        Metadata->RSSI = PacketRSSI(Channel);
        ChannelPrintf( Channel, 10, 1, "Packet SNR = %d, RSSI = %d      ", Metadata->SNR, Metadata->RSSI);

        FreqError = FrequencyError( Channel ) / 1000;
        ChannelPrintf( Channel, 11, 1, "Freq. Error = %5.1lfkHz ", FreqError);

        writeRegister( Channel, REG_FIFO_ADDR_PTR, currentAddr );

        data[0] = REG_FIFO;
        wiringPiSPIDataRW( Channel, data, Bytes + 1 );
        for ( i = 0; i <= Bytes; i++ )
        {
            message[i] = data[i + 1];
        }

        message[Bytes] = '\0';

        Metadata->Timestamp = time( NULL );
        Metadata->Frequency = Config.LoRaDevices[Channel].activeFreq;
        Metadata->FrequencyError =  FreqError / 1000;
        Metadata->ImplicitOrExplicit = Config.LoRaDevices[Channel].ImplicitOrExplicit;
        Metadata->Bandwidth = Config.LoRaDevices[Channel].CurrentBandwidth;
        Metadata->ErrorCoding = Config.LoRaDevices[Channel].ErrorCoding;
        Metadata->SpreadingFactor = Config.LoRaDevices[Channel].SpreadingFactor;
        Metadata->LowDataRateOptimize = Config.LoRaDevices[Channel].LowDataRateOptimize;

        LogPacket(Metadata, Bytes, message[1]);

        if (Config.LoRaDevices[Channel].AFC && (fabs( FreqError ) > 0.5))
        {
			if (Config.LoRaDevices[Channel].MaxAFCStep > 0)
			{
				// Limit step to MaxAFCStep
				if (FreqError > Config.LoRaDevices[Channel].MaxAFCStep)
				{
					FreqError = Config.LoRaDevices[Channel].MaxAFCStep;
				}
				else if (FreqError < -Config.LoRaDevices[Channel].MaxAFCStep)
				{
					FreqError = -Config.LoRaDevices[Channel].MaxAFCStep;
				}
			}
            ReTune( Channel, FreqError / 1000 );
        }
    }

    // Clear all flags
    writeRegister( Channel, REG_IRQ_FLAGS, 0xFF );

    return Bytes;
}

void RemoveTrailingSlash(char *Value)
{
	int Len;
	
	if ((Len = strlen(Value)) > 0)
	{
		if ((Value[Len-1] == '/') || (Value[Len-1] == '\\'))
		{
			Value[Len-1] = '\0';
		}
	}
}

void LoadConfigFile(void)
{
    FILE *fp;
    char *filename = "gateway.txt";
    char *sample_filename = "gateway-sample.txt";
    int Channel, MainSection;
	
	if (access(filename, F_OK) != 0)
	{
		LogMessage("%s missing\n", filename);
		if (access(sample_filename, F_OK) == 0)
		{
			LogMessage("Renaming %s as %s\n", sample_filename, filename);
			rename(sample_filename, filename);
		}
	}

	// Default configuration
    Config.latitude = -999;
    Config.longitude = -999;
	Config.CallingTimeout = 300;
    Config.NetworkLED = -1;
    Config.InternetLED = -1;
    Config.LoRaDevices[0].ActivityLED = -1;
    Config.LoRaDevices[1].ActivityLED = -1;

    // Default pin allocations
    Config.LoRaDevices[0].DIO0 = 6;
    Config.LoRaDevices[0].DIO5 = 5;
    Config.LoRaDevices[1].DIO0 = 27;
    Config.LoRaDevices[1].DIO5 = 26;
	
	Config.LoRaDevices[0].Frequency = -1;
	Config.LoRaDevices[1].Frequency = -1;

    if ( ( fp = fopen( filename, "r" ) ) == NULL )
    {
        exit_error("Failed to open config file\n");
    }
	
	RegisterConfigFile(filename);
	
	// Get reference to main settings section
	MainSection = RegisterConfigSection("");
	
    // Receiver config
	RegisterConfigString(MainSection, -1, "tracker", Config.Tracker, sizeof(Config.Tracker), NULL);
    LogMessage( "Tracker = '%s'\n", Config.Tracker );

    // Enable uploads
    RegisterConfigBoolean(MainSection, -1, "EnableHabitat", &Config.EnableHabitat, NULL);
    RegisterConfigBoolean(MainSection, -1, "EnableSSDV", &Config.EnableSSDV, NULL);

    // Enable telemetry logging
    RegisterConfigBoolean(MainSection, -1, "LogTelemetry", &Config.EnableTelemetryLogging, NULL);

    // Enable packet logging
    RegisterConfigBoolean(MainSection, -1, "LogPackets", &Config.EnablePacketLogging, NULL);

    // Calling mode
    RegisterConfigInteger(MainSection, -1, "CallingTimeout", &Config.CallingTimeout, NULL);

    // LED allocations
    RegisterConfigInteger(MainSection, -1, "NetworkLED", &Config.NetworkLED, NULL);
    RegisterConfigInteger(MainSection, -1, "InternetLED", &Config.InternetLED, NULL);
    RegisterConfigInteger(MainSection, -1, "ActivityLED_0", &Config.LoRaDevices[0].ActivityLED, NULL);
    RegisterConfigInteger(MainSection, -1, "ActivityLED_1", &Config.LoRaDevices[1].ActivityLED, NULL);

    // Socket
    RegisterConfigInteger(MainSection, -1, "ServerPort", &Config.ServerPort, NULL);		// JSON server
    RegisterConfigInteger(MainSection, -1, "HABPort", &Config.HABPort, NULL);			// Telnet server
	
	// Timeout for HAB Telnet uplink
	Config.HABTimeout = 4000;
    RegisterConfigInteger(MainSection, -1, "HABTimeout", &Config.HABTimeout, NULL);

	// LoRa Channel for HAB Telnet uplink
	Config.HABChannel = 0;
    RegisterConfigInteger(MainSection, -1, "HABChannel", &Config.HABChannel, NULL);
	
    // SSDV Settings
	RegisterConfigString(MainSection, -1, "JPGFolder", Config.SSDVJpegFolder, sizeof(Config.SSDVJpegFolder), NULL);
    if ( Config.SSDVJpegFolder[0] )
    {
        // Create SSDV Folder
        struct stat st = { 0 };

        if ( stat( Config.SSDVJpegFolder, &st ) == -1 )
        {
            mkdir( Config.SSDVJpegFolder, 0777 );
        }
    }

    // ftp images
	RegisterConfigString(MainSection, -1, "ftpserver", Config.ftpServer, sizeof(Config.ftpServer), NULL);
	RegisterConfigString(MainSection, -1, "ftpUser", Config.ftpUser, sizeof(Config.ftpUser), NULL);
	RegisterConfigString(MainSection, -1, "ftpPassword", Config.ftpPassword, sizeof(Config.ftpPassword), NULL);
	RegisterConfigString(MainSection, -1, "ftpFolder", Config.ftpFolder, sizeof(Config.ftpFolder), NULL);

    // Listener
    RegisterConfigDouble(MainSection, -1, "Latitude", &Config.latitude, NULL);
    RegisterConfigDouble(MainSection, -1, "Longitude", &Config.longitude, NULL);
	RegisterConfigString(MainSection, -1, "antenna", Config.antenna, sizeof(Config.antenna), NULL);

    // Dev mode
    RegisterConfigBoolean(MainSection, -1, "EnableDev", &Config.EnableDev, NULL);

    // SMS upload to tracker
	RegisterConfigString(MainSection, -1, "SMSFolder", Config.SMSFolder, sizeof(Config.SMSFolder), NULL);
    if (Config.SMSFolder[0])
    {
		RemoveTrailingSlash(Config.SMSFolder);
        LogMessage("Folder %s will be scanned for messages to upload\n", Config.SMSFolder);
    }

    for (Channel = 0; Channel <= 1; Channel++)
    {
		RegisterConfigDouble(MainSection, Channel, "frequency", &Config.LoRaDevices[Channel].Frequency, NULL);
        if (Config.LoRaDevices[Channel].Frequency > 100)
        {
			// Defaults
            Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
            Config.LoRaDevices[Channel].ErrorCoding = ECToInt(ERROR_CODING_4_8);
            Config.LoRaDevices[Channel].Bandwidth = BandwidthToDouble(BANDWIDTH_20K8);
            Config.LoRaDevices[Channel].SpreadingFactor = SFToInt(SPREADING_11);
            Config.LoRaDevices[Channel].LowDataRateOptimize = 0;
            Config.LoRaDevices[Channel].AFC = FALSE;
            Config.LoRaDevices[Channel].Power = PA_MAX_UK;
            Config.LoRaDevices[Channel].UplinkMode = -1;
            Config.LoRaDevices[Channel].UplinkTime = -1;
            Config.LoRaDevices[Channel].UplinkCycle = -1;
			Config.LoRaDevices[Channel].IdleUplink = FALSE;

            LogMessage( "Channel %d frequency set to %.3lfMHz\n", Channel, Config.LoRaDevices[Channel].Frequency);
            Config.LoRaDevices[Channel].InUse = 1;

            // DIO0 / DIO5 overrides
			RegisterConfigInteger(MainSection, Channel, "DIO0", &Config.LoRaDevices[Channel].DIO0, NULL);
			RegisterConfigInteger(MainSection, Channel, "DIO5", &Config.LoRaDevices[Channel].DIO5, NULL);

            LogMessage( "LoRa Channel %d DIO0=%d DIO5=%d\n", Channel, Config.LoRaDevices[Channel].DIO0, Config.LoRaDevices[Channel].DIO5 );

            // Uplink
			RegisterConfigInteger(MainSection, Channel, "UplinkTime", &Config.LoRaDevices[Channel].UplinkTime, NULL);
			RegisterConfigInteger(MainSection, Channel, "UplinkCycle", &Config.LoRaDevices[Channel].UplinkCycle, NULL);
			if ((Config.LoRaDevices[Channel].UplinkTime >= 0) && (Config.LoRaDevices[Channel].UplinkCycle > Config.LoRaDevices[Channel].UplinkTime))
			{
				LogMessage( "Channel %d UplinkTime %d Uplink Cycle %d\n", Channel, Config.LoRaDevices[Channel].UplinkTime, Config.LoRaDevices[Channel].UplinkCycle);

				RegisterConfigInteger(MainSection, Channel, "Power", &Config.LoRaDevices[Channel].Power, NULL);


				LogMessage( "Channel %d power set to %02Xh\n", Channel, Config.LoRaDevices[Channel].Power );
				
				RegisterConfigBoolean(MainSection, Channel, "SSDVUplink", &Config.LoRaDevices[Channel].SSDVUplink, NULL);
				if (Config.LoRaDevices[Channel].SSDVUplink)
				{
					LogMessage( "Channel %d SSDV Uplink Enabled\n", Channel);
				}
				RegisterConfigBoolean(MainSection, Channel, "IdleUplink", &Config.LoRaDevices[Channel].IdleUplink, NULL);
				if (Config.LoRaDevices[Channel].IdleUplink)
				{
					LogMessage( "Channel %d Idle Uplink Enabled\n", Channel);
				}
			}

			RegisterConfigInteger(MainSection, Channel, "Power", &Config.LoRaDevices[Channel].Power, NULL);

			RegisterConfigInteger(MainSection, Channel, "UplinkMode", &Config.LoRaDevices[Channel].UplinkMode, NULL);
			if (Config.LoRaDevices[Channel].UplinkMode >= 0)
			{
				LogMessage( "Channel %d uplink mode %d\n", Channel, Config.LoRaDevices[Channel].UplinkMode);
			}

			RegisterConfigDouble(MainSection, Channel, "UplinkFrequency", &Config.LoRaDevices[Channel].UplinkFrequency, NULL);
			if (Config.LoRaDevices[Channel].UplinkFrequency > 0)
            {
				LogMessage( "Channel %d uplink frequency %.3lfMHz\n", Channel, Config.LoRaDevices[Channel].UplinkFrequency);
			}

            Config.LoRaDevices[Channel].SpeedMode = 0;

			RegisterConfigInteger(MainSection, Channel, "mode", &Config.LoRaDevices[Channel].SpeedMode, NULL);
			if ((Config.LoRaDevices[Channel].SpeedMode < 0) || (Config.LoRaDevices[Channel].SpeedMode >= sizeof(LoRaModes)/sizeof(LoRaModes[0]))) Config.LoRaDevices[Channel].SpeedMode = 0;

			// Defaults for this LoRa Mode
			Config.LoRaDevices[Channel].ImplicitOrExplicit = LoRaModes[Config.LoRaDevices[Channel].SpeedMode].ImplicitOrExplicit;
			Config.LoRaDevices[Channel].ErrorCoding = ECToInt(LoRaModes[Config.LoRaDevices[Channel].SpeedMode].ErrorCoding);
			Config.LoRaDevices[Channel].Bandwidth = BandwidthToDouble(LoRaModes[Config.LoRaDevices[Channel].SpeedMode].Bandwidth);
			Config.LoRaDevices[Channel].SpreadingFactor = SFToInt(LoRaModes[Config.LoRaDevices[Channel].SpeedMode].SpreadingFactor);
			Config.LoRaDevices[Channel].LowDataRateOptimize = LowOptToInt(LoRaModes[Config.LoRaDevices[Channel].SpeedMode].LowDataRateOptimize);

			// Overrides
			if (RegisterConfigInteger(MainSection, Channel, "sf", &Config.LoRaDevices[Channel].SpreadingFactor, NULL))
			{
                LogMessage( "Setting SF=%d\n", Config.LoRaDevices[Channel].SpreadingFactor);
            }

			if (RegisterConfigDouble(MainSection, Channel, "bandwidth", &Config.LoRaDevices[Channel].Bandwidth, NULL))
            {
                LogMessage( "Setting Bandwidth=%.2lfkHz\n", Config.LoRaDevices[Channel].Bandwidth);
            }

			RegisterConfigBoolean(MainSection, Channel, "implicit", &Config.LoRaDevices[Channel].ImplicitOrExplicit, NULL);

			if (RegisterConfigInteger(MainSection, Channel, "coding", &Config.LoRaDevices[Channel].ErrorCoding, NULL))
			{
                LogMessage( "Setting Error Coding=%d\n", Config.LoRaDevices[Channel].ErrorCoding);
            }

			RegisterConfigBoolean(MainSection, Channel, "lowopt", &Config.LoRaDevices[Channel].LowDataRateOptimize, NULL);

			RegisterConfigBoolean(MainSection, Channel, "AFC", &Config.LoRaDevices[Channel].AFC, NULL);
			if (Config.LoRaDevices[Channel].AFC)
			{
                ChannelPrintf( Channel, 11, 24, "AFC" );
				
				RegisterConfigDouble(MainSection, Channel, "MaxAFCStep", &Config.LoRaDevices[Channel].MaxAFCStep, NULL);
				if (Config.LoRaDevices[Channel].MaxAFCStep > 0)
				{
					LogMessage("Maximum AFC Step = %.0lfkHz\n", Config.LoRaDevices[Channel].MaxAFCStep);
				}
				
				RegisterConfigInteger(MainSection, Channel, "AFCTimeout", &Config.LoRaDevices[Channel].AFCTimeout, NULL);
				if (Config.LoRaDevices[Channel].AFCTimeout > 0)
				{
					LogMessage("AFC Timeout = %.0ds\n", Config.LoRaDevices[Channel].AFCTimeout);
				}
            }

            // Clear any flags left over from a previous run
            writeRegister( Channel, REG_IRQ_FLAGS, 0xFF );
        }
    }

    fclose(fp);
}

WINDOW *InitDisplay(void)
{
    WINDOW *mainwin;
    int Channel;

    /*  Initialize ncurses  */

    if ( ( mainwin = initscr(  ) ) == NULL )
    {
        fprintf( stderr, "Error initialising ncurses.\n" );
        exit( EXIT_FAILURE );
    }

    start_color(  );            /*  Initialize colours  */

    init_pair( 1, COLOR_WHITE, COLOR_BLUE );
    init_pair( 2, COLOR_YELLOW, COLOR_BLUE );
    init_pair( 3, COLOR_YELLOW, COLOR_BLACK );

    color_set( 1, NULL );
    // bkgd(COLOR_PAIR(1));
    // attrset(COLOR_PAIR(1) | A_BOLD);

    char buffer[80];

    sprintf( buffer, "LoRa Habitat and SSDV Gateway by M0RPI, M0RJX - " VERSION);

    // Title bar
    mvaddstr( 0, ( 80 - strlen( buffer ) ) / 2, buffer );

    // Help 
    sprintf( buffer, "Press (H) for Help");
    color_set( 3, NULL );
    mvaddstr( 15, ( 80 - strlen( buffer ) ) / 2, buffer );

    color_set( 1, NULL );
    refresh(  );

    // Windows for LoRa live data
    for ( Channel = 0; Channel <= 1; Channel++ )
    {
        Config.LoRaDevices[Channel].Window =
            newwin( 14, 38, 1, Channel ? 41 : 1 );
        wbkgd( Config.LoRaDevices[Channel].Window, COLOR_PAIR( 2 ) );

        // wcolor_set(Config.LoRaDevices[Channel].Window, 2, NULL);
        // waddstr(Config.LoRaDevices[Channel].Window, "WINDOW");
        // mvwaddstr(Config.LoRaDevices[Channel].Window, 0, 0, "Window");
        wrefresh( Config.LoRaDevices[Channel].Window );
    }

    curs_set( 0 );

    return mainwin;
}


uint16_t
CRC16( unsigned char *ptr )
{
    uint16_t CRC;
    int j;

    CRC = 0xffff;               // Seed

    for ( ; *ptr; ptr++ )
    {                           // For speed, repeat calculation instead of looping for each bit
        CRC ^= ( ( ( unsigned int ) *ptr ) << 8 );
        for ( j = 0; j < 8; j++ )
        {
            if ( CRC & 0x8000 )
                CRC = ( CRC << 1 ) ^ 0x1021;
            else
                CRC <<= 1;
        }
    }

    return CRC;
}

void
ProcessKeyPress( int ch )
{
    int Channel = 0;

    /* shifted keys act on channel 1 */
    if ( ch >= 'A' && ch <= 'Z' )
    {
        Channel = 1;
        /* change from upper to lower case */
        ch += ( 'a' - 'A' );
    }

    if ( ch == 'q' )
    {
        run = FALSE;
        return;
    }

    /* ignore if channel is not in use */
    if ( !Config.LoRaDevices[Channel].InUse && ch !='h' )
    {
        return;
    }

    switch ( ch )
    {
        case 'f':
            Config.LoRaDevices[Channel].AFC =
                !Config.LoRaDevices[Channel].AFC;
            ChannelPrintf( Channel, 11, 24, "%s",
                           Config.LoRaDevices[Channel].AFC ? "AFC" : "   " );
            break;
        case 'a':
            ReTune( Channel, 0.1 );
            break;
        case 'z':
            ReTune( Channel, -0.1 );
            break;
        case 's':
            ReTune( Channel, 0.01 );
            break;
        case 'x':
            ReTune( Channel, -0.01 );
            break;
        case 'd':
            ReTune( Channel, 0.001 );
            break;
        case 'c':
            ReTune( Channel, -0.001 );
            break;
        case 'p':
            break;
        case 'h':
            help_win_displayed = 1;

            gui_show_help();

            for (Channel=0; Channel<=1; Channel++)
            {
                if ( Config.LoRaDevices[Channel].InUse ) displayChannel (Channel); 
            }

            help_win_displayed = 0;
            
            break;
        default:
            // LogMessage("KeyPress %d\n", ch);
            return;
    }
}

int
prog_count( char *name )
{
    DIR *dir;
    struct dirent *ent;
    char buf[512];
    long pid;
    char pname[100] = { 0, };
    char state;
    FILE *fp = NULL;
    int Count = 0;

    if ( !( dir = opendir( "/proc" ) ) )
    {
        perror( "can't open /proc" );
        return 0;
    }

    while ( ( ent = readdir( dir ) ) != NULL )
    {
        long lpid = atol( ent->d_name );
        if ( lpid < 0 )
            continue;
        snprintf( buf, sizeof( buf ), "/proc/%ld/stat", lpid );
        fp = fopen( buf, "r" );

        if ( fp )
        {
            if ( ( fscanf( fp, "%ld (%[^)]) %c", &pid, pname, &state ) ) !=
                 3 )
            {
                printf( "fscanf failed \n" );
                fclose( fp );
                closedir( dir );
                return 0;
            }

            if ( !strcmp( pname, name ) )
            {
                Count++;
            }
            fclose( fp );
        }
    }

    closedir( dir );

    return Count;
}

int
GetTextMessageToUpload( int Channel, char *Message )
{
    DIR *dp;
    struct dirent *ep;
    int Result;

    Result = 0;

    if ( Config.SMSFolder[0] )
    {
		LogMessage("Checking for SMS file in '%s' folder ...\n", Config.SMSFolder);
        dp = opendir( Config.SMSFolder );
        if ( dp != NULL )
        {
            while ( ( ep = readdir( dp ) ) && !Result )
            {
                if ( strstr( ep->d_name, ".sms" ) )
                {
                    FILE *fp;
                    char Line[256], FileName[256];
                    int FileNumber;

                    sprintf( FileName, "%s/%s", Config.SMSFolder, ep->d_name );
                    sscanf( ep->d_name, "%d", &FileNumber );

                    if ( ( fp = fopen( FileName, "rt" ) ) != NULL )
                    {
                        if ( fscanf( fp, "%[^\r]", Line ) )
                        {
                            // #001,@daveake: Good Luck Tim !!\n
                            sprintf( Message, "#%d,%s\n", FileNumber, Line );

                            LogMessage( "UPLINK: %s", Message );
                            Result = 1;
                        }
                        else
                        {
                            LogMessage( "FAIL\n" );
                        }
                        fclose( fp );
                    }
                }
            }
            closedir( dp );
        }
		else
		{
			LogMessage("Failed to open folder - error code %d\n", errno);
		}
    }

    return Result;
}

int
GetExternalListOfMissingSSDVPackets( int Channel, char *Message )
{
    // First, create request file
    FILE *fp;

	if (Config.LoRaDevices[Channel].SSDVUplink)
    {
        int i;

        // Now wait for uplink.txt file to appear.
        // Timeout before the end of our Tx slot if no file appears
		
        for ( i = 0; i < 20; i++ )
        {
            if ( ( fp = fopen( "uplink.txt", "r" ) ) )
            {
                Message[0] = '\0';
                fgets( Message, 256, fp );

                fclose( fp );

                LogMessage( "Got uplink.txt %d bytes\n", strlen( Message ) );

                // remove("get_list.txt");
                remove( "uplink.txt" );

                return strlen( Message );
            }

            usleep( 100000 );
        }

        // LogMessage("Timed out waiting for file\n");
        // remove("get_list.txt");
    }

    return 0;
}


void SendTelnetMessage(int Channel, struct TServerInfo *TelnetInfo, int TimedOut)
{
	// Send message regardless of if we have content to send
	// This is so that that HAB gets a chance to send anything it needs (further replies from earlier messages, telemetry, etc)
	char Message[256], FirstByte;
	int Length;
	
	// If HAB Acked us last time, we can go on to the next message; if not we should re-send

	if (Config.LoRaDevices[Channel].HABAck)
	{
		// Prepare new packet
		Config.LoRaDevices[Channel].PacketID++;				// Packet acked, so onto next packet
		Config.LoRaDevices[Channel].HABUplinkCount = 0;		// Remove existing packet contents
		if (TelnetInfo->Connected)
		{
			if (Config.LoRaDevices[Channel].FromTelnetBufferCount > 0)
			{
				memcpy(Config.LoRaDevices[Channel].HABUplink, Config.LoRaDevices[Channel].FromTelnetBuffer, Config.LoRaDevices[Channel].FromTelnetBufferCount);
			}
			Config.LoRaDevices[Channel].HABUplinkCount = Config.LoRaDevices[Channel].FromTelnetBufferCount;
			Config.LoRaDevices[Channel].HABUplink[Config.LoRaDevices[Channel].HABUplinkCount] = 0;
			Config.LoRaDevices[Channel].FromTelnetBufferCount = 0;
			
			if (Config.LoRaDevices[Channel].HABUplinkCount > 0)
			{
				LogMessage("Received %d bytes from HAB client\n", Config.LoRaDevices[Channel].HABUplinkCount);
			}

			// echo
			// sprintf(sendBuff, "%c", *line);	
		}
	}
	else if (TimedOut)
	{
		LogMessage("TIMED OUT\n");
	}
	else if (!Config.LoRaDevices[Config.HABChannel].GotHABReply)
	{
		LogMessage("Other Reply - RESEND\n");
	}
	else
	{
		// Resend last packet
		LogMessage("NAK - RESEND\n");
	}
		
	FirstByte = (TelnetInfo->Connected ? 0x80 : 0x00) | 				// Bit 7:		CONN - 1 = Telnet client is connected (telling HAB that it ought to connect to telnetd now)
				0x00 |													// Bit 6:		ACK bit, not used in uplink
				(Config.LoRaDevices[Channel].PacketID & 0x3F);			// Bits 5-0:	Packet number
	
	if (Config.LoRaDevices[Channel].HABUplinkCount > 0)
	{
		Length = sprintf(Message, "+%c%s", FirstByte, Config.LoRaDevices[Channel].HABUplink);
		LogMessage("SENDING %d BYTES +[%02X]%s\n", Length, FirstByte, Config.LoRaDevices[Channel].HABUplink);
	}
	else
	{
		Length = sprintf(Message, "+%c", FirstByte);
		LogMessage("SENDING +[%02X]\n", FirstByte);
	}
	
	Config.LoRaDevices[Channel].HABAck = 0;
		
	SendLoRaData(Channel, Message, Length);
}

void SendUplinkMessage( int Channel )
{
    char Message[512];

    // Decide what type of message we need to send
	if (*Config.LoRaDevices[Channel].UplinkMessage)
	{
		SendLoRaData(Channel, Config.LoRaDevices[Channel].UplinkMessage, strlen(Config.LoRaDevices[Channel].UplinkMessage)+1);
		*Config.LoRaDevices[Channel].UplinkMessage = 0;
	}
    else if (GetTextMessageToUpload( Channel, Message))
    {
        SendLoRaData(Channel, Message, 255);
    }
    else if (GetExternalListOfMissingSSDVPackets( Channel, Message))
    {
        SendLoRaData(Channel, Message, 255);
    }
    else if (Config.LoRaDevices[Channel].IdleUplink)
    {
        SendLoRaData(Channel, "", 1);
    }
}

void displayChannel (int Channel) {

    displayFrequency ( Channel, Config.LoRaDevices[Channel].Frequency );

    displayLoRaParameters( 
        Channel, 
        Config.LoRaDevices[Channel].ImplicitOrExplicit,
        Config.LoRaDevices[Channel].ErrorCoding, 
        Config.LoRaDevices[Channel].Bandwidth, 
        Config.LoRaDevices[Channel].SpreadingFactor, 
        Config.LoRaDevices[Channel].LowDataRateOptimize
        );

    if (Config.LoRaDevices[Channel].AFC)
        ChannelPrintf( Channel, 11, 24, "AFC" );
    else
        ChannelPrintf( Channel, 11, 24, "   " );
 
}


int main( int argc, char **argv )
{
    int ch;
    int LoopPeriod, MSPerLoop;
	int Channel;
    pthread_t SSDVThread, FTPThread, NetworkThread, HabitatThread, ServerThread, TelnetThread, ListenerThread;
	struct TServerInfo JSONInfo, TelnetInfo;

	atexit(bye);
	
    if ( wiringPiSetup(  ) < 0 )
    {
		exit_error("Failed to open wiringPi\n");
    }

	// Clear config to zeroes so we only have to set non-zero defaults
	memset((void *)&Config, 0, sizeof(Config));

    if ( prog_count( "gateway" ) > 1 )
    {
        printf( "\nThe gateway program is already running!\n\n" );
        exit( 1 );
    }
	
    curl_global_init( CURL_GLOBAL_ALL );    // RJH thread safe

    mainwin = InitDisplay();

    // Settings for character input
    noecho(  );
    cbreak(  );
    nodelay( stdscr, TRUE );
    keypad( stdscr, TRUE );

    LEDCounts[0] = 0;
    LEDCounts[1] = 0;

    LoadConfigFile();

    int result;

    result = pipe( telem_pipe_fd );
    if ( result < 0 )
    {
        exit_error("Error creating telemetry pipe\n");
    }

    result = pipe( ssdv_pipe_fd );
    if ( result < 0 )
    {
        exit_error("Error creating ssdv pipe\n" );
    }

    if ( Config.LoRaDevices[0].ActivityLED >= 0 )
        pinMode( Config.LoRaDevices[0].ActivityLED, OUTPUT );
    if ( Config.LoRaDevices[1].ActivityLED >= 0 )
        pinMode( Config.LoRaDevices[1].ActivityLED, OUTPUT );
    if ( Config.InternetLED >= 0 )
        pinMode( Config.InternetLED, OUTPUT );
    if ( Config.NetworkLED >= 0 )
        pinMode( Config.NetworkLED, OUTPUT );

    setupRFM98( 0 );
    setupRFM98( 1 );

    ShowPacketCounts( 0 );
    ShowPacketCounts( 1 );


    LoopPeriod = 0;
	MSPerLoop = 10;

    // Initialise the vars
    stsv.parent_status = RUNNING;
    stsv.packet_count = 0;
	
    if (Config.EnableSSDV)
	{
		if ( pthread_create( &SSDVThread, NULL, SSDVLoop, ( void * ) &stsv ) )
		{
			fprintf( stderr, "Error creating SSDV thread\n" );
			return 1;
		}
	}

	if ( pthread_create( &FTPThread, NULL, FTPLoop, NULL ) )
	{
		fprintf( stderr, "Error creating FTP thread\n" );
		return 1;
	}


    // Initialise the vars
    htsv.parent_status = RUNNING;
    htsv.packet_count = 0;


    if (Config.EnableHabitat)
	{
		if ( pthread_create (&HabitatThread, NULL, HabitatLoop, ( void * ) &htsv))
		{
			fprintf( stderr, "Error creating Habitat thread\n" );
			return 1;
		}
    }

    if (Config.ServerPort > 0)
    {
		JSONInfo.Port = Config.ServerPort;
		JSONInfo.ServerIndex = 0;
		JSONInfo.Connected = 0;
		
        if (pthread_create(&ServerThread, NULL, ServerLoop, (void *)(&JSONInfo)))
        {
            fprintf( stderr, "Error creating JSON server thread\n" );
            return 1;
        }
    }
	
    if (Config.HABPort > 0)
    {
		TelnetInfo.Port = Config.HABPort;
		TelnetInfo.ServerIndex = 1;
		TelnetInfo.Connected = 0;
		
        if (pthread_create(&TelnetThread, NULL, ServerLoop, (void *)(&TelnetInfo)))
        {
            fprintf( stderr, "Error creating HAB server thread\n" );
            return 1;
        }
    }

    if ( ( Config.NetworkLED >= 0 ) && ( Config.InternetLED >= 0 ) )
    {
        if ( pthread_create( &NetworkThread, NULL, NetworkLoop, NULL ) )
        {
            fprintf( stderr, "Error creating Network thread\n" );
            return 1;
        }
    }

    if ( ( Config.latitude > -90 ) && ( Config.longitude > -90 ) )
    {
        if ( pthread_create( &ListenerThread, NULL, ListenerLoop, NULL ) )
        {
            fprintf( stderr, "Error creating Listener thread\n" );
            return 1;
        }
    }

    LogMessage( "Starting now ...\n" );

    while ( run )
    {
		// Keypress tests
        if ((ch = getch()) != ERR )
        {
            ProcessKeyPress( ch );
        }

		// Telnet uplink to HAB
		if (Config.HABPort > 0)
		{
			Config.LoRaDevices[Config.HABChannel].TimeSinceLastTx += MSPerLoop;
			
			if ((Config.LoRaDevices[Config.HABChannel].TimeSinceLastTx >= Config.HABTimeout) || Config.LoRaDevices[Config.HABChannel].GotReply)
			{
				SendTelnetMessage(Config.HABChannel, &TelnetInfo, Config.LoRaDevices[Config.HABChannel].TimeSinceLastTx >= Config.HABTimeout);
				Config.LoRaDevices[Config.HABChannel].GotReply = 0;
				Config.LoRaDevices[Config.HABChannel].GotHABReply = 0;
				Config.LoRaDevices[Config.HABChannel].TimeSinceLastTx = 0;
			}
		}
			
        if (LoopPeriod > 1000)
        {
            // Every 1 second
            time_t now;
            struct tm *tm;

            now = time( 0 );
            tm = localtime( &now );

            LoopPeriod = 0;

            for (Channel=0; Channel<=1; Channel++)
            {
                if ( Config.LoRaDevices[Channel].InUse )
                {
                    ShowPacketCounts( Channel );

                    ChannelPrintf( Channel, 12, 1, "Current RSSI = %4d   ", CurrentRSSI(Channel));

					// Calling mode timeout?
                    if ( Config.LoRaDevices[Channel].InCallingMode
                         && ( Config.CallingTimeout > 0 )
                         && ( Config.LoRaDevices[Channel].ReturnToCallingModeAt > 0 )
                         && ( time( NULL ) > Config.LoRaDevices[Channel].ReturnToCallingModeAt ) )
                    {
                        Config.LoRaDevices[Channel].InCallingMode = 0;
                        Config.LoRaDevices[Channel].ReturnToCallingModeAt = 0;

                        LogMessage( "Return to calling mode\n" );

                        setLoRaMode( Channel );

                        SetDefaultLoRaParameters( Channel );

                        setMode( Channel, RF98_MODE_RX_CONTINUOUS );
                    }
					
					// AFC Timeout ?
                    if (!Config.LoRaDevices[Channel].InCallingMode &&
                        (Config.LoRaDevices[Channel].AFCTimeout > 0) &&
                        (Config.LoRaDevices[Channel].ReturnToOriginalFrequencyAt > 0) &&
                        (time(NULL) > Config.LoRaDevices[Channel].ReturnToOriginalFrequencyAt))
                    {
                        Config.LoRaDevices[Channel].ReturnToOriginalFrequencyAt = 0;

                        LogMessage("AFC timeout - return to original frequency\n");
						
						setMode(Channel, RF98_MODE_SLEEP);
						setFrequency(Channel, Config.LoRaDevices[Channel].Frequency);
						startReceiving(Channel);
                    }					

					// Uplink cycle time ?
                    if ((Config.LoRaDevices[Channel].UplinkTime >= 0) && (Config.LoRaDevices[Channel].UplinkCycle > 0))
                    {
                        long CycleSeconds;

                        CycleSeconds = (tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec ) % Config.LoRaDevices[Channel].UplinkCycle;

                        if (CycleSeconds == Config.LoRaDevices[Channel].UplinkTime)
                        {
                            LogMessage("%02d:%02d:%02d - Time to send uplink message\n", tm->tm_hour, tm->tm_min, tm->tm_sec);

                            SendUplinkMessage(Channel);
                        }
                    }

					// LEDs
                    if (LEDCounts[Channel] && ( Config.LoRaDevices[Channel].ActivityLED >= 0))
                    {
                        if ( --LEDCounts[Channel] == 0 )
                        {
                            digitalWrite(Config.LoRaDevices[Channel].ActivityLED, 0);
                        }
                    }
                }
            }
        }

        delay(MSPerLoop);
        LoopPeriod += MSPerLoop;
    }
	
	LogMessage("Disabling DIO0 ISRs\n");
	for (Channel=0; Channel<2; Channel++)
	{
		if (Config.LoRaDevices[Channel].InUse)
		{
			wiringPiISR(Config.LoRaDevices[Channel].DIO0, INT_EDGE_RISING, &DIO_Ignore_Interrupt_0);
		}
	}

    LogMessage( "Closing SSDV pipe\n" );
    close( ssdv_pipe_fd[1] );

    LogMessage( "Closing Habitat pipe\n" );
    close( telem_pipe_fd[1] );

    LogMessage( "Stopping SSDV thread\n" );
    stsv.parent_status = STOPPED;

	LogMessage( "Stopping Habitat thread\n" );
	htsv.parent_status = STOPPED;

    if (Config.EnableSSDV)
	{
		LogMessage( "Waiting for SSDV thread to close ...\n" );
		pthread_join( SSDVThread, NULL );
		LogMessage( "SSDV thread closed\n" );
	}
	
    if (Config.EnableHabitat)
	{
		LogMessage( "Waiting for Habitat thread to close ...\n" );
		pthread_join( HabitatThread, NULL );
		LogMessage( "Habitat thread closed\n" );
	}
	
    // CloseDisplay( mainwin );

    pthread_mutex_destroy( &var );

    curl_global_cleanup(  );    // RJH thread safe

    if ( Config.NetworkLED >= 0 )
        digitalWrite( Config.NetworkLED, 0 );
    if ( Config.InternetLED >= 0 )
        digitalWrite( Config.InternetLED, 0 );
    if ( Config.LoRaDevices[0].ActivityLED >= 0 )
        digitalWrite( Config.LoRaDevices[0].ActivityLED, 0 );
    if ( Config.LoRaDevices[1].ActivityLED >= 0 )
        digitalWrite( Config.LoRaDevices[1].ActivityLED, 0 );

    return 0;

}

