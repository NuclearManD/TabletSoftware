
import time, math

class PopupBox:

    BUTTON_HEIGHT = 32
    PADDING = 5

    def __init__(self, width, height, buttons, content, bgcolor=0x5050F0):
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
            for i in range(math.ceil(len(self.content) / text_cols)):
                y = top + self.PADDING + i * 12
                display.setCursor(left + self.PADDING, y)
                display.writeText(self.content[text_cols*i:text_cols * (i+1)])
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
                    return self.buttons[button_id]
