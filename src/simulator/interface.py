
import tkinter as tk
import time, _thread
from .input import Input

class TabletEPaperDisplay:
    def __init__(self, tabletHardware, screen_res = (800, 480), scale = 2):
        if type(scale) != int:
            raise ValueError("scale parameter must be an integer")

        self.screen_res = screen_res
        self.scale = scale
        self._canvas_width = screen_res[0] * scale
        self._canvas_height = screen_res[1] * scale
        self.inputHandler = Input(self)
        self.tabletHardware = tabletHardware
        self.bg = 0

        _thread.start_new_thread(self.mainloop, ())

    def mainloop(self):
        # init tk
        self.root = tk.Tk()

        # create canvas
        self.canvas = tk.Canvas(
            self.root, bg="white",
            height=self._canvas_height, width=self._canvas_width)

        self.canvas.bind("<ButtonPress-1>", self.inputHandler.onDragStart)
        self.canvas.bind("<ButtonRelease-1>", self.inputHandler.onDragStop)
        self.canvas.bind("<B1-Motion>", self.inputHandler.onDragPoint)
        self.canvas.pack()

        # add to window and show
        self.root.mainloop()

    def setPixel(self, x, y, color=1):
        s = self.scale
        x *= s
        y *= s
        self.canvas.create_rectangle(x, y, x+s-1, y+s-1)

    # Input class callback
    def onDragStartCB(self,x,y):
        pass
    def onDragPointCB(self,x,y):
        self.tabletHardware.display.setPixel(x, y, 1 - 0)
    def onDragStopCB(self,x,y):
        print("Lifted the pen at", (x, y))

    # setter/getter
    def setBG(self, bg):
        self.bg=bg
    def getBG(self):
        return self.bg

class TabletHardware:
    def __init__(self):
        self.display = TabletEPaperDisplay(self)
        self.start_time = time.time()

    def getBatteryVoltage(self):
        x = (time.time() - self.start_time) / 1000

        if x > 3.6:
            # It's been over an hour.  Pretend the battery magically refilled.
            # x is in kiloseconds (ik weird unit, makes the polynomial easier), and
            # 3.6 kiloseconds is one hour
            x = 0
            self.start_time = time.time()

        # This battery curve simulates a lithium battery completely discharging over
        # an hour of use.  Don't worry, I did the math and the real battery will last
        # longer than that!
        y = 0.0107*(x**4) - .16*(x**3) + .63*(x**2) - .96*x + 4.2

        return round(y, 4)

    def setVibrate(self, isOn):
        # We ignore this because the simulator cannot vibrate.
        # This function needs to be here so simulation of real
        # programs is possible, however
        pass
