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
            if isinstance(msg,message.MessageJoinAccept):
                # 加入成功马上发起呼叫
                gevent.spawn(self.call_out)

    def call_out(self):
        # 开始向外呼叫
        gevent.sleep(.5)

        msg = message.MessageCall()
        msg.src.id = '2-1-1-1'
        msg.src.type = message.CallPeerType.INNER_SCREEN
        msg.src.audio_stream_id = 5678

        msg.dest.ip = '127.0.0.1'
        msg.dest.port = PORT_CALL_PEER  #
        msg.dest.type = message.CallPeerType.OUTER_BOX
        msg.dest.video_stream_id = 5679

        print 'call out start..'
        self.conn.sendData(msg.marshall())

        gevent.spawn(self.call_keep)

    def call_keep(self):
        """呼叫保持"""
        gevent.sleep(3)
        self.conn.sendData(message.MessageCallKeep().marshall())
        gevent.spawn(self.call_keep)


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
export PYTHONPATH=~/Desktop/Project2019/Branches/
"""
