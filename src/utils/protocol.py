

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
    if type(rgb) == tuple:
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
        self._input_buffer

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


class TabletDisplay:

    def __init__(self, iface):
        self.iface = iface

    def setTextColor(self, rgb):
        color = rgb_to_u16(rgb)
        self.iface._sendBytes(COMMAND)
