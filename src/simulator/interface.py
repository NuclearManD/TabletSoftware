
import tkinter as tk
import time, _thread

class EPaperDisplay:
    def __init__(self, handler):
        _thread.start_new_thread(self.mainloop, ())
        self.handler = handler

    def mainloop(self):
        # init tk
        self.root = tk.Tk()

        # create canvas
        self.canvas = tk.Canvas(self.root, bg="white", height=800, width=1600)
        self.canvas.bind("<ButtonPress-1>", self._drag_start)
        self.canvas.bind("<ButtonRelease-1>", self._drag_stop)
        self.canvas.bind("<B1-Motion>", self._drag)
        self.canvas.pack()

        # add to window and show
        self.root.mainloop()

    def setPixel(self, x, y, color=1):
        x *= 2
        y *= 2
        self.canvas.create_rectangle(x, y, x+1, y+1)

    def _drag_start(self, event):
        self.handler.onDragStart(event.x // 2, event.y // 2)

    def _drag_stop(self, event):
        self.handler.onDragStop(event.x // 2, event.y // 2)

    def _drag(self, event):
        self.handler.onDragPoint(event.x // 2, event.y // 2)

class Tb:
    def __init__(self, bg = 0):
        self.display = EPaperDisplay(self)
        self.bg = bg

    def onDragStart(self, x, y):
        self.start = (x, y)

    def onDragPoint(self, x, y):
        self.display.setPixel(x, y, 1 - self.bg)

    def onDragStop(self, x, y):
        print("Lifted the pen at", (x, y))

tb = Tb()

