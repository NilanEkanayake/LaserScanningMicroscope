PCB and schematic for an STM32G431-based Laser Scanning Microscope v2

![A slow zoom in to a CD, gif](https://github.com/NilanEkanayake/LaserScanningMicroscope/blob/main/CD.gif?raw=true) ![A slow zoom in to a rose leaf, gif](https://github.com/NilanEkanayake/LaserScanningMicroscope/blob/main/rose.gif?raw=true) 

The above GIFs show a slow zoom into a CD (all the way to the pits and lands), and a rose leaf, respectively. Both were taken with V1 of the microscope.

### Notes about V2 laser burnout and overheating:
When V2 (likely V1 too) is powered on, the DAC/PWM pins are pulled high during the boot process. This cannot be fixed through software as far as I know.
The effect is that the laser driver circuit is given 3.3v on the DAC line briefly each time the PCB is plugged in. The current then provided to the laser is beyond what it can handle, and while it does not instantly burn out, it gets noticeably dimmer over time as the PCB is power-cycled. This effect is worsed by flashing firmware either with an STLink or through USB, as while in programming mode the pins are also pulled high. Therefore, make sure to disconnect the OPUs before flashing new firmware.

The solution I've found is to add a pull-down resistor to the laser DAC's output. I was able to hand-solder one, but a PCB fix will be required nonetheless as doing so is very finicky.

This issue also affects the VCMs as well. This results in a loose sample being thrown off the holder if one is present on power-on.

Also, U5 and the VCM/DAC_COARSE and VCM/DAC_Y resistors get uncomfortably hot during prolonged operation. This may be exacerbated by using heavier samples that increase the needed current to drive the bottom VCM.

For now, I recommend not directly using the project, and more as a source of inspiration. I am currently working on a new LSM version that fixes these issues (and others), but it will take a while.

-----------------------------------------------------------------

Uses two Optical Pickup Units (or OPU), model HOP-150x, one to hold the sample, the other to scan the sample with a laser. Each pickup covers one horizontal axis, while both use the Z axis to focus (bottom pickup for coarse focus, top one for fine focus)

Required parts (V2):

1x 3mm diameter metal rod, preferably 5cm. (I salvage mine from old CD/DVD players)

1x 26pin, 0.5mm pitch, 10cm length FFC with pins on the same side (for bottom OPU) (https://www.aliexpress.com/item/1005004140412156.html works for this and next part)

1x 26pin, 0.5mm pitch, 10cm length FFC with pins on the opposite side (for top OPU)

1x V2 PCB (I had mine entirely manufactured by JLCPCB - I chose as many of JLCPCB's "basic parts" as possible to reduce assembly cost)

1x 3D-printed enclosure (I printed mine using basic PLA on my own printer, but I assume you can also use JLCPCB's 3d-printing service for this too)

1x 3D-printed sample holder (WIP, not ready for prime time)

2x HOP-150x OPUs (I get mine from aliexpress)

Assembly (V2):
1. Remove the lens from the OPU destined to be the sample holder using a pair of tweezers, connect the FFC cable that has pins on the same side, and thread the cable through the slot in the enclosure, from the inside to the outside.
2. Gently push the OPU directly into the enclosure, making sure that the FFC cable doesn't snag.
3. Using a soldering iron, remove the large blob of solder on the edge of the large PCB on the top OPU, so that the pins are exposed and not shorted, and connect the FFC cable with pins on opposite sides
4. Slot the OPU lens-down into the protrusions sticking up from the enclosure, and slot the 3mm rod through the aligned OPU and enclosure on the side above the bottom OPU's FFC cable slot. The top OPU should now be able to swing upwards like a hinge for easy sample access.
5. Place the PCB into the slot on the side of the enclosure with the two FFC connectors facing up and right respectively, and connect the top OPU's cable to the connector on top of the PCB, and the bottom OPU's to the one on the right.
6. Swing the top OPU upwards and place the sample holder in the hole previously occupied by the lens on the bottom OPU. Samples are placed in the center of the holder.


To flash the PCB:

Method 1 (requires STLink, allows debugging):
1. Connect the PCB to a PC with USB and an STLink (Use the header's SWDIO, SWCLK and GND pins, pinout in the schematic).
2. Open LSM-scanner in STM32CubeIDE and flash it to the PCB.

Method 2 (no STLink required, but no debugging):
1. Install https://www.st.com/en/development-tools/stm32cubeprog.html
2. Open LSM-scanner in STM32CubeIDE, right-click the project in the left bar and select "Build Project".
3. Expand the "Binaries" tab under the project and right-click->copy the .elf file within, then paste it somewhere you'll remember in your filesystem
4. Open STM32CubeProgrammer and connect the PCB to a PC via USB while holding down the button above the USB-C port.
5. Click the little reload icon within "USB Configuration", and the PCB should show up as USB1.
6. Select the "Open File" tab and browse to the .elf file from before, then click "Download".
7. After the download is complete, unplug and replug the USB connection, without holding down the button (only needed when flashing with USB)

To install the required packages and load LSM-client, open a terminal in the LSM-client folder and run:
```
python3 -m pip install -r requirements.txt
```
Then to launch the software:
```
python3 LSMclient.py
```

NOTES:
1. FES signal in V2 is noisy, haven't figured out why yet, overall doesn't hugely affect results.
2. OPU sample holder is a WIP
3. V2 doesn't include the debug header pins in the BOM, as wires or pins can be manually soldered with very little effort and aren't necessary for most users.

Credit to https://github.com/kototoibashi/dvd-pickup-microscope-poc and https://github.com/GaudiLabs/OpenLaserScanningMicroscope for schematic ideas and OPU pinout
