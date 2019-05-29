#coding: utf-8

"""
室外机模拟程序

"""
import sys,time,datetime,os
import gevent
import json

from gevent import monkey
monkey.patch_all()

from mantis.fundamental.utils.useful import object_assign,hash_object


from mantis.fundamental.network.socketutils import Server,SocketConnection,ConnectionEventHandler
from mantis.fundamental.network.accumulator import JsonDataAccumulator
from message import parseMessage
import message

PORT_STATUS = 17899 # 室外机状态端口
PORT_CALL = 7895   # 室外机呼叫监听端口



# MessageSplitter = '\0'
MaxSize_MessagePayload = 1024*10




class CallEventHandler(ConnectionEventHandler):
    """服务器被叫呼叫处理"""
    def __init__(self):
        ConnectionEventHandler.__init__(self)
        self.conn = None
        self.accumulator = JsonDataAccumulator()

    def onConnected(self,conn,address):
        """连接成功，发送 call_keep 保活"""
        self.conn = conn
        print 'call connected ..'
        gevent.spawn(self.keepalive)

    def keepalive(self):
        """定时发送报活包"""
        while True:
            self.conn.sendData(message.MessageCallKeep().marshall())
            gevent.sleep(3)

    def onDisconnected(self,conn):
        print 'call disconnected ..'

    def onData(self,conn,data):
        print 'call onData ..'
        data_list = self.accumulator.enqueue(data)
        for data in data_list:
            msg = parseMessage(data)
            if isinstance(msg, message.MessageJoinAccept):
                pass
                # 加入成功马上发起呼叫
                # gevent.spawn(self.call_out)


# class StatusEventHandler(ConnectionEventHandler):
#     def __init__(self):
#         ConnectionEventHandler.__init__(self)
#
#     def onConnected(self,conn,address):
#         pass
#
#     def onDisconnected(self,conn):
#         pass
#
#     def onData(self,conn,data):
#         pass


def run_outbox():
    """室外机运行入口"""
    call_service = Server().init(host='127.0.0.1',port=PORT_CALL,handler=CallEventHandler())
    call_service.start()

    # call_service = Server().init(host='127.0.0.1',port=PORT_STATUS,handlerCls=StatusEventHandler)
    # call_service.start()
    print 'outbox started..'
    gevent.sleep(1000000)


class ClientHandler(ConnectionEventHandler):
    def __init__(self):
        ConnectionEventHandler.__init__(self)
        self.conn = None

    def onConnected(self,conn,address):
        print 'client connected ..'
        self.conn = conn
        gevent.spawn(self.keepalive)

    def keepalive(self):
        while True:
            self.conn.sendData(message.MessageCallKeep().marshall())
            gevent.sleep(3)

    def onDisconnected(self,conn):
        print 'client disconnected ..'

    def onData(self,conn,data):
        print 'client onData ..'
        print data


def run_client():
    print 'Client start..'
    conn = SocketConnection( consumer= ClientHandler())
    conn.connect('localhost',PORT_CALL)
    gevent.spawn(lambda :conn.recv())

    for _ in range(10):
        gevent.sleep(2)
        msg = message.MessageHeartbeat()
        print msg.marshall()
        conn.sendData(msg.marshall())

    print 'client started..'
    gevent.sleep(1000000)

if __name__=='__main__':
    run_outbox()

    # if sys.argv[-1] == 'client':
    #     run_client()
    # else:
    #     run_outbox()

"""
export PYTHONPATH=~/Desktop/Project2019/Branches/
"""
