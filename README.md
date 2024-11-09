# Ignition timer

A digital ignition timer for the Matra Murena 2.2 and possibly other cars.

# Background

I started this project for my matra Murena 2.2 because I wanted to be able to time the ignition precisely. The Murena 2.2 was delivered from factory with a diagnostic connector fitted on the engine which had connections to a crank sensor device. The diagnostic connector also had other connections, e.g. to the alternator and sensors on the engine, and the ignition coil circuit from the ignition module. By measuring the time between the crank sensor signal and the ignition coil signal adjusting for the RPM, it would be possible to measure the advance precisely. 

A device that does this is briefly described in one of the original workshop manuals for the engine, however, nobody I've talked to have ever seen such a device, and I've started to doubt whether it existed at all. Also, documentation of the sensor is not available anywhere, so both the device and the sensor remained a mystery.

The sensor fitted looks like a normal, modern sensor, but turned out to be a little odd. Crank sensors are usually either of the Hall-type or pre-magnetized with a permanent magnet. This sensor is more like an inductive proximity sensor with no permanent magnet, but just a coil on what appears to be a ferrite core. It has a rather low DC resistance of about 50 Ohm. The sensor is fitted close to the flywheel which contains two indents, one at TDC and one at TDC + 180 degrees.

# Scope

This project describes the hardware and software of an analogue detector and Microprocessor based device which connects via the Murena diagnostic connector to the sensor and the ignition coil wires,  measures the signals, calculates the RPM and ignition advance, and displays the values in two rows of 7 segment displays. It also has provision for a serial port so that the values can be downloaded to a PC while the engine is running, and subsequently analyzed.

I've designed the device with a classic touch using my favorite MCU, the 8052. It's old and simple, but I wanted to justify that the device could have been made in the 1980's when the car was new. The circuit and PCB is designed in KiCad. The code consists of a single source file written in C for the SDCC compiller. The code is making well use of the three timers to run all the functions of the board, but the code is small and the 8K flahs ROM is far from used. Many of the 256 bytes of RAM are also in surplus.

The analogue section of the board is built around a differential amplifier and acomparator. I've DC-biased the sensor to create a permanent magnetic field which is then disturbed by the flywheel indent. Another comparator (actually the other half of the same IC) is used to detect ignition signals.

# Project history

I started the project in 2023, prototyping ways of making the analogue section. By the end of 2024, I have it working on the engine. The current PCB has several layout issues and I will make an updated version. It's also made for a specific TEKO box which I'm not sure is available from TEKO any more (it is still in stock from my preferred parts dealer).

The is thus still work-in-progress. 

The design is open source. 

# License

/* Copyright (c) 2024 Anders Dinsen anders@dinsen.net - All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided that the above copyright notice
 * and this paragraph are duplicated in all such forms and that any documentation, advertising materials, and 
 * other materials related to such distribution and use acknowledge that the software was developed by the 
 * copyright holder. The name of the copyright holder may not be used to endorse or promote products derived 
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT 
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
