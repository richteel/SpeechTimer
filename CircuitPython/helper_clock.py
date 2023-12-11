import board
import neopixel
import gc
import time

import helper_rtc

NUM_OF_PIXELS = 58
pixels = None
digits = "0123456789 "
segments = [
    # a,b,c,d,e,f,g
    [1, 1, 1, 1, 1, 1, 0],    # 0
    [0, 1, 1, 0, 0, 0, 0],    # 1
    [1, 1, 0, 1, 1, 0, 1],    # 2
    [1, 1, 1, 1, 0, 0, 1],    # 3
    [0, 1, 1, 0, 0, 1, 1],    # 4
    [1, 0, 1, 1, 0, 1, 1],    # 5
    [1, 0, 1, 1, 1, 1, 1],    # 6
    [1, 1, 1, 0, 0, 0, 0],    # 7
    [1, 1, 1, 1, 1, 1, 1],    # 8
    [1, 1, 1, 0, 0, 1, 1],    # 9
    [0, 0, 0, 0, 0, 0, 0],    # Space
    [0, 0, 0, 0, 0, 0, 1]     # dash
]

# digits_clock = [" ", " ", ":", " ", " "]

digit_segments = [
    [0, 0, 0, 0, 0, 0, 0],    # hours tens
    [0, 0, 0, 0, 0, 0, 0],    # hours ones
    [0, 0, 0, 0, 0, 0, 0],    # colon, only first two used
    [0, 0, 0, 0, 0, 0, 0],    # minutes tens
    [0, 0, 0, 0, 0, 0, 0]     # minutes ones
]

neopixel_colors = [255, 255, 255]

neopixels_per_segment = 2
neopixels_for_colon_segment = 1


def clock_clear():
    for i in range(NUM_OF_PIXELS):
        pixels[i] = (0, 0, 0)
    pixels.show()


def clock_setup():
    global pixels

    pixels = neopixel.NeoPixel(
        pin=board.GP28, n=NUM_OF_PIXELS, pixel_order=neopixel.RGB)
    pixels.brightness = 0.1
    # pixels.fill((neo_red, neo_green, neo_blue))

    update_clock_display("-- --")


def update_clock_display(digits_clock):
    # Make certain we are attempting to display max of 5 characters
    if len(digits_clock) > 5:
        digits_clock = digits_clock[0:5]
    # need to reverse the string as it is wored with least significant digit first
    # set the segments on or off in the array
    for idx, dig in enumerate(reversed(digits_clock)):
        digit_segments[idx] = [0, 0, 0, 0, 0, 0, 0]
        if idx == 2:
            if dig == ":":
                digit_segments[idx] = [1, 1, 0, 0, 0, 0, 0]
            elif dig == ".":
                digit_segments[idx] = [1, 0, 0, 0, 0, 0, 0]
            elif dig == "-":
                digit_segments[idx] = [0, 1, 0, 0, 0, 0, 0]
        else:
            digit_idx = 10
            try:
                digit_idx = int(dig)
            except:
                digit_idx = 10
                if dig == "-":
                    digit_idx = 11

            if digit_idx > len(digits):
                digit_idx = 10

            digit_segments[idx] = segments[digit_idx]

    neopixel_idx = 0
    for num_idx, num_segments in enumerate(digit_segments):
        for seg_idx, seg_pixel in enumerate(num_segments):
            if num_idx == 2:
                if seg_idx < 2:
                    for i in range(neopixels_for_colon_segment):
                        # print(f"neopixel_idx = {neopixel_idx}\tnum_idx = {num_idx}\tseg_idx = {seg_idx}\ti = {i}")
                        pixels[neopixel_idx] = (
                            neopixel_colors[0] * seg_pixel, neopixel_colors[1] * seg_pixel, neopixel_colors[2] * seg_pixel)
                        neopixel_idx = neopixel_idx + 1
            else:
                for i in range(neopixels_per_segment):
                    # print(f"neopixel_idx = {neopixel_idx}\tnum_idx = {num_idx}\tseg_idx = {seg_idx}\ti = {i}")
                    pixels[neopixel_idx] = (
                        neopixel_colors[0] * seg_pixel, neopixel_colors[1] * seg_pixel, neopixel_colors[2] * seg_pixel)
                    neopixel_idx = neopixel_idx + 1

    pixels.show()
    gc.collect()
