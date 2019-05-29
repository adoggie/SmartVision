#coding: utf-8

"""
模拟多路室外机呼叫进入处理

"""
import sys,time,datetime,os
import gevent
import random
import json

from gevent import monkey
monkey.patch_all()

from mantis.fundamental.utils.useful import object_assign,hash_object


from mantis.fundamental.network.socketutils import Server,SocketConnection,ConnectionEventHandler
from mantis.fundamental.network.accumulator import JsonDataAccumulator
from message import parseMessage
import message

PORT_INNER_MGR = 7891   # 室内机管理端口
PORT_CALL_PEER = 7895   # 室外机端口
MaxSize_MessagePayload = 1024*10



"""
室内app连接事件处理
"""
class ClientHandler(ConnectionEventHandler):
    def __init__(self):
        ConnectionEventHandler.__init__(self)
        self.conn = None
        self.accumulator = JsonDataAccumulator()
        self.status = 'call_start'

    def onConnected(self,conn,address):
        print 'client connected ..'
        self.conn = conn
        # gevent.spawn(self.keepalive)

        # 连接成功，马上发送加入家庭
        msg = message.MessageJoinFamily()

        conn.sendData(msg.marshall())

    def keepalive(self):
        while True:
            self.conn.sendData(message.MessageCallKeep().marshall())
            gevent.sleep(3)

    def onDisconnected(self,conn):
        print 'client disconnected ..'

    def onData(self,conn,data):
        print '<< client onData ..'
        data_list = self.accumulator.enqueue(data)
        for data in data_list:
            msg = parseMessage(data)
            print msg.name_
            if isinstance(msg,message.MessageJoinAccept):
                # 加入成功马上发起呼叫
                print 'Box Join Accepted. Waiting for Call in ..'
                # gevent.spawn(self.call_out)
            if isinstance(msg,message.MessageCall):
                print 'Remote Call in..'
                print data
                gevent.spawn(self.pick_up)
                # gevent.spawn(self.reject)
                # gevent.spawn(self.call_keep)
            if isinstance(msg,message.MessageCallEnd):
                self.status = 'call_end'

    def reject(self):
        gevent.sleep(3)
        print 'Reject Call..'
        self.conn.sendData(message.MessageCallReject().marshall())

    def pick_up(self):
        """接听"""
        if sys.argv[-1] != 'accept':
            return

        gevent.sleep(random.randint(0,5))
        # if random.randint(0,5) % 2 == 0: #随机接听
        if True: #随机接听
            print "Pick up Call .."
            self.conn.sendData(message.MessageCallAccept().marshall())
            self.status = 'call_start'
            gevent.spawn(self.process_call_in)
            gevent.sleep(2)
            print 'OpenDoor ordering..'
            self.conn.sendData(message.MessageOpenDoor().marshall())


    def process_call_in(self):
        # 模拟呼叫持续过程
        gevent.sleep(5)
        print 'Hang Up ..'
        # 模拟挂断通话
        self.conn.sendData(message.MessageCallEnd().marshall())
        self.status = 'call_end'

    def call_keep(self):
        """呼叫保持"""
        gevent.sleep(3)
        if self.status == 'call_start':
            print 'send call_keep to peer..'
            self.conn.sendData(message.MessageCallKeep().marshall())
            gevent.spawn(self.call_keep)
        else:
            print 'stop call_keep send..'


def run_client():
    print 'Client start..'
    conn = SocketConnection( consumer= ClientHandler())
    conn.connect('localhost',PORT_INNER_MGR)
    gevent.spawn(lambda :conn.recv())


    print 'client started..'
    gevent.sleep(1000000)

if __name__=='__main__':

    run_client()


"""
多个室内app模拟运行

python innerscreen_multiscreen_call_in.py accept  接听

export PYTHONPATH=~/Desktop/Project2019/Branches/
"""
