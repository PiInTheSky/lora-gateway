#ifndef _H_Gateway
#define _H_Gateway

#include "global.h"

int receiveMessage( int Channel, char *message, rx_metadata_t *Metadata );
void hexdump_buffer( const char *title, const char *buffer,
                     const int len_buffer );
void LogPacket( rx_metadata_t *Metadata, int Bytes, unsigned char MessageType );
void LogTelemetryPacket(int Channel, char *Telemetry);
void LogError(int ErrorCode, char *Message1, char *Message2);
void LogMessage( const char *format, ... );
void ChannelPrintf( int Channel, int row, int column, const char *format,
                    ... );
int ValidCRC16(char *ptr);					
void displayChannel (int Channel); 

#endif
