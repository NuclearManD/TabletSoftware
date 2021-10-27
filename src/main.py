

from utils import platform, stdio_stream, protocol, compression
import shell
from application.library import *
import time
from ui import *

from PIL import Image

is_real_tablet = platform.is_real_tablet()

if not is_real_tablet:
    # Not an actual tablet, prepare to set up the simulator

    # We don't import tkinter by default because it shouldn't be installed
    # on the real tablet, only on machines doing simulation.
    # Importing the simulation modules will import tkinter.
    from simulator import peripherald

    stream = peripherald.PeripheraldEmulator(scale = 2)
    #stream.drawBitmap16(0, 0, 255, 255, 0)
    #quit()

else:
    stream = stdio_stream.StdIOStream()

tablet = protocol.TabletInterface(stream)
display = tablet.getDisplay()

#display.fillScreen(0x202020)

sys = shell.SystemShell([
    shell.Application("Paint", None, 0x882297, None),
    LibraryApp()
])

sys.loadImages(display)

sys.mainloop(tablet)
