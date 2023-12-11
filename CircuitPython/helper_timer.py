import time

timer_min = 5
timer_max = 7
timer_start = time.time()

def get_remaining_time_seconds():
    return time.time() - timer_start

def get_remaining_time():
    seconds = get_remaining_time_seconds()

    seconds_min = timer_min * 60
    seconds_max = timer_max * 60
    color = [255, 255, 255]

    if seconds >= seconds_max:
        color = [255, 0, 0]
    elif seconds >= (seconds_max + seconds_min)/2:
        color = [255, 255, 0]
    elif seconds >= seconds_min:
        color = [0, 255, 0]

    minutes = int(seconds/60)
    seconds = seconds - (minutes * 60)
    timestring = f"{minutes:2d}:{seconds:02d}"
    
    return {
        "timestring": timestring,
        "color": color,
    }

def start_timer():
    global timer_start

    timer_start = time.time()