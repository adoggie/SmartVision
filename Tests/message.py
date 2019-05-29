#coding: utf-8

import json
from mantis.fundamental.utils.useful import hash_object,object_assign
from mantis.fundamental.network.message import JsonMessage as Message
"""
{
    id,
    name,
    values:{
        ..
    }:
}

"""

"""
enum CallPeerType{
	INNER_BOX = 1,
	INNER_SCREEN = 2,
	OUTER_BOX = 3,
	PROPERTY_APP = 4,
	PROPERTY_CENTER = 5
};

"""

class CallPeerType(object):

    Unknown = 0
    INNER_BOX = 1
    INNER_SCREEN = 2
    OUTER_BOX = 3
    PROPERTY_APP = 4
    PROPERTY_CENTER = 5

class CallEndpoint(object):
    def __init__(self):
        self.ip = ''
        self.port = 0
        # self.room_id =''
        self.id = ''
        self.type = CallPeerType.Unknown
        self.video_stream_id = ''
        self.audio_stream_id = ''



class MessageJoinFamily(Message):
    """加入家庭"""
    NAME = 'join_family'
    def __init__(self):
        Message.__init__(self,self.NAME)
        self.token = ''
        self.id = ''
        self.type = ''

class MessageJoinReject(Message):
    """加入家庭拒绝"""
    NAME = 'join_reject'
    def __init__(self):
        Message.__init__(self,self.NAME)
        self.reason = ''

class MessageJoinAccept(Message):
    """加入家庭成功"""
    NAME ='join_accept'
    def __init__(self):
        Message.__init__(self,self.NAME)
        self.session_id = ''    # 会话id
        self.room_id = ''       # 房号

class MessageCall(Message):
    """呼叫"""
    NAME = 'call_req'
    def __init__(self):
        Message.__init__(self,self.NAME)
        self.sid = ''
        self.src = CallEndpoint()
        self.dest = CallEndpoint()

    def assign(self,data):
        object_assign(self.src,data.get('src',{}))
        object_assign(self.dest,data.get('dest',{}))

    def values(self):
        src = hash_object(self.src)
        dest = hash_object(self.dest)
        return dict(sid=self.sid,src=src,dest=dest)

# class MessageCallIn(MessageCall):
#     """呼叫进入"""
#     NAME = 'call_in'
#     def __init__(self):
#         MessageCall.__init__(self,self.NAME)
#
#
# class MessageCallOut(MessageCall):
#     """呼叫外出"""
#     NAME = 'call_out'
#     def __init__(self):
#         MessageCall.__init__(self,self.NAME)

class MessageCallAccept(Message):
    """呼叫接听"""
    NAME = 'call_accept'
    def __init__(self):
        Message.__init__(self,self.NAME)

class MessageCallReject(Message):
    """呼叫拒绝"""
    NAME = 'call_reject'
    def __init__(self):
        Message.__init__(self,self.NAME)

class MessageCallEnd(Message):
    """呼叫结束"""
    NAME = 'call_end'
    def __init__(self):
        Message.__init__(self,self.NAME)

class MessageCallKeep(Message):
    """呼叫状态保持"""
    NAME = 'call_keep'
    def __init__(self):
        Message.__init__(self,self.NAME)

class MessageHeartbeat(Message):
    """呼叫进入"""
    NAME = 'heartbeat'
    def __init__(self):
        Message.__init__(self,self.NAME)


class MessageEmergency(Message):
    """呼叫进入"""
    NAME = 'emergency'
    def __init__(self):
        Message.__init__(self,self.NAME)

class MessageOpenDoor(Message):
    """开门"""
    NAME = 'open_door'
    def __init__(self):
        Message.__init__(self,self.NAME)

MessageClsDict ={}

def registerMessageObject(msgcls):
    MessageClsDict[msgcls.NAME] = msgcls


for key,value in locals().items():
    if key.find('Message')==0 and key not in ('MessageClsDict','Message','MessageType','MessageSplitter'):
        registerMessageObject(value)

def parseMessage(data):
    print data
    if  isinstance(data,str):
        data = json.loads(data)

    message = data.get('name')
    msgcls = MessageClsDict.get(message)
    if not msgcls:
        print 'Message Type unKnown. value:{}'.format(message)
        return None
    data = data.get('values',{})
    msg = msgcls()
    msg.assign(data)
    return msg


if __name__ == '__main__':
    print MessageClsDict

