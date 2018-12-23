#coding:utf-8

import json
import gevent
from  mantis.fundamental.application.app import instance
from mantis.fundamental.utils.useful import singleton
from mantis.fundamental.utils.timeutils import timestamp_current
from socket_server import SocketConnection,Server
import message
from conn import PropAppConnection,ExternalAppConnection,LineStatusType

class ConnectionAdapter(object):
    """连接管理器"""

    def __init__(self):
        self.cfgs =  {}
        self.sock_servers = []
        self.running = False
        self.wait = 10
        self.task =  None

        self.audiostream_url = ''

    def init(self,**cfgs):
        listen_type = cfgs.get('listen_type')
        addresses = instance.serviceManager.get('main').getConfig().get('listen_address',{}).get(listen_type)
        addresses = map(lambda _:_.split('/'),addresses)
        addresses = map(lambda _:(_[0],int(_[1])),addresses) # get invalid listen address ( host,port)
        for addr in addresses:
            cfgs = dict(host=addr[0], port=addr[1],handler = self)
            sockserver = Server().init(cfgs)
            sockserver.start()
            print 'server started({})'.format(addr)
            self.sock_servers.append(sockserver)

        return self

    def start(self):
        self.task = gevent.spawn(self.workThread)


    def stop(self):
        self.running = False
        self.task.join()

    def workThread(self):
        self.running = True
        while self.running:
            gevent.sleep(self.wait)


    def onConnected(self,conn,address):
        pass

    def onDisconnected(self,conn):
        pass

    def onData(self, data, conn):
        raise NotImplementedError


@singleton
class PropAppConnectionAdapter(ConnectionAdapter):
    def __init__(self):
        ConnectionAdapter.__init__(self)
        self.apps = {}

    def init(self,**cfgs):
        ConnectionAdapter.init(self,listen_type='prop_app')
        return self

    def onData(self,data,conn):
        print 'on app line:',data
        if self.apps.has_key(conn):
            self.apps[conn].onData(data)

    def onConnected(self,conn,address):
        app = PropAppConnection(self)
        app.conn = conn
        app.onConnected(conn)
        self.apps[conn] = app
        # conn.setTimeout(60)

    def onDisconnected(self,conn):
       if self.apps.has_key(conn):
           self.apps[conn].onDisconnected()
           del self.apps[conn]

    def getIdleAppConnection(self):
        result = []
        for app in self.apps.values():
            if app.line_status == LineStatusType.IDLE:
                result.append(app)
        return result

    def getAppConnectionByCallPeerAddress(self,peer_ipaddr):
        result = None
        for app in self.apps.values():
            if app.call_peer and app.call_peer.ipaddr == peer_ipaddr:
                result =  app
        return result

@singleton
class ExternalAppConnectionAdapter(ConnectionAdapter):
    def __init__(self):
        ConnectionAdapter.__init__(self)
        self.apps = {}

    def init(self,**cfgs):
        ConnectionAdapter.init(self,listen_type='external_app')
        return self

    def onData(self,data,conn):
        if self.apps.has_key(conn):
            self.apps[conn].onData()

    def onConnected(self,conn,address):
        app = ExternalAppConnection(self)
        app.conn = conn
        app.onConnected(conn)

    def onDisconnected(self,conn):
       if self.apps.has_key(conn):
           self.apps[conn].onDisconnected()
           del self.apps[conn]

   