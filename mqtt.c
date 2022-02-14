#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>              // Standard input/output definitions
#include <string.h>             // String function definitions
#include <stdlib.h>             // String function definitions

#include "MQTTClient.h"

#include "global.h"
#include "lifo_buffer.h"

extern lifo_buffer_t MQTT_Upload_Buffer;

#define QOS         1
#define TIMEOUT     10000L

volatile MQTTClient_deliveryToken deliveredtoken;


void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // LogMessage("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;
    LogMessage("Message arrived\n");
    LogMessage("     topic: %s\n", topicName);
    LogMessage("   message: ");
    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    LogMessage("\nMQTT Connection lost\n");
    LogMessage("     cause: %s\n", cause);
}

bool UploadMQTTPacket(mqtt_connect_t * mqttConnection, received_t * t )
{
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    char address[256];
    sprintf(address, "tcp://%s:%s", mqttConnection->host,mqttConnection->port);

    MQTTClient_create(&client, address, mqttConnection->clientId,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = mqttConnection->user;
    conn_opts.password = mqttConnection->pass; 
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    // LogMessage("Attempting publication on host: %s\n",
	// address);
            //"on topic %s for client with ClientID: %s\n",
            //t->Message, address, mqttConnection->topic, mqttConnection->clientId);
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        LogMessage("MQTT: Failed to connect, return code %d\n", rc);
        return false;
    }
    pubmsg.payload = t->Message;
    pubmsg.payloadlen = strlen(t->Message);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    deliveredtoken = 0;
    MQTTClient_publishMessage(client, mqttConnection->topic, &pubmsg, &token);
    while(deliveredtoken != token);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return true;
}

void *MQTTLoop( mqtt_connect_t *mqttConnection )
{
    if ( Config.EnableMQTT )
    {
        received_t *dequeued_telemetry_ptr;

        // Keep looping until the parent quits
        while ( true )
        {
            dequeued_telemetry_ptr = lifo_buffer_waitpop(&MQTT_Upload_Buffer);

            if(dequeued_telemetry_ptr != NULL)
            {
                if(UploadMQTTPacket(mqttConnection, dequeued_telemetry_ptr ))
                {
                    free(dequeued_telemetry_ptr);
                }
                else
                {
                    if(!lifo_buffer_requeue(&MQTT_Upload_Buffer, dequeued_telemetry_ptr))
                    {
                        /* Requeue failed, drop packet */
                        free(dequeued_telemetry_ptr);
                    }
                }
	    }
	    else
	    {
                /* NULL returned: We've been asked to quit */
                /* Don't bother free()ing stuff, as application is quitting */
		break;
	    }
	}
    }

    return NULL;
}

