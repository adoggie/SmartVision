#coding: utf-8

"""
室外机模拟程序

"""
import sys,time,datetime,os
import traceback
from datetime import datetime
import socket
import gevent
import gevent.socket
from 	gevent.server import StreamServer
import 	gevent.event
import gevent.ssl

import json

from collections import OrderedDict

from gevent import monkey
monkey.patch_all()

import yamlparser
# from socketutils import *



class AppInstance(object):
    OFFLINE = 'offline'
    IDLE = 'idle'
    BUSY = 'busy'

    def __init__(self,app_cfg):
        self.id = app_cfg.get('id')
        self.ip = app_cfg.get('ip')
        self.port = app_cfg.get('port')
        self.status = self.OFFLINE

confs = yamlparser.YamlConfigParser('./settings.yaml')

app_list = OrderedDict()

for _ in confs.props.get('app_list',[]):
    app = AppInstance(_)
    app_list[app.id] = app

def get_app_status(app):
    app.status = AppInstance.OFFLINE
    # print 'get app status: ', app.id ,app.ip , app.port
    sock = socket.socket()
    try:
        dest = app.ip,app.port
        sock.connect( dest )
        msg = json.dumps( dict(method = 'phone_state') )
        sock.send(msg)
        sock.settimeout(3)
        data = sock.recv(513)
        # print '<<' ,data
        jsondata = json.loads(data)
        text = jsondata.get('method')
        if text ==  'phone_busy':
            app.status = AppInstance.BUSY
        elif text == 'phone_idle':
            app.status = AppInstance.IDLE
    finally:
        sock.close()
        # print 'status:',app.status


def check_app_status():
    """检查各个物业app的当前状态"""
    global app_list
    while True:
        for id,_ in app_list.items():
            try:
                get_app_status(_)
            except:
                pass
        gevent.sleep(5)




class Server(object):
    def __init__(self):
        self.cfgs = None
        self.conns = []
        self.server = None

    @property
    def name(self):
        return self.cfgs.get('name')

    def init(self,**kwargs):
        """
            host :
            port :
            handler: 连接处理器
        """
        self.cfgs = kwargs
        return self

    def stop(self):
        self.server.stop()

    def start(self):
        ssl = self.cfgs.get('ssl')
        if ssl:
            self.server = StreamServer((self.cfgs.get('host'),self.cfgs.get('port')),
                                        self._service,keyfile=self.cfgs.get('keyfile'),
                certfile=self.cfgs.get('certfile'))
        else:
            self.server = StreamServer((self.cfgs.get('host'),self.cfgs.get('port')), self._service)

        print 'Socket Server Start On ',self.cfgs.get('host'), ':',self.cfgs.get('port')
        self.server.start() #serve_forever() , not block

    def _service(self,sock,address):
        """支持 连接处理函数对象 或者 处理对象类型 """
        sock.settimeout(5)
        try:
            data = sock.recv(1024)
            jsondata = json.loads(data)
            method = jsondata.get('method')
            if method in  ('call_from_innerbox','call_from_outerbox'):
                self.delivery_call_to_idle_app(sock, method , data )

            if method == 'call_ending':
                # 广播给所有的管理app
                self.broadcast_call_ending(data)


        finally:
            sock.close()

    def delivery_call_to_idle_app(self,sock,method,data):
        """选择一个空闲 app 并发送call请求"""
        idle = None
        for app in app_list.values():
            if app.status == AppInstance.IDLE:
                print 'find  a idle app : ', app.id, app.ip, app.ip
                idle = app
                break

        if not idle :
            # 没有空闲的话机
            resp = json.dumps(dict( result = "line_is_busy")) + '^'
            sock.sendall(resp)
            return

        # 产生一个新的工作线程，不阻塞外部处理线程
        # gevent.spawn(self.do_app_call)
        appsock = socket.socket()

        self.do_app_call(sock,idle,data)
        # try:
        #     gevent.spawn(self.do_app_call,sock,idle,data,appsock)
        #     data = sock.recv(1024)
        # except:
        #     # innerproc disconnected
        #     print 'inner call disconnected..'
        #     appsock.close()

    def do_app_call(self,sock,app,data,appsock=None):
        # 找到空闲话机
        if not appsock:
            appsock = socket.socket()
        try:
            appsock.connect((app.ip, app.port))
            appsock.sendall(data)
            data = appsock.recv(1024)
            sock.sendall(data)  # 回送 app的响应消息到 呼叫发起方
            print '<< response from property app data:',data
        finally:
            appsock.close()

    def broadcast_call_ending(self,data):
        """广播到所有app呼叫挂断"""
        for app in app_list.values():
            if app.status == AppInstance.OFFLINE:
                continue
            print 'broadcast call end to app : ' , app.id,app.ip,app.port

            sock = socket.socket()
            try:
                sock.settimeout(2)
                sock.connect((app.ip,app.port))
                sock.send(data)
            finally:
                sock.close()

def main():
    print 'Server Start..'
    Server().init(host='',port=18699).start()

    task = gevent.spawn(check_app_status)
    task.join()

if __name__=='__main__':

    main()


"""
export PYTHONPATH=~/Desktop/Project2019/Branches/
"""
