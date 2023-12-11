import board
import busio
import displayio
import terminalio
from adafruit_display_text import label
import adafruit_displayio_ssd1306
import gc

WIDTH = 128
HEIGHT = 32  # Change to 64 if needed
LINE_COUNT = 3
LINE_CHARS = 21
splash = displayio.Group()
TEXT_LINES = []

def setup_display():
    # ************ Initialize Display ************
    # https://learn.adafruit.com/monochrome-oled-breakouts/circuitpython-usage
    # https://www.instructables.com/Raspberry-Pi-Pico-With-I2C-Oled-Display-and-Circui/
    displayio.release_displays()

    i2c = busio.I2C(scl=board.GP1, sda=board.GP0)  # This RPi Pico way to call I2C
    display_bus = displayio.I2CDisplay(i2c, device_address=0x3c)

    BORDER = 5

    display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=WIDTH, height=HEIGHT)

    # Make the display context
    
    display.show(splash)
    color_bitmap = displayio.Bitmap(WIDTH, HEIGHT, 1)
    color_palette = displayio.Palette(1)
    color_palette[0] = 0xFFFFFF  # White

    bg_sprite = displayio.TileGrid(
        color_bitmap, pixel_shader=color_palette, x=0, y=0)
    splash.append(bg_sprite)

    # Draw a smaller inner rectangle
    inner_bitmap = displayio.Bitmap(WIDTH - BORDER * 2, HEIGHT - BORDER * 2, 1)
    inner_palette = displayio.Palette(1)
    inner_palette[0] = 0x000000  # Black
    inner_sprite = displayio.TileGrid(
        inner_bitmap, pixel_shader=inner_palette, x=BORDER, y=BORDER
    )
    splash.append(inner_sprite)

    # Draw a label
    text = "Speech Timer v1"
    text_area = label.Label(
        terminalio.FONT, text=text, color=0xFFFFFF, x=20, y=HEIGHT // 2 - 1
    )
    splash.append(text_area)

def center_text(text):
    spacestoadd = int((LINE_CHARS - len(text))/2)
    if spacestoadd < 0:
        spacestoadd = 0

    return (" " * spacestoadd) + text

def clear_splash_screen():
    if splash is None:
        return

    # ************ Display - Clear the splash screen ************
    splash.pop()
    splash.pop()
    splash.pop()

    # ************ Display - Add labels for lines ************
    text = "23456789012345678901"
    pixels_per_line = int(HEIGHT / LINE_COUNT) + 1

    line_y = int(pixels_per_line / 2)
    for i in range(LINE_COUNT):
        TEXT_LINES.append(label.Label(terminalio.FONT, text=str(i + 1) + text, color=0xFFFFFF, x=0, y=line_y))
        line_y = line_y + pixels_per_line
        splash.append(TEXT_LINES[i])

    gc.collect()

def update_display(line, message, center=True):
    if len(TEXT_LINES) == 0:
        clear_splash_screen()
    
    if splash is None or len(TEXT_LINES) == 0 or line > len(TEXT_LINES) or line < 1:
        return

    if center:
        message = center_text(message)
    
    TEXT_LINES[line - 1].text = message
    gc.collect()