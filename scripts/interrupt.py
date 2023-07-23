# Allow Ctrl-C to abort all tasks submitted to a multiprocessing Pool.
# Any existing tasks will still run to completion, but any pending
# tasks will exit immediately.  See https://stackoverflow.com/a/68695455/5495732
#
# Usage:
#   from interrupt import handle_ctrl_c, init_pool
#
#   @handle_ctrl_c
#   def task_function(args):
#      # ...
#
#   # In main:
#   signal.signal(signal.SIGINT, signal.SIG_IGN)
#   pool = Pool(num_threads, initializer=init_pool)
#
#   # Submit tasks to pool here
# 
#   results = pool.map(task_function, process_args)
#   if any(map(lambda x: isinstance(x, KeyboardInterrupt), results)):
#       print('Ctrl-C was entered.')
#       exit(1)
#   pool.close()
#   pool.join()                

import signal

from functools import wraps

def handle_ctrl_c(func):
    @wraps(func)
    def wrapper(*args, **kwargs):
        global ctrl_c_entered
        if not ctrl_c_entered:
            signal.signal(signal.SIGINT, default_sigint_handler) # the default
            try:
                return func(*args, **kwargs)
            except KeyboardInterrupt:
                ctrl_c_entered = True
                return KeyboardInterrupt()
            finally:
                signal.signal(signal.SIGINT, pool_ctrl_c_handler)
        else:
            return KeyboardInterrupt()
    return wrapper

def pool_ctrl_c_handler(*args, **kwargs):
    global ctrl_c_entered
    ctrl_c_entered = True

def init_pool():
    # set global variable for each process in the pool:
    global ctrl_c_entered
    global default_sigint_handler
    ctrl_c_entered = False
    default_sigint_handler = signal.signal(signal.SIGINT, pool_ctrl_c_handler)

