
from utils import platform, stdio_stream, protocol
import time

is_real_tablet = platform.is_real_tablet()

if not is_real_tablet:
    # Not an actual tablet, prepare to set up the simulator

    # We don't import tkinter by default because it shouldn't be installed
    # on the real tablet, only on machines doing simulation.
    # Importing the simulation modules will import tkinter.
    from simulator import peripherald

    stream = peripherald.PeripheraldEmulator(scale = 2)

else:
    stream = stdio_stream.StdIOStream()

tablet = protocol.TabletInterface(stream)
display = tablet.getDisplay()

display.fillScreen(0x202020)

while True:
    time.sleep(0.010)
    for point in tablet.getPresses():
        x = point[0]
        y = point[1]
        display.drawPixel(x, y, 0xFFFFFF)
