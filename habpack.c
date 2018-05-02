#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "habpack.h"
#include "global.h"
#include "base64.h"

#include "cmp.h"

extern void LogMessage( const char *format, ... );

#define SECONDS_IN_A_DAY    86400

static int hb_buf_ptr = 0;
static int hb_ptr_max = 20;

static bool file_reader(cmp_ctx_t *ctx, void *data, size_t limit)
{
    uint32_t i;

    if (hb_buf_ptr + limit >= hb_ptr_max)
        return false;

    for(i = 0; i < limit; i++)
    {
        *(uint8_t *)data = ((uint8_t *)(ctx->buf))[hb_buf_ptr];
        data++;
        hb_buf_ptr++;
    }

    return true;
}

static bool file_skipper(struct cmp_ctx_s *ctx, size_t count)
{
    if (hb_buf_ptr + count >= hb_ptr_max)
        return false;

    hb_buf_ptr += count;

    return true;
}

static size_t file_writer(cmp_ctx_t *ctx, const void *data, size_t count)
{
    uint32_t i;
    
    if (hb_buf_ptr + count >= hb_ptr_max)
        return 0;

    for (i = 0; i < count; i++)
    {
        ((char*)ctx->buf)[hb_buf_ptr] = *((uint8_t*)data);
        data++;
        hb_buf_ptr++;
    }

    return count;
}

static uint16_t CRC16(unsigned char *ptr)
{
    uint16_t CRC;
    int j;

    CRC = 0xffff;

    for ( ; *ptr; ptr++ )
    {
        CRC ^= ( ( ( unsigned int ) *ptr ) << 8 );
        for ( j = 0; j < 8; j++ )
        {
            if ( CRC & 0x8000 )
                CRC = ( CRC << 1 ) ^ 0x1021;
            else
                CRC <<= 1;
        }
    }

    return CRC;
}

#define TRY_PARSE_STRING(processing)    \
    if(cmp_object_is_str(&item) \
        && cmp_object_to_str(&cmp, &item, &temp[temp_ptr], (uint32_t)(temp_remaining))) \
    { \
        processing \
    }

#define TRY_PARSE_FLOAT(processing)     \
    if(cmp_object_is_float(&item) \
        && cmp_object_as_float(&item, &tf32)) \
    { \
        processing \
    }

#define TRY_PARSE_DOUBLE(processing)    \
    if(cmp_object_is_double(&item) \
        && cmp_object_as_double(&item, &tf64)) \
    { \
        processing \
    }

#define TRY_PARSE_UNSIGNED_INTEGER(processing)  \
    if(cmp_object_is_uinteger(&item) \
        && cmp_object_as_uinteger(&item, &tu64)) \
    { \
        processing \
    }

#define TRY_PARSE_SIGNED_INTEGER(processing)    \
    if(cmp_object_is_sinteger(&item) \
        && cmp_object_as_sinteger(&item, &ts64)) \
    { \
        processing \
    }

#define TRY_PARSE_ARRAY(processing) \
    if(cmp_object_is_array(&item) \
        && cmp_object_as_array(&item, &array_size)) \
    { \
        for (j = 0; j < array_size; j++) \
        { \
            /* Read array element */ \
            if(!cmp_read_object(&cmp, &item)) \
                return 0; \
            \
            processing \
        } \
    }

#define TRY_PARSE_ARRAY_AT_ONCE(processing) \
    if(cmp_object_is_array(&item) \
        && cmp_object_as_array(&item, &array_size)) \
    { \
        processing \
    }

#define TRY_PARSE_ARRAY_AT_ONCE_NESTED(processing) \
    if(cmp_object_is_array(&item) \
        && cmp_object_as_array(&item, &array_size2)) \
    { \
        processing \
    }

void Habpack_telem_store_field(habpack_telem_linklist_entry_t *initial_entry, uint64_t map_id, char *field_name, int64_t *ptr_integer, double *ptr_real)
{
    habpack_telem_linklist_entry_t *walk_ptr;

    walk_ptr = initial_entry;
    while(walk_ptr != NULL)
    {
        walk_ptr = walk_ptr->next;
    }

    walk_ptr = malloc(sizeof(habpack_telem_linklist_entry_t));

    walk_ptr->type = map_id;
    strncpy(walk_ptr->name, field_name, 31);
    if(ptr_integer != NULL)
    {
        walk_ptr->value_type = HB_VALUE_INTEGER;
        walk_ptr->value.integer = *ptr_integer;
    }
    else if(ptr_real != NULL)
    {
        walk_ptr->value_type = HB_VALUE_REAL;
        walk_ptr->value.real = *ptr_real;
    }
    walk_ptr->next = NULL;
}


#define HABPACK_TELEM_STORE_AS_INT64(value) \
    ts64 = (int64_t)value; \
    Habpack_telem_store_field( \
        Received->Telemetry.habpack_extra, \
        map_id, \
        field_name, \
        &ts64, \
        NULL \
    )

#define HABPACK_TELEM_STORE_AS_DOUBLE(value) \
    tf64 = (double)value; \
    Habpack_telem_store_field( \
        Received->Telemetry.habpack_extra, \
        map_id, \
        field_name, \
        NULL, \
        &tf64 \
    )

void Habpack_Telem_Destroy(received_t *Received)
{
    habpack_telem_linklist_entry_t *walk_ptr, *last_ptr;

    walk_ptr = Received->Telemetry.habpack_extra;
    while(walk_ptr != NULL)
    {
        last_ptr = walk_ptr;
        walk_ptr = walk_ptr->next;

        free(last_ptr);
    }
}


int Habpack_Process_Message(received_t *Received)
{
    cmp_ctx_t cmp;
    uint32_t map_size;
    uint32_t array_size,array_size2;
    uint32_t i,j,k;

    uint64_t map_id;
    uint64_t tu64;
    int64_t ts64;
    float tf32;
    double tf64;
    char field_name[32];
    
    //When converting habpack to $$uhkas, the habpack fields should be ordered.
    //This stores the output prior to reordering
    char temp[1024];
    memset(temp,0,sizeof(char)*1024);
    int temp_ptr = 0;
    int temp_remaining = 1024-temp_ptr;
    int out_ptr = 0;
    
    uint8_t keys[128];
    uint16_t key_val[128];
    uint8_t key_ptr = 0;
    cmp_object_t item;
    
    cmp_init(&cmp, (void*)Received->Message, file_reader, file_skipper, file_writer);

    hb_buf_ptr = 0;
    hb_ptr_max = 256;

    Received->isCallingBeacon = false;
    
    /* Open MessagePack Map */
    if (!cmp_read_map(&cmp, &map_size))
        return 0;
    
    /* Loop through each element of the Map */
    for (i=0; i < map_size; i++)
    {
        /* Read Map ID (aka HABpack field ID) */
        if (!(cmp_read_uinteger(&cmp, &map_id)))
            return 0;

        /* Read HABpack field object */
        if(!cmp_read_object(&cmp, &item))
            return 0;
        
        /* Parse according to Map ID (aka HABpack field ID) */
        switch(map_id)
        {
            case HABPACK_CALLSIGN:
                key_val[key_ptr] = temp_ptr;                
                keys[key_ptr++] = map_id;

                temp_remaining = 1024-temp_ptr;

                TRY_PARSE_STRING(
                    strncpy(Received->Telemetry.Callsign, &temp[temp_ptr], 31);

                    while(temp[temp_ptr])
                        temp_ptr++;
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    snprintf(Received->Telemetry.Callsign, 31, "%lld", tu64);

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);
                )
                else return 0;
                
                temp_ptr++;
                break;

            case HABPACK_SENTENCE_ID:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.SentenceId = tu64;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_TIME:
                key_val[key_ptr] = temp_ptr;            
                keys[key_ptr++] = map_id;    
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    if(tu64 > SECONDS_IN_A_DAY)
                    {
                        /* Larger than 1 day, so must be unix epoch time */
                        Received->Telemetry.Time = tu64;

                        /* Strip out date information. TODO: Find a good way of including this in the UKHAS ASCII */
                        tu64 -= (uint64_t)(tu64 / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
                    }
                    else
                    {
                        /* Smaller than 1 day, either we've gone over 88mph, or it's seconds since midnight today */
                        Received->Telemetry.Time = tu64 + (uint64_t)((uint64_t)time(NULL) / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
                    }

                    int hours = tu64/(60*60);
                    tu64 = tu64 - (hours*60*60);
                    int mins = (tu64/60);
                    int secs = tu64-mins*60;

                    snprintf(Received->Telemetry.TimeString, 9, "%02d:%02d:%02d",hours,mins,secs);

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%02d:%02d:%02d",hours,mins,secs);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_POSITION:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                
                TRY_PARSE_ARRAY_AT_ONCE(
                    /* Check size of the array */
                    if(array_size == 2 || array_size == 3)
                    {
                        /* Latitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }

                        Received->Telemetry.Latitude = (double)ts64 / 10e6;

                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld,",ts64);
                        
                        /* Longitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }

                        Received->Telemetry.Longitude = (double)ts64 / 10e6;

                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld,",ts64);

                        /* Altitude (optional) - Signed Integer */
                        Received->Telemetry.Altitude = 0.0;
                        if(array_size == 3)
                        {
                            if(!(cmp_read_sinteger(&cmp, &ts64)))
                            {
                                return 0;
                            }

                            Received->Telemetry.Altitude = ts64;

                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);
                        }
                    }
                    else return 0;
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_GNSS_SATELLITES:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "gnss_satellites", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_GNSS_LOCK:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "gnss_lock_type", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_VOLTAGE:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "voltage", 31);

                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for mV
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for mV
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "voltage%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for mV
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for mV
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_INTERNAL_TEMPERATURE:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "internal_temperature", 31);
                
                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "internal_temperature%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_EXTERNAL_TEMPERATURE:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "external_temperature", 31);
                
                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "external_temperature%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_PRESSURE:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "pressure", 31);
                
                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-bar
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "pressure%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-bar
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_RELATIVE_HUMIDITY:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "relative_humidity", 31);
                
                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tu64);
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "relative_humidity%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tu64);
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_ABSOLUTE_HUMIDITY:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "absolute_humidity", 31);
                
                TRY_PARSE_FLOAT(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-grams / m^3
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "absolute_humidity%d", j);

                    TRY_PARSE_FLOAT(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-grams / m^3
                    )
                    else return 0;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_FREQUENCY:
                strncpy(field_name, "downlink_frequency", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->isCallingBeacon = true;
                    Received->Telemetry.DownlinkFrequency = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_MODE:
                strncpy(field_name, "downlink_lora_mode", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_IMPLICIT:
                strncpy(field_name, "downlink_lora_implicit", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraImplicit = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_ERRORCODING:
                strncpy(field_name, "downlink_lora_error_coding", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraErrorCoding = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_BANDWIDTH:
                strncpy(field_name, "downlink_lora_bandwidth", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;

                    /* Convert integer ID to float bandwidth. Ref: https://ukhas.org.uk/communication:habpack */
                    switch(tu64)
                    {
                        case 0:
                            tf64 = 7.8;
                            break;
                        case 1:
                            tf64 = 10.4;
                            break;
                        case 2:
                            tf64 = 15.6;
                            break;
                        case 3:
                            tf64 = 20.8;
                            break;
                        case 4:
                            tf64 = 31.25;
                            break;
                        case 5:
                            tf64 = 41.7;
                            break;
                        case 6:
                            tf64 = 62.5;
                            break;
                        case 7:
                            tf64 = 125;
                            break;
                        case 8:
                            tf64 = 250;
                            break;
                        case 9:
                            tf64 = 500;
                            break;
                        default:
                            return 0;
                    }
                    Received->Telemetry.DownlinkLoraBandwidth = tf64;

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_SPREADING:
                strncpy(field_name, "downlink_lora_spreading_factor", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraSpreadingFactor = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_DOWNLINK_LORA_LDO:
                strncpy(field_name, "downlink_lora_ldo_on", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraLowDatarateOptimise = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_UPLINK_COUNT:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                strncpy(field_name, "uplink_count", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_PREDICTED_LANDING_TIME:
                key_val[key_ptr] = temp_ptr;            
                keys[key_ptr++] = map_id;  
                strncpy(field_name, "predicted_landing_time", 31);  
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    if(tu64 > SECONDS_IN_A_DAY)
                    {
                        /* Larger than 1 day, so must be unix epoch time */
                        HABPACK_TELEM_STORE_AS_INT64(tu64);

                        /* Strip out date information. TODO: Find a good way of including this in the UKHAS ASCII */
                        tu64 -= (uint64_t)(tu64 / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
                    }
                    else
                    {
                        /* Smaller than 1 day, either we've gone over 88mph, or it's seconds since midnight today */
                        HABPACK_TELEM_STORE_AS_INT64(tu64 + (uint64_t)((uint64_t)time(NULL) / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY);
                    }

                    int hours = tu64/(60*60);
                    tu64 = tu64 - (hours*60*60);
                    int mins = (tu64/60);
                    int secs = tu64-mins*60;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%02d:%02d:%02d",hours,mins,secs);
                )
                else return 0;

                temp_ptr++;
                break;

            case HABPACK_PREDICTED_LANDING_POSITION:
                key_val[key_ptr] = temp_ptr;
                keys[key_ptr++] = map_id;
                
                TRY_PARSE_ARRAY_AT_ONCE(
                    /* Check size of the array */
                    if(array_size == 2 || array_size == 3)
                    {
                        /* Latitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld,",ts64);

                        strncpy(field_name, "predicted_landing_latitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);
                        
                        /* Longitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }
                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld,",ts64);

                        strncpy(field_name, "predicted_landing_longitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);

                        /* Altitude (optional) - Signed Integer */
                        if(array_size == 3)
                        {
                            if(!(cmp_read_sinteger(&cmp, &ts64)))
                            {
                                return 0;
                            }
                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                            strncpy(field_name, "predicted_landing_altitude", 31);
                            HABPACK_TELEM_STORE_AS_INT64(ts64);
                        }
                    }
                    else return 0;
                )
                else return 0;

                temp_ptr++;
                break;

            default: // Unknown 
                TRY_PARSE_STRING(
                    key_val[key_ptr] = temp_ptr;                
                    keys[key_ptr++] = map_id;

                    temp_remaining = 1024-temp_ptr;
                    while(temp[temp_ptr])
                        temp_ptr++;

                    /* TODO: Store in habpack data list */

                    temp_ptr++;
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    key_val[key_ptr] = temp_ptr;
                    keys[key_ptr++] = map_id;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                    snprintf(field_name, 31, "%lld", map_id);
                    HABPACK_TELEM_STORE_AS_INT64(tu64);

                    temp_ptr++;
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    key_val[key_ptr] = temp_ptr;
                    keys[key_ptr++] = map_id;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                    snprintf(field_name, 31, "%lld", map_id);
                    HABPACK_TELEM_STORE_AS_INT64(ts64);

                    temp_ptr++;
                )
                else TRY_PARSE_FLOAT(
                    key_val[key_ptr] = temp_ptr;
                    keys[key_ptr++] = map_id;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                    snprintf(field_name, 31, "%lld", map_id);
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);

                    temp_ptr++;
                )
                else TRY_PARSE_DOUBLE(
                    key_val[key_ptr] = temp_ptr;
                    keys[key_ptr++] = map_id;

                    temp_remaining = 1024-temp_ptr;
                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                    snprintf(field_name, 31, "%lld", map_id);
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);

                    temp_ptr++;
                )
                else TRY_PARSE_ARRAY_AT_ONCE(
                    key_val[key_ptr] = temp_ptr;
                    keys[key_ptr++] = map_id;

                    for (j = 0; j < array_size; j++)
                    {
                        /* Read array element */
                        if(!cmp_read_object(&cmp, &item))
                            return 0;

                        TRY_PARSE_STRING(
                            while(temp[temp_ptr])
                                temp_ptr++;
                        )
                        else TRY_PARSE_UNSIGNED_INTEGER(
                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                            snprintf(field_name, 31, "%lld.%d", map_id, j);
                            HABPACK_TELEM_STORE_AS_INT64(tu64);
                        )
                        else TRY_PARSE_SIGNED_INTEGER(
                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                            snprintf(field_name, 31, "%lld.%d", map_id, j);
                            HABPACK_TELEM_STORE_AS_INT64(ts64);
                        )
                        else TRY_PARSE_FLOAT(
                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                            snprintf(field_name, 31, "%lld.%d", map_id, j);
                            HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                        )
                        else TRY_PARSE_DOUBLE(
                            temp_remaining = 1024-temp_ptr;
                            temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                            snprintf(field_name, 31, "%lld.%d", map_id, j);
                            HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                        )
                        else TRY_PARSE_ARRAY_AT_ONCE_NESTED(
                            for (k = 0; k < array_size2; k++)
                            {
                                /* Read array element */
                                if(!cmp_read_object(&cmp, &item))
                                    return 0;

                                TRY_PARSE_STRING(
                                    while(temp[temp_ptr])
                                        temp_ptr++;
                                )
                                else TRY_PARSE_UNSIGNED_INTEGER(
                                    temp_remaining = 1024-temp_ptr;
                                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",tu64);

                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);
                                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                                )
                                else TRY_PARSE_SIGNED_INTEGER(
                                    temp_remaining = 1024-temp_ptr;
                                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%lld",ts64);

                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);
                                    HABPACK_TELEM_STORE_AS_INT64(ts64);
                                )
                                else TRY_PARSE_FLOAT(
                                    temp_remaining = 1024-temp_ptr;
                                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf32);

                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);
                                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                                )
                                else TRY_PARSE_DOUBLE(
                                    temp_remaining = 1024-temp_ptr;
                                    temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,"%f",tf64);

                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);
                                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                                )
                                else return 0;

                                temp_remaining = 1024-temp_ptr;
                                temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                            }

                            /* Replace last comma with null */
                            temp[temp_ptr] = '\0';
                        )
                        else return 0;

                        temp_remaining = 1024-temp_ptr;
                        temp_ptr += snprintf(&temp[temp_ptr],temp_remaining,",");
                    }

                    /* Replace last comma with null */
                    temp[temp_ptr] = '\0';
                )
                break;
        }
    }
    
    uint16_t crc;

    if (key_ptr == 0)
        return 0;
    
    out_ptr += snprintf(Received->UKHASstring,UKHASstring_length,"$$");
    
    //now order the items into the output string
    
    int copy_ptr = 0;
    
    for (i=0; i < 128; i++) {
        for (j=0; j < key_ptr; j++) {
            if (keys[j] == i){
                copy_ptr = key_val[j];
                if (temp[copy_ptr]){
                    while((out_ptr < UKHASstring_length-1) && (temp[copy_ptr])){
                        Received->UKHASstring[out_ptr++] = temp[copy_ptr++];
                    }
                    Received->UKHASstring[out_ptr++] = ',';
                }
            }
        }
    }
    out_ptr--;    
    Received->UKHASstring[out_ptr] = '\0';
    
    /* Base64 encoding of original packet - TODO: Flag?
    size_t out_len;
    base64_encode(Received->Message, Received->Bytes, &out_len, &Received->UKHASstring[out_ptr]);
    out_ptr += out_len;
    Received->UKHASstring[out_ptr] = '\0';
    */
    
    // add checksum to keep everything happy 
    crc = CRC16((unsigned char *)&Received->UKHASstring[2]);
    snprintf(&Received->UKHASstring[out_ptr],UKHASstring_length-out_ptr-7,"*%04x\n",crc);
    
    return 1;
}