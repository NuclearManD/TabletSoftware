
from PIL import Image

def compressToPaletteImage(image):
    return image.convert("P", palette=Image.ADAPTIVE, colors=16)
    
