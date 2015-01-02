Part of the LoRa Balloon Tracking System

Runs on a Raspberry Pi with 1 or 2 RFM98HW modules attached to the SPI port.
Also works with other compatible HopeRF and Semtec LoRa devices.

Connections
===========

Connect the LoRa module to the Pi A+/B+ like so:

LORA     PI
====    ====
3.3V	3.3V Power
GND		Ground
MOSI	MOSI (pin 19)
MISO	MISO (pin 21)
NSS		CE0 (pin 24) (CE1 (pin 26) for 2nd module)
SCK		SLCK
DIO0	Wiring Pi 31 (Pin 28) (Wiring Pi 6 (pin 22) for 2nd module)
DIO5	Wiring Pi 26 (Pin 32) (Wiring Pi 5 (pin 18) for 2nd module)

For the model A/B boards, use the following for DIO0/DIO5:

DIO0	Wiring Pi 6 (Pin 22) (Wiring Pi 1 (pin 12) for 2nd module)
DIO5	Wiring Pi 5 (Pin 18) (Wiring Pi 0 (pin 11) for 2nd module)


Installation
============

Enable SPI in raspi-config.

Install WiringPi:

	1. cd ~
	2. git clone git://git.drogon.net/wiringPi
	3. cd wiringPi
	4. ./build

Install the curl library:

	sudo apt-get install libcurl4-openssl-dev
	
Install the ncurses library

	sudo apt-get install libncurses5-dev

Install the LoRa gateway


	1. cd ~ 
	2. git clone https://github.com/PiInTheSky/lora-gateway.git
	3. cd lora-gateway
	4. make


Configuration
=============

The configuration is in the file gateway.txt.  Example:

	tracker=M0RPI

	frequency_0=434.451
	mode_0=2

	#frequency_1=434.450
	#mode_1=0

The options are:
	
	tracker=<callsign>.  This is whatever callsign you want to appear as on the tracking map and/or SSDV page.
	
	frequency_<n>=<freq in MHz>.  This sets the frequency for LoRa module <n> (0 for first, 1 for second).  e.g. frequency_0=434.450
	
	mode_<n>=<mode>.  Sets the "mode" for the selected LoRa module.  This offers a simple way of setting the various
					LoRa parameters (SF etc.) in one go.  The modes are:
					
					0 = (normal for telemetry)	Explicit mode, Error coding 4:8, Bandwidth 20.8kHz, SF 11, Low data rate optimize on
					1 = (normal for SSDV) 		Implicit mode, Error coding 4:5, Bandwidth 20.8kHz,  SF 6, Low data rate optimize off
					2 = (normal for repeater)	Explicit mode, Error coding 4:8, Bandwidth 62.5kHz,  SF 8, Low data rate optimize off
					3 = (normal for fast SSDV)	Explicit mode, Error coding 4:6, Bandwidth 250kHz,   SF 7, Low data rate optimize off
					
	SF_<n>=<Spreading Factor>  e.g. SF_0=7
	
	Bandwidth_<n>=<Bandwidth>.  e.g. Bandwidth_0=41K7.  Options are 7K8, 10K4, 15K6, 20K8, 31K25, 41K7, 62K5, 125K, 250K, 500K
	
	Implicit_<n>=<Y/N>.  e.g. Implicit_0=Y
	
	Coding_<n>=<error_coding>.  e.g. Coding_0=5 (4:5)
	
	lowopt_<n>=<Y/N>.  Enables or disables low data rate optimization.

	
Use
===

Run with:

	sudo ./gateway
		

