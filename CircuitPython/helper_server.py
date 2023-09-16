from adafruit_httpserver import Server, Request, Response, POST
import binascii
import gc
import microcontroller
import os
import time

import helper_clock
import helper_led
import helper_print
import helper_wifi

FUNCTION_CLOCK = "CLOCK"
FUNCTION_SPEECH_TIMER = "SPEECH_TIMER"
FUNCTION_TEST = "TEST"
SPEECH_TIMER_START_TEXT = "START"
SPEECH_TIMER_STOP_TEXT = "STOP"

server_variables = {}
server = None
function = FUNCTION_CLOCK
speech_timer_runing = False
speech_timer_min = 5
speech_timer_max = 7

SERVER_REQUEST_HANDLED_RESPONSE_SENT = "request_handled_response_sent"


def server_setup(debug=False):
    global server

    if not helper_wifi.wifi_connected():
        helper_wifi.wifi_connect(debug)

    if not helper_wifi.wifi_connected():
        return False

    server_folder = "/static"
    helper_print.debug_msg(debug, f"helper_wifi.pool: {helper_wifi.pool}")
    if path_exists('/sd/wwwroot/index.html'):
        server_folder = "/sd/wwwroot"
    elif path_exists("/wwwroot/index.html"):
        server_folder = "/wwwroot"
        
    server = Server(helper_wifi.pool, server_folder, debug=debug)
    # server.require_authentication([Basic("clock", "toastmasters")])

    server_variables["visits"] = 0
    server_variables["selected_tab"] = 0
    server_variables["min_time"] = 5
    server_variables["max_time"] = 7
    server_variables["timer_button_text"] = SPEECH_TIMER_START_TEXT

    server_variables["server_folder"] = server_folder
    server_variables["cpu_freq"] = f"{microcontroller.cpu.frequency:,}"
    server_variables["reset_reason"] = microcontroller.cpu.reset_reason
    # https://programtalk.com/python-more-examples/microcontroller.cpu.uid/
    server_variables["cpu_uid"] = binascii.hexlify(
        microcontroller.cpu.uid).decode("utf-8")
    server_variables["cpu_voltage"] = microcontroller.cpu.voltage

    #  route default static IP
    @server.route("/?")
    def base(request: Request):  # pylint: disable=unused-argument
        #  serve the HTML f string
        #  with content type text/html
        server_variables["visits"] = server_variables["visits"] + 1
        helper_print.debug_msg(
            debug, f"Base Visits = {server_variables['visits']}")
        return Response(request, f"{webpage()}", content_type='text/html')

    #  if a button is pressed on the site
    @server.route("/", POST)
    def buttonpress(request: Request):
        #  get the raw text
        # raw_text = request.raw_request.decode("utf8")
        form_data = parse_request_body(request)

        if form_data is None:
            return Response(request, f"{webpage()}", content_type='text/html')

        process_form_data(form_data)

        return Response(request, f"{webpage()}", content_type='text/html')

    helper_print.debug_msg(debug, "starting server..")
    # startup the server

    try:
        helper_print.debug_msg(debug, f"try server: {server}")
        server.start(str(helper_wifi.wifi_ip()))
        helper_print.debug_msg(
            debug, f"Listening on http://{helper_wifi.wifi_ip()}:80")
    #  if the server fails to begin, restart the pico w
    except OSError:
        time.sleep(5)
        helper_print.debug_msg(debug, "restarting..")
        microcontroller.reset()

    return True


def parse_request_body(request: Request):
    # Example body = "myid=000&LED_ON=ON"
    raw_form_data = request.body.decode('utf8')

    if len(raw_form_data) == 0:
        return None

    key_val_pairs = raw_form_data.split("&")
    retval = {}

    for pair in key_val_pairs:
        key_val = pair.split("=")
        if len(key_val) == 2:
            retval[key_val[0]] = key_val[1]

    return retval


def process_form_data(form_data):
    global function, speech_timer_runing, speech_timer_min, speech_timer_max

    speech_timer_runing = False

    if "TAB" in form_data:
        server_variables["selected_tab"] = form_data['TAB']
    if "LED" in form_data and form_data['LED'] == "ON":
        #  turn on the onboard LED
        helper_led.led.value = True
    elif "LED" in form_data and form_data['LED'] == "OFF":
        #  turn the onboard LED off
        helper_led.led.value = False

    if "CLOCK" in form_data:
        function = FUNCTION_CLOCK
        helper_clock.neopixel_colors = [255, 255, 255]
        server_variables["timer_button_text"] = SPEECH_TIMER_START_TEXT
        #  Change the Neopixel colors
        try:
            helper_clock.neopixel_colors[0] = int(form_data['RED'])
        except:
            print("ERROR: Failed to get red value")
        try:
            helper_clock.neopixel_colors[1] = int(form_data['GREEN'])
        except:
            print("ERROR: Failed to get green value")
        try:
            helper_clock.neopixel_colors[2] = int(form_data['BLUE'])
        except:
            print("ERROR: Failed to get blue value")

    if "MIN_TIME" in form_data:
        try:
            server_variables["min_time"] = int(form_data["MIN_TIME"])
            if server_variables["min_time"] < 1:
                server_variables["min_time"] = 1
        except:
            print("ERROR: Failed to get min time value")

    if "MAX_TIME" in form_data:
        try:
            server_variables["max_time"] = int(form_data["MAX_TIME"])
            if server_variables["max_time"] < 1:
                server_variables["max_time"] = 1
        except:
            print("ERROR: Failed to get max time value")

    if server_variables["min_time"] > server_variables["max_time"]:
        temp_time = server_variables["max_time"]
        server_variables["max_time"] = server_variables["min_time"]
        server_variables["min_time"] = temp_time

    # Determine function
    if FUNCTION_SPEECH_TIMER in form_data:
        if form_data[FUNCTION_SPEECH_TIMER] == SPEECH_TIMER_START_TEXT:
            function = FUNCTION_SPEECH_TIMER
            speech_timer_min = server_variables["min_time"]
            speech_timer_max = server_variables["max_time"]
            speech_timer_runing = True
            server_variables["timer_button_text"] = SPEECH_TIMER_STOP_TEXT
        else:
            function = ""
            speech_timer_runing = False
            server_variables["timer_button_text"] = SPEECH_TIMER_START_TEXT
    
    if FUNCTION_TEST in form_data:
        if form_data[FUNCTION_TEST] == FUNCTION_TEST:
            function = FUNCTION_TEST

    #  reload site
    server_variables["visits"] = server_variables["visits"] + 1


def update_server_varables():
    gc.collect()
    mem_used = gc.mem_alloc()
    mem_free = gc.mem_free()
    mem_total = mem_used + mem_free

    server_variables["neo_red"] = helper_clock.neopixel_colors[0]
    server_variables["neo_green"] = helper_clock.neopixel_colors[1]
    server_variables["neo_blue"] = helper_clock.neopixel_colors[2]

    server_variables["cpu_temp_c"] = f"{microcontroller.cpu.temperature:.2f}"
    server_variables["cpu_temp_f"] = f"{microcontroller.cpu.temperature * 1.8 + 32:.2f}"
    server_variables["mem_free"] = f"{mem_free:,}"
    server_variables["mem_used"] = f"{mem_used:,}"
    server_variables["mem_total"] = f"{mem_total:,}"
    server_variables["mem_percent_used"] = f"{(mem_used/mem_total)*100:.1f}"
    server_variables["mem_percent_free"] = f"{(mem_free/mem_total)*100:.1f}"


def webpage():
    html = ""
    webpage_file = "/sd/wwwroot/index.html"
    if not path_exists(webpage_file):
        webpage_file = "/wwwroot/index.html"

    if path_exists(webpage_file):
        with open(webpage_file, "r") as f:
            html = f.read()

        update_server_varables()

        html = html.format(**server_variables)
    else:
        html = None

    return html


def path_exists(filename):
    try:
        os.stat(filename)
        return True
    except OSError:
        return False
