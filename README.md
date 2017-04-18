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
	DIO5	Wiring Pi 26 (Pin 32) (Wiring Pi 5 (pin 18) for 2nd module)


Installation
============

Enable SPI in raspi-config.

Install the dependencies:

	sudo apt-get install git wiringpi libcurl4-openssl-dev libncurses5-dev 

Install SSDV so you can view locally downloaded images.  If you skip this section then the gateway will still work but SSDV data will not be decoded into JPG files locally (but can still be viewed online).

	1. cd
	2. git clone https://github.com/fsphil/ssdv.git
	3. cd ssdv
	4. sudo make install

Install the LoRa gateway

	1. cd ~ 
	2. git clone https://github.com/PiInTheSky/lora-gateway.git
	3. cd lora-gateway
	4. make
	5. cp gateway-sample.txt gateway.txt
	

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
	
	JPGFolder=<folder>.  Tells the gateway where to save local JPEG files built from incoming SSDV packets.

	LogTelemetry=<Y/N>.  Enables logging of telemetry packets (ASCII only at present) to telemetry.txt.	
	
	LogPackets=<Y/N>.  Enables logging of packet information (SNR, RSSI, length, type) to packets.txt.	
	
	SMSFolder=<folder>.  Tells the gateway to check for incoming SMS messages or tweets that should be sent to the tracker via the uplink.

	CallingTimeout=<seconds>.  Sets a timeout for returning to calling mode after a period with no received packets.
	
	ServerPort=<port>.  Opens a server socket which can have 1 client connected.  Sends JSON telemetry and status information to that client.
	
	Latitude=<decimal position>
	Longitude=<decimal position>.  These let you tell the gateway your position, for uploading to habitat, so your listener icon appears on the map in the correct position.
	Antenna=<antenna make/model>.  Lets you specify your antenna make/model or type.  This appears on the map if your listener icon is clicked on.
	
	NetworkLED=<wiring pi pin>
	InternetLED=<wiring pi pin>
	ActivityLED_0=<wiring pi pin>
	ActivityLED_1=<wiring pi pin>.  These are used for LED status indicators. Useful for packaged gateways that don't have a monitor attached.
	
	
and the channel-specific options are:
	
	frequency_<n>=<freq in MHz>.  This sets the frequency for LoRa module <n> (0 for first, 1 for second).  e.g. frequency_0=434.450
	
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

There is no current standard claling channel.


Use
===

Run with:

	sudo ./gateway


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

	
07/10/2015
----------

	fsphil: Tidied up compiler warnings, makefile, file permissions
	
	

