
import tkinter as tk
import time, _thread
import ntios_font
#from .input import Input

COMMAND_SET_TEXT_CURSOR    = 0x01
COMMAND_WRITE_TEXT         = 0x02
COMMAND_DRAW_PIXEL         = 0x03
COMMAND_FILL_RECT          = 0x04
COMMAND_DRAW_RECT          = 0x05
COMMAND_SET_TEXT_COLOR     = 0x06
COMMAND_WRITE_VRAM         = 0x07
COMMAND_DRAW_BITMAP        = 0x08
COMMAND_SELECT_DISPLAY     = 0x09
COMMAND_DRAW_PALETTE_IMAGE = 0x0A
COMMAND_FILL_DISPLAY       = 0x0B
COMMAND_SET_VIBRATE        = 0x0C

EVENT_ON_CTP_CHANGE = 0x01
EVENT_BATTERY_DATA  = 0x02
EVENT_ON_KEYPRESS   = 0x03


def u16_to_rgb(c):
    red = (c << 3) & 0xFF
    green = (c >> 3) & 0xFC
    blue = (c >> 8) & 0xF8

    return (red, green, blue)

def RGBtoStr(ombreRGB):
    color = "#"
    for rgb in ombreRGB:
        if rgb < 16:
            color+="0"
        color+=format(int(rgb), 'x')
    return color

class PeripheraldEmulator:
    def __init__(self, screen_res = (800, 480), scale = 1):
        if type(scale) != int:
            raise ValueError("scale parameter must be an integer")

        self.screen_res = screen_res
        self.scale = scale
        self._canvas_width = screen_res[0] * scale
        self._canvas_height = screen_res[1] * scale
        self.output_buffer = b''
        self.text_cursor = (0, 0)
        self.text_color = '#FFFFFF'

        _thread.start_new_thread(self.mainloop, ())

        while not hasattr(self, 'canvas'):
            time.sleep(0.01)

    def mainloop(self):
        # init tk
        self.root = tk.Tk()

        # create canvas
        self.canvas = tk.Canvas(
            self.root, bg="black",
            height=self._canvas_height, width=self._canvas_width)

        self.canvas.bind("<ButtonPress-1>", self.onDragStartCB)
        self.canvas.bind("<ButtonRelease-1>", self.onDragStopCB)
        self.canvas.bind("<B1-Motion>", self.onDragPointCB)
        self.canvas.pack()

        # add to window and show
        self.root.mainloop()

    def setPixel(self, x, y, color):
        if type(color) == str:
            rgb_str = color
        else:
            rgb = u16_to_rgb(color)
            rgb_str = RGBtoStr(rgb)
        s = self.scale
        x *= s
        y *= s
        if s != 1:
            self.canvas.create_rectangle(x, y, x+s-1, y+s-1, fill=rgb_str, outline=rgb_str)
        else:
            self.canvas.create_line(x, y, x + 1, y, fill=rgb_str)

    # Input class callback
    def onDragStartCB(self,x,y):
        pass
    def onDragPointCB(self,x,y):
        pass
    def onDragStopCB(self,x,y):
        pass

    def write(self, data):
        cmd = data[0]

        if cmd == COMMAND_SET_TEXT_CURSOR:
            x = (data[1] << 8) | data[2]
            y = (data[3] << 8) | data[4]
            self.text_cursor = (x, y)

        elif cmd == COMMAND_WRITE_TEXT:
            n = data[1]
            x = self.text_cursor[0]
            y = self.text_cursor[1]
            for char in data[2:n + 2]:
                if char == 10:
                    x = 0
                    y += 12
                else:
                    ntios_font.draw_char_on_canvas(self, x, y, char, self.text_color)
                    x += 8
                

        elif cmd == COMMAND_SET_TEXT_COLOR:
            rgb = u16_to_rgb((data[1] << 8) | data[2])
            self.text_color = RGBtoStr(rgb)

        

    def available(self):
        return len(self.output_buffer) > 0

    def read(self):
        data = self.output_buffer
        self.output_buffer = b''
        return data

v = PeripheraldEmulator()
v.write(bytes([COMMAND_WRITE_TEXT, 5]) + b'hello')
#v.canvas.create_rectangle(0, 12, 8*5, 24, fill="#EEEEFF", outline="#EEEEFF")
