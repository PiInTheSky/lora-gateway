#ifndef __HABPACK_H__
#define __HABPACK_H__

#include "global.h"

/* Core Telemetry */
#define HABPACK_CALLSIGN            0
#define HABPACK_SENTENCE_ID         1
#define HABPACK_TIME                2
#define HABPACK_POSITION            3
#define HABPACK_GNSS_SATELLITES     4
#define HABPACK_GNSS_LOCK           5
#define HABPACK_VOLTAGE             6

/* Environmental Telemetry */
#define HABPACK_INTERNAL_TEMPERATURE    10
#define HABPACK_EXTERNAL_TEMPERATURE    11
#define HABPACK_PRESSURE    12
#define HABPACK_RELATIVE_HUMIDITY       13
#define HABPACK_ABSOLUTE_HUMIDITY       14

/* Calling Beacon */
#define HABPACK_DOWNLINK_FREQUENCY          20
/* Common LoRa Mode */
#define HABPACK_DOWNLINK_LORA_MODE          21
/* _or_ Custom LoRa Parameters */
#define HABPACK_DOWNLINK_LORA_IMPLICIT      22
#define HABPACK_DOWNLINK_LORA_ERRORCODING   23
#define HABPACK_DOWNLINK_LORA_BANDWIDTH     24
#define HABPACK_DOWNLINK_LORA_SPREADING     25
#define HABPACK_DOWNLINK_LORA_LDO           26

/* Uplink */
#define HABPACK_UPLINK_COUNT                30

/* Landing Prediction */
#define HABPACK_PREDICTED_LANDING_TIME      40
#define HABPACK_PREDICTED_LANDING_POSITION  41

/* Multi-Position. TODO: Parsing unimplemented */
#define HABPACK_MULTI_POS_POSITION_SCALING  60
#define HABPACK_MULTI_POS_ALTITUDE_SCALING  61
#define HABPACK_MULTI_POS_ARRAY             62

int Habpack_Process_Message(received_t *Received);
void Habpack_Telem_Destroy(received_t *Received);

#endif /* __HABPACK_H__ */