
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
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
#include <math.h>
#include <dirent.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "urlencode.h"
#include "base64.h"
#include "ssdv.h"
#include "ftp.h"
#include "habitat.h"
#include "network.h"
#include "global.h"
#include "server.h"

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

const char *Modes[6] = {"Slow", "SSDV", "Repeater", "Turbo", "TurboX", "Calling"};

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

	if (strlen(Buffer) > 79)
	{
		Buffer[77] = '.';
		Buffer[78] = '.';
		Buffer[79] = '\n';
		Buffer[80] = 0;
	}
	
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

void setFrequency(int Channel, double Frequency)
{
	unsigned long FrequencyValue;

	FrequencyValue = (unsigned long)(Frequency * 7110656 / 434);

	writeRegister(Channel, 0x06, (FrequencyValue >> 16) & 0xFF);		// Set frequency
	writeRegister(Channel, 0x07, (FrequencyValue >> 8) & 0xFF);
	writeRegister(Channel, 0x08, FrequencyValue & 0xFF);

	Config.LoRaDevices[Channel].activeFreq = Frequency;

	// LogMessage("Set Frequency to %lf\n", Frequency);
	
	ChannelPrintf(Channel, 1, 1, "Channel %d %8.4lfMHz ", Channel, Frequency);
	// ChannelPrintf(Channel, 1, 1, "Channel %d %8.4lfMHz  %s mode", Channel, Frequency, Modes[Config.LoRaDevices[Channel].SpeedMode]);
	// ChannelPrintf(Channel, 1, 11, "%8.4fMHz", Frequency);
}

void setLoRaMode(int Channel)
{
	double Frequency;

	// LogMessage("Setting LoRa Mode\n");
	setMode(Channel, RF96_MODE_SLEEP);
	writeRegister(Channel, REG_OPMODE,0x80);
	
	setMode(Channel, RF96_MODE_SLEEP);
  
	if (sscanf(Config.LoRaDevices[Channel].Frequency, "%lf", &Frequency))
	{
		// LogMessage("Set Default Frequency\n");
		setFrequency(Channel, Frequency);
	}
}

char *BandwidthString(int Bandwidth)
{
	if (Bandwidth == BANDWIDTH_7K8) return "7.8k";
	if (Bandwidth == BANDWIDTH_10K4) return "10.4k";
	if (Bandwidth == BANDWIDTH_15K6) return "15.6k";
	if (Bandwidth == BANDWIDTH_20K8) return "20.8k";
	if (Bandwidth == BANDWIDTH_31K25) return "31.25k";
	if (Bandwidth == BANDWIDTH_41K7) return "41.7k";
	if (Bandwidth == BANDWIDTH_62K5) return "62.5k";
	if (Bandwidth == BANDWIDTH_125K) return "125k";
	if (Bandwidth == BANDWIDTH_250K) return "250k";
	if (Bandwidth == BANDWIDTH_500K) return "500k";
	return "??k";
}

void SetLoRaParameters(int Channel, int ImplicitOrExplicit, int ErrorCoding, int Bandwidth, int SpreadingFactor, int LowDataRateOptimize)
{
	writeRegister(Channel, REG_MODEM_CONFIG, ImplicitOrExplicit | ErrorCoding | Bandwidth);
	writeRegister(Channel, REG_MODEM_CONFIG2, SpreadingFactor | CRC_ON);
	writeRegister(Channel, REG_MODEM_CONFIG3, 0x04 | LowDataRateOptimize);									// 0x04: AGC sets LNA gain
	writeRegister(Channel, REG_DETECT_OPT, (readRegister(Channel, REG_DETECT_OPT) & 0xF8) | ((SpreadingFactor == SPREADING_6) ? 0x05 : 0x03));	// 0x05 For SF6; 0x03 otherwise
	writeRegister(Channel, REG_DETECTION_THRESHOLD, (SpreadingFactor == SPREADING_6) ? 0x0C : 0x0A);		// 0x0C for SF6, 0x0A otherwise
	
	Config.LoRaDevices[Channel].CurrentBandwidth = Bandwidth;
	
	ChannelPrintf(Channel, 2, 1, "%s, %s, SF%d, EC4:%d %s",
								ImplicitOrExplicit == IMPLICIT_MODE ? "Implicit" : "Explicit",
								BandwidthString(Bandwidth),
								SpreadingFactor >> 4,
								(ErrorCoding >> 1) + 4,
								LowDataRateOptimize ? "LDRO" : "");
}

void SetDefaultLoRaParameters(int Channel)
{
	// LogMessage("Set Default Parameters\n");
	
	SetLoRaParameters(Channel,
					  Config.LoRaDevices[Channel].ImplicitOrExplicit,
					  Config.LoRaDevices[Channel].ErrorCoding,
					  Config.LoRaDevices[Channel].Bandwidth,
					  Config.LoRaDevices[Channel]. SpreadingFactor,
					  Config.LoRaDevices[Channel].LowDataRateOptimize);
}

/////////////////////////////////////
//    Method:   Setup to receive continuously
//////////////////////////////////////
void startReceiving(int Channel)
{
	// writeRegister(Channel, REG_MODEM_CONFIG, Config.LoRaDevices[Channel].ImplicitOrExplicit | Config.LoRaDevices[Channel].ErrorCoding | Config.LoRaDevices[Channel].Bandwidth);
	// writeRegister(Channel, REG_MODEM_CONFIG2, Config.LoRaDevices[Channel].SpreadingFactor | CRC_ON);
	// writeRegister(Channel, REG_MODEM_CONFIG3, 0x04 | Config.LoRaDevices[Channel].LowDataRateOptimize);									// 0x04: AGC sets LNA gain
	// writeRegister(Channel, REG_DETECT_OPT, (readRegister(Channel, REG_DETECT_OPT) & 0xF8) | ((Config.LoRaDevices[Channel].SpreadingFactor == SPREADING_6) ? 0x05 : 0x03));	// 0x05 For SF6; 0x03 otherwise
	// writeRegister(Channel, REG_DETECTION_THRESHOLD, (Config.LoRaDevices[Channel].SpreadingFactor == SPREADING_6) ? 0x0C : 0x0A);		// 0x0C for SF6, 0x0A otherwise

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

void ReTune(int Channel, double FreqShift)
{
	setMode(Channel, RF96_MODE_SLEEP);
	LogMessage("Retune by %lf kHz\n", FreqShift * 1000);
	setFrequency(Channel, Config.LoRaDevices[Channel].activeFreq + FreqShift);
	startReceiving(Channel);
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

		SetDefaultLoRaParameters(Channel);
		
		startReceiving(Channel);
	}
}

void ShowPacketCounts(int Channel)
{
	if (Config.LoRaDevices[Channel].InUse)
	{
		ChannelPrintf(Channel, 7, 1, "Telem Packets = %d", Config.LoRaDevices[Channel].TelemetryCount);
		ChannelPrintf(Channel, 8, 1, "Image Packets = %d", Config.LoRaDevices[Channel].SSDVCount);
		ChannelPrintf(Channel, 9, 1, "Bad CRC = %d Bad Type = %d", Config.LoRaDevices[Channel].BadCRCCount, Config.LoRaDevices[Channel].UnknownCount);
	}
}

double FrequencyReference(int Channel)
{
	switch (Config.LoRaDevices[Channel].CurrentBandwidth)
	{
		case  BANDWIDTH_7K8:	return 7800;
		case  BANDWIDTH_10K4:   return 10400; 
		case  BANDWIDTH_15K6:   return 15600; 
		case  BANDWIDTH_20K8:   return 20800; 
		case  BANDWIDTH_31K25:  return 31250; 
		case  BANDWIDTH_41K7:   return 41700; 
		case  BANDWIDTH_62K5:   return 62500; 
		case  BANDWIDTH_125K:   return 125000; 
		case  BANDWIDTH_250K:   return 250000; 
		case  BANDWIDTH_500K:   return 500000; 
	}
	
	return 0;
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

	return - ((double)Temp * (1<<24) / 32000000.0) * (FrequencyReference(Channel) / 500000.0);
}	

int receiveMessage(int Channel, unsigned char *message)
{
	int i, Bytes, currentAddr, x;
	unsigned char data[257];
	double FreqError;

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
		ChannelPrintf(Channel, 4, 1, "CRC Failure %02Xh!!\n", x);
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
		ChannelPrintf(Channel, 10, 1, "Packet SNR = %d, RSSI = %d      ", (char)(readRegister(Channel, REG_PACKET_SNR)) / 4, readRegister(Channel, REG_PACKET_RSSI) - 157);
		FreqError = FrequencyError(Channel) / 1000;
		ChannelPrintf(Channel, 11, 1, "Freq. Error = %5.1lfkHz ", FreqError);		
		

		writeRegister(Channel, REG_FIFO_ADDR_PTR, currentAddr);   
		
		data[0] = REG_FIFO;
		wiringPiSPIDataRW(Channel, data, Bytes+1);
		for (i=0; i<=Bytes; i++)
		{
			message[i] = data[i+1];
		}
		
		message[Bytes] = '\0';
	
		if(Config.LoRaDevices[Channel].AFC && (fabs(FreqError)>0.5))
		{
			ReTune(Channel, FreqError/1000);
		}
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

void ConvertStringToHex(char *Target, unsigned char *Source, int Length)
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

void LogTelemetryPacket(char *Telemetry)
{
	if (Config.EnableTelemetryLogging)
	{
		FILE *fp;
		
		if ((fp = fopen("telemetry.txt", "at")) != NULL)
		{
			time_t now;
			struct tm *tm;
			
			now = time(0);
			tm = localtime(&now);
		
			fprintf(fp, "%02d:%02d:%02d - %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Telemetry);
			fclose(fp);
		}
	}
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
			sprintf(PostFields, "callsign=%s&string=%s\n&string_type=ascii&metadata={}", Config.Tracker, Telemetry);
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
		sprintf(PostFields, "callsign=%s&encoding=%s&packet=%s", Config.Tracker, EncodedEncoding, EncodedData);
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
	Config.EnableTelemetryLogging = 0;
	Config.ftpServer[0] = '\0';
	Config.ftpUser[0] = '\0';
	Config.ftpPassword[0] = '\0';
	Config.ftpFolder[0] = '\0';
	
	if ((fp = fopen(filename, "r")) == NULL)
	{
		printf("\nFailed to open config file %s (error %d - %s).\nPlease check that it exists and has read permission.\n", filename, errno, strerror(errno));
		exit(1);
	}

	// Receiver config
	ReadString(fp, "tracker", Config.Tracker, sizeof(Config.Tracker), 1);
	LogMessage("Tracker = '%s'\n", Config.Tracker);
	
	// Enable uploads
	ReadBoolean(fp, "EnableHabitat", 0, &Config.EnableHabitat);
	ReadBoolean(fp, "EnableSSDV", 0, &Config.EnableSSDV);
	
	// Enable logging
	ReadBoolean(fp, "LogTelemetry", 0, &Config.EnableTelemetryLogging);

	// Calling mode
	Config.CallingTimeout = ReadInteger(fp, "CallingTimeout", 0, 300);
	
	// LED allocations
	Config.NetworkLED = ReadInteger(fp, "NetworkLED", 0, -1);
	Config.InternetLED = ReadInteger(fp, "InternetLED", 0, -1);
	Config.LoRaDevices[0].ActivityLED = ReadInteger(fp, "ActivityLED_0", 0, -1);
	Config.LoRaDevices[1].ActivityLED = ReadInteger(fp, "ActivityLED_1", 0, -1);
	
	// Server Port
	Config.ServerPort = ReadInteger(fp, "ServerPort", 0, -1);	
	
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
			Config.LoRaDevices[Channel].AFC = FALSE;
			
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

			if (Config.LoRaDevices[Channel].SpeedMode == 5)
			{
				// Calling channel
				Config.LoRaDevices[Channel].ImplicitOrExplicit = EXPLICIT_MODE;
				Config.LoRaDevices[Channel].ErrorCoding = ERROR_CODING_4_8;
				Config.LoRaDevices[Channel].Bandwidth = BANDWIDTH_41K7;
				Config.LoRaDevices[Channel].SpreadingFactor = SPREADING_11;
				Config.LoRaDevices[Channel].LowDataRateOptimize = 0;
			}
			else if (Config.LoRaDevices[Channel].SpeedMode == 4)
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
			
			sprintf(Keyword, "AFC_%d", Channel);
			if (ReadBoolean(fp, Keyword, 0, &Temp))
			{
				if (Temp)
				{
					Config.LoRaDevices[Channel].AFC = TRUE;
					ChannelPrintf(Channel, 11, 24, "AFC");
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
	sscanf(Line+2, "%15[^,],%u,%8[^,],%lf,%lf,%u",
				Config.LoRaDevices[Channel].Payload,
				&Config.LoRaDevices[Channel].Counter,
				Config.LoRaDevices[Channel].Time,
				&Config.LoRaDevices[Channel].Latitude,
				&Config.LoRaDevices[Channel].Longitude,
				&Config.LoRaDevices[Channel].Altitude);
	
	// HAB->HAB_status = FieldCount == 6;
}

void DoPositionCalcs(int Channel)
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
	
	ChannelPrintf(Channel, 5, 1, "%8.5lf, %8.5lf, %05u   ", 
								Config.LoRaDevices[Channel].Latitude,
								Config.LoRaDevices[Channel].Longitude,
								Config.LoRaDevices[Channel].Altitude);
}

uint16_t CRC16(unsigned char *ptr)
{
    uint16_t CRC;
	int j;
	
    CRC = 0xffff;           // Seed
   
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

void ProcessKeyPress(int ch)
{
	int Channel = 0;

	/* shifted keys act on channel 1 */
	if (ch >= 'A' && ch <= 'Z')
	{
		Channel = 1;
		/* change from upper to lower case */
		ch += ('a' - 'A');
	}

	if (ch == 'q')
	{
		run = FALSE;
		return;
	}

	/* ignore if channel is not in use */
	if (!Config.LoRaDevices[Channel].InUse)
	{
		return;
	}

	switch(ch)
	{
		case 'f':
			Config.LoRaDevices[Channel].AFC = !Config.LoRaDevices[Channel].AFC;
			ChannelPrintf(Channel, 11, 24, "%s", Config.LoRaDevices[Channel].AFC?"AFC":"   ");
			break;
		case 'a':
			ReTune(Channel, 0.1);
			break;
		case 'z':
			ReTune(Channel, -0.1);
			break;
		case 's':
			ReTune(Channel, 0.01);
			break;
		case 'x':
			ReTune(Channel, -0.01);
			break;
		case 'd':
			ReTune(Channel, 0.001);
			break;
		case 'c':
			ReTune(Channel, -0.001);
			break;
		default:
			//LogMessage("KeyPress %d\n", ch);
			return;
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
	
	ChannelPrintf(Channel, 4, 1, "Calling message %d bytes ", strlen(Message));
													
	if (sscanf(Message+2, "%15[^,],%lf,%d,%d,%d,%d,%d,%*d",
						Payload,
						&Frequency,
						&ImplicitOrExplicit,
						&ErrorCoding,
						&Bandwidth,
						&SpreadingFactor,
						&LowDataRateOptimize) == 7)
	{
		if (Config.LoRaDevices[Channel].AFC)
		{
			double MasterFrequency;
			
			sscanf(Config.LoRaDevices[Channel].Frequency, "%lf", &MasterFrequency);
		
			Frequency += Config.LoRaDevices[Channel].activeFreq - MasterFrequency;
		}
		
		LogMessage("Ch %d: Calling message, new frequency %7.3lf\n", Channel, Frequency);
		
		// Decoded OK
		setMode(Channel, RF96_MODE_SLEEP);
		
		// setFrequency(Channel, Config.LoRaDevices[Channel].activeFreq + );
		setFrequency(Channel, Frequency);

		SetLoRaParameters(Channel, ImplicitOrExplicit, ErrorCoding, Bandwidth, SpreadingFactor, LowDataRateOptimize);
		
		setMode(Channel, RF96_MODE_RX_CONTINUOUS); 
		
		Config.LoRaDevices[Channel].InCallingMode = 1;
		
		// ChannelPrintf(Channel, 1, 1, "Channel %d %7.3lfMHz              ", Channel, Frequency);
	}
}
			
void ProcessTelemetryMessage(int Channel, char *Message)
{
	if (strlen(Message+1) < 150)
	{
		char *startmessage, *endmessage;

		ChannelPrintf(Channel, 4, 1, "Telemetry %d bytes       ", strlen(Message+1));

		endmessage = Message;
								
		startmessage = endmessage;
		endmessage = strchr(startmessage, '\n');

		if(endmessage == NULL)
                	endmessage = strrchr(startmessage, '*')+5;

		if (endmessage != NULL)
		{
			time_t now;
			struct tm *tm;
			
			*endmessage = '\0';

			LogTelemetryPacket(startmessage);
			
			UploadTelemetryPacket(startmessage);

			ProcessLine(Channel, startmessage);
		

			now = time(0);
			tm = localtime(&now);
		
			LogMessage("%02d:%02d:%02d Ch%d: %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, Channel, startmessage);
		}
		
		// DoPositionCalcs(Channel);
		
		Config.LoRaDevices[Channel].TelemetryCount++;								
	}
}

void ProcessSSDVMessage(int Channel, unsigned char *Message)
{
	// SSDV packet
	static uint32_t PreviousCallsignCode=0;
	static int PreviousImageNumber=-1, PreviousPacketNumber=0;
	uint32_t CallsignCode;
	char Callsign[7], *FileMode, *EncodedCallsign, *EncodedEncoding, *EncodedData, HexString[513];
	int ImageNumber, PacketNumber;
	char filename[100];
	FILE *fp;
	
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

	LogMessage("Ch%d: SSDV Packet, Callsign %s, Image %d, Packet %d, %d Missing\n", Channel, Callsign, Message[6], Message[7] * 256 + Message[8], Config.LoRaDevices[Channel].SSDVMissing);
	ChannelPrintf(Channel, 4, 1, "SSDV Packet            ");
	
	PreviousImageNumber = ImageNumber;
	PreviousPacketNumber = PacketNumber;
	PreviousCallsignCode = CallsignCode;

	// Save to file
	
	sprintf(filename, "/tmp/%s_%d.bin", Callsign, ImageNumber);

	if ((fp = fopen(filename, FileMode)))
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


int prog_count(char* name)
{
    DIR* dir;
    struct dirent* ent;
    char buf[512];
    long  pid;
    char pname[100] = {0,};
    char state;
    FILE *fp=NULL; 
	int Count=0;

    if (!(dir = opendir("/proc")))
	{
        perror("can't open /proc");
        return 0;
    }

    while((ent = readdir(dir)) != NULL)
	{
        long lpid = atol(ent->d_name);
        if (lpid < 0)
            continue;
        snprintf(buf, sizeof(buf), "/proc/%ld/stat", lpid);
        fp = fopen(buf, "r");

        if (fp)
		{
            if ((fscanf(fp, "%ld (%[^)]) %c", &pid, pname, &state)) != 3 )
			{
                printf("fscanf failed \n");
                fclose(fp);
                closedir(dir);
                return 0;
            }
			
            if (!strcmp(pname, name))
			{
                Count++;
            }
            fclose(fp);
        }
    }

	closedir(dir);
	
	return Count;
}

int main(int argc, char **argv)
{
	unsigned char Message[257];
	int Bytes, ch;
	uint32_t LoopCount[2];
	pthread_t SSDVThread, FTPThread, NetworkThread, HabitatThread, ServerThread;
	WINDOW * mainwin;
	int LEDCounts[2];
	
	if (prog_count("gateway") > 1)
	{
		printf("\nThe gateway program is already running!\n\n");
		exit(1);
	}

	mainwin = InitDisplay();
	
	// Settings for character input
	noecho();
	cbreak();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
		
	Config.LoRaDevices[0].InUse = 0;
	Config.LoRaDevices[1].InUse = 0;
	
	LEDCounts[0] = 0;
	LEDCounts[1] = 0;	
	
	// Remove any old SSDV files
	// system("rm -f /tmp/*.bin");	
	
	// Default pin allocations

	Config.LoRaDevices[0].DIO0 = 6;
	Config.LoRaDevices[0].DIO5 = 5;
	
	Config.LoRaDevices[1].DIO0 = 27;
	Config.LoRaDevices[1].DIO5 = 26;
	
	LoadConfigFile();
	LoadPayloadFiles();
	
	if (wiringPiSetup() < 0)
	{
		fprintf(stderr, "Failed to open wiringPi\n");
		exit(1);
	}
	
	if (Config.LoRaDevices[0].ActivityLED >= 0) pinMode(Config.LoRaDevices[0].ActivityLED, OUTPUT);
	if (Config.LoRaDevices[1].ActivityLED >= 0) pinMode(Config.LoRaDevices[1].ActivityLED, OUTPUT);
	if (Config.InternetLED >= 0) pinMode(Config.InternetLED, OUTPUT);
	if (Config.NetworkLED >= 0) pinMode(Config.NetworkLED, OUTPUT);
	
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

	if (pthread_create(&FTPThread, NULL, FTPLoop, NULL))
	{
		fprintf(stderr, "Error creating FTP thread\n");
		return 1;
	}

	if (pthread_create(&HabitatThread, NULL, HabitatLoop, NULL))
	{
		fprintf(stderr, "Error creating Habitat thread\n");
		return 1;
	}
	
	if (Config.ServerPort > 0)
	{
		if (pthread_create(&ServerThread, NULL, ServerLoop, NULL))
		{
			fprintf(stderr, "Error creating server thread\n");
			return 1;
		}
	}

	if ((Config.NetworkLED >= 0) && (Config.InternetLED >= 0))
	{
		if (pthread_create(&NetworkThread, NULL, NetworkLoop, NULL))
		{
			fprintf(stderr, "Error creating Network thread\n");
			return 1;
		}
	}

	while (run)
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
						if (Config.LoRaDevices[Channel].ActivityLED >= 0)
						{
							digitalWrite(Config.LoRaDevices[Channel].ActivityLED, 1);
							LEDCounts[Channel] = 5;
						}
						// LogMessage("Channel %d data available - %d bytes\n", Channel, Bytes);
						// LogMessage("Line = '%s'\n", Message);

						if (Message[1] == '!')
						{
							ProcessUploadMessage(Channel, (char *) Message+1);
						}
						else if (Message[1] == '^')
						{
							ProcessCallingMessage(Channel, (char *) Message+1);
						}
						else if (Message[1] == '$')
						{
							ProcessTelemetryMessage(Channel, (char *) Message+1);
						}
						else if (Message[1] == 'L')
						{
							ProcessTelemetryMessage(Channel, (char *) Message+7);
						}
						else if (Message[1] == 0x66)
						{
							ProcessSSDVMessage(Channel, Message);
						}
						else
						{
							LogMessage("Unknown packet type is %02Xh, RSSI %d\n", Message[1], readRegister(Channel, REG_PACKET_RSSI) - 157);
							ChannelPrintf(Channel, 4, 1, "Unknown Packet %d, %d bytes", Message[0], Bytes);
							Config.LoRaDevices[Channel].UnknownCount++;
						}
						
						Config.LoRaDevices[Channel].LastPacketAt = time(NULL);
						
						if (Config.LoRaDevices[Channel].InCallingMode && (Config.CallingTimeout > 0))
						{
							Config.LoRaDevices[Channel].ReturnToCallingModeAt = time(NULL) + Config.CallingTimeout;
						}
						

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
						ChannelPrintf(Channel, 6, 1, "%us since last packet   ", (unsigned int)(time(NULL) - Config.LoRaDevices[Channel].LastPacketAt));
					}
					
					if (Config.LoRaDevices[Channel].InCallingMode && (Config.CallingTimeout > 0) && (Config.LoRaDevices[Channel].ReturnToCallingModeAt > 0) && (time(NULL) > Config.LoRaDevices[Channel].ReturnToCallingModeAt))
					{
						Config.LoRaDevices[Channel].InCallingMode = 0;
						Config.LoRaDevices[Channel].ReturnToCallingModeAt = 0;
						
						LogMessage("Return to calling mode\n");
						// setMode(Channel, RF96_MODE_SLEEP);
						
						// setFrequency(Channel, Frequency);

						// SetLoRaParameters(Channel, ImplicitOrExplicit, ErrorCoding, Bandwidth, SpreadingFactor, LowDataRateOptimize);
						
						// setMode(Channel, RF96_MODE_RX_CONTINUOUS); 
					
						setLoRaMode(Channel);

						SetDefaultLoRaParameters(Channel);
		
						setMode(Channel, RF96_MODE_RX_CONTINUOUS); 
						
						ChannelPrintf(Channel, 1, 1, "Channel %d %sMHz  %s mode", Channel, Config.LoRaDevices[Channel].Frequency, Modes[Config.LoRaDevices[Channel].SpeedMode]);
					}
						 
					if ((ch = getch()) != ERR)
					{
						ProcessKeyPress(ch);
					}
					
					if (LEDCounts[Channel] && (Config.LoRaDevices[Channel].ActivityLED >= 0))
					{
						if (--LEDCounts[Channel] == 0)
						{
							digitalWrite(Config.LoRaDevices[Channel].ActivityLED, 0);
						}
					}
				}
			}
		}
		// delay(5);
 	}

	CloseDisplay(mainwin);
	
	if (Config.NetworkLED >= 0) digitalWrite(Config.NetworkLED, 0);
	if (Config.InternetLED >= 0) digitalWrite(Config.InternetLED, 0);
	if (Config.LoRaDevices[0].ActivityLED >= 0) digitalWrite(Config.LoRaDevices[0].ActivityLED, 0);
	if (Config.LoRaDevices[1].ActivityLED >= 0) digitalWrite(Config.LoRaDevices[1].ActivityLED, 0);	
	
	return 0;
}

