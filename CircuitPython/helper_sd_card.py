import board
import busio
import digitalio
import adafruit_sdcard
import storage
import os

cd = None

def sd_card_setup(debug):
    global cd

    # ************ Initialize SD Card ************
    # https://learn.adafruit.com/micropython-hardware-sd-cards/tdicola-circuitpython
    spi = busio.SPI(board.GP10, MOSI=board.GP11, MISO=board.GP8)
    SD_CS = board.GP9
    SD_CD = board.GP12
    cs = digitalio.DigitalInOut(SD_CS)
    cd = digitalio.DigitalInOut(SD_CD)

    if sd_card_present():
        sdcard = adafruit_sdcard.SDCard(spi, cs)
        vfs = storage.VfsFat(sdcard)

        storage.mount(vfs, "/sd")

        if debug:
            print("Files on filesystem:")
            print("====================")
            print_directory("/sd")
    else:
        if debug:
            print("No SD Card")

def sd_card_present():
    return cd.value

def print_directory(path, tabs=0):
    if not sd_card_present():
        return

    for file in os.listdir(path):
        stats = os.stat(path + "/" + file)
        filesize = stats[6]
        isdir = stats[0] & 0x4000

        if filesize < 1000:
            sizestr = str(filesize) + " by"
        elif filesize < 1000000:
            sizestr = "%0.1f KB" % (filesize / 1000)
        else:
            sizestr = "%0.1f MB" % (filesize / 1000000)

        prettyprintname = ""
        for _ in range(tabs):
            prettyprintname += "   "
        prettyprintname += file
        if isdir:
            prettyprintname += "/"
        print('{0:<40} Size: {1:>10}'.format(prettyprintname, sizestr))

        # recursively print directory contents
        if isdir:
            print_directory(path + "/" + file, tabs + 1)