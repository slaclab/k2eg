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

def test_multiple_put(k:k2eg):
    def monitor_handler(pv_name, new_value):
       pass
        
    monitor_put = {
        'pva://variable:a': '-99830.6330126242',
        'pva://variable:b': '-99830.6330126242',
        'pva://variable:a': '-99830.6330126242',
        'pva://variable:b': '-99830.6330126242',
        'pva://variable:a': '-99830.6330126242',
        'pva://variable:b': '-99830.6330126242',
    }
    time_start = time.time()
    with ThreadPoolExecutor(10) as executor:
        for key, value in monitor_put.items():
            executor.submit( put, k, key, value)
    time_end = time.time()
    print(f"Time taken to put: {time_end - time_start} - {(time_end - time_start)/len(monitor_put) })")

def put(k, key, value):
    try:
        k.put(key, value, 10)
        print(f"Put {key} with value {value}")
    except Exception as e:
        print(f"An error occured: {e}")

if __name__ == "__main__":
    k = None
    try:
        logging.basicConfig(
            format="[%(levelname)-8s] %(message)s",
            level=logging.DEBUG,
        )
        k = k2eg.dml('test', 'app')
        counter = 0
        while counter < 10000:
            print("This is iteration", counter + 1)
            test_multiple_put(k)
            counter += 1
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