#coding:utf-8

import os,os.path,sys
sys.path.insert(0,'../src')

from vendor.concox.accumulator import DataAccumulator
from vendor.concox.message import *

data_login_req = '78 78 11 01 07 52 53 36 78 90 02 42 70 00 32 01 00 05 12 79 0D 0A'
data_heartbeat = '78 78 0A 13 40 04 04 00 01 00 0F DC EE 0D 0A'
data_gps_location = '78 78 22 22 0F 0C 1D 02 33 05 C9 02 7A C8 18 0C 46 58 60 00 14 00 01 CC 00 28 7D 00 1F 71 00 00 01 00 08 20 86 0D 0A'
data_alarm_data = '78 78 25 26 0F 0C 1D 03 0B 26 C9 02 7A C8 18 0C 46 58 60 00 04 00 09 01 CC 00 28 7D 00 1F 71 80 04 04 13 02 00 0C 47 2A 0D 0A'
data_adjust_time = '78 78 05 8A 00 06 88 29 0D 0A'

test_items =[
    # data_login_req,
    # data_heartbeat,
    # data_gps_location,
    # data_alarm_data,
    data_adjust_time,
]

print MessageLogin.__name__

def test_main():
    for data in test_items:
        bytes = ''.join(map(chr,map(lambda _:int(_,16),data.split())))
        print 'Raw data:'
        print bytes
        acc = DataAccumulator()
        messages = acc.enqueue(bytes)
        print 'Message Parsed:'
        print messages
        for msg in messages:
            if isinstance(msg,MessageLogin):
                print msg.extra.to_bytes()

            if isinstance(msg,MessageAdjustTime):
                print msg.response()



"""
相关问题： 
======
1. 如何修改上报gps的间隔时间？

"""
test_main()