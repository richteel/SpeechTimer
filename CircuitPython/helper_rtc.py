import adafruit_requests
import ssl
import rtc
import time

import helper_print
import helper_wifi

MAX_TIME_CONNECT_RETRY = 4
real_time_clock = rtc.RTC()


def Set_RTC_Time(debug=False):
    connection_attempts = 0
    successful = False

    if not helper_wifi.wifi_connected():
        helper_wifi.wifi_connect(debug)

    if not helper_wifi.wifi_connected():
        return {
            "connection_attempts": helper_wifi.MAX_CONNECT_RETRY,
            "successful": successful,
        }

    while connection_attempts < MAX_TIME_CONNECT_RETRY and not successful:
        try:
            
            request = adafruit_requests.Session(
                helper_wifi.pool, ssl.create_default_context())
            
            time_url = "http://worldtimeapi.org/api/ip"

            if helper_wifi.timezone != "":
                time_url = f"http://worldtimeapi.org/api/timezone/{helper_wifi.timezone}"

            helper_print.debug_msg(debug, "Getting current time...")
            response = request.get(time_url)
            time_data = response.json()
            helper_print.debug_msg(debug, f"time_data: {time_data}")
            tz_hour_offset = int(time_data['utc_offset'][0:3])
            tz_min_offset = int(time_data['utc_offset'][4:6])
            if (tz_hour_offset < 0):
                tz_min_offset *= -1
            unixtime = int(time_data['unixtime'] +
                           (tz_hour_offset * 60 * 60)) + (tz_min_offset * 60)

            helper_print.debug_msg(
                debug, f"URL time: {response.headers['date']}")

            # create time struct and set RTC with it
            rtc.RTC().datetime = time.localtime(unixtime)

            successful = True
        except Exception as e:
            connection_attempts = connection_attempts + 1
            helper_print.debug_msg(debug, f"ERROR: {str(e)}")
            helper_print.debug_msg(debug, f"successful: {successful}")
            print(f"Failed to connect {connection_attempts} times.")

    return {
        "connection_attempts": connection_attempts,
        "successful": successful,
    }

def get_time_dict():
    dt = real_time_clock.datetime
    return {
        "hour": dt.tm_hour,
        "minute": dt.tm_min,
        "second": dt.tm_sec,
    }

def str_time():
    dt = real_time_clock.datetime
    return f"{dt.tm_hour:02d}:{dt.tm_min:02d}:{dt.tm_sec:02d}"