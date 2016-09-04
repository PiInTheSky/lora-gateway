#include <stdint.h>

void build_decoding_table(  );
char *base64_encode( const char *data, size_t input_length,
                     size_t * output_length, char *encoded_data );
char *base64_decode( const char *data, size_t input_length,
                     size_t * output_length );
void base64_cleanup(  );
