
import math

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


def rgb_to_u16(rgb):
    if type(rgb) == tuple or type(rgb) == list:
        red = rgb[0]
        green = rgb[1]
        blue = rgb[2]
    else:
        red = rgb & 255
        green = (rgb >> 8) & 255
        blue = (rgb >> 16) & 255

    return (red >> 3) | ((green << 3) & 0x7e0) | ((blue << 8) & 0xf800)


class TabletInterface:

    def __init__(self, stream):
        self.stream = stream
        self.lastBatteryVoltage = None
        self.lastBatteryCurrent = None
        self.presses = []
        self._input_buffer = b''

    def getDisplay(self, index = 0):
        return TabletDisplay(self)

    def getBatteryVoltage(self):
        self._update()
        return self.lastBatteryVoltage

    def setVibrate(self, vibe):
        if vibe:
            vibe = 1
        else:
            vibe = 0
        self._sendBytes(COMMAND_SET_VIBRATE, vibe)

    def getPresses(self):
        '''Use this over the presses field so the data gets updated.'''
        self._update()
        return self.presses

    def _sendBytes(self, *li):
        self.stream.write(bytes(li))

    def _update(self):

        if not self.stream.available():
            # Nothing changed since last update, return
            return

        # Load up the input buffer
        while self.stream.available():
            self._input_buffer += self.stream.read()

        while len(self._input_buffer) > 0:
            event = self._input_buffer[0]

            if event == EVENT_BATTERY_DATA:
                if len(self._input_buffer) < 5:
                    break

                self.lastBatteryVoltage = ((self._input_buffer[1] << 8) | self._input_buffer[2]) / 100
                self.lastBatteryCurrentA = ((self._input_buffer[3] << 8) | self._input_buffer[4]) / 1000

                # Handle conversion to signed integer (well, float here, but integer as
                # far as the protocol is concerned)
                if self.lastBatteryCurrentA > 32767 / 1000:
                    self.lastBatteryCurrentA -= 65536 / 1000

                sz = 5

            elif event == EVENT_ON_CTP_CHANGE:
                if len(self._input_buffer) < 2:
                    break

                n = self._input_buffer[1]

                if len(self._input_buffer) < 2 + 6 * n:
                    break

                presses = []
                for i in range(n):
                    x = (self._input_buffer[6*i + 2] << 8) | self._input_buffer[6*i + 3]
                    y = (self._input_buffer[6*i + 4] << 8) | self._input_buffer[6*i + 5]
                    z = (self._input_buffer[6*i + 6] << 8) | self._input_buffer[6*i + 7]
                    presses.append((x, y, z))

                self.presses = presses
                sz = 2 + 6 * n

            else:
                # Invalid
                sz = 1

            self._input_buffer = self._input_buffer[sz:]


class VRAMObject:
    def __init__(self, first_sector, num_sectors, value):
        self.first_sector = first_sector
        self.num_sectors = num_sectors
        self.last_sector = first_sector + num_sectors - 1
        self.value = value
        self.num_accesses = 1


class VRAMCache:
    def __init__(self, total_sectors):
        self.items = []
        self.total_sectors = total_sectors

    def getSectorOf(self, value):
        for i in self.items:
            if i.value == value:
                i.num_accesses += 1
                return i.first_sector

        return None

    def getItemInSector(self, sector):
        for i in self.items:
            if sector >= i.first_sector and sector <= i.last_sector:
                return i

        return None

    def getItemsInSectors(self, first, last):
        found = []
        for i in self.items:
            if last >= i.first_sector and first <= i.last_sector:
                found.append(i)

        return found

    def addItem(self, first_sector, num_sectors, value):
        item = VRAMObject(first_sector, num_sectors, value)

        # Remove things that were overwritten
        items_overwritten = self.getItemsInSectors(item.first_sector, item.last_sector)
        for i in items_overwritten:
            self.items.remove(i)

        self.items.append(item)

    def getFreeChunk(self, size):
        proposed_start = 0
        end = proposed_start + size - 1
        i = 0
        while i < len(self.items):
            if end >= self.items[i].first_sector and proposed_start <= self.items[i].last_sector:
                # Recompute proposed start
                proposed_start = self.items[i].last_sector + 1
                end = proposed_start + size - 1

                # Check if we're out of VRAM
                if end >= self.total_sectors:
                    return None

                # Reset item iterator
                i = 0
            else:
                # Check the next item
                i += 1

        return proposed_start

    def getBestChunk(self, size):
        # TODO: Implement this
        return 0
        


class TabletDisplay:

    def __init__(self, iface):
        self.iface = iface
        self.vram_cache = VRAMCache(1024)

    def setTextColor(self, rgb):
        color = rgb_to_u16(rgb)
        self.iface._sendBytes(COMMAND_DRAW_BITMAP)

    def drawPixel(self, x, y, rgb):
        c = rgb_to_u16(rgb)
        self.iface._sendBytes(COMMAND_DRAW_PIXEL, x >> 8, x & 255, y >> 8, y & 255, c >> 8, c & 255)

    def fillRect(self, x, y, w, h, rgb):
        c = rgb_to_u16(rgb)
        x2 = x + w
        y2 = y + h
        self.iface._sendBytes(
            COMMAND_FILL_RECT,
            x >> 8, x & 255, y >> 8, y & 255,
            x2 >> 8, x2 & 255, y2 >> 8, y2 & 255,
            c >> 8, c & 255
        )

    def drawRect(self, x, y, w, h, rgb):
        c = rgb_to_u16(rgb)
        x2 = x + w
        y2 = y + h
        self.iface._sendBytes(
            COMMAND_DRAW_RECT,
            x >> 8, x & 255, y >> 8, y & 255,
            x2 >> 8, x2 & 255, y2 >> 8, y2 & 255,
            c >> 8, c & 255
        )

    def writeText(self, text):
        for i in range(math.ceil(len(text) / 255)):
            s = text[i*255: i*255 + 255].encode()
            self.iface._sendBytes(COMMAND_WRITE_TEXT, len(s), *s)

    def setCursor(self, x, y):
        self.iface._sendBytes(
            COMMAND_SET_TEXT_CURSOR,
            x >> 8, x & 255,
            y >> 8, y & 255
        )

    def writeVRAM(self, sector, data):
        if type(data) != list:
            raise TypeError("Need a list of 16-bit integers, not " + str(type(data)))
        if len(data) > 256:
            raise ValueError(f"Wrong data size: {len(data)} (expected <= 256 16-bit words)")

        li = []
        for i in data:
            li.append(i >> 8)
            li.append(i & 255)

        if len(li) < 512:
            li += [0] * (512 - len(li))

        self.iface._sendBytes(COMMAND_WRITE_VRAM, sector >> 8, sector & 255, *li)

    def loadBitmap(self, start_sector, image):
        xs = image.width
        ys = image.height

        # Load up the image data because it's not cached
        bitmap_data = []
        for y in range(ys):
            for x in range(xs):
                color = image.getpixel((x, y))
                u16 = rgb_to_u16(color)
                bitmap_data.append(u16)

        num_sectors = math.ceil(xs * ys / 256)
        for i in range(num_sectors):
            self.writeVRAM(start_sector + i, bitmap_data[i*256:(i+1)*256])

        self.vram_cache.addItem(start_sector, math.ceil(xs * ys / 256), image)

    def loadPaletteImage(self, start_sector, image):
        xs = image.width
        ys = image.height

        bitmap_data = []

        # Get color data first
        color_data = image.getpalette()
        n_colors = len(image.getcolors())
        for i in range(n_colors):
            rgb = color_data[i*3:i*3 + 3]
            print(i, rgb)
            bitmap_data.append(rgb_to_u16(rgb))

        print(bitmap_data[:16])
        # Now the pixel data
        for y in range(ys):
            for x in range(0, xs, 4):
                p0 = image.getpixel((x + 0, y))
                p1 = image.getpixel((x + 1, y))
                p2 = image.getpixel((x + 2, y))
                p3 = image.getpixel((x + 3, y))
                bitmap_data.append((p0 << 12) | (p1 << 8) | (p2 << 4) | p3)

        num_sectors = math.ceil((xs * ys / 4 + n_colors) / 256)
        for i in range(num_sectors):
            self.writeVRAM(start_sector + i, bitmap_data[i*256:(i+1)*256])

        self.vram_cache.addItem(start_sector, math.ceil(xs * ys / 256), image)

    def drawLoadedBitmap(self, sector, x, y, w, h):
        self.iface._sendBytes(COMMAND_DRAW_BITMAP, sector >> 8, sector & 255,
                              x >> 8, x & 255,
                              y >> 8, y & 255,
                              w, h)

    def loadImage(self, image):
        xs = image.width
        ys = image.height
        is_palette_image = image.mode == 'P' and (len(image.getcolors()) <= 16)

        # Try to find the image in the cache
        start_sector = self.vram_cache.getSectorOf(image)

        if start_sector is None:
            if is_palette_image:
                num_sectors = math.ceil((xs * ys / 4 + 16) / 256)
            else:
                num_sectors = math.ceil(xs * ys / 256)
            start_sector = self.vram_cache.getFreeChunk(num_sectors)

            if start_sector is None:
                # No free space of the requested size
                start_sector = self.vram_cache.getBestChunk(num_sectors)

            # Load up the image data because it's not cached
            if is_palette_image:
                self.loadPaletteImage(start_sector, image)
            else:
                self.loadBitmap(start_sector, image)

        return start_sector

    def drawImage(self, xp, yp, image):
        xs = image.width
        ys = image.height
        is_palette_image = image.mode == 'P' and (len(image.getcolors()) <= 16)

        # We could use multiple draws to do large images later
        if xs > 255 or ys > 255:
            raise ValueError("Image must be within 256x256 pixels")

        start_sector = self.loadImage(image)

        # Draw the image
        if is_palette_image:
            self.drawPaletteImage(start_sector, xp, yp, xs, ys, len(image.getcolors()))
        else:
            self.drawLoadedBitmap(start_sector, xp, yp, xs, ys)

    def drawPaletteImage(self, sector, x, y, w, h, paletteSize):
        self.iface._sendBytes(COMMAND_DRAW_PALETTE_IMAGE, sector >> 8, sector & 255,
                              x >> 8, x & 255,
                              y >> 8, y & 255,
                              w, h, paletteSize)

    def fillScreen(self, rgb):
        color = rgb_to_u16(rgb)
        self.iface._sendBytes(
            COMMAND_FILL_DISPLAY,
            color >> 8, color & 255
        )

    def getWidth(self):
        # TODO: Make this a real protocol/api thing
        return 800

    def getHeight(self):
        # TODO: Make this a real protocol/api thing
        return 480

