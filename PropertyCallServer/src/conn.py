#coding:utf-8

import json
import gevent
from gevent import Timeout
from gevent.event import AsyncResult

import socket
from  mantis.fundamental.application.app import instance
from mantis.fundamental.utils.useful import singleton
from mantis.fundamental.utils.timeutils import timestamp_current
from socket_server import SocketConnection,Server
import message

class LineStatusType(object):
    OFFLINE = 0
    ONLINE = 1
    JOINED =  2
    IDLE  = 3
    CONNECTING = 4
    BUSY = 5

class CallPeerType(object):
    Undefined = 'undefined'
    InnerBox = 'innerbox'
    OuterBox = 'outerbox'
    PropertyApp ='property_app'
    PropertyCenter ='property_center'


class CallPeer(object):
    def __init__(self,ipaddr='',port=0,type=CallPeerType.Undefined):
        self.type = type
        self.ipaddr = ipaddr
        self.port = port

class ConnectionIncoming(object):
    def __init__(self):
        self.buffer = ''

class PropAppConnection(ConnectionIncoming):
    """物业app连接"""
    def __init__(self,mgr):
        ConnectionIncoming.__init__(self)
        self.line_status = LineStatusType.OFFLINE
        self.online_time = 0
        self.join_time = 0
        self.status_time = 0
        self.live_time = 0

        self.conn = None    # socket 连接
        self.mgr = mgr
        self.inspect  = None
        self.logger = instance.getLogger()
        self.running = False
        self.name = ''
        self.audiostream_url=''

        self.call_wait = None
        self.call_peer = None   # 对点呼叫信息

    def onConnected(self,conn):
        self.conn = conn
        self.online_time = timestamp_current()
        self.line_status = LineStatusType.ONLINE
        self.inspect = gevent.spawn(self.inspectThread)
        self.live_time = timestamp_current()

    def onDisconnected(self):
        self.line_status = LineStatusType.OFFLINE
        self.running = False

    def onJoin(self,msg):
        self.join_time = timestamp_current()
        self.line_status = LineStatusType.JOINED
        self.name = msg.qrcode

    def onLineStatus(self,status):
        if status == message.prop_app_line_status.IDLE:
            self.line_status = LineStatusType.IDLE
        if status == message.prop_app_line_status.CONNECTING:
            self.line_status = LineStatusType.CONNECTING
        if status == message.prop_app_line_status.BUSY:
            self.line_status = LineStatusType.BUSY

    def onQueryOuterBox(self):
        """查询室外机列表"""
        service = instance.serviceManager.get('main')
        box_list = service.getOuterBoxList()
        data = dict(result='ipconf_of_outerbox',values=box_list)
        self.sendData(json.dumps(data))

    def onQueryInnerBox(self):
        """查询室内机列表"""
        service = instance.serviceManager.get('main')
        box_list = service.getInnerBoxList()
        data = dict(result='ipconf_of_innerbox',values=box_list)
        self.sendData(json.dumps(data))

    def parse(self,data):
        from message import MessageClsDict,object_assign
        # if isinstance(data, str):
        #     data = json.loads(data)

        msg_list = []
        self.buffer+=data
        while True:
            end = self.buffer.find('\0')
            if end == -1:
                break
            data = self.buffer[:end]
            self.buffer = self.buffer[end+1:]
            data = json.loads(data)
            method = data.get('method')
            if not method:
                continue
            CLS = MessageClsDict.get(method)
            if not CLS:
                continue
            msg = None
            try:
                msg = CLS()
                object_assign(msg, data)
                if data.has_key('params'):
                    object_assign(msg, data['params'])
            except:
                msg = None
            if msg:
                msg_list.append(msg)

        return msg_list

    def onMessage(self,msg):
        print 'Message onPropApp connection :',str(msg)
        if isinstance(msg,message.option_joinfamily):
            self.onJoin(msg)
        if isinstance(msg,message.prop_app_line_status):
            self.onLineStatus(msg.status)
        # app 查询室外机
        if isinstance(msg,message.ipaddr_query_of_outerbox):
            self.onQueryOuterBox()
        if isinstance(msg,message.ipaddr_query_of_innerbox):
            self.onQueryInnerBox()

        if isinstance(msg,message.call_outerbox):
            self.onCallOuterBox(msg)

        if isinstance(msg,message.call_innerbox):
            self.onCallInnerBox(msg)

        if isinstance(msg,message.call_accept_from_innerbox):
            self.onCallAcceptFromInnerBox(msg)

        if isinstance(msg,message.door_open_processing):
            self.onDoorOpenProcessing(msg)

        if isinstance(msg,message.opendoor_permit):
            self.onOpenDoorPermit(msg)

        if isinstance(msg,message.opendoor_deny):
            self.onOpenDoorDeny(msg)

        if isinstance(msg,(message.call_ending,message.call_ending_of_timeout)):
            self.onCallEnding(msg)
        if isinstance(msg,message.propertyserver_netstat):
            self.onPropertyServerNetStat(msg)

    def onPropertyServerNetStat(self,msg):
        main = instance.serviceManager.get('main')
        addr,port = main.getConfig().get('property_server').split(':')
        succ = ShortConnectionOutgoing(addr,int(port)).sendData('')
        if succ:
            self.conn.sendData(json.dumps(dict(result='propertyserver_is_online')))
        else:
            self.conn.sendData(json.dumps(dict(result='propertyserver_is_offline')))

    def setCallPeer(self,peer):
        self.call_peer = peer

    def sendCallEnding(self):
        self.sendData(message.call_ending().marshall())
        self.call_peer = None

    def onCallEnding(self,msg):
        if self.call_peer:
            msg = message.call_ending()
            msg.ipaddr = ''
            issue = ShortConnectionOutgoing(self.call_peer.ipaddr,self.call_peer.port)
            issue.sendData(msg.marshall())
        self.call_peer = None

    def onOpenDoorDeny(self,msg):
        """物业app拒绝开门"""
        if self.call_wait:
            resp = msg.marshall()
            self.call_wait(resp)

    def onOpenDoorPermit(self,msg):
        """物业app点击开门"""
        if self.call_wait:
            resp = msg.marshall()
            self.call_wait.set(resp)

    def onDoorOpenProcessing(self,msg):
        """要求通知在线的app，已经由其他app接收到开门请求，这里只有一个app能接收到呼叫进入请求，故无需处理"""
        pass




    def onCallAcceptFromInnerBox(self,msg):
        """室内机呼叫物业app，app回复应答到后台
            app->server->innerbox
        """
        # resp = json.dumps(dict(method='call_accept',audio=self.audiostream_url))
        resp = message.call_accept()
        resp.audio =  self.audiostream_url

        if self.call_wait:
            self.call_wait.set(resp) # 异步通知返回呼叫接受
        resp = json.dumps(dict(method='call_accpet_from_innerbox_cmd_sent')) # 还是个错误，见procallsver.c:3219
        self.sendData(resp)

    def onCallInnerBox(self,msg):
        main = instance.serviceManager.get('main')
        # error
        ipaddr = main.getInnerBoxIpByRoomId(msg.doorid)
        if not ipaddr:
            # response error to app
            error = message.call_error()
            error.message = u'错误呼叫房号'
            self.sendData(error.marshall())
            return
        # send to innberbox
        # 发送给室内机
        req = message.call_from_property()
        req.audio = self.audiostream_url
        req.ipaddr = self.conn.address[0]

        port = main.getConfig().get('service_ports').get('innerbox')

        issue = ShortConnectionOutgoing(ipaddr,port)
        issue.sendData(req.marshall())

        # response to app
        resp = json.dumps(dict(method='call_innerbox_request_sent'))
        self.sendData(resp)

    def onCallOuterBox(self,msg):
        """呼叫室外机"""
        main = instance.serviceManager.get('main')
        resp = message.call_outerbox_params()
        resp.video_from_outerbox = main.getVideoStreamPushAddressOfOuterBox(msg.ipaddr)
        self.sendData(resp.marshall())

    def onData(self,data):
        """解包json-streaming"""
        msg_list = self.parse(data)
        self.live_time = timestamp_current()
        for msg in msg_list:
            self.onMessage(msg)

    def sendData(self,data,close=False):
        self.conn.sendData(data+'\0')
        if close:
            self.close()

    def close(self):
        self.conn.close()

    def inspectThread(self):
        self.running = True
        while self.running:
            gevent.sleep(1)
            if timestamp_current() - self.live_time > 60 :
                self.logger.warn('no payload on line long time.(>60s)')
                self.close()
                break
            if self.join_time and timestamp_current() - self.status_time > 10:
                self.logger.warn('line_status recv timeout.')
                self.close()
                break

    def callWaiting(self,timeout=15):
        """等待app accpet 或者 reject"""

        self.call_wait = AsyncResult()
        try:
            result = self.call_wait.get(timeout=timeout)
        except:
            result = None
            self.call_wait = None
        return result



class ExternalAppConnection(ConnectionIncoming):
    """外部连接，innerbox,outerbox,prop_app,
        一次接收完整的数据请求包（这是阿福协议设计的问题)
        并返回
    """
    def __init__(self,mgr):
        ConnectionIncoming.__init__(self)
        self.line_status = LineStatusType.OFFLINE
        self.online_time = 0
        self.join_time = 0
        self.conn = None    # socket 连接
        self.mgr = mgr
        self.mainservice = instance.serviceManager.get('main')

    def onConnected(self,conn):
        self.conn = conn
        self.online_time = timestamp_current()
        self.line_status = LineStatusType.ONLINE

    def onDisconnected(self):
        self.line_status = LineStatusType.OFFLINE


    def onJoin(self):
        self.join_time = timestamp_current()
        self.line_status = LineStatusType.JOINED

    def onCallFromOuterBox(self,req):
        """处理室外机呼叫"""
        adapter = self.mainservice.getPropAppConnectionAdapter()
        app = adapter.getIdleAppConnection()
        if not app:
            resp = message.line_is_busy().marshall()
            return self.sendData(resp)

        peer_addr = self.conn.address[0]
        msg = message.opendoorplease()
        msg.audio = app.audiostream_url
        msg.video = self.mainservice.getVideoStreamPushAddressOfOuterBox(peer_addr)
        app.sendData(msg.marshall()) #  发送给物业app

        self.sendData(message.opendoor_request_sent().marshall())
        # 等待 app 应答
        result = app.callWaiting()
        if  result:
            self.sendData(result)



    def onCallFromInnerBox(self,msg):
        """处理室内机呼叫,等待物业app应答之后返回accept 或者 busy
            交互协议设计存在问题
        """
        adapter = self.mainservice.getPropAppConnectionAdapter()
        app = adapter.getIdleAppConnection()
        if not app:
            resp = message.line_is_busy().marshall()
            return self.sendData(resp,close=True)

        # 发送给物业app
        req = message.call_from_innerbox()
        req.doorid = self.mainservice.getRoomIdByInnerBoxIp(msg.ipaddr)
        req.stream_url_from_innerbox = msg.audio
        req.stream_url_to_innerbox = app.audiostream_url
        app.sendData(msg.marshall())

        # 等待 app 应答
        result = app.callWaiting()
        if not result :
            result = message.line_is_busy().marshall()

        if isinstance(result,message.call_accept):
            peer = CallPeer(self.conn.address[0],5678,CallPeerType.InnerBox)
            app.setCallPeer(peer)
        self.sendData(result)

    def onCallEnding(self,msg):
        """室内机、室外机发送挂断信号，找到当前app的连接，发送挂断消息过去"""
        from adatper import PropAppConnectionAdapter
        resp = json.dumps(dict(method='call_ending_has_done'))
        self.sendData(resp)
        #
        ipaddr = self.conn.address[0]
        app = PropAppConnectionAdapter().getAppConnectionByCallPeerAddress(ipaddr)
        if app:
            app.sendCallEnding()

    def onMessage(self,msg):
        # 室外机呼入
        if isinstance(msg,message.call_from_outerbox):
            self.onCallFromOuterBox(msg)
        if isinstance(msg,message.call_from_innerbox):
            self.onCallFromInnerBox(msg)

        if isinstance(msg,(message.call_ending,message.call_ending_of_timeout)):
            self.onCallEnding(msg)
        self.conn.close()

    def onData(self,data):
        """解包json-streaming"""
        msg = message.parse(data)
        if not msg:
            return
        self.onMessage(msg)

    def sendData(self,data,close=True):
        self.conn.sendData(data)
        if close:
            self.conn.close()

class ShortConnectionOutgoing(object):
    """短连接请求"""
    def __init__(self,host,port):
        self.host =  host
        self.port = port
        self.sock = None

    def sendData(self,data):
        try:
            self.sock = socket.socket()
            with gevent.Timeout(5) as timeout:
                self.sock.connect((self.host, self.port))
                if data:
                    self.sock.sendall(data)
            return True
        except:
            return False
        finally:
            self.sock.close()
            self.sock = None

    def sendAndRecv(self,req,timeout=0):
        """
        发送并等待接收返回数据
        :param data:
        :param timeout: 指定等待接收的最大时间超时
        :return:
        """
        try:
            self.sock = socket.socket()
            data =''
            self.sock.connect((self.host, self.port))
            print '>> send msg :',req
            self.sock.sendall(req)
            if timeout:
                while True:
                    resp = None
                    with gevent.Timeout(timeout) as to:
                        resp = self.sock.recv(1024)
                    if resp:
                        data= data + resp
                    else:
                        break
            return data
        finally:
            self.sock.close()
            self.sock = None