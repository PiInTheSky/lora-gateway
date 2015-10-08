#include <curses.h>

struct TLoRaDevice
{
	int InUse;
	int DIO0;
	int DIO5;
	int TXEN;
	int RXEN;
	char Frequency[16];
	double activeFreq;
	bool AFC;	
	int SpeedMode;
	int PayloadLength;
	int ImplicitOrExplicit;
	int ErrorCoding;
	int Bandwidth;
	int SpreadingFactor;
	int LowDataRateOptimize;
	int CurrentBandwidth;
	
	WINDOW *Window;
	
	unsigned int TelemetryCount, SSDVCount, BadCRCCount, UnknownCount, SSDVMissing;
	
	char Payload[16], Time[12];
	unsigned int Counter;
	unsigned long Seconds;
	double Longitude, Latitude;
	unsigned int Altitude, PreviousAltitude;
	unsigned int Satellites;
	unsigned long LastPositionAt;
	time_t LastPacketAt;
	float AscentRate;
	time_t ReturnToCallingModeAt;
	int InCallingMode;
	int ActivityLED;
};

struct TConfig
{
	char Tracker[16];
	int EnableHabitat;
	int EnableSSDV;
	int EnableTelemetryLogging;
	int CallingTimeout;
	char ftpServer[100];
	char ftpUser[32];
	char ftpPassword[32];
	char ftpFolder[64];
	struct TLoRaDevice LoRaDevices[2];
	int NetworkLED;
	int InternetLED;
	int ServerPort;
};

extern struct TConfig Config;

void LogMessage(const char *format, ...);