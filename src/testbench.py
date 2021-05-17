
from simulator.interface import TabletHardware

# This file demonstrates basic usage of the TabletHardware classes.
# Use it as an example to write your own programs.

class Tb:
    def __init__(self, hardware, bg = 0):

        # Register this class so we get notified of
        # events, like pen presses.
        hardware.registerCallbackHandler(self)

        self.hardware = hardware
        self.bg = bg

    def onDragStart(self, x, y):
        self.start = (x, y)

    def onDragPoint(self, x, y):
        self.hardware.setPixel(x, y, 1 - self.bg)

    def onDragStop(self, x, y):
        print("Lifted the pen at", (x, y))

hardware = TabletHardware()
tb = Tb(hardware)
