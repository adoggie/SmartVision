#coding: utf-8

"""
模拟室外机呼入室内机

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


PORT_CALL = 7892   # 室外内机呼叫端口



# MessageSplitter = '\0'
MaxSize_MessagePayload = 1024*10






class ClientHandler(ConnectionEventHandler):
    def __init__(self):
        ConnectionEventHandler.__init__(self)
        self.conn = None
        self.accumulator = JsonDataAccumulator()
        self.status = ''

    def onConnected(self,conn,address):
        print 'innerbox Connected ..'
        self.conn = conn
        self.status = 'call_start'
        gevent.spawn(self.keepalive)
        self.conn.sendData(message.MessageCall().marshall())

    def keepalive(self):
        while self.status == 'call_start':
            self.conn.sendData(message.MessageCallKeep().marshall())
            gevent.sleep(3)

    def onDisconnected(self,conn):
        print 'client disconnected ..'
        self.status = 'call_end'

    def onData(self,conn,data):
        print 'client onData ..'
        data_list = self.accumulator.enqueue(data)
        for data in data_list:
            msg = parseMessage(data)
            print msg.name_
            if isinstance(msg, message.MessageJoinAccept):
                print 'Remote CallAccept'
            if isinstance(msg,message.MessageCallEnd):
                print 'Remote Call End'
                self.status = 'call_end'
            if isinstance(msg,message.MessageCallReject):
                print 'Remote Reject Call'
                self.status = 'call_end'



def run_client():
    print 'OuterBox Client start..'
    conn = SocketConnection( consumer= ClientHandler())
    conn.connect('localhost',PORT_CALL)
    gevent.spawn(lambda :conn.recv())



    # for _ in range(10):
    #     gevent.sleep(2)
    #     msg = message.MessageHeartbeat()
    #     print msg.marshall()
    #     conn.sendData(msg.marshall())

    print 'client started..'
    gevent.sleep(1000000)

if __name__=='__main__':
    run_client()


"""
export PYTHONPATH=~/Desktop/Project2019/Branches/
"""
