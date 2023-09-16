import time

import helper_clock
import helper_display
import helper_print
import helper_rtc
import helper_server
import helper_timer
import helper_wifi
import helper_sd_card

import board
import pulseio
import adafruit_irremote

debug = False
last_neopixel_colors = helper_clock.neopixel_colors


def set_time():
    global time_set

    timeset_status = helper_rtc.Set_RTC_Time(debug)
    time_set = timeset_status["successful"]
    helper_print.debug_msg(debug, timeset_status)


# ***** SETUP *****
time_set = False
helper_display.setup_display()
helper_sd_card.sd_card_setup(debug)
helper_wifi.setup_wifi(debug)


helper_display.update_display(1, f"IP: {helper_wifi.wifi_ip()}")
helper_display.update_display(2, "Current Time")
helper_display.update_display(3, "")

last_update = time.monotonic()  # Time in seconds since power on

helper_clock.clock_setup()

set_time()

# helper_clock.update_clock_display(helper_rtc.str_time())

server_is_available = helper_server.server_setup(debug)

# ***** MAIN LOOP *****
last_function = helper_server.function
last_test_step = 0
last_time_display = ""
current_time_display = ""
last_test_update = time.monotonic()  # Time in seconds since power on

# IR Remote Setup
pulsein = pulseio.PulseIn(board.A0, maxlen=120, idle_state=True)
decoder = adafruit_irremote.NonblockingGenericDecode(pulsein)
t0 = next_heartbeat = time.monotonic()

def read_ir():
    global next_heartbeat

    for message in decoder.read():
        print(f"t={time.monotonic() - t0:.3} New Message")
        print("Heard", len(message.pulses), "Pulses:", message.pulses)
        if isinstance(message, adafruit_irremote.IRMessage):
            print("Decoded:", message.code)
        elif isinstance(message, adafruit_irremote.NECRepeatIRMessage):
            print("NEC repeat!")
        elif isinstance(message, adafruit_irremote.UnparseableIRMessage):
            print("Failed to decode", message.reason)
        print("----------------------------")

    # This heartbeat confirms that we are not blocked somewhere above.
    t = time.monotonic()
    if t > next_heartbeat:
        # print(f"t={time.monotonic() - t0:.3} Heartbeat")
        next_heartbeat = t + 0.1

while True:
    read_ir()
    if time.monotonic() - last_update >= 0.1:
        if not time_set:
            set_time()

        helper_display.update_display(1, f"IP: {helper_wifi.wifi_ip()}")

        last_update = time.monotonic()

        if time_set:
            helper_display.update_display(3, helper_rtc.str_time())

        if last_function == helper_server.FUNCTION_SPEECH_TIMER:
            timer = helper_timer.get_remaining_time()
            last_colors = helper_clock.neopixel_colors
            helper_clock.neopixel_colors = timer["color"]
            current_time_display = timer["timestring"]
        elif last_function == helper_server.FUNCTION_CLOCK:
            current_time_display = "-- --"

            if time_set:
                current_time_display = helper_rtc.str_time()
        elif last_function == helper_server.FUNCTION_TEST:
            if time.monotonic() - last_test_update >= 1.0:
                last_test_update = time.monotonic()
                current_time_display = "88:88"

                if last_test_step > 6:
                    last_test_step = 0

                if last_test_step == 0:
                    helper_clock.neopixel_colors = [128, 0, 0]
                elif last_test_step == 1:
                    helper_clock.neopixel_colors = [0, 128, 0]
                elif last_test_step == 2:
                    helper_clock.neopixel_colors = [0, 0, 128]
                elif last_test_step == 3:
                    helper_clock.neopixel_colors = [0, 128, 128]
                elif last_test_step == 4:
                    helper_clock.neopixel_colors = [128, 0, 128]
                elif last_test_step == 5:
                    helper_clock.neopixel_colors = [128, 128, 0]
                elif last_test_step == 6:
                    helper_clock.neopixel_colors = [128, 128, 128]

                last_test_step = last_test_step + 1
                helper_clock.update_clock_display(current_time_display)

    if current_time_display != last_time_display:
        helper_clock.update_clock_display(current_time_display)
        last_time_display = current_time_display

    if last_function != helper_server.function:
        last_function = helper_server.function
        if last_function == helper_server.FUNCTION_SPEECH_TIMER:
            last_neopixel_colors = helper_clock.neopixel_colors
            helper_timer.timer_min = helper_server.speech_timer_min
            helper_timer.timer_max = helper_server.speech_timer_max
            helper_timer.start_timer()
        elif last_function == helper_server.FUNCTION_TEST:
            last_test_step = 0

    #  poll the server for incoming/outgoing requests
    if server_is_available:
        try:
            # Process any waiting requests
            poll_result = helper_server.server.poll()

            if poll_result == helper_server.SERVER_REQUEST_HANDLED_RESPONSE_SENT:
                # Do something only after handling a request
                pass

        except BrokenPipeError as e:
            helper_print.debug_msg(True, f"Exception: {e}")
            server_is_available = helper_server.server_setup(debug)
            continue
