import math
import time
import traceback
from typing import List

from PIL.Image import Image

from utils.protocol import TabletInterface, TabletDisplay
from utils.stringutil import splitlines


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

                if left <= x <= right and y >= button_top and y <= button_bottom:
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


class UIElement:
    def __init__(self, x, y):
        """x and y arguments are the location of the element (center of element is subclass dependant)"""
        self.x = x
        self.y = y

    def update(self):
        pass

    def onPress(self, x, y):
        pass

    def onRelease(self, x, y):
        pass

    def onClick(self, x, y):
        pass

    def render(self, display: TabletDisplay, x, y):
        """x and y arguments are the location of the upper-left corner of the window"""
        raise NotImplementedError("Must be implemented in subclass")

    def onDrag(self, x1, y1, x2, y2):
        pass


class ApplicationWindow:
    # Maximum amount of time a click can last - the user must tap and untap within this time
    # for it to count as a click.  Otherwise it becomes a drag.  Measured in seconds.
    PRESS_CLICK_TIME_CUTOFF = 0.5

    def __init__(self, elements: List[UIElement]):
        self.elements = elements
        self.keepRunningMainLoop = False
        self.pressTimer = 0
        self.wasPressed = False
        self.lastPress = None

    def update(self):
        for i in self.elements:
            i.update()

    def render(self, display: TabletDisplay):
        for i in self.elements:
            i.render(display, 0, 0)

    def mainloop(self, tablet: TabletInterface):
        display = tablet.getDisplay()
        self.render(display)

        self.keepRunningMainLoop = True
        while self.keepRunningMainLoop:
            time.sleep(0.010)
            presses = tablet.getPresses()
            if len(presses) == 1:
                press = presses[0]
                if not self.wasPressed:
                    self.wasPressed = True
                    self.lastPress = press
                    self.pressTimer = time.time()
                    for i in self.elements:
                        i.onPress(self.lastPress[0], self.lastPress[1])

                elif self.pressTimer + self.PRESS_CLICK_TIME_CUTOFF < time.time():
                    # Dragging the cursor
                    for i in self.elements:
                        i.onDrag(self.lastPress[0], self.lastPress[1], press[0], press[1])
                    self.lastPress = press

            elif len(presses) == 0:
                # Unpress
                if self.lastPress is not None:
                    for i in self.elements:
                        i.onRelease(self.lastPress[0], self.lastPress[1])
                    if self.wasPressed and self.pressTimer + self.PRESS_CLICK_TIME_CUTOFF > time.time():
                        # Click
                        for i in self.elements:
                            i.onClick(self.lastPress[0], self.lastPress[1])

                self.wasPressed = False

            else:
                # Multitouch
                self.wasPressed = False
                for point in presses:
                    x = point[0]
                    y = point[1]

                    # What do we do with multitouch?

    def stopMainLoop(self):
        """
        You can easily use this as the callback for a button, as long as you don't have cleanup to do before
        your application exits.
        """
        self.keepRunningMainLoop = False


class TextButtonElement(UIElement):
    def __init__(self, x, y, width, height,
                 text: str, color, callback = None, xPad = 3, yPad = 2,
                 pressColor=None, display: TabletDisplay = None,
                 textColor=0xFFFFFF):
        """x and y arguments are the location of the element"""
        super().__init__(x, y)
        self.width = width
        self.height = height
        self.xPad = xPad
        self.yPad = yPad
        self.color = color
        self.pressColor = pressColor
        self.display = display
        self.text = text
        self.textColor = textColor
        self.cb = callback

    def onPress(self, x, y):
        # Don't process the event if it's not in this button
        if x < self.x or y < self.y or x > self.x + self.width or y > self.y + self.height:
            return

        if self.pressColor is not None and self.display is not None:
            self.display.fillRect(self.x, self.y, self.width, self.height, self.pressColor)
            self.display.setCursor(self.x + self.xPad, self.y + self.yPad)
            self.display.setTextColor(self.textColor)
            self.display.writeText(self.text)

    def onRelease(self, x, y):
        # Don't process the event if it's not in this button
        if x < self.x or y < self.y or x > self.x + self.width or y > self.y + self.height:
            return

        if self.pressColor is not None and self.display is not None:
            self.render(self.display, 0, 0)

    def onClick(self, x, y):
        # Don't process the event if it's not in this button
        if x < self.x or y < self.y or x > self.x + self.width or y > self.y + self.height:
            return

        if self.cb is not None:
            self.cb(self)

    def render(self, display: TabletDisplay, x, y):
        """x and y arguments are the location of the upper-left corner of the window"""
        self.display = display

        display.fillRect(self.x, self.y, self.width, self.height, self.color)
        display.setCursor(self.x + self.xPad, self.y + self.yPad)
        display.setTextColor(self.textColor)
        display.writeText(self.text)


class IconButtonElement(UIElement):
    TEXT_X_PAD = 2
    TEXT_Y_PAD = 2

    def __init__(self, x, y, icon: Image, text: str, callback = None,
                 display: TabletDisplay = None,
                 textColor=0xFFFFFF):
        """x and y arguments are the location of the element"""
        super().__init__(x, y)
        self.width = icon.width
        self.height = icon.height
        self.display = display
        self.textColor = textColor
        self.cb = callback
        self.icon = icon

        text = text.strip()

        # Now we need to intelligently split the characters.
        num_chars_per_line = math.floor((self.width - self.TEXT_X_PAD*2) / 8)
        self.text = splitlines(text, ' ', num_chars_per_line)

        self.height += 12 * len(self.text)

    def onClick(self, x, y):
        # Don't process the event if it's not in this button
        if x < self.x or y < self.y or x > self.x + self.width or y > self.y + self.height:
            return

        if self.cb is not None:
            self.cb(self)

    def render(self, display: TabletDisplay, x, y):
        """x and y arguments are the location of the upper-left corner of the window"""
        self.display = display
        x += self.x
        y += self.y

        display.drawImage(x, y, self.icon)
        for i in range(len(self.text)):
            line = self.text[i]
            display.setCursor(x + self.TEXT_X_PAD, y + self.height + self.TEXT_Y_PAD + i*12)
            display.setTextColor(self.textColor)
            display.writeText(line)


class MarkdownElement(UIElement):
    def __init__(self, x, y, w, h, markdown):
        super().__init__(x, y)
        self.width = w
        self.height = h
        self.markdown = markdown

    def render(self, display: TabletDisplay, x, y):
        x += self.x
        y += self.y
        display.setCursor(x, y)
        display.setTextColor(0xE0E0E0)

        display.writeText(self.markdown)
