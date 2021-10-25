
import tkinter as tk
import time, _thread
from . import ntios_font
from PIL import ImageTk, Image, ImageDraw
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
    blue = (c << 3) & 0xFF
    green = (c >> 3) & 0xFC
    red = (c >> 8) & 0xF8

    return (red, green, blue)


def RGBtoStr(ombreRGB):
    color = "#"
    for rgb in ombreRGB:
        if rgb < 16:
            color+="0"
        color+=format(int(rgb), 'x')
    return color


class PeripheraldEmulator:
    def __init__(self, screen_res = (800, 480), scale = 1, vram_sectors=1024):
        if type(scale) != int:
            raise ValueError("scale parameter must be an integer")

        self.screen_res = screen_res
        self.scale = scale
        self._canvas_width = screen_res[0] * scale
        self._canvas_height = screen_res[1] * scale
        self.output_buffer = b''
        self.text_cursor = (0, 0)
        self.text_color = '#FFFFFF'
        self.vram = [0] * (vram_sectors * 256)
        self.imcache = []

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
    def onDragStartCB(self, evt):
        z = 500
        x = evt.x // self.scale
        y = evt.y // self.scale
        self.output_buffer += bytes([EVENT_ON_CTP_CHANGE, 1,
                                  x >> 8, x & 255,
                                  y >> 8, y & 255,
                                  z >> 8, z & 255,
                              ])

    def onDragPointCB(self, evt):
        z = 500
        x = evt.x // self.scale
        y = evt.y // self.scale
        self.output_buffer += bytes([EVENT_ON_CTP_CHANGE, 1,
                                  x >> 8, x & 255,
                                  y >> 8, y & 255,
                                  z >> 8, z & 255,
                              ])

    def onDragStopCB(self, evt):
        self.output_buffer += bytes([EVENT_ON_CTP_CHANGE, 0])

    def write(self, data):
        cmd = data[0]

        # Simulate the time delay
        #print(f"Delay: {len(data) / 11.520}ms")
        time.sleep(len(data) / 11520)

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
                    # Newline
                    x = 0
                    y += 12
                elif char == 32:
                    # Space
                    x += 8
                else:
                    ntios_font.draw_char_on_canvas(self, x, y, char, self.text_color)
                    x += 8

        elif cmd == COMMAND_SET_TEXT_COLOR:
            rgb = u16_to_rgb((data[1] << 8) | data[2])
            self.text_color = RGBtoStr(rgb)

        elif cmd == COMMAND_FILL_RECT:
            x1 = ((data[1] << 8) | data[2]) * self.scale
            y1 = ((data[3] << 8) | data[4]) * self.scale
            x2 = ((data[5] << 8) | data[6]) * self.scale
            y2 = ((data[7] << 8) | data[8]) * self.scale
            rgb = u16_to_rgb((data[9] << 9) | data[10])
            rgb_str = RGBtoStr(rgb)
            self.canvas.create_rectangle(x1, y1, x2, y2, fill=rgb_str, outline=rgb_str)

        elif cmd == COMMAND_DRAW_RECT:
            x1 = ((data[1] << 8) | data[2]) * self.scale
            y1 = ((data[3] << 8) | data[4]) * self.scale
            x2 = ((data[5] << 8) | data[6]) * self.scale
            y2 = ((data[7] << 8) | data[8]) * self.scale
            rgb = u16_to_rgb((data[9] << 8) | data[10])
            rgb_str = RGBtoStr(rgb)
            self.canvas.create_rectangle(x1, y1, x2, y2, outline=rgb_str)

        elif cmd == COMMAND_DRAW_PIXEL:
            x = (data[1] << 8) | data[2]
            y = (data[3] << 8) | data[4]
            color = (data[5] << 8) | data[6]
            self.setPixel(x, y, color)

        elif cmd == COMMAND_FILL_DISPLAY:
            rgb = u16_to_rgb((data[1] << 8) | data[2])
            rgb_str = RGBtoStr(rgb)
            xs = self._canvas_width
            ys = self._canvas_height
            self.canvas.create_rectangle(0, 0, xs, ys, fill=rgb_str, outline=rgb_str)

            # No images left, clear the image cache
            self.imcache = []

        elif cmd == COMMAND_WRITE_VRAM:
            first_word = ((data[1] << 8) | data[2]) * 256

            # Read a sector into VRAM
            for i in range(256):
                self.vram[first_word + i] = (data[i*2 + 3] << 8) | data[i*2 + 4]

        elif cmd == COMMAND_DRAW_BITMAP:
            first_word = ((data[1] << 8) | data[2]) * 256
            x = ((data[3] << 8) | data[4]) * self.scale
            y = ((data[5] << 8) | data[6]) * self.scale
            width = data[7]
            height = data[8]
            self.drawBitmap16(x, y, width, height, first_word)

        elif cmd == COMMAND_DRAW_PALETTE_IMAGE:
            first_word = ((data[1] << 8) | data[2]) * 256
            x = ((data[3] << 8) | data[4]) * self.scale
            y = ((data[5] << 8) | data[6]) * self.scale
            width = data[7]
            height = data[8]
            n_colors = data[9]
            self.drawPaletteImage(x, y, width, height, first_word, n_colors)

    def available(self):
        return len(self.output_buffer) > 0

    def drawBitmap16(self, xp, yp, width, height, first_word):

        # Create Image
        pilImage = Image.new(mode="RGB", size=(width, height))
        for y in range(height):
            for x in range(width):
                color = u16_to_rgb(self.vram[first_word + x + y*width])
                pilImage.putpixel((x, y), color)

        pilImage = pilImage.resize((width * self.scale, height * self.scale))

        # Apply to Canvas
        self.imcache.append(ImageTk.PhotoImage(pilImage))
        self.canvas.create_image((xp, yp), image=self.imcache[-1], anchor='nw')

    def drawPaletteImage(self, xp, yp, width, height, first_word, n_colors):

        # Create Image
        pilImage = Image.new(mode="P", size=(width, height))
        palette = []
        for i in range(n_colors):
            palette += u16_to_rgb(self.vram[first_word + i])
        pilImage.putpalette(palette + [0]*(768 - len(palette)))
        for y in range(height):
            for x in range(0, width, 4):
                pixel_data = self.vram[n_colors + first_word + ((x + y*width) // 4)]
                pilImage.putpixel((x + 0, y), (pixel_data >> 12) & 15)
                pilImage.putpixel((x + 1, y), (pixel_data >> 8) & 15)
                pilImage.putpixel((x + 2, y), (pixel_data >> 4) & 15)
                pilImage.putpixel((x + 3, y), (pixel_data >> 0) & 15)

        pilImage = pilImage.resize((width * self.scale, height * self.scale))

        # Apply to Canvas
        self.imcache.append(ImageTk.PhotoImage(pilImage))
        self.canvas.create_image((xp, yp), image=self.imcache[-1], anchor='nw')

    def read(self):
        data = self.output_buffer
        self.output_buffer = b''
        return data

