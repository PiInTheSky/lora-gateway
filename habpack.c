#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

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
        && cmp_object_to_str(&cmp, &item, str, str_max_length)) \
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

void Habpack_telem_store_field(received_t *Received, uint64_t map_id, char *field_name, int64_t *ptr_integer, double *ptr_real, char *ptr_string)
{
    habpack_telem_linklist_entry_t *walk_ptr, *last_ptr;

    if(Received->Telemetry.habpack_extra == NULL)
    {
        /* First entry */
        Received->Telemetry.habpack_extra = malloc(sizeof(habpack_telem_linklist_entry_t));
        walk_ptr = Received->Telemetry.habpack_extra;
    }
    else
    {
        /* Not first entry, so walk */
        last_ptr = Received->Telemetry.habpack_extra;
        while(last_ptr->next != NULL)
        {
            last_ptr = last_ptr->next;
        }
        last_ptr->next = malloc(sizeof(habpack_telem_linklist_entry_t));
        walk_ptr = last_ptr->next;
    }

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
    else if(ptr_string != NULL)
    {
        walk_ptr->value_type = HB_VALUE_STRING;
        walk_ptr->value.string = strdup(ptr_string);
    }
    walk_ptr->next = NULL;
}


#define HABPACK_TELEM_STORE_AS_INT64(value) \
    ts64 = (int64_t)value; \
    Habpack_telem_store_field( \
        Received, \
        map_id, \
        field_name, \
        &ts64, \
        NULL, \
        NULL  \
    )

#define HABPACK_TELEM_STORE_AS_DOUBLE(value) \
    tf64 = (double)value; \
    Habpack_telem_store_field( \
        Received, \
        map_id, \
        field_name, \
        NULL, \
        &tf64, \
        NULL  \
    )

#define HABPACK_TELEM_STORE_AS_STRING(value) \
    Habpack_telem_store_field( \
        Received, \
        map_id, \
        field_name, \
        NULL, \
        NULL, \
        value  \
    )

void Habpack_Telem_UKHAS_String(received_t *Received, char *ukhas_string, uint32_t max_length)
{
    uint16_t crc;
    uint32_t str_index;
    habpack_telem_linklist_entry_t *walk_ptr;

    /* $,CALLSIGN */
    str_index = snprintf(
        ukhas_string,
        max_length,
        "$$"
    );

    /* All other fields */
    walk_ptr = Received->Telemetry.habpack_extra;
    while(walk_ptr != NULL)
    {
        if(walk_ptr->value_type == HB_VALUE_INTEGER)
        {
            if(strcmp(walk_ptr->name, "time") == 0)
            {
                /* Convert time, urgh */
                uint64_t temp;
                temp = walk_ptr->value.integer % SECONDS_IN_A_DAY;
                int hours = temp/(60*60);
                temp -= (hours*60*60);
                int mins = (temp/60);
                int secs = temp-mins*60;

                str_index += snprintf(
                    &ukhas_string[str_index],
                    (max_length - str_index),
                    "%02d:%02d:%02d,",
                    hours,mins,secs
                );
            }
            else
            {
                str_index += snprintf(
                    &ukhas_string[str_index],
                    (max_length - str_index),
                    "%"PRId64",",
                    walk_ptr->value.integer
                );
            }
        }
        else if(walk_ptr->value_type == HB_VALUE_REAL)
        {
            str_index += snprintf(
                &ukhas_string[str_index],
                (max_length - str_index),
                "%f,",
                walk_ptr->value.real
            );
        }
        else if(walk_ptr->value_type == HB_VALUE_STRING)
        {
            str_index += snprintf(
                &ukhas_string[str_index],
                (max_length - str_index),
                "%s,",
                walk_ptr->value.string
            );
        }

        walk_ptr = walk_ptr->next;
    }

    /* Replace last comma with null */
    ukhas_string[--str_index] = '\0';

    crc = CRC16((unsigned char *)&ukhas_string[2]);
    snprintf(&ukhas_string[str_index],
        (max_length - str_index),
        "*%04x\n",
        crc
    );
}

void Habpack_Telem_JSON(received_t *Received, char *json_string, uint32_t max_length)
{
    uint32_t str_index;
    habpack_telem_linklist_entry_t *walk_ptr;

    str_index = snprintf(
        json_string,
        max_length,
        "{\"type\":\"PAYLOAD_TELEMETRY\""
    );

    /* All other fields */
    walk_ptr = Received->Telemetry.habpack_extra;
    while(walk_ptr != NULL)
    {
        str_index += snprintf(
            &json_string[str_index],
            (max_length - str_index),
            ",\"%s\":",
            walk_ptr->name
        );
        if(walk_ptr->value_type == HB_VALUE_INTEGER)
        {
            str_index += snprintf(
                &json_string[str_index],
                (max_length - str_index),
                "%"PRId64,
                walk_ptr->value.integer
            );
        }
        else if(walk_ptr->value_type == HB_VALUE_REAL)
        {
            str_index += snprintf(
                &json_string[str_index],
                (max_length - str_index),
                "%f",
                walk_ptr->value.real
            );
        }
        else if(walk_ptr->value_type == HB_VALUE_STRING)
        {
            str_index += snprintf(
                &json_string[str_index],
                (max_length - str_index),
                "\"%s\"",
                walk_ptr->value.string
            );
        }

        walk_ptr = walk_ptr->next;
    }

    str_index += snprintf(
        &json_string[str_index],
        (max_length - str_index),
        "}"
    );
}

void Habpack_Telem_Destroy(received_t *Received)
{
    habpack_telem_linklist_entry_t *walk_ptr, *last_ptr;

    walk_ptr = Received->Telemetry.habpack_extra;
    while(walk_ptr != NULL)
    {
        if(walk_ptr->value_type == HB_VALUE_STRING)
        {
            free(walk_ptr->value.string);
        }

        last_ptr = walk_ptr;
        walk_ptr = walk_ptr->next;

        free(last_ptr);
    }
}


int Habpack_Process_Message(received_t *Received)
{
    cmp_ctx_t cmp;
    cmp_object_t item;
    uint32_t map_size;
    uint32_t array_size,array_size2;
    uint32_t i,j,k;

    uint64_t map_id;
    uint64_t tu64;
    int64_t ts64;
    float tf32;
    double tf64;
    uint32_t str_max_length = 255;
    char str[255];
    char field_name[32];
    
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
                strncpy(field_name, "callsign", 31);

                TRY_PARSE_STRING(
                    strncpy(Received->Telemetry.Callsign, str, 31);
                    HABPACK_TELEM_STORE_AS_STRING(str);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    snprintf(Received->Telemetry.Callsign, 31, "%lld", tu64);
                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;
                
                break;

            case HABPACK_SENTENCE_ID:
                strncpy(field_name, "sentence_id", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.SentenceId = tu64;
                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_TIME:
                strncpy(field_name, "time", 31);  
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    if(tu64 > SECONDS_IN_A_DAY)
                    {
                        /* Larger than 1 day, so must be unix epoch time */
                        Received->Telemetry.Time = tu64;
                        HABPACK_TELEM_STORE_AS_INT64(tu64);

                        /* Strip out date information. TODO: Find a good way of including this in the UKHAS ASCII */
                        tu64 -= (uint64_t)(tu64 / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
                    }
                    else
                    {
                        /* Smaller than 1 day, either we've gone over 88mph, or it's seconds since midnight today */
                        uint64_t unix_epoch_time;
                        unix_epoch_time = tu64 + (uint64_t)((uint64_t)time(NULL) / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
                        Received->Telemetry.Time = unix_epoch_time;
                        HABPACK_TELEM_STORE_AS_INT64(unix_epoch_time);
                    }

                    int hours = tu64/(60*60);
                    tu64 = tu64 - (hours*60*60);
                    int mins = (tu64/60);
                    int secs = tu64-mins*60;

                    snprintf(Received->Telemetry.TimeString, 9, "%02d:%02d:%02d",hours,mins,secs);
                )
                else return 0;

                break;

            case HABPACK_POSITION:
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
                        strncpy(field_name, "latitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);
                        
                        /* Longitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }

                        Received->Telemetry.Longitude = (double)ts64 / 10e6;
                        strncpy(field_name, "longitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);

                        /* Altitude (optional) - Signed Integer */
                        Received->Telemetry.Altitude = 0.0;
                        if(array_size == 3)
                        {
                            if(!(cmp_read_sinteger(&cmp, &ts64)))
                            {
                                return 0;
                            }

                            Received->Telemetry.Altitude = ts64;
                            strncpy(field_name, "altitude", 31);
                            HABPACK_TELEM_STORE_AS_INT64(ts64);
                        }
                    }
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_GNSS_SATELLITES:
                strncpy(field_name, "gnss_satellites", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_GNSS_LOCK:
                strncpy(field_name, "gnss_lock_type", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_VOLTAGE:
                strncpy(field_name, "voltage", 31);

                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for mV
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for mV
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "voltage%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for mV
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for mV
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_INTERNAL_TEMPERATURE:
                strncpy(field_name, "internal_temperature", 31);
                
                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "internal_temperature%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_EXTERNAL_TEMPERATURE:
                strncpy(field_name, "external_temperature", 31);
                
                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "external_temperature%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    else TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-degrees
                    )
                    TRY_PARSE_SIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 1000.0); // Scale for milli-degrees
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_PRESSURE:
                strncpy(field_name, "pressure", 31);
                
                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-bar
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "pressure%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-bar
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_RELATIVE_HUMIDITY:
                strncpy(field_name, "relative_humidity", 31);
                
                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tu64);
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "relative_humidity%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tu64);
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_ABSOLUTE_HUMIDITY:
                strncpy(field_name, "absolute_humidity", 31);
                
                TRY_PARSE_FLOAT(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-grams / m^3
                )
                else TRY_PARSE_ARRAY(
                    snprintf(field_name, 31, "absolute_humidity%d", j);

                    TRY_PARSE_FLOAT(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                    )
                    else TRY_PARSE_DOUBLE(
                        HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                    )
                    TRY_PARSE_UNSIGNED_INTEGER(
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)tu64 / 1000.0); // Scale for milli-grams / m^3
                    )
                    else return 0;
                )
                else return 0;

                break;

            case HABPACK_DOWNLINK_FREQUENCY:
                strncpy(field_name, "downlink_frequency", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->isCallingBeacon = true;
                    Received->Telemetry.DownlinkFrequency = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_DOWNLINK_LORA_MODE:
                strncpy(field_name, "downlink_lora_mode", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_DOWNLINK_LORA_IMPLICIT:
                strncpy(field_name, "downlink_lora_implicit", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraImplicit = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_DOWNLINK_LORA_ERRORCODING:
                strncpy(field_name, "downlink_lora_error_coding", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraErrorCoding = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

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

                break;

            case HABPACK_DOWNLINK_LORA_SPREADING:
                strncpy(field_name, "downlink_lora_spreading_factor", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraSpreadingFactor = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_DOWNLINK_LORA_LDO:
                strncpy(field_name, "downlink_lora_ldo_on", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    Received->Telemetry.DownlinkLoraMode = -1;
                    Received->Telemetry.DownlinkLoraLowDatarateOptimise = tu64;

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_UPLINK_COUNT:
                strncpy(field_name, "uplink_count", 31);
                
                TRY_PARSE_UNSIGNED_INTEGER(
                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else return 0;

                break;

            case HABPACK_PREDICTED_LANDING_TIME:
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
                )
                else return 0;

                break;

            case HABPACK_PREDICTED_LANDING_POSITION:                
                TRY_PARSE_ARRAY_AT_ONCE(
                    /* Check size of the array */
                    if(array_size == 2 || array_size == 3)
                    {
                        /* Latitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }

                        strncpy(field_name, "predicted_landing_latitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);
                        
                        /* Longitude - Signed Integer */
                        if (!(cmp_read_sinteger(&cmp, &ts64)))
                        {
                            return 0;
                        }

                        strncpy(field_name, "predicted_landing_longitude", 31);
                        HABPACK_TELEM_STORE_AS_DOUBLE((double)ts64 / 10e6);

                        /* Altitude (optional) - Signed Integer */
                        if(array_size == 3)
                        {
                            if(!(cmp_read_sinteger(&cmp, &ts64)))
                            {
                                return 0;
                            }

                            strncpy(field_name, "predicted_landing_altitude", 31);
                            HABPACK_TELEM_STORE_AS_INT64(ts64);
                        }
                    }
                    else return 0;
                )
                else return 0;

                break;

            default: // Unknown 
                TRY_PARSE_STRING(
                    snprintf(field_name, 31, "%lld", map_id);

                    HABPACK_TELEM_STORE_AS_STRING(str);
                )
                else TRY_PARSE_UNSIGNED_INTEGER(
                    snprintf(field_name, 31, "%lld", map_id);

                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                )
                else TRY_PARSE_SIGNED_INTEGER(
                    snprintf(field_name, 31, "%lld", map_id);

                    HABPACK_TELEM_STORE_AS_INT64(ts64);
                )
                else TRY_PARSE_FLOAT(
                    snprintf(field_name, 31, "%lld", map_id);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                )
                else TRY_PARSE_DOUBLE(
                    snprintf(field_name, 31, "%lld", map_id);

                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                )
                else TRY_PARSE_ARRAY_AT_ONCE(
                    for (j = 0; j < array_size; j++)
                    {
                        /* Read array element */
                        if(!cmp_read_object(&cmp, &item))
                            return 0;

                        TRY_PARSE_STRING(
                            snprintf(field_name, 31, "%lld.%d", map_id, j);
                            
                            HABPACK_TELEM_STORE_AS_STRING(str);
                        )
                        else TRY_PARSE_UNSIGNED_INTEGER(
                            snprintf(field_name, 31, "%lld.%d", map_id, j);

                            HABPACK_TELEM_STORE_AS_INT64(tu64);
                        )
                        else TRY_PARSE_SIGNED_INTEGER(
                            snprintf(field_name, 31, "%lld.%d", map_id, j);

                            HABPACK_TELEM_STORE_AS_INT64(ts64);
                        )
                        else TRY_PARSE_FLOAT(
                            snprintf(field_name, 31, "%lld.%d", map_id, j);

                            HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                        )
                        else TRY_PARSE_DOUBLE(
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
                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);
                                    
                                    HABPACK_TELEM_STORE_AS_STRING(str);
                                )
                                else TRY_PARSE_UNSIGNED_INTEGER(
                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);

                                    HABPACK_TELEM_STORE_AS_INT64(tu64);
                                )
                                else TRY_PARSE_SIGNED_INTEGER(
                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);

                                    HABPACK_TELEM_STORE_AS_INT64(ts64);
                                )
                                else TRY_PARSE_FLOAT(
                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);

                                    HABPACK_TELEM_STORE_AS_DOUBLE(tf32);
                                )
                                else TRY_PARSE_DOUBLE(
                                    snprintf(field_name, 31, "%lld.%d.%d", map_id, j, k);

                                    HABPACK_TELEM_STORE_AS_DOUBLE(tf64);
                                )
                                else return 0;
                            }
                        )
                        else return 0;
                    }
                )
                break;
        }
    }

    Habpack_Telem_UKHAS_String(Received, Received->UKHASstring, 255);
    
    return 1;
}