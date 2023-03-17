Flash/modify with STM32CUBEIDE

Very basic, just to test the hardware.
Needs improvements and client software for PC.

I use HTerm to send and recieve data, and save the image data to a file:
click "save output" in HTerm, save to data.txt, open and delete the final line, as well as the lines up until the image data at the top.

To turn the data into an image I use this very basic python script:
```
from matplotlib import pyplot as plt
from numpy import genfromtxt
image = genfromtxt('data.txt', delimiter=',', dtype="uint32")

plt.imshow(image, cmap='gray')
plt.show()
```