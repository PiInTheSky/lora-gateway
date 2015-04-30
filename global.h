#include <curses.h>

struct TLoRaDevice
{
	int InUse;
	int DIO0;
	int DIO5;
	char Frequency[16];
	int SpeedMode;
	int PayloadLength;
	int ImplicitOrExplicit;
	int ErrorCoding;
	int Bandwidth;
	double Reference;
	int SpreadingFactor;
	int LowDataRateOptimize;
	
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
};

struct TConfig
{
	char Tracker[16];
	int EnableHabitat;
	int EnableSSDV;
	char ftpServer[100];
	char ftpUser[32];
	char ftpPassword[32];
	char ftpFolder[64];
	struct TLoRaDevice LoRaDevices[2];
	int EnableKML;
};

extern struct TConfig Config;
