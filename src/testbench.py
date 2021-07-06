
from simulator.interface import TabletHardware

# This file demonstrates basic usage of the TabletHardware classes.
# Use it as an example to write your own programs.

class Tb:   
    def __init__(self, hardware, bg = 0):

        # Register this class so we get notified of     
        # events, like pen presses.
        #hardware.display.registerCallbackHandler(self)

        self.hardware = hardware
        self.bg = bg
            
hardware = TabletHardware()
tb = Tb(hardware)
