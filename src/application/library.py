import json
import os

from PIL import Image

from shell import Application
from utils.protocol import TabletInterface
from ui import ApplicationWindow, TextButtonElement, IconButtonElement, MarkdownElement


class Book:
    def __init__(self, path):
        # Load the book's data
        info_path = os.path.join(path, 'info.json')
        icon_path = os.path.join(path, 'icon.png')
        with open(info_path) as f:
            data = json.load(f)
            self.name = data['name']
            self.chapters = data['chapters']

        self.icon = Image.open(icon_path)
        self.path = path

    def getIconAsElement(self, x, y, callback):
        return IconButtonElement(x, y, self.icon, self.name, callback)


def bookMiniApp(tablet: TabletInterface, root:str, chapters: dict):
    def back_function(*anything):
        window.stopMainLoop()

    display = tablet.getDisplay(0)
    display.fillScreen(0)
    elements = [
        TextButtonElement(20, 20, 100, 20, "<- Back", 0x808080, callback=back_function, pressColor=0x606060)
    ]
    i = 0
    for k, v in chapters.items():
        if type(v) == dict:
            def callback(x, chapters=v):
                bookMiniApp(tablet, root, chapters)
                display.fillScreen(0)
                window.render(display)
        elif type(v) == str:
            def callback(x, name=v):
                pageMiniApp(tablet, os.path.join(root, name))
                display.fillScreen(0)
                window.render(display)
        else:
            continue
        elements.append(TextButtonElement(30, 40 + i*20, 480, 15, k, 0x202020, callback))
        i += 1

    window = ApplicationWindow(elements)
    window.mainloop(tablet)


def pageMiniApp(tablet: TabletInterface, path: str):
    def back_function(*anything):
        window.stopMainLoop()

    with open(path) as f:
        markdown = f.read()

    display = tablet.getDisplay(0)
    display.fillScreen(0)
    elements = [
        TextButtonElement(20, 20, 100, 20, "<- Back", 0x808080, callback=back_function, pressColor=0x606060),
        MarkdownElement(10, 30, 780, 440, markdown, os.path.dirname(path))
    ]

    window = ApplicationWindow(elements)
    window.mainloop(tablet)


class LibraryApp(Application):
    def __init__(self):
        super().__init__("Library", Image.open("../res/library_icon.png"), 0x101070, self.main)

        self.window = None

    def main(self, tablet: TabletInterface):
        books = [Book(f'../books_builtin/{i}') for i in os.listdir('../books_builtin')]

        display = tablet.getDisplay(0)
        display.fillScreen(0)
        elements = [
            TextButtonElement(20, 20, 100, 20, "<- Exit", 0x808080, callback=self.exitcb, pressColor=0x606060)
        ]
        for i in range(len(books)):
            def callback(x, i=i):
                bookMiniApp(tablet, books[i].path, books[i].chapters)
                display.fillScreen(0)
                self.window.render(display)
            elements.append(books[i].getIconAsElement(5 + i * 80, 50, callback))

        self.window = ApplicationWindow(elements)
        self.window.mainloop(tablet)

    def exitcb(self, button):
        self.window.stopMainLoop()
