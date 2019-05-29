#coding:utf-8

import json
import time
import datetime
import os.path
import os
from collections import OrderedDict
from flask import Flask,request,g
from flask import Response
import requests
import base64
from StringIO import StringIO

from flask import render_template
from mantis.fundamental.application.app import  instance
from mantis.fundamental.utils.useful import cleaned_json_data


from mantis.fundamental.flask.webapi import ErrorReturn,CR
from mantis.fundamental.utils.timeutils import current_datetime_string,timestamp_current,timestamp_to_str
from mantis.fanbei.smartvision.errors import ErrorDefs
from mantis.fanbei.smartvision.base import SystemDeviceType
from mantis.fanbei.smartvision import model
from token import token_encode
from token import  login_check
from mantis.fanbei.smartvision.base import Constants


def ping():
    return CR().response

def init_innerbox():
    """室内机设备请求运行参数"""

    ip= request.remote_addr.split(':')[-1]
    box = model.InnerBox.get(ip = ip) # 根据ip检查设备是否登记
    if not box:
        return ErrorReturn(ErrorDefs.ObjectNotExist,u'室内主机未登记')

    box.login_time = datetime.datetime.now()
    box.save()

    data = {}
    garden = model.HomeGarden.get(id = '8888')
    data['propcenter_ip'] = garden.property_call_server
    data['net_ip'] = ip
    data['family_ip'] = ''
    data['room_id'] = box.room_id

    sentry_list = []
    rs = model.SentryApp.find(garden_id = garden.id)
    for _ in rs:
        sentry_list.append(dict(name=_.name,ip= _.ip))

    outerbox_list = []
    rs = model.OuterBox.find(garden_id = garden.id,building_id = box.building_id)
    for _ in rs:
        outerbox_list.append(dict(type='B',name=_.name, ip= _.ip, is_primary = _.is_primary, floor=_.floor))

    rs = model.OuterBox.find(garden_id=garden.id, type = 'A') # 围墙机
    for _ in rs:
        outerbox_list.append(dict(type='A', name=_.name, ip=_.ip, is_primary=_.is_primary, floor=_.floor))


    data['sentry_list'] = sentry_list
    data['outerbox_list'] = outerbox_list
    result = data

    return CR(result = result)


def init_outerbox():
    """室外机登录"""
    ip = request.remote_addr.split(':')[-1]
    box = model.OuterBox.get(ip=ip)  # 根据ip检查设备是否登记
    if not box:
        return ErrorReturn(ErrorDefs.ObjectNotExist, u'室外主机未登记')

    box.login_time = timestamp_current()
    box.save()

    data = {}
    garden = model.HomeGarden.get(id='8888')
    data['propcenter_ip'] = garden.property_call_server
    data['stream_server_url'] = garden.stream_server_url

    innerbox_list = []
    rs = model.InnerBox.find(garden_id=garden.id, building_id=box.building_id)
    for _ in rs:
        innerbox_list.append(dict( room_id = _.room_id, ip = _.ip ))

    sentry_list = []
    rs = model.SentryApp.find(garden_id=garden.id)
    for _ in rs:
        sentry_list.append(dict(name=_.name, ip=_.ip))

    data['innerbox_list'] = innerbox_list
    data['sentry_list'] = sentry_list

    result = data
    return CR(result=result)

def init_innerscreen():
    """室内屏初始化"""
    return init_innerbox()

def init_propapp(type_):
    ip = request.remote_addr.split(':')[-1]
    app = None
    if type_ == SystemDeviceType.PropCallApp:
        app = model.PropertyCallApp.get(ip = ip)
    if type_ == SystemDeviceType.PropSentryApp:
        app = model.SentryApp.get(ip = ip)
    if not app:
        return ErrorReturn(ErrorDefs.ObjectNotExist, u'室内主机未登记')

    app.login_time = datetime.datetime.now()
    app.save()

    data = {}
    garden = model.HomeGarden.get(id='8888')
    data['stream_server_url'] = garden.stream_server_url

    sentry_list = []
    rs = model.SentryApp.find(garden_id=garden.id)
    for _ in rs:
        sentry_list.append(dict(name=_.name, ip=_.ip))

    outerbox_list = []
    rs = model.OuterBox.find(garden_id=garden.id)
    for _ in rs:
        outerbox_list.append(dict(type='B', name=_.name, ip=_.ip, is_primary=_.is_primary , floor=_.floor))

    rs = model.OuterBox.find(garden_id=garden.id, type='A')  # 围墙机
    for _ in rs:
        outerbox_list.append(dict(type='A', name=_.name, ip=_.ip, is_primary=_.is_primary, floor=_.floor))

    data['sentry_list'] = sentry_list
    data['outerbox_list'] = outerbox_list
    result = data

    return CR(result=result)

def normalize_ip(ip):
    """
    '::ffff:11.21.2.11'
    :param ip:
    :return:
    """
    return ip.split(':')[-1]

def init_data():
    """设备启动获取运行参数"""

    # id = request.values.get('id')
    type_ = request.values.get('type',None,type=int)
    ip = request.remote_addr

    os = request.values.get('os','')

    result = ErrorReturn(ErrorDefs.UnknownError)

    # if not id or  not(os) or not(type_):
    #     return ErrorReturn(ErrorDefs.ParameterInvalid).response

    if type_ not in SystemDeviceType.ValidatedList:
        return ErrorReturn(ErrorDefs.ParameterInvalid,u'设备类型不存在').response

    if type_ == SystemDeviceType.InnerBox:
        result = init_innerbox()

    if type_ == SystemDeviceType.OuterBox:
        result = init_outerbox()

    if type_ == SystemDeviceType.InnerScreen:
        result = init_innerbox()

    if type_ in (SystemDeviceType.PropCallApp,SystemDeviceType.PropSentryApp):
        result = init_propapp(type_)
    return result.response

# @login_check
def get_innerbox_list():
    """查询室内机列表"""
    # auth = g.auth
    # token = request.values.get('token')
    type_ = request.values.get("type",type=int)
    ip = request.remote_addr.split(':')[-1]

    result = []
    if type_ in SystemDeviceType.ValidatedList and type_ != SystemDeviceType.OuterBox:
        """室内发起获取园区所有室内机的请求"""
        rs = model.InnerBox.find()
        for r in rs:
            result.append(dict( ip=r.ip,room_id=r.room_id ))

    if type_ == SystemDeviceType.OuterBox:
        """室外机查询室内机"""
        # 判别是围墙机还是单元机
        box = model.OuterBox.get(ip = ip)
        if not box:
            return ErrorReturn(ErrorDefs.ObjectNotExist, u'室外主机未登记')

        if box.type == 'A': # 围墙机，返回所有
            rs = model.InnerBox.find()
            for r in rs:
                result.append(dict(ip=r.ip, room_id=r.room_id))
        if box.type == 'B' : # 单元机 ,返回本单元相关的室内主机
            rs = model.InnerBox.find(building_id=box.building_id)
            for r in rs:
                result.append(dict(ip=r.ip, room_id=r.room_id))

    return CR(result=result).response


# @login_check
def report_device_event():
    '''上报设备运行事件'''
    from dateutil.parser import parse
    import time

    dev_type = request.values.get('dev_type')
    dev_id = request.values.get('dev_id','')
    event = request.values.get("event")
    time = request.values.get("time",time.time())
    content = request.values.get("content")
    encode = request.values.get('encode','plain')

    time =datetime.datetime.fromtimestamp(float(time))

    colls = {
        SystemDeviceType.InnerBox:model.InnerBoxLog,
        SystemDeviceType.InnerScreen: model.InnerScreenLog,
        SystemDeviceType.OuterBox: model.OuterBoxLog,
        SystemDeviceType.PropCallApp: model.PropertyAppLog,
        SystemDeviceType.PropSentryApp: model.SentryAppLog,
    }
    dev_type = int(dev_type)
    if dev_type  not in colls.keys():
        return CR().response


    fields = dict(
        name = '',
        id = dev_id,
        os ='',
        ver = '',
        ip = request.remote_addr.split(':')[-1],
        time = time,
        event = event,
        sys_time = datetime.datetime.now(),
        content = content
    )
    if encode == 'json':
        jsondata = json.loads(content)
        fields.update( jsondata )

    collCls = colls[dev_type]

    doc = collCls()
    doc.assign( fields )
    doc.save()

    if event == 'emergency':
        processEmergency()
    return CR().response

def processEmergency():
    """保存报警记录"""
    import time
    dev_type = request.values.get('dev_type',type=int)
    dev_id = request.values.get('dev_id', '')
    event = request.values.get("event")
    time = request.values.get("time", time.time())
    content = request.values.get("content")

    encode = request.values.get('encode', 'plain')
    e = json.loads(content)

    port = e['port']
    name = e['name']
    secretkey = e['rand_key']

    box = model.InnerBox.get(ip = request.remote_addr.split(':')[-1])
    if box:
        _ = model.Emergency()
        _.room_id = box.room_id
        _.type = ''
        _.sys_time = datetime.datetime.now()
        _.port = port
        _.name = name
        _.detail = ''
        _.secret_key = secretkey
        _.save()

def query_device_event():
    '''查询设备运行事件'''
    """
        - dev_type 	设备类型 1: 室内主机 ，2：室内屏设备 ，3：室外机 ， 4： 物业App
        - dev_id 		设备编号
        - room_id 	房间号 （可选)
        - event		事件名称
        - start		开始时间
        - end			结束时间 ( 不提供则一天)
    """
    dev_type = request.values.get('dev_type', type=int)
    dev_id = request.values.get('dev_id', '')
    room_id = request.values.get('room_id', '')
    event = request.values.get('event', '')

    start = request.values.get('start', 0)
    end = request.values.get('end', 0)
    if not end:
        end = start + 3600 *24

    start = datetime.datetime.fromtimestamp(float(start))
    end  = datetime.datetime.fromtimestamp(float(end))

    colls = {
        SystemDeviceType.InnerBox: model.InnerBoxLog,
        SystemDeviceType.InnerScreen: model.InnerScreenLog,
        SystemDeviceType.OuterBox: model.OuterBoxLog,
        SystemDeviceType.PropCallApp: model.PropertyAppLog,
        SystemDeviceType.PropSentryApp: model.SentryAppLog,
    }
    dev_type = int(dev_type)
    if dev_type not in colls.keys():
        return CR().response

    fields = dict( )
    if room_id:
        fields['room_id'] = room_id
    if dev_id:
        fields['dev_id'] = dev_id
    if event:
        fields['event'] = event
    fields['start'] = start
    fields['end'] = end

    collCls = colls[dev_type]
    rs = collCls.find( **fields ).sort('time',1)
    result = []
    for r in rs:
        result.append(dict(dev_id=r.id, event=r.event,time = str(r.time), content = r.content ))

    return CR(result=result).response

def report_device_status():
    '''上报设备状态'''
    from dateutil.parser import parse

    from mantis.fundamental.utils.useful import object_assign
    import time
    dev_type = request.values.get('dev_type',type=int)
    dev_id = request.values.get('dev_id','')
    time = request.values.get('time',0)
    ver = request.values.get('ver','')
    detail = request.values.get('detail','')

    start = request.values.get('start',0)
    elapsed = request.values.get('elapsed','')

    if time:
        time = datetime.datetime.fromtimestamp( float(time) )
    else:
        time = None

    # 支持任意多的状态参数输入
    fields =  request.values.to_dict()

    fields.update( dict(
        name = '',
        type = dev_type,
        id = dev_id,
        time = time ,
        ip = request.remote_addr.split(':')[-1],
        os = '',
        ver = '',
        sys_time = datetime.datetime.now(),
        detail = detail
    ))

    _ = model.DeviceBaseStatus()
    object_assign(_,fields,add_new=True)
    _.save()

    return CR().response

def vision_image_upload():
    """上传对讲影像"""
    service = instance.serviceManager.get('main')
    store_path = service.getConfig().get('image_store_path')
    f = request.files['file']
    device_id = request.values.get('dev_id') # 设备硬件编号
    box = model.InnerBox.get(ip = request.remote_addr.split(':')[-1])
    if not box:
        return CR().response

    room_id = box.room_id

    fmt='%Y-%m-%d_%H-%M-%S.mp4'
    timetick = time.strftime(fmt, time.localtime())
    store_path = os.path.join(store_path,room_id)
    if not os.path.exists(store_path):
        os.makedirs(store_path)
    store_path = os.path.join(store_path,timetick)
    f.save(store_path)
    return CR().response


#室外机请求qr码到绿城家的验证
def qr_opendoor():
    result = 0
    return CR(result=result).response

# 请求室内主机开门
def opendoor_innerbox():
    room_id = request.values.get('room_id')
    secret_key = request.values.get('secret_key')
    if not room_id or not secret_key:
        return ErrorReturn(ErrorDefs.ParameterInvalid).response

    # 查询室内机主机ip
    box = model.InnerBox.get(room_id=room_id)  # 根据ip检查设备是否登记
    if not box:
        return ErrorReturn(ErrorDefs.ObjectNotExist, u'室内主机未登记')
    ip = box.ip

    # 发送开门请求
    url = 'http://{}:7890/smartbox/api/emergency/opendoor'.format( box.ip )
    token = Constants.SUPER_ACCESS_TOKEN
    requests.post(url,data=dict( token=token, rand_key = secret_key) )

    return CR().response