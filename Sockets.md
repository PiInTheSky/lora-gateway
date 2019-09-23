# LoRa Gateway Socket Interfaces #

The LoRa Gateway provides some socket interfaces, configurable in gateway.txt:

- ServerPort - TCP/IP server socket, allowing a single client.  Sends status and packet information in JSON format.  Also allows for gateway settings (e.g. frequency) to be polled or changed. 
- DataPort - TCP/IP server socket, allowing a single client.  Sends raw telemetry packets (e.g. $$...).
- UDPPort - UDP client broadcast socket, sending raw telemetry.
- OziPlotterPort - UDP client broadcast socket, sending basic telemetry reformatted as OziPlotter CSV format.
- OziMuxPort - UDP client broadcast socket, sending basic telemetry reformatted as OziMux JSON format.



## JSON Port ##

This port has several functions:

1. Sends telemetry packets, soon after they are received.
2. Sends RSSI, every 100ms
2. Accept single-word commands
3. Accept new settings
4. Accept messages to send to HAB

### 1. Telemetry Packets ###

These are JSON packets with these fields:

1. class = "POSN"
2. index = Payload index - from 0 up to max payloads (32 currently) minus 1.
3. channel = LoRa module channel (SPI channel) - 0 or 1
4. payload = payload ID
5. time = UTC time from payload
6. lat = Latitude from payload
7. lon = Longitude from payload
8. alt = Altitude from payload
9. rate = Calculated ascent (+ve) or descent (-ve) rate, as calculated by gateway
10. sentence = Raw sentence sent by payload, including CRC, excluding terminating LF.

Example:
 
    {"class":"POSN","index":0,"channel":1,"payload":"RTLS3","time":"14:59:07","lat":
    51.95068,"lon":-2.54703,"alt":59,"rate":0.0,"sentence":"$$RTLS3,1,14:59:07,51.95
    068,-2.54703,00059,0,0,11,34.7,26.5,987,35.7,0.50,0.00000,0.00000,0.0,0,None,92,
    0,0,0,0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0.0,0,0,0.0,0,0,0.0,0.0*FA61"}
    

### 2. RSSI Reporting ###

To Do ...

### 3. Commands ###

All commands are case-insensitive, and are terminated with a <LF>.

#### Settings command ####

Returns the current gateway settings.  Sample excerpt:

    {"class":"SET","set":"tracker","val":"M0RPI/5"}
    {"class":"SET","set":"EnableHabitat","val":0}
    {"class":"SET","set":"EnableSSDV","val":0}
    {"class":"SET","set":"LogTelemetry","val":1}
    {"class":"SET","set":"LogPackets","val":0}
    {"class":"SET","set":"CallingTimeout","val":60}
    
#### Save command ####

Saves the current gateway settings to gateway.txt.

### 4. New settings ###

General format is "[setting]=[new value]"

e.g.:

    tracker=M0RPI
    frequency_0=434.450


### 5 Uplink Messages ###

#### Send ####

Format is "send:[message]"
