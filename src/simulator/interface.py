
import tkinter as tk
import time, _thread

class TabletHardware:
    def __init__(self, screen_res = (800, 480), scale = 2):
        if type(scale) != int:
            raise ValueError("scale parameter must be an integer")

        self.handler = None
        self.screen_res = screen_res
        self.scale = scale
        self._canvas_width = screen_res[0] * scale
        self._canvas_height = screen_res[1] * scale
        self._was_dragging = False

        _thread.start_new_thread(self.mainloop, ())

    def mainloop(self):
        # init tk
        self.root = tk.Tk()

        # create canvas
        self.canvas = tk.Canvas(
            self.root, bg="white",
            height=self._canvas_height, width=self._canvas_width)

        self.canvas.bind("<ButtonPress-1>", self._drag_start)
        self.canvas.bind("<ButtonRelease-1>", self._drag_stop)
        self.canvas.bind("<B1-Motion>", self._drag)
        self.canvas.pack()

        # add to window and show
        self.root.mainloop()

    def registerCallbackHandler(self, handler):
        self.handler = handler

    def setPixel(self, x, y, color=1):
        s = self.scale
        x *= s
        y *= s
        self.canvas.create_rectangle(x, y, x+s-1, y+s-1)

    def _drag_start(self, event):
        self._was_dragging = True
        self.handler.onDragStart(event.x // self.scale, event.y // self.scale)

    def _drag_stop(self, event):
        # If the pen is pulled offscreen, _drag will send an onDragStop
        # event.  If the user stops clicking off-screen then this function
        # is called, which would make onDragStop called again.  Instead
        # we'll check if it was already called first.
        if self._was_dragging:
            self.handler.onDragStop(event.x // self.scale, event.y // self.scale)

    def _drag(self, event):
        x = event.x // self.scale
        y = event.y // self.scale

        # If the pen left the screen then simulate the drag restarting.  This is how a real
        # touchscreen would respond.
        if x < 0 or y < 0 or x >= self.screen_res[0] or y >= self.screen_res[1]:
            # Constrain the values - a real touchscreen cannot detect presses
            # off the screen.  The last point then would be on-screen.
            if x < 0:
                x = 0
            if y < 0:
                y = 0
            if x >= self.screen_res[0]:
                x = self.screen_res[0] - 1
            if y >= self.screen_res[1]:
                y = self.screen_res[1] - 1

            if self._was_dragging:
                # Notify the software that the drag stopped
                self.handler.onDragStop(x, y)
                self._was_dragging = False

        else:
            # Don't call both onDragStart and onDragPoint for the same (x, y) unless
            # this function (_drag) is called twice.  This just checks and ensures the drag
            # restart is registered properly.
            if not self._was_dragging:
                self.handler.onDragStart(x, y)
                self._was_dragging = True
            else:
                self.handler.onDragPoint(x, y)

