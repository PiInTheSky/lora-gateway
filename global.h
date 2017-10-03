#include <curses.h>

#define RUNNING 1               // The main program is running
#define STOPPED 0               // The main program has stopped

#define MAX_PAYLOADS	32

struct TPayload
{
	int InUse;
	int Channel;
	
	time_t LastPacketAt;
	
	char Telemetry[256];
	char Payload[32];
	
	char Time[12];
	unsigned int Counter, LastCounter;
	double Longitude, Latitude;
	unsigned int Altitude, PreviousAltitude;
	float AscentRate;
	unsigned long LastPositionAt;
}; struct TLoRaDevice {
	double Frequency;
	double Bandwidth;
	double CurrentBandwidth;
			int InUse;    	int DIO0;	int DIO5;	double activeFreq;
	
	int AFC;					// Enable Automatic Frequency Control
	double MaxAFCStep;			// Maximum adjustment, in kHz, per packet
	int AFCTimeout;				// Revert to original frequency if no packets for this period (in seconds)
	
	int SpeedMode;
	int Power;
	int PayloadLength;
	int ImplicitOrExplicit;
	int ErrorCoding;
	int SpreadingFactor;
	int LowDataRateOptimize;
	WINDOW * Window;
	unsigned int TelemetryCount, SSDVCount, BadCRCCount, UnknownCount;
	int Sending;
	// unsigned long Seconds;
	// double PredictedLongitude, PredictedLatitude;
	// unsigned int Satellites;
	time_t LastSSDVPacketAt, LastTelemetryPacketAt;
	time_t ReturnToCallingModeAt;
	time_t ReturnToOriginalFrequencyAt;
	int InCallingMode;
	int ActivityLED;
	double UplinkFrequency;
	int UplinkMode;

	// Normal (non TDM) uplink
	int UplinkTime;
	int UplinkCycle;
	int SSDVUplink;
	char UplinkMessage[256];
	
	// Telnet uplink
	char Telemetry[256];
	unsigned char PacketID;
	int TimeSinceLastTx;
	int HABAck;						// Received message ACKd our last Tx
	int HABConnected;				// 1 if HAB has telnet connection
	char ToTelnetBuffer[256];			// Buffer for data Telnet --> LoRa
	int ToTelnetBufferCount;
	char FromTelnetBuffer[256];			// Buffer for data LoRa --> Telnet
	int FromTelnetBufferCount;
	char HABUplink[256];
	int HABUplinkCount;
	int GotHABReply;
	int GotReply;	
};
 struct TConfig  {   	char Tracker[16];				// Callsign or name of receiver
	double latitude, longitude;		// Receiver's location
     	int EnableHabitat;
	int EnableSSDV;
	int EnableTelemetryLogging;
	int EnablePacketLogging;
	int CallingTimeout;
	char SSDVJpegFolder[100];
	char ftpServer[100];
	char ftpUser[32];
	char ftpPassword[32];
	char ftpFolder[64];
	struct TLoRaDevice LoRaDevices[2];
	struct TPayload Payloads[MAX_PAYLOADS];
	int NetworkLED;
	int InternetLED;
	int ServerPort;					// JSON port for telemetry, settings
	int HABPort;				// Telnet style port for comms with HAB
	int HABTimeout;				// Timeout in ms for telnet uplink
	int HABChannel;				// LoRa Channel for uplink
	char SMSFolder[64];
	char antenna[64];
	int EnableDev;
	char UplinkCode[64];
};
 typedef struct {
    int parent_status;
    unsigned long packet_count;
} thread_shared_vars_t;

typedef struct {
    short int Channel;
    char Telemetry[257];
    int Packet_Number;
    double RxFrequency;
} telemetry_t;

typedef struct {
    short int Channel;
    char SSDV_Packet[257];
    int Packet_Number;
} ssdv_t;

struct TServerInfo
{
	int Port;
	int Connected;
	int ServerIndex;
	int sockfd;
};

extern struct TConfig Config;
extern int SSDVSendArrayIndex;
extern pthread_mutex_t ssdv_mutex;
 void LogMessage( const char *format, ... );
