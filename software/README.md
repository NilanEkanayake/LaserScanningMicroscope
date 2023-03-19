To get started:
1. Connect the PCB to a PC with USB and an STLink (Use the header's SWDIO, SWCLK and GND pins).
2. Open LSM-scanner in STM32CubeIDE and flash it to the PCB.
3. Setup and run the LSM-client software on the host PC and select the device in the menubar.

To install the required packages and load LSM-client, open a terminal in the LSM-client folder and run:
```
python3 -m pip install -r requirements.txt
```
Then to launch the software:
```
python3 LSMclient.py
```

 Flashing the PCB should also be possible using only USB and changing the BOOT0 switch, but I haven't tested it.
