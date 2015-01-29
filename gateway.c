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
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <curses.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "urlencode.h"
#include "base64.h"
#include "ssdv.h"
#include "global.h"


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
#define RF96_MODE_RX_CONTINUOUS     0x85
#define RF96_MODE_SLEEP             0x80
#define RF96_MODE_STANDBY           0x81

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
#define PA_OFF_BOOST                0x00
#define RFO_MIN                     0x00

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23  // 0010 0011
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN                0xC0  // 1100 0000

struct TPayload
{
	int InUse;
	char Payload[32];
};


struct TConfig Config;
struct TPayload Payloads[16];

#pragma pack(1)

struct TBinaryPacket
{
	uint8_t 	PayloadIDs;
	uint16_t	Counter;
	uint16_t	BiSeconds;
	float		Latitude;
	float		Longitude;
	uint16_t	Altitude;
};

const char *Modes[5] = {"slow", "SSDV", "repeater", "turbo", "TurboX"};

void writeRegister(int Channel, uint8_t reg, uint8_t val)
{
	unsigned char data[2];
	
	data[0] = reg | 0x80;
	data[1] = val;
	wiringPiSPIDataRW(Channel, data, 2);
}

uint8_t readRegister(int Channel, uint8_t reg)
{
	unsigned char data[2];
	uint8_t val;
	
	data[0] = reg & 0x7F;
	data[1] = 0;
	wiringPiSPIDataRW(Channel, data, 2);
	val = data[1];
	
    return val;
}


void LogMessage(const char *format, ...)
{
	static WINDOW *Window=NULL;
	char Buffer[200];
	
	if (Window == NULL)
	{
		// Window = newwin(25, 30, 0, 50);
		Window = newwin(9, 80, 16, 0);
		scrollok(Window, TRUE);		
	}
	
    va_list args;
    va_start(args, format);

    vsprintf(Buffer, format, args);

    va_end(args);

	waddstr(Window, Buffer);
	
	wrefresh(Window);
}

void ChannelPrintf(int Channel, int row, int column, const char *format, ...)
{
	char Buffer[80];
	
    va_list args;
    va_start(args, format);

    vsprintf(Buffer, format, args);

    va_end(args);

	mvwaddstr(Config.LoRaDevices[Channel].Window, row, column, Buffer);
	
	wrefresh(Config.LoRaDevices[Channel].Window);
}

void setMode(int Channel, uint8_t newMode)
{
  if(newMode == currentMode)
    return;  
  
  switch (newMode) 
  {
    case RF96_MODE_RX_CONTINUOUS:
      writeRegister(Channel, REG_PA_CONFIG, PA_OFF_BOOST);  // TURN PA OFF FOR RECIEVE??
      writeRegister(Channel, REG_LNA, LNA_MAX_GAIN);  // LNA_MAX_GAIN);  // MAX GAIN FOR RECIEVE
      writeRegister(Channel, REG_OPMODE, newMode);
      currentMode = newMode; 
      // LogMessage("Changing to Receive Continuous Mode\n");
      break;
    case RF96_MODE_SLEEP:
      writeRegister(Channel, REG_OPMODE, newMode);
      currentMode = newMode; 
      // LogMessage("Changing to Sleep Mode\n"); 
      break;
    case RF96_MODE_STANDBY:
      writeRegister(Channel, REG_OPMODE, newMode);
      currentMode = newMode; 
      // LogMessage("Changing to Standby Mode\n");
      break;
    default: return;
  } 
  
  if(newMode != RF96_MODE_SLEEP)
  {
	delay(1);
	}
	
  // LogMessage("Mode Change Done\n");
  return;
}


void setLoRaMode(int Channel)
{
	double Frequency;
	unsigned long FrequencyValue;

	// LogMessage("Setting LoRa Mode\n");
	setMode(Channel, RF96_MODE_SLEEP);
	writeRegister(Channel, REG_OPMODE,0x80);
	
	setMode(Channel, RF96_MODE_SLEEP);
  
	if (sscanf(Config.LoRaDevices[Channel].Frequency, "%lf", &Frequency))
	{
		FrequencyValue = (unsigned long)(Frequency * 7110656 / 434);
		// LogMessage("FrequencyValue = %06Xh\n", FrequencyValue);
		writeRegister(Channel, 0x06, (FrequencyValue >> 16) & 0xFF);		// Set frequency
		writeRegister(Channel, 0x07, (FrequencyValue >> 8) & 0xFF);
		writeRegister(Channel, 0x08, FrequencyValue & 0xFF);
	}

	// LogMessage("Mode = %d\n", readRegister(Channel, REG_OPMODE));
}

/////////////////////////////////////
//    Method:   Setup to receive continuously
//////////////////////////////////////
void startReceiving(int Channel)
{
	writeRegister(Channel, REG_MODEM_CONFIG, Config.LoRaDevices[Channel].ImplicitOrExplicit | Config.LoRaDevices[Channel].ErrorCoding | Config.LoRaDevices[Channel].Bandwidth);
	writeRegister(Channel, REG_MODEM_CONFIG2, Config.LoRaDevices[Channel].SpreadingFactor | CRC_ON);
	writeRegister(Channel, REG_MODEM_CONFIG3, 0x04 | Config.LoRaDevices[Channel].LowDataRateOptimize);									// 0x04: AGC sets LNA gain
	writeRegister(Channel, REG_DETECT_OPT, (readRegister(Channel, REG_DETECT_OPT) & 0xF8) | ((Config.LoRaDevices[Channel].SpreadingFactor == SPREADING_6) ? 0x05 : 0x03));	// 0x05 For SF6; 0x03 otherwise
	writeRegister(Channel, REG_DETECTION_THRESHOLD, (Config.LoRaDevices[Channel].SpreadingFactor == SPREADING_6) ? 0x0C : 0x0A);		// 0x0C for SF6, 0x0A otherwise

	LogMessage("Channel %d %s mode\n", Channel, Modes[Config.LoRaDevices[Channel].SpeedMode]);

	// writeRegister(Channel, REG_PAYLOAD_LENGTH, Config.LoRaDevices[Channel].PayloadLength);
	// writeRegister(Channel, REG_RX_NB_BYTES, Config.LoRaDevices[Channel].PayloadLength);
	writeRegister(Channel, REG_PAYLOAD_LENGTH, 255);
	writeRegister(Channel, REG_RX_NB_BYTES, 255);

	// writeRegister(Channel, REG_HOP_PERIOD,0xFF);
	
	// writeRegister(Channel, REG_FIFO_ADDR_PTR, readRegister(Channel, REG_FIFO_RX_BASE_AD));   
	writeRegister(Channel, REG_FIFO_RX_BASE_AD, 0);
	writeRegister(Channel, REG_FIFO_ADDR_PTR, 0);
  
	// Setup Receive Continous Mode
	setMode(Channel, RF96_MODE_RX_CONTINUOUS); 
}

void setupRFM98(int Channel)
{
	if (Config.LoRaDevices[Channel].InUse)
	{
		// initialize the pins
		pinMode(Config.LoRaDevices[Channel].DIO0, INPUT);
		pinMode(Config.LoRaDevices[Channel].DIO5, INPUT);

		if (wiringPiSPISetup(Channel, 500000) < 0)
		{
			fprintf(stderr, "Failed to open SPI port.  Try loading spi library with 'gpio load spi'");
			exit(1);
		}

		// LoRa mode 
		setLoRaMode(Channel);
  
		startReceiving(Channel);
	}
}

void ShowPacketCounts(int Channel)
{
	if (Config.LoRaDevices[Channel].InUse)
	{
		ChannelPrintf(Channel, 6, 1, "Telem Packets = %d", Config.LoRaDevices[Channel].TelemetryCount);
		ChannelPrintf(Channel, 7, 1, "Image Packets = %d", Config.LoRaDevices[Channel].SSDVCount);
		ChannelPrintf(Channel, 8, 1, "Bad CRC = %d Bad Type = %d", Config.LoRaDevices[Channel].BadCRCCount, Config.LoRaDevices[Channel].UnknownCount);
	}
}

double FrequencyError(int Channel)
{
	int32_t Temp;
	
	Temp = (int32_t)readRegister(Channel, REG_FREQ_ERROR) & 7;
	Temp <<= 8L;
	Temp += (int32_t)readRegister(Channel, REG_FREQ_ERROR+1);
	Temp <<= 8L;
	Temp += (int32_t)readRegister(Channel, REG_FREQ_ERROR+2);
	
	if (readRegister(Channel, REG_FREQ_ERROR) & 8)
	{
		Temp = Temp - 524288;
	}

	return - ((double)Temp * (1<<24) / 32000000.0) * (Config.LoRaDevices[Channel].Reference / 500000.0);
}	

int receiveMessage(int Channel, unsigned char *message)
{
	int i, Bytes, currentAddr, x;
	unsigned char data[257];

	Bytes = 0;
	
	x = readRegister(Channel, REG_IRQ_FLAGS);
	// LogMessage("Message status = %02Xh\n", x);
  
	// clear the rxDone flag
	writeRegister(Channel, REG_IRQ_FLAGS, 0x40); 
   
	// check for payload crc issues (0x20 is the bit we are looking for
	if((x & 0x20) == 0x20)
	{
		LogMessage("CRC Failure, RSSI %d\n", readRegister(Channel, REG_PACKET_RSSI) - 157);
		// reset the crc flags
		writeRegister(Channel, REG_IRQ_FLAGS, 0x20);
		ChannelPrintf(Channel, 3, 1, "CRC Failure %02Xh!!\n", x);
		Config.LoRaDevices[Channel].BadCRCCount++;
		ShowPacketCounts(Channel);
	}
	else
	{
		currentAddr = readRegister(Channel, REG_FIFO_RX_CURRENT_ADDR);
		// LogMessage("currentAddr = %d\n", currentAddr);
		Bytes = readRegister(Channel, REG_RX_NB_BYTES);
		// LogMessage("%d bytes in packet\n", Bytes);

		// LogMessage("RSSI = %d\n", readRegister(Channel, REG_PACKET_RSSI) - 137);
		ChannelPrintf(Channel,  9, 1, "Packet   SNR = %4d   ", (char)(readRegister(Channel, REG_PACKET_SNR)) / 4);
		ChannelPrintf(Channel, 10, 1, "Packet  RSSI = %4d   ", readRegister(Channel, REG_PACKET_RSSI) - 157);
		ChannelPrintf(Channel, 11, 1, "Freq. Error = %4.1lfkHz ", FrequencyError(Channel) / 1000);

		writeRegister(Channel, REG_FIFO_ADDR_PTR, currentAddr);   
		
		/*
		// now loop over the fifo getting the data
		for(i = 0; i < Bytes; i++)
		{
			message[i] = (unsigned char)readRegister(Channel, REG_FIFO);
		}
		*/
		data[0] = REG_FIFO;
		wiringPiSPIDataRW(Channel, data, Bytes+1);
		for (i=0; i<=Bytes; i++)
		{
			message[i] = data[i+1];
		}
		
		message[Bytes] = '\0';
	
		// writeRegister(Channel, REG_FIFO_ADDR_PTR, 0);  // currentAddr);   
		// writeRegister(Channel, REG_FIFO_ADDR_PTR, 0);
		// writeRegister(Channel, REG_FIFO_RX_BASE_AD, 0);
	} 

	// Clear all flags
	writeRegister(Channel, REG_IRQ_FLAGS, 0xFF); 
  
  return Bytes;
}

static char *decode_callsign(char *callsign, uint32_t code)
{
	char *c, s;
	
	*callsign = '\0';
	
	/* Is callsign valid? */
	if(code > 0xF423FFFF) return(callsign);
	
	for(c = callsign; code; c++)
	{
		s = code % 40;
		if(s == 0) *c = '-';
		else if(s < 11) *c = '0' + s - 1;
		else if(s < 14) *c = '-';
		else *c = 'A' + s - 14;
		code /= 40;
	}
	*c = '\0';
	
	return(callsign);
}

void ConvertStringToHex(unsigned char *Target, unsigned char *Source, int Length)
{
	const char Hex[16] = "0123456789ABCDEF";
	int i;
	
	for (i=0; i<Length; i++)
	{
		*Target++ = Hex[Source[i] >> 4];
		*Target++ = Hex[Source[i] & 0x0F];
	}

	*Target++ = '\0';
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}

void UploadTelemetryPacket(char *Telemetry)
{
	if (Config.EnableHabitat)
	{
		CURL *curl;
		CURLcode res;
		char PostFields[200];
	 
		/* In windows, this will init the winsock stuff */ 
		curl_global_init(CURL_GLOBAL_ALL);
	 
		/* get a curl handle */ 
		curl = curl_easy_init();
		if (curl)
		{
			// So that the response to the curl POST doesn;'t mess up my finely crafted display!
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

			// Set the URL that is about to receive our POST
			curl_easy_setopt(curl, CURLOPT_URL, "http://habitat.habhub.org/transition/payload_telemetry");
		
			// Now specify the POST data
			sprintf(PostFields, "callsign=%s&string=%s&string_type=ascii&metadata={}", Config.Tracker, Telemetry);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
	 
			// Perform the request, res will get the return code
			res = curl_easy_perform(curl);
		
			// Check for errors
			if(res != CURLE_OK)
			{
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			}
			
			// always cleanup
			curl_easy_cleanup(curl);
		}
	  
		curl_global_cleanup();
	}
}
			
void UploadImagePacket(char *EncodedCallsign, char *EncodedEncoding, char *EncodedData)
{
	CURL *curl;
	CURLcode res;
	char PostFields[1000];
 
	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		// So that the response to the curl POST doesn;'t mess up my finely crafted display!
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		
		/* Set the URL that is about to receive our POST. This URL can
		   just as well be a https:// URL if that is what should receive the
		   data. */ 
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.sanslogic.co.uk/ssdv/data.php");
	
		/* Now specify the POST data */ 
		sprintf(PostFields, "callsign=%s&encoding=%s&packet=%s", EncodedCallsign, EncodedEncoding, EncodedData);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
 
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
	
		/* Check for errors */ 
		if(res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		/* always cleanup */ 
		curl_easy_cleanup(curl);
	}
	  
	curl_global_cleanup();
}

void ReadString(FILE *fp, char *keyword, char *Result, int Length, int NeedValue)
{
	char line[100], *token, *value;
 
	fseek(fp, 0, SEEK_SET);
	*Result = '\0';

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		token = strtok(line, "=");
		if (strcasecmp(keyword, token) == 0)
		{
			value = strtok(NULL, "\n");
			strcpy(Result, value);
			return;
		}
	}

	if (NeedValue)
	{
		LogMessage("Missing value for '%s' in configuration file\n", keyword);
		exit(1);
	}
}

int ReadInteger(FILE *fp, char *keyword, int NeedValue, int DefaultValue)
{
	char Temp[64];

	ReadString(fp, keyword, Temp, sizeof(Temp), NeedValue);

	if (Temp[0])
	{
		return atoi(Temp);
	}
	
	return DefaultValue;
}

int ReadBoolean(FILE *fp, char *keyword, int NeedValue, int *Result)
{
	char Temp[32];

	ReadString(fp, keyword, Temp, sizeof(Temp), NeedValue);

	if (*Temp)
	{
		*Result = (*Temp == '1') || (*Temp == 'Y') || (*Temp == 'y') || (*Temp == 't') || (*Temp == 'T');
	}

	return *Temp;
}
	
void LoadConfigFile()
{
	FILE *fp;
	char *filename = "gateway.txt";
	char Keyword[32];
	int Channel, Temp;
	char TempString[16];

	Config.EnableHabitat = 1;
	Config.EnableSSDV = 1;
	Config.ftpServer[0] = '\0';
	Config.ftpUser[0] = '\0';
	Config.ftpPassword[0] = '\0';
	Config.ftpFolder[0] = '\0';
	
	if ((fp = fopen(filename, "r")) == NULL)
	{
		printf("\nFailed to open config file %s (error %d - %s).\nPlease check that it exists and has read permission.\n", filename, errno, strerror(errno));
		exit(1);
	}

	ReadString(fp, "tracker", Config.Tracker, sizeof(Config.Tracker), 1);
	LogMessage("Tracker = '%s'\n", Config.Tracker);
	
	ReadBoolean(fp, "EnableHabitat", 0, &Config.EnableHabitat);
	ReadBoolean(fp, "EnableSSDV", 0, &Config.EnableSSDV);

	ReadString(fp, "ftpserver", Config.ftpServer, sizeof(Config.ftpServer), 0);
	ReadString(fp, "ftpUser", Config.ftpUser, sizeof(Config.ftpUser), 0);
	ReadString(fp, "ftpPassword", Config.ftpPassword, sizeof(Config.ftpPassword), 0);
	ReadString(fp, "ftpFolder", Config.ftpFolder, sizeof(Config.ftpFolder), 0);	

	for (Channel=0; Channel<=1; Channel++)
	{
		// Defaults
		Config.LoRaDevices[Channel].Frequency[0] = '\0';
		
		sprintf(Keyword, "frequency_%d", Channel);
		ReadString(fp, Keyword, Config.LoRaDevices[Channel].Frequency, sizeof(Config.LoRaDevices[Channel].Frequency), 0);
		if (Config.LoRaDevices[Channel].Frequency[0])
		{
			Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
			Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_8;
			Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_20K8;
			Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_11;
			Config.LoRaDevices[Channel].LowDataRateOptimize = 0x00;		
			
			LogMessage("Channel %d frequency set to %s\n", Channel, Config.LoRaDevices[Channel].Frequency);
			Config.LoRaDevices[Channel].InUse = 1;

			// DIO0 / DIO5 overrides
			sprintf(Keyword, "DIO0_%d", Channel);
			Config.LoRaDevices[Channel].DIO0 = ReadInteger(fp, Keyword, 0, Config.LoRaDevices[Channel].DIO0);

			sprintf(Keyword, "DIO5_%d", Channel);
			Config.LoRaDevices[Channel].DIO5 = ReadInteger(fp, Keyword, 0, Config.LoRaDevices[Channel].DIO5);

			LogMessage("LoRa Channel %d DIO0=%d DIO5=%d\n", Channel, Config.LoRaDevices[Channel].DIO0, Config.LoRaDevices[Channel].DIO5);
			
			Config.LoRaDevices[Channel].SpeedMode = 0;
			sprintf(Keyword, "mode_%d", Channel);
			Config.LoRaDevices[Channel].SpeedMode = ReadInteger(fp, Keyword, 0, 0);
			// Config.LoRaDevices[Channel].PayloadLength = Config.LoRaDevices[Channel].SpeedMode == 0 ? 80 : 255;
			ChannelPrintf(Channel, 1, 1, "Channel %d %sMHz %s mode", Channel, Config.LoRaDevices[Channel].Frequency, Modes[Config.LoRaDevices[Channel].SpeedMode]);

			if (Config.LoRaDevices[Channel].SpeedMode == 4)
			{
				// Testing
				Config.LoRaDevices[Channel].ImplicitOrExplicit = IMPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_5;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_250K;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_6;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0;		
			}
			else if (Config.LoRaDevices[Channel].SpeedMode == 3)
			{
				// Normal mode for high speed images in 868MHz band
				Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_6;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_250K;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_7;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0;		
			}
			else if (Config.LoRaDevices[Channel].SpeedMode == 2)
			{
				// Normal mode for repeater network
				Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_8;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_62K5;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_8;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0x00;		
			}
			else if (Config.LoRaDevices[Channel].SpeedMode == 1)
			{
				// Normal mode for SSDV
				Config.LoRaDevices[Channel].ImplicitOrExplicit = IMPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_5;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_20K8;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_6;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0;
			}
			else
			{
				Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_8;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_20K8;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_11;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0x08;		
			}

			sprintf(Keyword, "sf_%d", Channel);
			Temp = ReadInteger(fp, Keyword, 0, 0);
			if ((Temp >= 6) && (Temp <= 12))
			{
				Config.LoRaDevices[Channel].SpreadingFactor = Temp << 4;
				LogMessage("Setting SF=%d\n", Temp);
			}

			sprintf(Keyword, "bandwidth_%d", Channel);
			ReadString(fp, Keyword, TempString, sizeof(TempString), 0);
			if (*TempString)
			{
				LogMessage("Setting BW=%s\n", TempString);
			}
			if (strcmp(TempString, "7K8") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_7K8;
			}
			else if (strcmp(TempString, "10K4") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_10K4;
			}
			else if (strcmp(TempString, "15K6") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_15K6;
			}
			else if (strcmp(TempString, "20K8") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_20K8;
			}
			else if (strcmp(TempString, "31K25") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_31K25;
			}
			else if (strcmp(TempString, "41K7") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_41K7;
			}
			else if (strcmp(TempString, "62K5") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_62K5;
			}
			else if (strcmp(TempString, "125K") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_125K;
			}
			else if (strcmp(TempString, "250K") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_250K;
			}
			else if (strcmp(TempString, "500K") == 0)
			{
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_500K;
			}

			switch (Config.LoRaDevices[Channel].Bandwidth)
			{
				case  BANDWIDTH_7K8:	Config.LoRaDevices[Channel].Reference = 7800; break;
				case  BANDWIDTH_10K4:   Config.LoRaDevices[Channel].Reference = 10400; break;
				case  BANDWIDTH_15K6:   Config.LoRaDevices[Channel].Reference = 15600; break;
				case  BANDWIDTH_20K8:   Config.LoRaDevices[Channel].Reference = 20800; break;
				case  BANDWIDTH_31K25:  Config.LoRaDevices[Channel].Reference = 31250; break;
				case  BANDWIDTH_41K7:   Config.LoRaDevices[Channel].Reference = 41700; break;
				case  BANDWIDTH_62K5:   Config.LoRaDevices[Channel].Reference = 62500; break;
				case  BANDWIDTH_125K:   Config.LoRaDevices[Channel].Reference = 125000; break;
				case  BANDWIDTH_250K:   Config.LoRaDevices[Channel].Reference = 250000; break;
				case  BANDWIDTH_500K:   Config.LoRaDevices[Channel].Reference = 500000; break;
			}
			
			sprintf(Keyword, "implicit_%d", Channel);
			if (ReadBoolean(fp, Keyword, 0, &Temp))
			{
				if (Temp)
				{
					Config.LoRaDevices[Channel].ImplicitOrExplicit = IMPLICIT_MODE;
				}
			}
			
			sprintf(Keyword, "coding_%d", Channel);
			Temp = ReadInteger(fp, Keyword, 0, 0);
			if ((Temp >= 5) && (Temp <= 8))
			{
				Config.LoRaDevices[Channel].ErrorCoding = (Temp-4) << 1;
				LogMessage("Setting Error Coding=%d\n", Temp);
			}

			sprintf(Keyword, "lowopt_%d", Channel);
			if (ReadBoolean(fp, Keyword, 0, &Temp))
			{
				if (Temp)
				{
					Config.LoRaDevices[Channel].LowDataRateOptimize = 0x08;
				}
			}
		}
	}

	fclose(fp);
}

void LoadPayloadFile(int ID)
{
	FILE *fp;
	char filename[16];
	char Keyword[32];
	int Channel, Temp;
	char TempString[16];
	
	sprintf(filename, "payload_%d.txt", ID);

	if ((fp = fopen(filename, "r")) != NULL)
	{
		LogMessage("Reading payload file %s\n", filename);
		ReadString(fp, "payload", Payloads[ID].Payload, sizeof(Payloads[ID].Payload), 1);
		LogMessage("Payload %d = '%s'\n", ID, Payloads[ID].Payload);
		
		Payloads[ID].InUse = 1;

		fclose(fp);
	}
	else
	{
		strcpy(Payloads[ID].Payload, "Unknown");
		Payloads[ID].InUse = 0;
	}
}

void LoadPayloadFiles(void)
{
	int ID;
	
	for (ID=0; ID<16; ID++)
	{
		LoadPayloadFile(ID);
	}
}

WINDOW * InitDisplay(void)
{
    WINDOW * mainwin;
	int Channel;

    /*  Initialize ncurses  */

    if ( (mainwin = initscr()) == NULL ) {
	fprintf(stderr, "Error initialising ncurses.\n");
	exit(EXIT_FAILURE);
    }

    start_color();                    /*  Initialize colours  */

	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_YELLOW, COLOR_BLUE);

	color_set(1, NULL);
	// bkgd(COLOR_PAIR(1));
	// attrset(COLOR_PAIR(1) | A_BOLD);

	// Title bar
    mvaddstr(0, 19, " LoRa Habitat and SSDV Gateway by daveake ");
    refresh();

	// Windows for LoRa live data
	for (Channel=0; Channel<=1; Channel++)
	{
		Config.LoRaDevices[Channel].Window = newwin(14, 38, 1, Channel ? 41 : 1);
		wbkgd(Config.LoRaDevices[Channel].Window, COLOR_PAIR(2));
		
		// wcolor_set(Config.LoRaDevices[Channel].Window, 2, NULL);
		// waddstr(Config.LoRaDevices[Channel].Window, "WINDOW");
		// mvwaddstr(Config.LoRaDevices[Channel].Window, 0, 0, "Window");
		wrefresh(Config.LoRaDevices[Channel].Window);
	}
	
	curs_set(0);
		   
	return mainwin;
}	

void CloseDisplay(WINDOW * mainwin)
{
    /*  Clean up after ourselves  */
    delwin(mainwin);
    endwin();
    refresh();
}
	
void ProcessLine(int Channel, char *Line)
{
	int FieldCount; 

	FieldCount = sscanf(Line+2, "%15[^,],%u,%8[^,],%lf,%lf,%u",
						&(Config.LoRaDevices[Channel].Payload),
						&(Config.LoRaDevices[Channel].Counter),
						&(Config.LoRaDevices[Channel].Time),
						&(Config.LoRaDevices[Channel].Latitude),
						&(Config.LoRaDevices[Channel].Longitude),
						&(Config.LoRaDevices[Channel].Altitude));
						
	// HAB->HAB_status = FieldCount == 6;
}

void DoPositionCalcs(Channel)
{	
	unsigned long Now;
	struct tm tm;
	float Climb, Period;

	strptime(Config.LoRaDevices[Channel].Time, "%H:%M:%S", &tm);
	Now = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
	
	if ((Config.LoRaDevices[Channel].LastPositionAt > 0) && (Now > Config.LoRaDevices[Channel].LastPositionAt))
	{
		Climb = (float)Config.LoRaDevices[Channel].Altitude - (float)Config.LoRaDevices[Channel].PreviousAltitude;
		Period = (float)Now - (float)Config.LoRaDevices[Channel].LastPositionAt;
		Config.LoRaDevices[Channel].AscentRate = Climb / Period;
	}
	else
	{
		Config.LoRaDevices[Channel].AscentRate = 0;
	}
	
	Config.LoRaDevices[Channel].PreviousAltitude = Config.LoRaDevices[Channel].Altitude;
	Config.LoRaDevices[Channel].LastPositionAt = Now;
	
	ChannelPrintf(Channel, 4, 1, "%8.5lf, %8.5lf, %05u   ", 
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude);
}

int NewBoard(void)
{
	FILE *cpuFd ;
	char line [120] ;
	char *c ;
	static int  boardRev = -1 ;

	if (boardRev < 0)
	{
		if ((cpuFd = fopen ("/proc/cpuinfo", "r")) != NULL)
		{
			while (fgets (line, 120, cpuFd) != NULL)
				if (strncmp (line, "Revision", 8) == 0)
					break ;

			fclose (cpuFd) ;

			if (strncmp (line, "Revision", 8) == 0)
			{
				// printf ("RPi %s", line);
				boardRev = ((strstr(line, "0010") != NULL) || (strstr(line, "0012") != NULL));	// B+ or A+
			}
		}
	}
	
	return boardRev;
}	

uint16_t CRC16(unsigned char *ptr)
{
    uint16_t CRC, xPolynomial;
	int j;
	
    CRC = 0xffff;           // Seed
    xPolynomial = 0x1021;
   
    for (; *ptr; ptr++)
    {   // For speed, repeat calculation instead of looping for each bit
        CRC ^= (((unsigned int)*ptr) << 8);
        for (j=0; j<8; j++)
        {
            if (CRC & 0x8000)
                CRC = (CRC << 1) ^ 0x1021;
            else
                CRC <<= 1;
        }
    }
	 
	return CRC;
}


int main(int argc, char **argv)
{
	unsigned char Message[257], Command[200], Telemetry[100], filename[100], *dest, *src;
	int Bytes, ImageNumber, PreviousImageNumber, PacketNumber, PreviousPacketNumber;
	uint32_t CallsignCode, PreviousCallsignCode, LoopCount[2];
	pthread_t /* CurlThread, */ SSDVThread;
	FILE *fp;
	WINDOW * mainwin;
		
	mainwin = InitDisplay();

	// LogMessage("**** LoRa Gateway by daveake ****\n");

	PreviousImageNumber = -1;
	PreviousCallsignCode = 0;
	PreviousPacketNumber = 0;
	
	fp = NULL;
	
	Config.LoRaDevices[0].InUse = 0;
	Config.LoRaDevices[1].InUse = 0;
	
	if (NewBoard())
	{
		// For dual card.  These are for the second prototype (earlier one will need overrides)

		Config.LoRaDevices[0].DIO0 = 6;
		Config.LoRaDevices[0].DIO5 = 5;
		
		Config.LoRaDevices[1].DIO0 = 31;
		Config.LoRaDevices[1].DIO5 = 26;
		
		LogMessage("Pi A+/B+ board\n");
	}
	else
	{
		Config.LoRaDevices[0].DIO0 = 6;
		Config.LoRaDevices[0].DIO5 = 5;
		
		Config.LoRaDevices[1].DIO0 = 3;
		Config.LoRaDevices[1].DIO5 = 4;

		LogMessage("Pi A/B board\n");
	}

	LoadConfigFile();
	LoadPayloadFiles();
	
	if (wiringPiSetup() < 0)
	{
		fprintf(stderr, "Failed to open wiringPi\n");
		exit(1);
	}
	
	setupRFM98(0);
	setupRFM98(1);
	
	ShowPacketCounts(0);
	ShowPacketCounts(1);

	LoopCount[0] = 0;
	LoopCount[1] = 0;
	
	if (pthread_create(&SSDVThread, NULL, SSDVLoop, NULL))
	{
		fprintf(stderr, "Error creating SSDV thread\n");
		return 1;
	}
	
	
	while (1)
	{
		int Channel;
		
		for (Channel=0; Channel<=1; Channel++)
		{
			if (Config.LoRaDevices[Channel].InUse)
			{
				if (digitalRead(Config.LoRaDevices[Channel].DIO0))
				{
					Bytes = receiveMessage(Channel, Message+1);
					
					if (Bytes > 0)
					{
						// LogMessage("Channel %d data available - %d bytes\n", Channel, Bytes);
						// LogMessage("Line = '%s'\n", Message);

						// Habitat upload
						// $$....
						if (Message[1] == '!')
						{
							LogMessage("Ch %d: Uploaded message %s\n", Channel, Message+1);
						}
						else if (Message[1] == '$')
						{
							ChannelPrintf(Channel, 3, 1, "Telemetry %d bytes    ", strlen(Message)-1);	//  Bytes);
							// LogMessage("Telemetry %d bytes\n", strlen(Message)-1);
															
							UploadTelemetryPacket(Message+1);

							ProcessLine(Channel, Message+1);
							
							DoPositionCalcs(Channel);
							
							Config.LoRaDevices[Channel].TelemetryCount++;

							Message[strlen(Message+1)] = '\0';
							LogMessage("Ch %d: %s\n", Channel, Message+1);
						}
						else if ((Message[1] & 0xC0) == 0xC0)
						{
							// Binary telemetry packet
							struct TBinaryPacket BinaryPacket;
							char Data[100], Sentence[100];
							int SourceID, SenderID;
							
							SourceID = Message[1] & 0x07;
							SenderID = (Message[1] >> 3) & 0x07;

							ChannelPrintf(Channel, 3, 1, "Binary Telemetry");

							memcpy(&BinaryPacket, Message+1, sizeof(BinaryPacket));
							
							strcpy(Config.LoRaDevices[Channel].Payload, "Binary");
							Config.LoRaDevices[Channel].Seconds = (unsigned long) BinaryPacket.BiSeconds * 2L;
							Config.LoRaDevices[Channel].Counter = BinaryPacket.Counter;
							Config.LoRaDevices[Channel].Latitude = BinaryPacket.Latitude;
							Config.LoRaDevices[Channel].Longitude = BinaryPacket.Longitude;
							Config.LoRaDevices[Channel].Altitude = BinaryPacket.Altitude;

							sprintf(Data, "%s,%u,%02d:%02d:%02d,%8.5f,%8.5f,%u",
										  Payloads[SourceID].Payload,
										  BinaryPacket.Counter,
										  (int)(Config.LoRaDevices[Channel].Seconds / 3600),
										  (int)((Config.LoRaDevices[Channel].Seconds / 60) % 60),
										  (int)(Config.LoRaDevices[Channel].Seconds % 60),
										  BinaryPacket.Latitude,
										  BinaryPacket.Longitude,
										  BinaryPacket.Altitude);
							sprintf(Sentence, "$$%s*%04X\n", Data, CRC16(Data));
							
							UploadTelemetryPacket(Sentence);

							DoPositionCalcs(Channel);
							
							Config.LoRaDevices[Channel].TelemetryCount++;

							LogMessage("Ch %d: Sender %d Source %d (%s) Position %8.5lf, %8.5lf, %05u\n",
								Channel,
								SenderID,
								SourceID,
								Payloads[SourceID].Payload,
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude);
						}
						else if ((Message[1] & 0xC0) == 0x80)
						{
							// Binary upload packet
							int SenderID, TargetID;
							
							ChannelPrintf(Channel, 3, 1, "Uplink Message");

							TargetID = Message[1] & 0x07;
							SenderID = (Message[1] >> 3) & 0x07;

							LogMessage("Ch %d: Sender %d Target %d (%s) Message %s\n",
										Channel,
										SenderID,
										TargetID,
										Payloads[TargetID].Payload,
										Message+2);
						}
						else if (Message[1] == 0x66)
						{
							// SSDV packet
							char Callsign[7], *FileMode, *EncodedCallsign, *EncodedEncoding, *Base64Data, *EncodedData, HexString[513], Command[1000];
							int output_length;
							
							Message[0] = 0x55;
							
							CallsignCode = Message[2]; CallsignCode <<= 8;
							CallsignCode |= Message[3]; CallsignCode <<= 8;
							CallsignCode |= Message[4]; CallsignCode <<= 8;
							CallsignCode |= Message[5];
							
							decode_callsign(Callsign, CallsignCode);
														
							ImageNumber = Message[6];
							PacketNumber = Message[8];
				
							// Create new file ?
							if ((ImageNumber != PreviousImageNumber) || (PacketNumber <= PreviousPacketNumber) || (CallsignCode != PreviousCallsignCode))
							{
								// New image so new file
								// FileMode = "wb";
								FileMode = "ab";
								Config.LoRaDevices[Channel].SSDVMissing = PacketNumber;
							}
							else
							{
								FileMode = "ab";
								if (PacketNumber > (PreviousPacketNumber+1))
								{
									Config.LoRaDevices[Channel].SSDVMissing += PacketNumber - PreviousPacketNumber - 1;
								}
							}

							LogMessage("SSDV Packet, Callsign %s, Image %d, Packet %d, %d Missing\n", Callsign, Message[6], Message[7] * 256 + Message[8], Config.LoRaDevices[Channel].SSDVMissing);
							ChannelPrintf(Channel, 3, 1, "SSDV Packet %d bytes  ", Bytes);
							
							PreviousImageNumber = ImageNumber;
							PreviousPacketNumber = PacketNumber;
							PreviousCallsignCode = CallsignCode;

							// Save to file
							
							sprintf(filename, "/tmp/%s_%d.bin", Callsign, ImageNumber);

							if (fp = fopen(filename, FileMode))
							{
								fwrite(Message, 1, 256, fp); 
								fclose(fp);
							}

							// Upload to server
							if (Config.EnableSSDV)
							{
								EncodedCallsign = url_encode(Callsign); 
								EncodedEncoding = url_encode("hex"); 

								// Base64Data = base64_encode(Message, 256, &output_length);
								// printf("output_length=%d, byte=%02Xh\n", output_length, Base64Data[output_length]);
								// Base64Data[output_length] = '\0';
								// printf ("Base64Data '%s'\n", Base64Data);
								ConvertStringToHex(HexString, Message, 256);
								EncodedData = url_encode(HexString); 

								UploadImagePacket(EncodedCallsign, EncodedEncoding, EncodedData);
								
								free(EncodedCallsign);
								free(EncodedEncoding);
								// free(Base64Data);
								free(EncodedData);
							}

							Config.LoRaDevices[Channel].SSDVCount++;
						}
						else
						{
							LogMessage("Unknown packet type is %02Xh, RSSI %d\n", Message[1], readRegister(Channel, REG_PACKET_RSSI) - 157);
							ChannelPrintf(Channel, 3, 1, "Unknown Packet %d, %d bytes", Message[0], Bytes);
							Config.LoRaDevices[Channel].UnknownCount++;
						}
						
						Config.LoRaDevices[Channel].LastPacketAt = time(NULL);

						ShowPacketCounts(Channel);
					}
				}
				
				if (++LoopCount[Channel] > 1000000)
				{
					LoopCount[Channel] = 0;
					ShowPacketCounts(Channel);
					ChannelPrintf(Channel, 12, 1, "Current RSSI = %4d   ", readRegister(Channel, REG_CURRENT_RSSI) - 157);
					if (Config.LoRaDevices[Channel].LastPacketAt > 0)
					{
						ChannelPrintf(Channel, 5, 1, "%us since last packet   ", (unsigned int)(time(NULL) - Config.LoRaDevices[Channel].LastPacketAt));
					}
				}
			}
		}
		// delay(5);
 	}

	CloseDisplay(mainwin);
	
	return 0;
}


