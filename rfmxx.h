#ifndef _RFMXX_H
#define _RFMXX_H

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

typedef struct TLoRaMode {
    int ImplicitOrExplicit;
    int ErrorCoding;
    int Bandwidth;
    int SpreadingFactor;
    int LowDataRateOptimize;
    int BaudRate;
    char *Description;
} TLoRaMode;


TLoRaMode LoRaModes[] = {
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_20K8, SPREADING_11, 1, 60, "Telemetry"},    // 0: Normal mode for telemetry
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_20K8, SPREADING_6, 0, 1400, "SSDV"},    // 1: Normal mode for SSDV
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_62K5, SPREADING_8, 0, 2000, "Repeater"},    // 2: Normal mode for repeater network  
    {EXPLICIT_MODE, ERROR_CODING_4_6, BANDWIDTH_250K, SPREADING_7, 0, 8000, "Turbo"},   // 3: Normal mode for high speed images in 868MHz band
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_250K, SPREADING_6, 0, 16828, "TurboX"}, // 4: Fastest mode within IR2030 in 868MHz band
    {EXPLICIT_MODE, ERROR_CODING_4_8, BANDWIDTH_41K7, SPREADING_11, 0, 200, "Calling"}, // 5: Calling mode
    {IMPLICIT_MODE, ERROR_CODING_4_5, BANDWIDTH_41K7, SPREADING_6, 0, 2800, "Uplink"}   // 6: Uplink mode for 868
};

struct TConfig Config;

struct TBandwidth {
    int LoRaValue;
    double Bandwidth;
    char *ConfigString;
} Bandwidths[] =
{
    {
    BANDWIDTH_7K8, 7.8, "7K8"},
    {
    BANDWIDTH_10K4, 10.4, "10K4"},
    {
    BANDWIDTH_15K6, 15.6, "15K6"},
    {
    BANDWIDTH_20K8, 20.8, "20K8"},
    {
    BANDWIDTH_31K25, 31.25, "31K25"},
    {
    BANDWIDTH_41K7, 41.7, "41K7"},
    {
    BANDWIDTH_62K5, 62.5, "62K5"},
    {
    BANDWIDTH_125K, 125.0, "125K"},
    {
    BANDWIDTH_250K, 250.0, "250K"},
    {
    BANDWIDTH_500K, 500.0, "500K"}
};


struct TBinaryPacket {
    uint8_t PayloadIDs;
    uint16_t Counter;
    uint16_t BiSeconds;
    float Latitude;
    float Longitude;
    uint16_t Altitude;
};

#endif
