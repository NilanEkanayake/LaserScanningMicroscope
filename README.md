PCB and schematic for an STM32G473-based Laser Scanning Microscope v0.1

I designed it to be printed and partially assembled by JLCPCB.
The diameters are very slightly under the 5cm by 5cm limit for the four layers for $2 deal.
As well, most of the parts are classified as "basic parts" by JLCPCB, meaning that there's no added cost on top of the component itself when getting it assembled.
The ones that aren't I ordered from LCSC and hand-soldered (which was a pain in the case of the STM32).

This is my first foray into PCB design and while it functions, it has some notable issues:
1. The two FFC connectors are too close together, making it impossible to hand-solder one of the support pads.
2. The chosen FFC connector's receptacle is too thin for a standard FFC cable (at least the ones I have, which have plastic support tabs on top of the cable ends).
3. The 3.3v power indicator LED is very bright, I had to put some electrical tape over it (would be fixed by a larger series resistor).

General notes:
1. The BOOT0 switch is pretty much useless
2. I peeled the support tab off of an FFC connector and added some tape on top to thicken it, which mostly solved the FFC connector clearance issue, but the connection is sometimes shoddy
3. For some reason, connecting the GNDA pins to the OPU via the header pins results in the laser dimming. I haven't figured out why yet.
4. I setup the ADC inputs to make use of the STM32's PGA (Programmable Gain Array), which lets me adjust the amplification of the photodiode output.
5. The laser power scales too high - the useable range is from 0-2000 (instead of 0-4096), after that the laser doesn't get any brighter, which means it's probably slowly getting fried.

Here's a picture of the FES (Focus Error Signal) I got when pointing the OPU at a DVD, and changing the coarse focus from min to max:
https://github.com/NilanEkanayake/LaserScanningMicroscope/raw/main/assets/PCB-FES.png
