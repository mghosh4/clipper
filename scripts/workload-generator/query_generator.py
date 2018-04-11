from __future__ import print_function
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys
import multiprocessing
import argparse
import os
import traceback

query_queue = multiprocessing.Queue()

def poisson_query_schedule_generator(query_rate, num_samples):
    '''
        Generator version of `genPoissonQuerySchedule` that returns
        only single schedule tuples of (number of queries, time to sleep (ms)).
    '''
    num_samples = int(1.25*num_samples)
    samples = np.random.poisson(query_rate, num_samples)
    num_queries = 0
    count = 0
    for sample in samples:
        if sample != 0:
            if count != 0:
                yield (0, count)
                count = 0
            yield (sample, 1)
            num_queries += sample
        else:
            count += 1

    if count != 0:
        yield (0, count)

def gen_poisson_query_schedule(query_rate, num_samples):
    '''
        Generates schedule of queries based on poisson arrival.
        Schedule is a list of tuples of (number of queries, time to sleep (ms)).
        
        Args:
            queryRate: Number of queries per second in milliseconds
            numSamples: Number of samples to generate a schedule for
            
        Returns:
            A tuple of (`actual number of samples`,`query schedule`).
            `actual number of samples`: total number of samples in schedule
                including buffer in case query count falls short.
            `query schedule`: a list of tuples of (number of queries, time to sleep (ms))
    '''
    num_samples = int(1.25*num_samples)
    samples = np.random.poisson(query_rate, num_samples)
    
    def fold_sample(state, sample):
        schedule, num_queries, count = state
        if sample != 0:
            if count != 0:
                schedule.append([0, count])
                count = 0
            schedule.append([sample, 1])
            num_queries += sample
            return (schedule, num_queries, count)
        else:
            return (schedule, num_queries, count + 1)
    query_schedule, actual_num_queries, rem_count = reduce(fold_sample, samples, ([], 0, 0))
    if rem_count != 0:
        query_schedule.append([0, rem_count])
        
    return actual_num_queries, query_schedule

MAX_SAMPLES = 100000

def predict(addr, model_name, x, batch=False):
    url = "http://%s/%s/predict" % (addr, model_name)

    if batch:
        req_json = json.dumps({'input_batch': x})
    else:
        req_json = json.dumps({'input': list(x)})

    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("%d: '%s', %f ms" % (os.getpid(), r.text, latency))

def worker_signal_handler(signal, frame):
    print("%d: Signal detected" % os.getpid())
    print("%d: Worker stopping: " % os.getpid())
    sys.exit(1)

def query_worker(queue):
    signal.signal(signal.SIGINT, worker_signal_handler)
    print(os.getpid(),"working")
    while True:
        item = query_queue.get(True)
        if item is None:
            print(os.getpid(),"received poison pill, exiting")
            return
        predict(*item)
    return

# TODO(avjykmr2): Add batch support
def generate_queries(clipper_conn, model_name, query_generator, query_rate, num_samples, on_exception=lambda e : print(e), schedule=None):
    
    if schedule is None:
        num_queries, schedule = gen_poisson_query_schedule(query_rate, num_samples)
    
    try:
        the_pool = multiprocessing.Pool(4, query_worker,(query_queue,))
        for number_of_queries, time_to_sleep in schedule:
            if number_of_queries == 0:
                time.sleep(time_to_sleep / 1000)
            else:
                for i in xrange(number_of_queries):
                    new_query = query_generator.next()
                    item = (clipper_conn.get_query_addr(), model_name, new_query)
                    query_queue.put(item)
                    # predict(*item)
    except StopIteration as stopEx:
        print("Reached limit of query generator")
        return
    except Exception as e:
        on_exception(e)
        traceback.print_exc()
        print("Stopping...")
        clipper_conn.stop_all()

def feature_sum(xs):
    return [str(sum(x)) for x in xs]

# Stop Clipper on Ctrl-C
def signal_handler(signal, frame):
    print("Stopping Clipper...")
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.stop_all()
    sys.exit(0)

def run(query_rate, num_samples):
    signal.signal(signal.SIGINT, signal_handler)
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.start_clipper()
    model_name = "simple-example"
    python_deployer.create_endpoint(clipper_conn, model_name, "doubles",
                                    feature_sum,num_replicas=4)
    time.sleep(2)

    # Temp query generator
    def query_generator():
        while True:
            yield np.random.random(200)
    
    generate_queries(clipper_conn, model_name, query_generator(), query_rate, num_samples)
    print("Stopping...")
    clipper_conn.stop_all()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate workload for clipper")
    parser.add_argument("--num-samples", action="store", help="Number of queries to generate", type=int, default=MAX_SAMPLES)
    parser.add_argument("--query-rate", action="store", help="Number of queries per millisecond", type=int)
    args = parser.parse_args()
    run(args.query_rate, args.num_samples)
    