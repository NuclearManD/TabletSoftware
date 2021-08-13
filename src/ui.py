
import time, math, traceback

class PopupBox:

    BUTTON_HEIGHT = 32
    PADDING = 5

    def __init__(self, width, height, buttons, content, bgcolor=0xF05050):
        self.width = width
        self.height = height
        self.buttons = buttons
        self.content = content
        self.bgcolor = bgcolor

    def drawContent(self, display):
        if type(self.content) == str:
            left = (display.getWidth() - self.width) // 2
            top = (display.getHeight() - self.height) // 2
            text_cols = (self.width - self.PADDING * 2) // 8

            i = 0
            remaining = self.content.replace('\r', '\n')
            while len(remaining) > 0:
                y = top + self.PADDING + i * 12
                if '\n' in remaining[:text_cols]:
                    split_idx = remaining.find('\n')
                else:
                    split_idx = text_cols
                next_line = remaining[:split_idx]
                remaining = remaining[split_idx + 1:]
                display.setCursor(left + self.PADDING, y)
                display.writeText(next_line)
                i+=1
        else:
            self.content(display)

    def draw(self, display):
        left = (display.getWidth() - self.width) // 2
        top = (display.getHeight() - self.height) // 2
        button_bottom = top + self.height
        button_top = button_bottom - self.BUTTON_HEIGHT
        button_width = self.width // len(self.buttons)

        display.fillRect(left, top, self.width, self.height - self.BUTTON_HEIGHT, self.bgcolor)

        self.drawContent(display)
        display.fillRect(left, button_top, self.width, button_bottom, 0x000000)

        for i in range(len(self.buttons)):
            x = i * button_width + left
            display.drawRect(x, button_top, button_width, self.BUTTON_HEIGHT, self.bgcolor)
            display.setCursor(x + 1, button_top + 1)
            display.writeText(self.buttons[i])

    def mainloop(self, tablet):
        display = tablet.getDisplay()
        left = (display.getWidth() - self.width) // 2
        right = left + self.width
        top = (display.getHeight() - self.height) // 2
        button_bottom = top + self.height
        button_top = button_bottom - self.BUTTON_HEIGHT

        self.draw(display)
        while True:
            time.sleep(0.010)
            for point in tablet.getPresses():
                x = point[0]
                y = point[1]

                if x >= left and x <= right and y >= button_top and y <= button_bottom:
                    # press
                    button_id = ((x - left) * len(self.buttons)) // self.width

                    # Now we need to wait for the press to end, otherwise the next program will get a press
                    while len(tablet.getPresses()) > 0:
                        time.sleep(0.010)

                    return self.buttons[button_id]


class ErrorPopupBox(PopupBox):

    def __init__(self, error, width=480, height=320):
        if isinstance(error, Exception):
            content = "".join(traceback.TracebackException.from_exception(error).format())
        else:
            content = str(error)

        super().__init__(width, height, ["Ok"], content, bgcolor=0x3030F0)

