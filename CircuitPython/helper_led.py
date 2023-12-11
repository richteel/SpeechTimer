import board
import digitalio

# ************ Initialize LED ************
#  onboard LED setup
led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT
led.value = False