
from PIL import Image
import time

from ui import *

class Application:
    def __init__(self, name, icon, color, main_f):
        if icon is not None and icon.mode != 'P':
            icon = icon.convert("P", palette=Image.ADAPTIVE, colors=16)
        self.name = name
        self.icon = icon
        self.color = color
        self.main = main_f


class SystemShell:
    APP_SIZE = 96

    PADDING = 15

    def __init__(self, applications):
        self.applications = applications

    def loadImages(self, display):
        for app in self.applications:
            if app.icon is not None:
                display.loadImage(app.icon)

    def drawHome(self, display):
        # Draw background
        #display.fillScreen(0x202020)
        display.fillScreen(0)

        # Draw each app icon
        for i in range(len(self.applications)):
            app = self.applications[i]

            # TODO: Support filling the screen
            x = ((self.APP_SIZE + self.PADDING) * i) + self.PADDING
            y = self.PADDING

            if app.icon is None:
                display.fillRect(x, y, self.APP_SIZE, self.APP_SIZE, app.color)
            else:
                display.drawImage(x, y, app.icon)
            display.setCursor(x, y + self.APP_SIZE + 1)
            display.writeText(app.name)

    def mainloop(self, tablet):
        display = tablet.getDisplay()
        self.drawHome(display)
        while True:
            time.sleep(0.010)
            for point in tablet.getPresses():
                x = point[0]
                y = point[1]
                col = (x - self.PADDING) // (self.APP_SIZE + self.PADDING)
                row = 0

                try:
                    app_idx = col
                    if app_idx < len(self.applications):
                        app = self.applications[app_idx]
                        app.main(tablet)
                except Exception as e:
                    self.drawHome(display)
                    popup = ErrorPopupBox(str(e))
                    popup.mainloop(tablet)

                # Re-render the homescreen
                self.drawHome(display)
