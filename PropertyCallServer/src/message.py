#coding:utf-8

"""
室内机消息定义

"""

import json,time
from collections import OrderedDict
from mantis.fundamental.utils.useful import hash_object,object_assign

class MessageRequest(object):
    def __init__(self,method):
        self.method = method
        self.params = {}
        self.key_maps ={}   # 属性字段替换，防止关键字的变量名

    def body(self):
        data = hash_object(self,excludes=('method','params','key_maps'))
        for k,v in self.key_maps.items():
            if data.has_key(k):
                data[v] = data[k]
                del data[k]
        return data

    def marshall(self):
        data = dict(method = self.method,params=self.body())
        return json.dumps(data)

class MessageResult(object):
    def __init__(self,result=''):
        self.result = result
        self.params = {}
        self.key_maps ={}   # 属性字段替换，防止关键字的变量名

    def body(self):
        data = hash_object(self,excludes=('result','params','key_maps'))
        for k,v in self.key_maps.items():
            if data.has_key(k):
                data[v] = data[k]
                del data[k]
        return data

    def marshall(self):
        data = dict(result = self.result,params=self.body())
        return json.dumps(data)

null_result = MessageResult()

class WRONG_PARAMS(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)

class line_is_busy(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)

class call_error(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.message = ''

class outerbox_is_offline(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)


class opendoor_permit_has_done(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)

class option_joinfamily(MessageRequest):
    """App 加入"""

    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.type = 'android-pad'
        self.qrcode = '13916624477'
        self.name = 'wangdaxian'
        self.version = '1.2.3.4'

class prop_app_line_status(MessageRequest):
    """物业app定时上报线路状态"""
    IDLE = 'idle'
    CONNECTING ='connecting'
    BUSY = 'busy'

    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.status = prop_app_line_status.IDLE

class join_permit(MessageRequest):
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.audio_url = ''
        self.sentrybox = []

class family_join_deny(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)

class family_join_permit(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.audio_url = ''
        self.sentrybox = []

class door_open_processing(MessageRequest):
    """app -> bg-server """
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class opendoor_deny(MessageRequest):
    """  """
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)



# =========== 呼叫室外机 ===============
class call_outerbox(MessageRequest):
    """物业app呼叫室外机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.ipaddr = ''    #  室外机ip

class call_outerbox_params(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.video_from_outerbox = ''
        self.audio_to_outerbox = '' #物业app的audio推流地址

# =========== 呼叫室内机 ===============
class call_accept(MessageRequest):
    """"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.audio = ''

class call_innerbox(MessageRequest):
    """物业app呼叫室内机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.doorid = ''

class call_accept_from_innerbox(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.stream_url_from_innerbox = ''
        self.stream_url_to_innerbox = ''

class call_from_property(MessageRequest):
    """物业app呼叫室外机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.audio = ''
        self.ipaddr = ''

class call_ending_from_app(MessageRequest):
    """物业app呼叫室外机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)


class call_ending(MessageRequest):
    """APP主动挂断时，发送JSON PACKET 49给后台服务。
    APP无人应答（超时1分钟），则发送JSON PAKCET 58给后台服务，同时关闭连接。呼叫方收到后，结束通话"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.ipaddr=''

class call_ending_of_timeout(MessageRequest):
    """"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class callhistory_request(MessageRequest):
    """APP查询对讲记录"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.page = 1


class callhistory_result(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.page = 0
        self.total = 0
        self.callhistory = [] #  {calltime,peertype,peerid,event}


class ipaddr_query_of_innerbox(MessageRequest):
    """查询小区住户所有室内机的IP地址信息"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class ipconf_of_innerbox(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.values = []
        # [{“doorid”:”101”,”ipaddr”:”1.1.1.1”},..]

class ipaddr_query_of_outerbox(MessageRequest):
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class ipconf_of_outerbox(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)
        self.values = []

class show_version(MessageRequest):
    """"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class propertyserver_netstat(MessageRequest):
    """查询物业中心服务器在线状态"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class opendoor_permit(MessageRequest):
    """开门"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)


# == 发送到物业服务器 ==

class ipaddr_query_of_all_outerbox(MessageRequest):
    """查询返回所有室外机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)

class ipaddr_query_of_all_innerbox(MessageRequest):
    """查询返回所有室内机"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)


# === 外部发送到物业后端 =====
class call_from_outerbox(MessageRequest):
    """查询物业中心服务器在线状态"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.ipaddr ='' # 室外机地址
        self.rtspurl='' # 室外机音视频流地址

class call_from_innerbox(MessageRequest):
    """查询物业中心服务器在线状态"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.ipaddr ='' # 室内机音频流地址
        self.audio='' # 室内机音视频流地址
        self.doorid = ''
        self.stream_url_from_innerbox = ''
        self.stream_url_to_innerbox = ''

class call_from_otherproperty(MessageRequest):
    """物业发送来的呼叫"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.audio =''
        self.ipaddr=''

class call_from_otherproperty2(MessageRequest):
    """物业发送来的呼叫"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.propertyname =''
        self.stream_url_from_otherproperty=''
        self.stream_url_to_otherproperty = ''


class opendoor_request_sent(MessageResult):
    def __init__(self):
        MessageResult.__init__(self, self.__class__.__name__)

# === 物业服务后台外呼 ==

class opendoorplease(MessageRequest):
    """物业发送来的呼叫"""
    def __init__(self):
        MessageRequest.__init__(self, self.__class__.__name__)
        self.doorid =''
        self.video=''    # 室外机摄像头音视频地址
        self.audio = '' # APP采集的音频推送到该地址

# === REACH  END ===


MessageClsDict = OrderedDict()

def registerMessageObject(msgcls):
    MessageClsDict[msgcls.__name__] = msgcls

for key,value in locals().items():
    # print value
    if key not in ('OrderedDict','MessageRequest','MessageResult',
                   'MessageClsDict','registerMessageObject',
                   'null_result') and  not key.startswith('__') :
        registerMessageObject(value)
        # print key,value

def parse(data):
    if isinstance(data,str):
        data = json.loads(data)

    method = data.get('method')
    if not method:
        return None
    CLS = MessageClsDict.get(method)
    if not CLS:
        return None
    msg = CLS()
    try:
        object_assign(msg,data)
        if data.has_key('params'):
            object_assign(msg,data['params'])
    except:
        return None
    return msg
