import socketpool
import wifi
import os
import json

from secrets import secrets
import helper_print

MAX_CONNECT_RETRY = 4
pool = None
timezone = ""

def path_exists(filename):
    try:
        os.stat(filename)
        return True
    except OSError:
        return False

def setup_wifi(debug = False):
    global timezone

    secrets_path = "/defaults/secrets.txt"
    if path_exists("/sd/secrets.txt"):
        secrets_path = "/sd/secrets.txt"

    if path_exists(secrets_path):
        sd_secrets = json.load(open(secrets_path))

        secrets["ssid"] = sd_secrets["ssid"].strip()
        secrets["password"] = sd_secrets["password"].strip()
        secrets["timezone"] = sd_secrets["timezone"].strip()
        
        if debug:
            print(f"Secrets loaded from {secrets_path}")
    else:
        if debug:
            print("Secrets not loaded")
    
    timezone = secrets["timezone"]

def wifi_connect(debug = False):
    global pool

    connection_attempts = 0
    successful = False

    while connection_attempts < MAX_CONNECT_RETRY and not successful:
        try:
            if not wifi_connected():
                helper_print.debug_msg(
                    debug, f"Connecting to WiFi with SSID of {secrets['ssid']} ")
                wifi.radio.connect(secrets["ssid"], secrets["password"])

            pool = socketpool.SocketPool(wifi.radio)

            print(f"wifi_connect pool: {pool}")

            successful = True
        except:
            connection_attempts = connection_attempts + 1
            print(f"WiFI failed to connect {connection_attempts} times.")

    return {
        "connection_attempts": connection_attempts,
        "successful": successful,
    }

def wifi_connected():
    connected = False

    if (wifi.radio.ipv4_address is not None and wifi.radio.connected and pool is not None):
        connected = True

    return connected

def wifi_ip():
    if wifi_connected():
        return wifi.radio.ipv4_address
    else:
        return "Not Connected"