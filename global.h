#include <curses.h>

#define RUNNING 1               // The main program is running
#define STOPPED 0               // The main program has stopped
 struct TLoRaDevice {
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
	char Telemetry[256];
	char Payload[16], Time[12];
	unsigned int Counter, LastCounter;
	unsigned long Seconds;
	double PredictedLongitude, PredictedLatitude;
	double Longitude, Latitude;
	unsigned int Altitude, PreviousAltitude;
	unsigned int Satellites;
	unsigned long LastPositionAt;
	time_t LastPacketAt, LastSSDVPacketAt, LastTelemetryPacketAt;
	float AscentRate;
	time_t ReturnToCallingModeAt;
	time_t ReturnToOriginalFrequencyAt;
	int InCallingMode;
	int ActivityLED;
	double UplinkFrequency;
	int UplinkMode;
	int Speed, Heading, PredictedTime, CompassActual, CompassTarget, AirDirection, ServoLeft, ServoRight, ServoTime, FlightMode;
	double cda, PredictedLandingSpeed, AirSpeed, GlideRatio;

	// Normal (non TDM) uplink
	int UplinkTime;
	int UplinkCycle;
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
	int NetworkLED;
	int InternetLED;
	int ServerPort;					// JSON port for telemetry, settings
	char SMSFolder[64];
	char antenna[64];
	int EnableDev;
};
 typedef struct {
    int parent_status;
    unsigned long packet_count;
} thread_shared_vars_t;

typedef struct {
    short int Channel;
    char Telemetry[257];
    int Packet_Number;
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
	int sockfd;
};

extern struct TConfig Config;
extern int SSDVSendArrayIndex;
extern pthread_mutex_t ssdv_mutex;
 void LogMessage( const char *format, ... );
