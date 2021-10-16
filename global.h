#ifndef _H_Global
#define _H_Global

#include <curses.h>

#define RUNNING 1               // The main program is running
#define STOPPED 0               // The main program has stopped

#define MAX_PAYLOADS	32

struct TPayload
{
	int InUse;
	int SendToClients;
	int Channel;
	
	time_t LastPacketAt;
	
	char Telemetry[256];
	char Payload[32];
	
	char Time[12];
	unsigned int Counter, LastCounter;
	double Longitude, Latitude;
	int Altitude, PreviousAltitude;
	float AscentRate;
	unsigned long LastPositionAt;
};
 
struct TLoRaDevice 
{
	int    Enabled;
	
	double Frequency;
    double PPM;
	double FrequencyOffset;
    
	double Bandwidth;
	double CurrentBandwidth;
		
	int InUse;    
	int DIO0;
	int DIO5;
	
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
	int IdleUplink;
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
    
    // Chat uplink
    int ChatMode;
    char ChatPayloadID[32];
    int TxMessageID;
    char TxChatMessage[200];
    int RxMessageID;
    char RxChatMessage[200];
	
	// Local data packets
	int LocalDataCount;
	char LocalDataBuffer[255];
	
	// Status
	int CurrentRSSI;
	int PacketSNR, PacketRSSI;
	double FrequencyError;
};
 
struct TConfig 
 {   
	char Tracker[16];				// Callsign or name of receiver
	double latitude, longitude;		// Receiver's location
     
	int EnableHabitat;
	int EnableSSDV;
    int EnableHablink;
	char HablinkAddress[32];
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
	int ServerPort;				// JSON port for telemetry, settings
    int ChatPort;               // Chat port
	int UDPPort;				// UDP Broadcast port for raw received data packets
	int OziPlotterPort;			// UDP Broadcast port for OziPlotter formatted packets
	int OziMuxPort;				// UDP Broadcast port for OziMux formatted packets
	int HABPort;				// Telnet style port for comms with HAB
	int HABTimeout;				// Timeout in ms for telnet uplink
	int HABChannel;				// LoRa Channel for uplink
	int DataPort;				// Raw received data packet port
	char SMSFolder[64];
	char radio[64];
	char antenna[64];
	int EnableDev;
	char UplinkCode[64];
	char Version[16];
        int DumpBuffer;
        char DumpFile[64];
};
 
typedef struct {
    int parent_status;
    unsigned long packet_count;
} thread_shared_vars_t;

typedef struct {
    /* Rx Metadata */
    short int Channel;
    time_t Timestamp;
    double Frequency;
    double FrequencyError;
    int ImplicitOrExplicit;
    double Bandwidth;
    int ErrorCoding;
    int SpreadingFactor;
    int LowDataRateOptimize;
    int SNR;
    int RSSI;
} rx_metadata_t;

/* HABpack Telemetry Storage */
typedef enum { HB_VALUE_INTEGER, HB_VALUE_REAL, HB_VALUE_STRING } HB_Value_Type;
typedef struct habpack_telem_linklist_entry {
    uint32_t type;
    char name[32];
    HB_Value_Type value_type;
    union {
        int64_t integer;
        double real;
        char *string;
    } value;
    struct habpack_telem_linklist_entry *next;
} habpack_telem_linklist_entry_t;

typedef struct {
	/* Core Telemetry */
	char Callsign[32];
	uint64_t SentenceId;
    uint64_t Time;
    char TimeString[9];
	double Latitude;
	double Longitude;
	int64_t Altitude;
	/* Calling Beacon Values */
	uint64_t DownlinkFrequency;
	int64_t DownlinkLoraMode;
	int DownlinkLoraImplicit;
	int DownlinkLoraErrorCoding;
	double DownlinkLoraBandwidth;
	int DownlinkLoraSpreadingFactor;
	int DownlinkLoraLowDatarateOptimise;
	/* Linked list of additional telem fields */
	habpack_telem_linklist_entry_t *habpack_extra;
} rx_telemetry_t;

#define Message_length	257
#define UKHASstring_length	257
typedef struct {
	char Message[Message_length];
	int Bytes;
	/* UKHAS ASCII Telemetry String for habitat upload */
    char UKHASstring[UKHASstring_length];
    char HabitatString[UKHASstring_length];
    bool isCallingBeacon;
    double AscentRate;
    /* Telemetry values */
	rx_telemetry_t Telemetry;
    rx_metadata_t Metadata;
} received_t;

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

#endif /* _H_Global */
