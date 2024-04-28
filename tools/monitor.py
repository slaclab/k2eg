from concurrent.futures import ThreadPoolExecutor
import logging
import sys
import os
import time
_dir = os.path.dirname(os.path.abspath(__file__))
os.environ['K2EG_PYTHON_CONFIGURATION_PATH_FOLDER'] = os.path.join(_dir, "environment")
print(os.environ['K2EG_PYTHON_CONFIGURATION_PATH_FOLDER'])
parent_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(parent_dir)
sys.path.insert(0, parent_dir)
import k2eg  # noqa: E402


def test_multiple_monitor(k:k2eg):
    def monitor_handler(pv_name, new_value):
        print(f"Event for PV: {pv_name} received")
        pass

    monitor_get = [
        'ca://SOLN:IN20:121:BACT',
        "ca://QUAD:IN20:121:BACT",
        "ca://QUAD:IN20:122:BACT",
        "ca://ACCL:IN20:300:L0A_PDES",
        "ca://ACCL:IN20:400:L0B_PDES",
        "ca://ACCL:IN20:300:L0A_ADES",
        "ca://ACCL:IN20:400:L0B_ADES",
        "ca://QUAD:IN20:361:BACT",
        "ca://QUAD:IN20:371:BACT",
        "ca://QUAD:IN20:425:BACT",
        "ca://QUAD:IN20:441:BACT",
        "ca://QUAD:IN20:511:BACT",
        "ca://QUAD:IN20:525:BACT",
        "ca://FBCK:BCI0:1:CHRG_S",
        "ca://CAMR:IN20:186:XRMS",
        "ca://CAMR:IN20:186:YRMS"
    ]
    time_start = time.time()
    k.monitor_many(monitor_get, monitor_handler, 10)
    time_end = time.time()
    print(f"Time taken to put: {time_end - time_start} - {(time_end - time_start)/len(monitor_get) })")

if __name__ == "__main__":
    k = None
    try:
        logging.basicConfig(
            format="[%(levelname)-8s] %(message)s",
            level=logging.DEBUG,
        )
        k = k2eg.dml('test', 'app', "group-test")
        test_multiple_monitor(k)
        time.sleep(600)
    except k2eg.OperationError as e:
        print(f"Remote error: {e.error} with message: {e.args[0]}")
    except k2eg.OperationTimeout:
        print("Operation timeout")
        pass
    except ValueError as e:
        print(f"Bad value {e}")
        pass
    except  TimeoutError as e:
        print(f"Client timeout: {e}")
        pass

    finally:
        if k is not None:
            k.close()