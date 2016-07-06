#include <curses.h>

#define SSDV_PACKETS	64

struct TSSDVPacket
{
	unsigned char Packet[256];
	char Callsign[7];
};

struct TSSDVPacketArray
{
	struct TSSDVPacket Packets[SSDV_PACKETS];
	int Count;
	int Sending;
};

struct TSSDVPackets
{
	int ImageNumber;
	int HighestPacket;
	bool Packets[1024];
};

struct TLoRaDevice
{
	int InUse;
	int DIO0;
	int DIO5;
	char Frequency[16];
	double activeFreq;
	bool AFC;	
	int SpeedMode;
	int Power;
	int PayloadLength;
	int ImplicitOrExplicit;
	int ErrorCoding;
	int Bandwidth;
	int SpreadingFactor;
	int LowDataRateOptimize;
	int CurrentBandwidth;
	
	WINDOW *Window;
	
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
	int InCallingMode;
	int ActivityLED;
	
	int Speed, Heading, PredictedTime, CompassActual, CompassTarget, AirDirection, ServoLeft, ServoRight, ServoTime, FlightMode;
	double cda, PredictedLandingSpeed, AirSpeed, GlideRatio;

	// Normal (non TDM) uplink
	int UplinkTime;
	int UplinkCycle;
	
	// SSDV Packet Log
	struct TSSDVPackets SSDVPackets[3];
};

struct TConfig
{
	char Tracker[16];
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
	int ServerPort;
	float latitude, longitude;
	char SMSFolder[64];
	char antenna[64];
	int EnableDev;
};

extern struct TConfig Config;
extern struct TSSDVPacketArray SSDVPacketArrays[];
extern int SSDVSendArrayIndex;
extern pthread_mutex_t ssdv_mutex;

void LogMessage(const char *format, ...);