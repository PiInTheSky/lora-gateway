Habitat LoRa Gateway
====================

Part of the LoRa Balloon Tracking System

Runs on a Raspberry Pi with 1 or 2 RFM98HW modules attached to the SPI port.
Also works with other compatible HopeRF and Semtec LoRa devices.

Connections
===========

If you're making your own board for the Pi, connect the LoRa module(s) like so:

	LORA     PI
	----     --
	3.3V	3.3V Power
	GND		Ground
	MOSI	MOSI (pin 19)
	MISO	MISO (pin 21)
	NSS		CE0 (pin 24) (CE1 (pin 26) for 2nd module)
	SCK		SLCK
	DIO0	Wiring Pi 31 (Pin 28) (Wiring Pi 6 (pin 22) for 2nd module)
	DIO5	Wiring Pi 26 (Pin 32) (Wiring Pi 5 (pin 18) for 2nd module). Set to -1 if not connected.


SPI
===

Enable SPI in raspi-config


WiringPi
========

Raspberry Pi OS has yet to be updated with the latest version of Wiring Pi, so if you are using a Pi 4B then you must install Wiring Pi from source as follows:
	
	cd /tmp
	wget https://project-downloads.drogon.net/wiringpi-latest.deb
	sudo dpkg -i wiringpi-latest.deb

for other Pi models you can save some typing by dpoing this instead:

	sudo apt install wiringpi
	
	
Dependencies
============

	sudo apt-get install git libcurl4-openssl-dev libncurses5-dev ssdv


Gateway
=======

	cd ~ 
	git clone https://github.com/PiInTheSky/lora-gateway.git
	cd lora-gateway
	make
	cp gateway-sample.txt gateway.txt


Configuration
=============

The configuration is in the file gateway.txt.  Example:

	tracker=M0RPI
	EnableHabitat=N
	EnableSSDV=Y
	LogTelemetry=Y
	LogPackets=Y
	CallingTimeout=60
	JPGFolder=ssdv
	CallingTimeout=60
	ServerPort=6004
	Latitude=51.95023
	Longitude=-2.5445 
	Radio=LoRa RFM98W
	Antenna=868MHz Yagi
	
	frequency_0=434.347
	mode_0=1
	DIO0_0=31
	DIO5_0=26
	AFC_0=N
	
	frequency_1=434.475
	mode_1=5
	DIO0_1=6
	DIO5_1=5
	AFC_1=Y

The global options are:
	
	tracker=<callsign>.  This is whatever callsign you want to appear as on the tracking map and/or SSDV page.
	
	EnableHabitat=<Y/N>.  Enables uploading of telemetry packets to Habitat.
	
	EnableSSDV=<Y/N>.  Enables uploading of SSDV image packets to the SSDV server.
	
	EnableHABLink=<Y/N>.  Enables uploading of telemetry packets to the hab.link server.
	
	JPGFolder=<folder>.  Tells the gateway where to save local JPEG files built from incoming SSDV packets.
	
	LogTelemetry=<Y/N>.  Enables logging of telemetry packets (ASCII only at present) to telemetry.txt.	
	
	LogPackets=<Y/N>.  Enables logging of packet information (SNR, RSSI, length, type) to packets.txt.	
	
	SMSFolder=<folder>.  Tells the gateway to check for incoming SMS messages or tweets that should be sent to the tracker via the uplink.
	
	CallingTimeout=<seconds>.  Sets a timeout for returning to calling mode after a period with no received packets.
	
	ServerPort=<port>.  Opens a server socket which can have 1 client connected.  Sends JSON telemetry and status information to that client.
	
	HABPort=<port>.  Opens a server socket which can have 1 client connected.  Port is a raw data stream between gateway and HAB (e.g. for Telnet-like communications).  Note: The corresponding functionality at the tracker end has not been published.
	
	DataPort=<port>.  Opens a server socket which can have 1 client connected.  Sends raw telemetry (i.e. $$payload,....) to that client.
	
	HABTimeout=<ms>.  Timeout in case of no response from HAB to raw data uplink.
	
	HABChannel=<channel>.  Specifies LoRa channel (0 or 1) used for telnet-style communications.
	
	Latitude=<decimal position>
	Longitude=<decimal position>.  These let you tell the gateway your position, for uploading to habitat, so your listener icon appears on the map in the correct position.
	Radio=<radio make/model>.  Lets you specify your radio/board make/model or type. This appears on the map if your listener icon is clicked on.
	Antenna=<antenna make/model>.  Lets you specify your antenna make/model or type.  This appears on the map if your listener icon is clicked on.
	
	NetworkLED=<wiring pi pin>
	InternetLED=<wiring pi pin>
	ActivityLED_0=<wiring pi pin>
	ActivityLED_1=<wiring pi pin>.  These are used for LED status indicators. Useful for packaged gateways that don't have a monitor attached.


​	
and the channel-specific options are:
​	
​	frequency_<n>=<freq in MHz>.  This sets the frequency for LoRa module <n> (0 for first, 1 for second).  e.g. frequency_0=434.450
​	
	PPM_<n>=<Parts per million offset of LoRa module.
	
	AFC_<n>=<Y/N>.  Enables or disables automatic frequency control (retunes by the frequency error of last received packet).
	
	mode_<n>=<mode>.  Sets the "mode" for the selected LoRa module.  This offers a simple way of setting the various
					LoRa parameters (SF etc.) in one go.  The modes are:
					
					0 = (normal for telemetry)	Explicit mode, Error coding 4:8, Bandwidth 20.8kHz, SF 11, Low data rate optimize on
					1 = (normal for SSDV) 		Implicit mode, Error coding 4:5, Bandwidth 20.8kHz,  SF 6, Low data rate optimize off
					2 = (normal for repeater)	Explicit mode, Error coding 4:8, Bandwidth 62.5kHz,  SF 8, Low data rate optimize off
					3 = (normal for fast SSDV)	Explicit mode, Error coding 4:6, Bandwidth 250kHz,   SF 7, Low data rate optimize off
					4 = Test mode not for normal use.
					5 = (normal for calling mode)	Explicit mode, Error coding 4:8, Bandwidth 41.7kHz, SF 11, Low data rate optimize off
					
	SF_<n>=<Spreading Factor>  e.g. SF_0=7
	
	Bandwidth_<n>=<Bandwidth>.  e.g. Bandwidth_0=41K7.  Options are 7K8, 10K4, 15K6, 20K8, 31K25, 41K7, 62K5, 125K, 250K, 500K
	
	Implicit_<n>=<Y/N>.  e.g. Implicit_0=Y
	
	Coding_<n>=<error_coding>.  e.g. Coding_0=5 (4:5)
	
	lowopt_<n>=<Y/N>.  Enables or disables low data rate optimization.
	
	power_<n>=<power>.  This is the power setting used for uplinks.  Refer to the LoRa manual for details on setting this.  ** Only set values that are legal in your location (for EU see IR2030) **
	
	UplinkTime_0=<seconds>.  When to send any uplink messages, measured as seconds into each cycle.
	
	UplinkCycle_0=<seconds>.  Cycle time for uplinks.  First cycle starts at 00:00:00.  So for uplink time=2 and cycle=30, any transmissions will start at 2 and 32 seconds after each minute.

Lines are commented out with "#" at the start.

If the frequency_n line is commented out, then that channel is disabled.

The program now performs some checks to determine if each selected LoRa module is present or not.  If you see a message "RFM not found on Channel" then check the SPI settings/wiring for that channel.  If you see "DIO5 pin is misconfigured on Channel" then check the DIO5 setting/wiring.  There is no current check for DIO0; any problems with that pin will result in packets not being received. 


Uplinks
=======

The gateway can uplink messages to the tracker.  Currently this is restricted to time-based uplink slots using "UplinkTime" and "UplinkCycle".

The code uses Linux system time, so the gateway should ideally be using a GPS receiver the GPSD daemon.  NTP may prove sufficient however.

For uplinks to work, both UplinkTime and UplinkCycle have to be set for the appropriate channel.

There are currently two types of uplink supported:

	-	Uplink of messages from the "SMSFolder" folder.  For this to work, "SMSFolder" has to be defined and present.  The gateway will then check for "*.sms" files in that folder.
	-	Uplink of SSD packet re-send requests.  The gateway looks for an "uplink.txt" file in the gateway folder.  The file is created by an external Python script (supplied) which interrogates the SSDV server.


Calling Mode
============

It is possible for trackers to send out messages on a special "calling channel" as well as telemetry on their main frequency.  The calling channel messages state the main frequency and LoRa modes.

This allows for gateways tp be normally left on the calling channel, so they then switch to each tracker as it comes within range.

There's nothing special about "calling mode" except that after a period (CallingTimeout seconds) of time without packets, the gateway returns to its default settings.

To enable calling mode, set the LoRa mode to 5, and the frequency to 433.650MHz.


Interactive use
===============

Run with:

	sudo ./gateway

Install as a Service
====================

Procserv can be used to run the gateway as a background service.

First edit hab-lora-gateway.conf to reflect the path to your installation, then:

	1. sudo cp hab-lora-gateway.conf /etc/
	2. sudo cp hab-lora-gateway /etc/init.d/
	3. sudo apt-get install procserv telnet
	4. sudo update-rc.d hab-lora-gateway defaults
	5. sudo service hab-lora-gateway start

The gateway script can now be reached using telnet:

	telnet localhost 50100

Display
=======

The display has a title bar at the top, scrolling log at the bottom, and 2 channel panels in the middle.  Each panel shows something like:

	Channel 0 869.8500MHz
	Explicit, 250k, SF7, EC4:6
	Telemetry 74 bytes
	51.95028, -2.54443, 00138
	Habitat        SSDV 0000
	0s since last packet
	Telem Packets = 37
	Image Packets = 0
	Bad CRC = 0 Bad Type = 0
	Packet SNR = 10, RSSI = -67
	Freq. Error =   1.0kHz
	Current RSSI =  -64

The "Habitat" text appears during uploads to habitat.  Normally it will flash up then disappear quickly; if it stays on (not flickering) then the upload is slow.

The "SSDV 0000" text shows the current state of the SSDV upload buffers.  There are 4 upload threads and each can handle up to 16 (0-9-A-F) packets in its queue.
Normally, even with fast SSDV, uplinks should happen quickly enough for there to be no more than 1 or 2 active threads each with 1 packet being uploaded.


Interactive Features
====================

The following key presses are available. Where appropriate unshifted keys affect Channel 0 and shifted keys affect Channel 1.
Many thanks to David Brooke for coding this feature and the AFC.

	q	quit
	
	a	increase frequency by 100kHz
	z	decrease frequency by 100kHz
	s	increase frequency by 10kHz
	x	decrease frequency by 10kHz
	d	increase frequency by 1kHz
	c	decrease frequency by 1kHz
	
	f	toggle AFC

Change History
==============

19/03/2021 - V1.8.43mbb
--------------------
	Added radio setting to gateway.txt

07/03/2021 - V1.8.43
--------------------

	Fixed erors in setting implicit/explicit bit on config register when sent from JSON client.  Thanks to Alan Hall for spotting this one.

23/02/2021 - V1.8.42
--------------------

	Can now disable use of DIO5 by setting DIO5_0=-1 or DIO5_1=-1

20/02/2021 - V1.8.41
--------------------

	telemetry.txt file includes LoRa channel 0 or 1

08/02/2021 - V1.8.40
--------------------

	Fixed bug where JSON server stopped when asked to set parameters for missing LoRa Module
	If you are using the gateway with HAB Base or PADD and just 1 LoRa module you *need* this update.

25/11/2020 - V1.8.39
--------------------

	Send callsign, listener type (LoRa Gateway) and version to hab.link server
	Don't include callsign with telemetry messages to hab.link server

06/05/2020 - V1.8.38
--------------------

	Send current frequency to client app, with current RSSI
	
06/05/2020 - V1.8.37
--------------------

	Reset AFC offset to zero after setting frequency from client app
	
10/02/2020 - V1.8.35
--------------------

	Fix to RSSI calculation when SNR < 0 (thanks to Alan Hall)

03/02/2020 - V1.8.34
--------------------

	Maximum payload ID (callsign) length in telemetry or calling mode is now 31 (was 15).

03/02/2020 - V1.8.33
--------------------

	Set length of received buffer with strlen() instead of using rx byte count, to avoid sending dross to clients (by Steve Randall)

19/01/2020 - V1.8.32
---------------------

    By proboscide99:
    
    Parameters received in a calling mode packet are saved. If the channel is also used for UpLink, saved
    parameters will be restored after transmission. This allows immediate reception with correct parameters
    without having to wait for a new calling mode packet.
    
    Fixed bug that forced 255 bytes in missing SSDV packet request (UpLink mode) even with 'explicit headers'
    
    When 'implicit headers' are used for UpLink mode, buffer is NULL terminated to prevent the tracker
    from processing garbage (danger)
    
    Both channels may be used for UpLink mode using different timing in the uplink slot (leave 1-2 secs gap)
    Timeout for seeking for missing SSDV file 'uplink.txt' reduced from 2sec to 300ms to allow small gap
    between channels (see above)
    
    Fixed bug measured frequency offset was retained when AFC timeout occurs (affects value set by keys)
    
    Fixed bug that prevented listener from being uploaded to map (longitude comparison)
    
    Channel number display added to many messages on log console


08/01/2020 - V1.8.31
--------------------

	Fixed bug measured frequency offset was retained when returning to calling mode
	Fixed bug where uplink messages were not set to 255 bytes in implicit mode
	Fixed bug where listeners with longitude between -180 and -90 were not uploaded to map


13/06/2019 - V1.8.30
--------------------

	Added ability to connect to hab.link server for responsive web dashboards.


13/06/2019 - V1.8.29
--------------------

	Fixed bug where only the first sentence in a multi-sentence packet was uploaded to Habitat
	Fixed bug where the message uplink sent 255 bytes even if message was shorter
	Fixed bug where after leaving help screen screen shows original frequency not AFC'd frequency
	Fixed bug where AFC correction was lost after uplink
	AFC correction is now upplied to uplink


09/05/2019 - V1.8.28
--------------------

	Store all payloads from combined packet for JSON transmission to client.

09/05/2019 - V1.8.27
--------------------

	Reduced RSSI server port output frequency from 10Hz to 1Hz

13/04/2019 - V1.8.26
--------------------

	Extended UDP status output with basic channel settings

11/03/2019 - V1.8.25
--------------------

	UDP broadcast output of gateway hostname, IP address and version number.

27/02/2019 - V1.8.24
--------------------

	Better colours
	autostart.pdf added to show how to autostart the software and use screen to easily attach to it.
	Thanks to Steve Hyde for these.

27/02/2019 - V1.8.23
--------------------

	Chat mode settings

12/09/2018 - V1.8.22
--------------------

	Added PPM setting and correction

09/05/2018 - V1.8.21
--------------------

	Remove superfluous trailing zeroes from the ASCII telemetry produced from HABPack

04/05/2018 - V1.8.20
--------------------

	Included HABPack decoding by Phil Crump M0DNY

16/04/2018 - V1.8.19
--------------------

	Disabled DIO5 check for now as it sometimes disabled use of a working device.

11/04/2018 - V1.8.18
--------------------

	JSON port now sends packet SNR, RSSI and frequency error
	Implemented callbacks for when settings are changed
	Reprogram LoRa module when frequency, bandwidth etc are changed via JSON port
	Update display when frequency, bandwidth, AFC etc are changed via JSON port


10/04/2018 - V1.8.17
--------------------

    JSON port only sends telemetry as it is received, instead of repeatedly
    JSON port now sends current RSSI
    Append \r\n to sentences sent to data port
    JSON port now accepts commands split over multiple packets (e.g. typed commands)
    When saving to gateway.txt, permissions are set to RW/RW/RW, and owner/group are maintained
    Added Sockets.md to document the available sockets. 


06/04/2018 - V1.8.16
--------------------

    Added raw output of $$... sentences to specified port (UDPPort=xx in gateway.txt)
    Added OziMux format output (TELEMETRY,time,lat,lon,alt) to specified port (OziPort=xx)


26/03/2018 - V1.18.15
---------------------

    By Phil Crump:
    
    	Detect if DIO5 isn't correctly mapped (default level != high), warn and disables the channel.
    
    	Detect if the RFM isn't responding (reg 0x42 == 0x00), warn and disables the channel.
    
    	Warn if both receivers are disabled or unconfigured.	
    
    	Removed message queue test code - functionality was behind that of the main code path, and would have required updating for the other changes in this set.
    
    	Added storage of at-time-of-receive-configuration in rx_metadata_t struct. This is copied into the queued telemetry_t structure when a Telemetry Packet is processed.
    
    	Added upload of receiver frequency (with detected offset) to Habitat with payload telemetry ( Issue #15 ). Mostly ported from Pull Req #28 which has been tested by @dbrooke .
    
    	Added logging of additional metadata parameters in packet.log (Issue #5 )
    
    	Removed duplicate 'LogTelemetryPacket()' call in habitat thread. (Already called in ProcessTelemetryMessage)
    	
    By me:
    
    	Added "Dataport" server socket for raw data (like port 7322 in dl-fldigi)
    	Added "<" sentence type, for telemetry that is not to be uploaded to Habitat.


12/02/2018 - V1.8.14
--------------------

    By Phil Crump:
    
    	Added separate thread for listener telemetry/information upload (Listener telemetry is uploaded every 30 minutes to maintain map marker)
    
    	Corrected incremental tuning units 'MHz' -> 'kHz', and added lowercase letters in tuning lookup table
    	
    By me:
    
    	Added config description to this file for telnet-like HAB communications link

01/10/2017 - V1.8.12
--------------------

    Added null uplink option
    
    Test for incoming null uplink message

21/09/2017 - V1.8.11
--------------------

    Added mode 8 (fastest settings for 62k 10% section of IR2030), for SSDV repeater network

11/09/2017 - V1.8.10
--------------------

    Fixed issue introduced in V1.8.8 which broke processing of repeated telemetry packets (those beginning with a "%" to show repeated)

01/09/2017 - V1.8.9
-------------------

    Added mode 7 for Telnet uplink
    
    Incorporated changes to support Telnet-style terminal to HAB

28/08/2017 - V1.8.8
-------------------

    Do not assume telemetry sentence has a LF at the end
    
    Do not assume telemetry has 2 $ signs at beginning

18/04/2017 - V1.8.7
-------------------

    New help screen added to show the available commands.
    
    Press H to access this feature
    
    Fixed a padding bug on telemetry line !

??/??/2017 - V1.8.6
-------------------

27/09/2016 - V1.8.5
-------------------

	New config setting - AFCMaxStep - limits AFC delta to this amount in kHz for each new packet
	New config setting - AFCTimeout - Resets AFC changes if no packets received in this period (in seconds)
	If gateway.txt missing, and gateway-sample.txt present, rename latter as former
	Fixed SSDV upload status marker
	Fix to frequency format when retuning after calling mode
	Some log display messages now only appear if the feature they describe is in use


​	
15/09/2016 - V1.8.4
-------------------

	Fix to handle disabled channel

14/09/2016 - V1.8.3
-------------------

	Save boolean settings sent by client
	Use of atexit() so ncurses is always closed properly on exit
	Added an exit-with-message function
	Fixed errors where threads were closed on exit even if they hadn't been created (i.e. their functions disabled in the config)
	Consistent RSSI calculations that take HF/LF port into account


​	
14/09/2016 - V1.8.2
-------------------

	Configuration all in a generic array
	Generic code to read/write config array/file
	Send configuration values to JSON client
	Accept commands and new config settings from client
	Fixed SMS folder error
	Fixed LDRO setting


03/09/2016 - V1.8
-----------------

	Add configuration of uplink frequency, mode and power
	LoRa modes now in array so easier to add new ones
	Added LoRa mode for uplink
	Re-instated logging to telemetry.txt
	Fixed pipe errors which happened if packets arrived during program exit
	Merged in changes to JSON format
	Sends data both LoRa channels in JSON
	Config disables CE0 by default (most cards have CE1 only)
	Fixed typos in gateway-sample.txt
	Accept new SSDV types


25/08/2016 - V1.7
-----------------

    Robert Harrison (RJH) has made numerous changes. 
    
    Highlights include :-
    
    Changed makefile to include -Wall and fixed all warnings generated
    Added pipes for Inter-Process Communication 
    Moved none thread safe curl funtions from threads and into main()
    Added reporting of curl errors to habitat and ssdv threads
    Changed color to green but requires 256 color support in your terminal
    
    For putty users please set your terminal as shown

![Alt text](http://i.imgur.com/B81bvEQ.png "Putty config")
    
    when you are connected to your pi
    
    # echo $TERM            # should show something with 256 in
    
    or
    
    # tpu colors            # Should show 256


27/06/2016 - V1.6
-----------------

	Single SSDV upload thread using new API to upload multiple packets at once
	Fixed 100% CPU (SSDV thread not sleeping)


​	
23/05/2016 - V1.5
-----------------

	Better status screen

13/05/2016
----------

	Bug fux to local conversion of large SSDV images
	Added packet logging


04/04/2016
----------

	SSDV 8 buffers
	JSON feed instead of old "transition" method


19/02/2016
----------

	Fixed listener_information JSON
	Added antenna setting to gateway.txt


16/02/2016
----------

	JSON telemetry feed via a server port
	JPEGFolder setting
	Uplink of text messages	to tracker (e.g. accepted from Twitter by an external script)
	Uplink of SSDV re-send requests.  These requests are built by an external Python script.
	Separate thread for uploading latest telemetry to habitat
	4 separate threads for uploading SSDV packets to the SSDV server
	Slightly different display layout, with extra information


​	
07/10/2015
----------

	fsphil: Tidied up compiler warnings, makefile, file permissions


​	

