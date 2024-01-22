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
    while True:
        start_time = time.time()
        update_iocs()
        time.sleep(max(0, 1.0 - (time.time() - start_time)))


if __name__ == "__main__":
    main()
