#!/usr/bin/python
import requests
import sys
import json
import jsonpath
import re
import argparse
import ConfigParser
import os
import logging
import datetime as dt
from datetime import date, timedelta
import time
from jsonpath_rw import jsonpath, parse
import nltk as nl
from nltk import sent_tokenize, word_tokenize
from collections import defaultdict
import pdb

def mkdir_p(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

current_time_millis = lambda: int(round(time.time() * 1000))
millis_one_day      = 86400000

TASK = "data_fetch_categorize"
scriptdir = os.path.dirname(os.path.abspath(__file__))
LOG_DIR = os.path.join(scriptdir, '..', 'var', 'log')
CONF_DIR = os.path.join(scriptdir, '..', 'etc')
mkdir_p(LOG_DIR)
logger = None


def configure_log():
    global logger
    current_time = dt.datetime.now()
    logfile = os.path.join(LOG_DIR, '%s.%s.log' % (TASK, current_time.strftime('%Y.%m.%d')))
    logging.basicConfig(filename=logfile, filemode='a', format='%(asctime)-15s |%(levelname)s| %(message)s', level=logging.DEBUG)
    logger = logging.getLogger()


def _analyze(element):
    
    return 1

''' fetch data from source_url, from beginning of num_days,
apply json extract_path'''

def _prepare_samplespace(source_url, num_fetch_days, extract_path, exclude_pattern):
    current_time = current_time_millis()
    _from = current_time - int(millis_one_day * int(num_fetch_days))
    _to   = current_time

    q = json.dumps(dict(fields=["_source", "_timestamp"],query={"filtered":{"filter":{"range":{"_timestamp":{"from": _from,"to": _to}}}}}))

    _raw          = requests.get(source_url, params=q)
    _raw.raise_for_status
    _dataset      = json.loads(_raw.text)

    positive_indicators = []
    negative_indicators = []

    jsonpath_expr = parse(extract_path)
    for match in jsonpath_expr.find(_dataset):
        arr_values = str(match.value).split("|")
        for element in arr_values[2].split('\n'):
            anz = _analyze(element)
            indicators.append(anz)

    return indicators


def _worker_run(config):
    read_source      = config['LOG_URI']
    read_days_range  = config['LOG_DAYS_RANGE']
    extract_path     = config['LOG_EXTRACT_PATH']
    exclude_pattern  = config['LOG_EXCLUDE_PATTERN']

    sample = _prepare_samplespace(read_source, read_days_range, extract_path, exclude_pattern.split(','))
    #_dump(space)


def main():
    parser = argparse.ArgumentParser(description='input arg parser')
    parser.add_argument('-e', type=str, action='store', help='environment dev/prod', default='dev')
    arguments = parser.parse_args()
    env = arguments.e

    prop_file = os.path.join(CONF_DIR, TASK + '.properties')
    props = ConfigParser.ConfigParser()
    props.read(prop_file)
    configure_log()
    logger.info("environment:" + env)
    config = {'LOG_URI': props.get(env, 'data_log_uri'),
              'LOG_EXTRACT_PATH': props.get(env, 'data_log_node'),
              'LOG_DAYS_RANGE': props.get(env, 'data_log_range_days'),
              'LOG_EXCLUDE_PATTERN': props.get(env, 'exclude')
              }
    
    _worker_run(config)

if __name__ == "__main__":
    main()
