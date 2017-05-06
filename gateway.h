#ifndef _H_Gateway
#define _H_Gateway

int receiveMessage( int Channel, char *message );
void hexdump_buffer( const char *title, const char *buffer,
                     const int len_buffer );
void LogPacket( int Channel, int8_t SNR, int RSSI, double FreqError,
                int Bytes, unsigned char MessageType );
void LogTelemetryPacket( char *Telemetry );
void LogMessage( const char *format, ... );
void ChannelPrintf( int Channel, int row, int column, const char *format,
                    ... );
void displayChannel( int Channel );
void toggleMode( int Channel );

#endif
