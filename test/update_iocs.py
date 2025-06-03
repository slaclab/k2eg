import logging
import random
import time
import numpy as np

from epics import PV
from p4p.client.thread import Context

logging.basicConfig(level=logging.WARNING)

temp_pv = PV('K2EG:TEST:TEMPERATURE')
ctxt = Context('pva')
table_pv = ctxt.get('K2EG:TEST:TWISS')

def update_iocs():
    global temp_pv, table_pv, image_pv, ctxt

    # Update temperature
    temp_pv.value = random.uniform(89, 91)

    # Update timestamp of table, any monitor will get a new result
    ctxt.put('K2EG:TEST:TWISS', table_pv)
    image_size = int(ctxt.get('K2EG:TEST:IMAGESIZE'))

    # Update image
    matrix = np.random.randint(0, 100, (image_size, image_size))
    ctxt.put('K2EG:TEST:IMAGE', matrix)
    

def main():
    fast_interval = 1.0 / 100  # 100 Hz
    slow_interval = 1.0        # 1 Hz
    last_slow_update = time.time()
    while True:
        now = time.time()

        # Fast update at 100 Hz
        ctxt.put("channel:random:fast", random.uniform(0, 100))

        # Slow update at 1 Hz
        if now - last_slow_update >= slow_interval:
            update_iocs()
            last_slow_update = now

        time.sleep(fast_interval)


if __name__ == "__main__":
    main()
