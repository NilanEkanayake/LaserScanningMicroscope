Flash/modify with STM32CUBEIDE

Very basic, just to test the hardware.
Needs many improvements and client software for PC.

I use HTerm to send and recieve data.

to save an image: click "save output" in HTerm, save to data.txt, open and delete the final line, as well as the lines up until the image data at the top, along with the first line of the image as the software has a bug that makes it have garbage values.

To turn the data into an image I use this very basic python script:
```
from matplotlib import pyplot as plt
from numpy import genfromtxt
image = genfromtxt('data.txt', delimiter=',', dtype="uint32")

plt.imshow(image, cmap='gray')
plt.show()
```
