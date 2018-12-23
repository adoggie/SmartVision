#coding:utf-8

import json
from flask import Flask,request,g
from flask import Response
import requests
import base64
from StringIO import StringIO

from flask import render_template
from mantis.fundamental.application.app import  instance
from mantis.fundamental.utils.useful import cleaned_json_data

from mantis.fundamental.flask.webapi import ErrorReturn,CR
from mantis.fundamental.utils.timeutils import timestamp_current
from mantis.BlueEarth import model
from mantis.fundamental.utils.useful import object_assign,hash_object
from token import login_check,AuthToken
from mantis.BlueEarth.errors import ErrorDefs
from bson import ObjectId
from mantis.BlueEarth import constants
from mantis.BlueEarth.types import FenceType
import capacity


def get_ticket():
    """上传微信 code , 与微信服务器交互，获得open_id,并返回token到前端"""
    main = instance.serviceManager.get('main')
    code = request.values.get('code')
    encryptedData = request.values.get('encryptedData')
    iv = request.values.get('encryptedData')

    if not code:
        return ErrorReturn(ErrorDefs.UserNotExist).response

    appid = main.getConfig().get('wx_settings').get('appid')
    secret = main.getConfig().get('wx_settings').get('secret')

    params=dict(
        appid = appid,
        secret = secret,
        js_code = code,
        grant_type = 'authorization_code'
    )
    url = main.getConfig().get('wx_settings').get('code2session_url')
    resp = requests.get(url,params)
    data = resp.json()
    open_id = ''
    session_key = ''
    union_id = ''

    # if data.get('errcode') == 0:
    open_id = data.get('openid')
    session_key = data.get('session_key')
        # union_id  = data.get('unionid')

    if not open_id:
        return ErrorReturn(ErrorDefs.AccessDenied,u'微信服务器返回错误:{}'.format(data.get('errcode'))).response

    user = model.User.get_or_new(platform='wx',account=open_id)
    user.platform = 'wx'
    user.last_login = timestamp_current()
    user.save()

    auth = AuthToken()
    auth.login_time = timestamp_current()
    auth.user_id = user.account
    auth.user_name = user.name
    auth.platform = 'wx'
    auth.open_id = open_id
    user.token = auth.encode()

    user.save()

    result = dict( open_id=open_id, union_id=union_id,token = user.token)
    return CR(result= result).response

@login_check
def update_wxuser_info():
    """更新微信用户信息"""
    user = g.user

    wxuser = model.WxUser.get_or_new(user_id=user.id)
    props = request.values.to_dict()
    object_assign(wxuser,props)
    wxuser.user_id = user.id
    wxuser.open_id = g.auth.open_id
    wxuser.save()
    return CR().response


@login_check
def get_wxuser_info():
    """查询微信用户信息"""
    user = g.user
    user_id = request.values.get('user_id')

    wxuser = model.WxUser.get(user_id=user_id)
    if not wxuser:
        wxuser = model.WxUser()
        wxuser.user_id = user.id
    return CR(result=wxuser.dict()).response


@login_check
def update_wxsystem_info():
    """更新微信系统信息 ， 一个微信账户可能在多台主机登录"""
    user = g.user
    props = request.values.to_dict()
    wxsys = model.WxSystemInfo.get_or_new(user_id=user.id)
    object_assign(wxsys,props)
    wxsys.open_id = g.auth.open_id
    wxsys.save()

    return CR().response

@login_check
def add_device():
    """添加用户设备
    默认密码为设备编码后缀4位数字
    """
    user = g.user
    device_id = request.values.get('device_id')
    password = request.values.get('password')
    name = request.values.get('name')
    device  = model.Device.get(device_id = device_id)
    if not password:
        return ErrorReturn(ErrorDefs.NeedPassword).response

    if not device:
        return ErrorReturn(ErrorDefs.ObjectNotExist,u'设备不存在').response

    if not device.password: # 密码不存在就采用设备后四位
        if password != device_id[-4:]:
            return ErrorReturn(ErrorDefs.PasswordError).response
    else:
        if password != device.password:
            return ErrorReturn(ErrorDefs.PasswordError).response

    if not device.active:
        device.active = True
        device.save()

    # 检测设备是否已经添加了
    rel = model.DeviceUserRelation.get(user_id= str(user.id),device_id=device_id )
    if rel:
        return CR(result=str(rel.id)).response



    rel = model.DeviceUserRelation()
    rel.user_id = user.id
    rel.device_id = device_id
    rel.update_time = timestamp_current()
    rel.device_type = device.device_type
    rel.save()
    return CR(result=rel.id).response


@login_check
def update_device():
    user = g.user
    device_id = request.values.get('device_id')
    password = request.values.get('password')
    name = request.values.get('name')
    mobile =request.values.get('mobile') # 设备内置电话号码
    admin_mobile = request.values.get('admin_mobile') # 管理人手机号
    image = request.values.get('image')
    map_scale = request.values.get('map_scale',0,type=int)

    device = model.Device.get(device_id=device_id)
    if not device:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response

    # 检测设备是否已经添加了
    rel = model.DeviceUserRelation.get(user_id=str(user.id), device_id=device_id)
    if not rel: # 设备不存在
        return ErrorReturn(ErrorDefs.ObjectNotExist).response
    if name:
        rel.device_name = name  # 仅修改用户设备名称，原始设备名称并不修改
    if map_scale:
        rel.map_scale = map_scale # 保存设备当前的地图缩放级别

    if rel.is_share_device: # 共享设备的话，用户只能修改名称
        if name:
            rel.save()
        return CR().response

    if name or map_scale:
        rel.save()

    kwargs ={}

    if password:
        kwargs['password'] = password
    if mobile:
        kwargs['mobile'] = mobile
    if admin_mobile:
        kwargs['admin_mobile'] = admin_mobile
    if image:
        kwargs['image'] = image

    if kwargs:
        device.update(**kwargs)

    return CR().response


@login_check
def set_view_device_top():
    """设备信息置顶"""
    user = g.user
    device_id = request.values.get('device_id')

    device = model.Device.get(device_id=device_id)
    if not device:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response

    # 检测设备是否已经添加了
    rel = model.DeviceUserRelation.get(user_id=str(user.id), device_id=device_id)
    if not rel: # 设备不存在
        return ErrorReturn(ErrorDefs.ObjectNotExist).response


    kwargs ={}
    kwargs['order'] = -timestamp_current()

    device.update(**kwargs)

    return CR().response

@login_check
def remove_device():
    """删除设备时，要连同删除被分享出去的设备"""
    user = g.user
    device_id = request.values.get('device_id')
    rel = model.DeviceUserRelation.get(user_id=str(user.id), device_id=device_id)
    if not rel:  # 设备不存在
        return CR(result=[]).response

    if not rel.is_share_device:
        # 查找设备的分享链接
        link = model.SharedDeviceLink.get(user_id=user.id,device_id=device_id)
        if link:
            coll = model.DeviceUserRelation.collection()
            coll.delete_many({'is_share_device':True,'share_device_link':link.id})
            link.delete()

    rel.delete()

    return CR().response

@login_check
def get_device_info():
    """"""
    user = g.user
    device_id = request.values.get('device_id')
    rel = model.DeviceUserRelation.get(user_id=str(user.id), device_id=device_id)
    if not rel:  # 设备不存在
        return ErrorReturn(ErrorDefs.ObjectNotExist).response
    device  = model.Device.get(device_id=device_id)

    result = dict(
        device_id = device_id,
        device_type = device.device_type,
        imei = device.imei,
        sim = device.sim ,
        mobile = device.mobile,
        admin_mobile = device.admin_mobile,
        name = rel.device_name,
        image = rel.device_image,
        update_time = rel.update_time,
        is_share_device = rel.is_share_device,
        share_user_id = rel.share_user_id,
        share_device_link = rel.share_device_link,
        password = device.password,
        map_scale=rel.map_scale
    )
    if not result['password']:
        result['password'] = ''
    result['capacity'] = capacity.get_product_features(device.device_type)

    if not rel.device_name:
        result['name'] = device.name
    return CR(result=result).response

def make_query_orders(orders):
    """ 支持排序参数：
            path?orders=+name,-age
        返回 ：
            [（name,1),(age,-1) ]
    """
    fs = map(lambda _:_.strip(),orders.split(','))
    result=[]
    for f in fs:
        order =[f,1]
        if f[0] in ('+','-'):
            order[0] = f[1:]  # asce
            if f[0] == '-':
                order[1] = -1  # desc
        result.append(order)
    return result

@login_check
def get_device_list():
    """查询设备列表"""
    user = g.user
    limit = request.values.get('limit',10000,type=int)

    orders = request.values.get('orders')
    if orders:
        orders = make_query_orders(orders)
        rs = model.DeviceUserRelation.collection().find({'user_id':user.id}).sort(orders).limit(limit)
    else:
        rs = model.DeviceUserRelation.collection().find({'user_id': user.id}).limit(limit)

    result = []
    for r in rs:
        rel = model.DeviceUserRelation()
        rel.assign(r)
        device = model.Device.get(device_id=r['device_id'])
        data = _get_last_position(device.device_id)
        obj = dict(
            device_id= device.device_id,
            device_type=device.device_type,
            imei=device.imei,
            sim=device.sim,
            mobile=device.mobile,
            admin_mobile=device.admin_mobile,
            name=rel.device_name,
            image=rel.device_image,
            update_time=rel.update_time,
            is_share_device=rel.is_share_device,
            share_user_id=rel.share_user_id,
            share_device_link=rel.share_device_link,
            map_scale = rel.map_scale,
            position = data
        )
        if not rel.device_name:
            obj['name'] = device.name
        obj['capacity'] = capacity.get_product_features(device.device_type)
        result.append(obj)
    return CR(result=result).response

@login_check
def create_share_device():
    """创建设备共享
        url: /share-device
        post
    """
    import share
    return share.create_share_device()


@login_check
def update_share_device():
    """更新共享设备信息"""
    import share
    return share.update_share_device()

@login_check
def remove_share_device():
    """删除设备共享, 共享发起人删除共享的设备，导致所有的共享连接失效"""
    import share
    return share.remove_share_device()

@login_check
def take_share_device():
    """用户接收共享设备，添加成为自己的设备
        如果设置了密码，且用户未上传密码，则提示密码输入
        @:return 返回新设备id
    """
    import share
    return share.take_share_device()

@login_check
def get_share_device_follower_list():
    """获取关注位置设备的好友
    """
    import share
    return share.get_share_device_follower_list()

# /share-device
@login_check
def get_device_share_info():
    """查询本人创建的共享设备信息
        支持 共享设备编号查询 或 设备编号查询
    """
    import share
    return share.get_device_share_info()

@login_check
def get_share_device_list():
    """查询本人创建的所有共享设备列表
    """
    import share
    return share.get_share_device_list()


# /share-device/access-user
@login_check
def get_share_device_access_user():
    """共享设备的访问用户信息
    """
    import share
    return share.get_share_device_access_user()

# /share-device/access-user/list
@login_check
def get_share_device_access_user_list():
    """查询指定共享设备关联的所有访问用户记录
    """
    import share
    return share.get_share_device_access_user_list()

def _get_last_position(device_id):
    """
    我去,网上的 wgs84转gcj02是错误的，还是利用之前高德的转换函数是okay的

    """
    import traceback
    from mantis.BlueEarth.types import PositionSource
    from mantis.BlueEarth.tools.geotools import wgs84_to_gcj02
    name = constants.DevicePositionLastest.format(device_id=device_id)
    redis = instance.datasourceManager.get('redis').conn
    data = redis.hgetall(name)
    try:
        data['locate_mode'] = '/'
        ps = int(data.get('position_source',0))
        if ps == PositionSource.GPS:
            data['locate_mode'] ='GPS'
        if ps == PositionSource.LBS:
            data['locate_mode'] ='LBS'
        if ps == PositionSource.WIFI:
            data['locate_mode'] ='WIFI'
        lon = float(data['lon'])
        lat = float(data['lat'])
        print data['lon'],data['lat']
        data['lon'],data['lat'] = wgs84_to_gcj02(lon,lat)
        print data['lon'], data['lat']
    except:
        traceback.print_exc()
    return data

@login_check
def get_last_position():
    """查询设备最近的位置信息
    """
    user = g.user
    device_id = request.values.get('device_id')
    data = _get_last_position(device_id)
    pos = model.Position()
    object_assign(pos,data)
    # return CR(result=pos.__dict__).response
    return CR(result=data).response

@login_check
def get_last_position_multi():
    """查询多个设备最近的位置信息
    """
    user = g.user
    text = request.values.get('device_ids','')
    if not text:
        return CR(result=[]).response
    device_ids = text.split(',')
    result = []
    for device_id in device_ids:
        data = _get_last_position(device_id)
        # pos = model.Position()
        # object_assign(pos,data)
        result.append(data)
    # return CR(result=pos.__dict__).response
    return CR(result=result).response

def _get_position_path(start,end,granule,device_id):
    """查询设备历史轨迹点
        start - 开始时间
        end - 结束时间 , 最大时长不能超过 1 周
    """
    from mantis.BlueEarth.tools.geotools import wgs84_to_gcj02
    import path

    DAY = 3600 * 24
    WEEK = 3600 * 24 * 7

    user = g.user
    # device_id = request.values.get('device_id')
    # start = request.values.get('start',type=int)
    # end = request.values.get('end',0,type=int)
    # granule = request.values.get('granule',0,type=int) # 时间刻度(分钟)
    # duration = request.values.get('duration',1,type=int)
    if granule:
        granule = granule * 60

    if not end :
        end = start + DAY

    if end - start > WEEK:
        return ErrorReturn(ErrorDefs.ReachLimit).response

    rel = model.DeviceUserRelation.get(user_id=user.id,device_id=device_id)
    if not rel:
        return ErrorReturn(ErrorDefs.ObjectNotExist,u'设备不存在').response
    coll = model.Position.collection()
    rs = coll.find({'device_id':device_id,'timestamp':{'$gte':start,'$lt':end}}).sort('timestamp',1)
    rs = list(rs)
    result =rs
    # last = None
    result = path.clean_path(rs)
    for p in result:
        del p['_id']
        # if p['speed'] == 0:
        #     continue
        p['lon'], p['lat'] = wgs84_to_gcj02(p['lon'], p['lat'])
    return result

@login_check
def get_position_path():
    """查询设备历史轨迹点
        start - 开始时间
        end - 结束时间 , 最大时长不能超过 1 周
    """
    user = g.user
    device_id = request.values.get('device_id')
    start = request.values.get('start',type=int)
    end = request.values.get('end',0,type=int)
    granule = request.values.get('granule',0,type=int) # 时间刻度(分钟)
    result = _get_position_path(start,end,granule,device_id)

    return CR(result=result).response


@login_check
def get_position_path_multi():
    """查询多设备历史轨迹点
        start - 开始时间
        end - 结束时间 , 最大时长不能超过 1 周
    """
    user = g.user
    device_ids = request.values.get('device_ids').split(',')
    start = request.values.get('start',type=int)
    end = request.values.get('end',0,type=int)
    granule = request.values.get('granule',0,type=int) # 时间刻度(分钟)
    result = []
    for device_id in device_ids:
        path = _get_position_path(start,end,granule,device_id)
        result.append(path)
    return CR(result=result).response

@login_check
def get_device_config():
    """查询设备配置参数
    """
    import device_config
    return device_config.get_device_config()

@login_check
def set_device_config():
    """设置设备配置参数
    """
    import device_config
    return device_config.set_device_config()


@login_check
def get_device_fence():
    """获得设备的围栏参数
    """
    user = g.user
    device_id = request.values.get('device_id')
    fence = model.Fence.get(device_id=device_id)
    if not fence:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response
    result = fence.dict()
    return CR(result=result).response

@login_check
def set_device_fence():
    """设置设备的围栏参数
    """
    user = g.user
    device_id = request.values.get('device_id')
    data = {}
    for k, v in request.args.items():
        data[k] = v

    fence = model.Fence.get_or_new(device_id=device_id)
    object_assign(fence,data)
    if fence.type not in FenceType.ALL:
        return ErrorReturn(ErrorDefs.ParameterInvalid).response
    fence.index = int(fence.index)
    fence.enable = int(fence.enable)
    fence.cx = float(fence.cx)
    fence.cy = float(fence.cy)
    fence.radius = int(fence.radius)
    fence.x1 = float(fence.x1)
    fence.y1 = float(fence.y1)
    fence.x2 = float(fence.x2)
    fence.y2 = float(fence.y2)
    fence.alarm_type = int(fence.alarm_type)

    device = model.Device.get(device_id = device_id)
    main = instance.serviceManager.get('main')
    # 发送设置围栏命令
    cmd = ''
    cc = main.getCommandController(device.device_type)
    if fence.type == FenceType.CIRCLE:
        cmd = cc.setCircleFence(fence.cx,fence.cy,fence.radius,fence.inout)
    if cmd:
        main.sendCommand(device_id,cmd)
    fence.save()
    return CR().response

@login_check
def cmd_start_audio_record():
    """启动远程录像
    """
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)
    cmd = cc.start_audio_record()
    cmd_send(device_id, cmd, online=True)
    # main.sendCommand(device_id, cmd)
    return CR().response

@login_check
def cmd_start_audio_listen():
    """启动远程监听
    """
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)
    cmd = cc.start_audio_listen()
    # main.sendCommand(device_id, cmd)
    cmd_send(device_id, cmd, online=True)
    return CR().response

@login_check
def cmd_position_now_gps():
    """立即定位
    """
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)
    cmd = cc.positionNowGps()
    # main.sendCommand(device_id, cmd)
    cmd_send(device_id, cmd, online=True)
    cmd = cc.positionNowLbs()
    cmd_send(device_id, cmd, online=True)
    return CR().response

@login_check
def cmd_position_now_lbs():
    """立即定位
    """
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)
    cmd = cc.positionNowLbs()
    # main.sendCommand(device_id, cmd)
    cmd_send(device_id, cmd, online=True)
    return CR().response

@login_check
def cmd_position_now():
    """立即定位, 这里要进行请求阀控
     2018.10.21  禁止发送 lbs 定位请求，在实时监控过程中，gps信号okey的
     情况下，请求lbs，导致轨迹漂移
    """
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)


    #设备上一次发送立即定位命令的时间
    # redis记录最近一次发送时间，直到key失效之前不能重复发送
    # 保证大量的客户端不会对设备发送大量的立即定位命令
    name = constants.DevicePositionRequestTimeKey.format(device_id)
    redis = instance.datasourceManager.get('redis').conn
    value = redis.get(name)
    if not value:
        cmd = cc.positionNowGps()
        cmd_send(device_id, cmd, online=True)
        # cmd = cc.positionNowLbs()
        # cmd_send(device_id, cmd, online=True)
        expire = main.getConfig().get('max_position_request_interval',10)
        redis.set(name,timestamp_current(),expire)

    # cmd = cc.positionNowLbs()
    # main.sendCommand(device_id, cmd)
    # cmd = cc.positionNowGps()
    # main.sendCommand(device_id, cmd)
    return CR().response

@login_check
def cmd_shutdown():
    """此命令将被立即执行，不能进入队列，设备不在线就丢弃"""
    user = g.user
    device_id = request.values.get('device_id')
    main = instance.serviceManager.get('main')
    device = model.Device.get(device_id=device_id)
    cc = main.getCommandController(device.device_type)
    cmd = cc.shutdown()

    cmd_send(device_id,cmd,online=True)
    return CR().response

def cmd_send(device_id,command,online=False):
    main = instance.serviceManager.get('main')

    # url = main.getConfig().get('location_server_api_command')
    if not online:
        main.sendCommand(device_id, command)
    else:
        redis = instance.datasourceManager.get('redis').conn
        landing_url_key = constants.DeviceLandingServerKey.format(device_id)
        url = redis.get(landing_url_key)
        params = dict(device_id=device_id, online=True, command=command)
        ret = requests.post(url, params)

@login_check
def cmd_reboot():
    """此命令将被立即执行，不能进入队列，设备不在线就丢弃"""
    user = g.user
    device_id = request.values.get('device_id')


@login_check
def get_audio_record_list():
    """获取录音记录列表"""
    user = g.user
    device_id = request.values.get('device_id')
    limit = request.values.get('limit')

    coll = model.AudioRecord.collection()
    if not limit:
        limit = 30
    rs = coll.find({'device_id':device_id}).sort('report_time',-1).limit(limit)

    result = cleaned_json_data(list(rs),('content',))  # 不传输音频文件内容
    return CR(result=result).response

@login_check
def get_audio_content():
    """获取录音记录
    """
    user = g.user
    # device_id = request.args.get('device_id')
    audio_id = request.args.get('audio_id')
    audio = model.AudioRecord.get(_id=ObjectId(audio_id))

    def generate():
        data = base64.b64decode(audio.content)
        stream = StringIO(data)
        data = stream.read(1024)
        while data:
            yield data
            data = stream.read(1024)

    return Response(generate(), mimetype="audio/mpeg3")

@login_check
def get_device_password():
    """找回设备密码 ,系统并不创建密码
    如果设备未激活，未绑定手机，则读取设备密码并发送短信到用户手机
    支持短设备号
    """
    from mantis.BlueEarth.utils import make_password,encrypt_text
    user = g.user
    open_id = g.auth.open_id
    device_id = request.values.get('device_id')
    # phone = request.values.get('phone')
    phone = ''
    if  not device_id:
        return ErrorReturn(ErrorDefs.ParameterInvalid).response

    device_id = device_id.lower()
    device = model.Device.get(device_id=device_id)
    if not device:
        device = model.Device.get(short_id=device_id)
    if not device:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response

    # 设备已经被激活了，且存在电话号码，则向电话号码发送密码
    if device.active:
        phone = device.admin_mobile
    else:
        return ErrorReturn(ErrorDefs.AccessDenied,u'设备未激活,无法发送设备密码! ').response

    password = device.password
    if not password:
        return ErrorReturn(ErrorDefs.ObjectNotExist,u'设备密码未设置,请用默认密码登录! ').response
    if not phone:
        return ErrorReturn(ErrorDefs.ObjectNotExist,u'设备未设置电话号码，请联系系统运营人员! ').response

    # if not password: #  密码不存在则创建初始设备密码
    #     password = encrypt_text(make_password())
    #     device.password = password
    #     device.save()
    phone = phone.strip()
    if phone:
        main = instance.serviceManager.get('main')
        main.sendDevicePassword(phone,device.device_id,password)
    return CR(errmsg=u'密码已成功发送至'+ phone).response

@login_check
def create_share_device_code():
    """生成分享码"""
    import share
    share.create_share_device_code()

# -----------------------------------


@login_check
def create_favorite():
    """
     收藏产品
    """
    import favorite
    return favorite.create_favorite()


@login_check
def get_favorite_list():
    """
    查询收藏商品
    """
    import favorite
    return favorite.get_favorite_list()

@login_check
def get_favorite():
    """
    """
    import favorite
    return favorite.get_favorite()

@login_check
def remove_favorite():
    """
    """
    import favorite
    return favorite.remove_favorite()


#- ---- ---------

@login_check
def create_cargo_address():
    """查询本人创建的共享设备信息
        支持 共享设备编号查询 或 设备编号查询
        如果共享记录未创建则创建，并返回
    """
    import cargo_address
    return cargo_address.create_cargo_address()

@login_check
def get_cargo_address_list():
    """查询本人创建的共享设备信息
        支持 共享设备编号查询 或 设备编号查询
        如果共享记录未创建则创建，并返回
    """
    import cargo_address
    return cargo_address.get_cargo_address_list()

@login_check
def get_cargo_address():
    """查询本人创建的共享设备信息
        支持 共享设备编号查询 或 设备编号查询
        如果共享记录未创建则创建，并返回
    """
    import cargo_address
    return cargo_address.get_cargo_address()

@login_check
def update_cargo_address():
    """
    """
    import cargo_address
    return cargo_address.update_cargo_address()

@login_check
def remove_cargo_address():
    """
    """
    import cargo_address
    return cargo_address.remove_cargo_address()

@login_check
def share_device_follower_deny():
    import share
    return share.share_device_follower_deny()

@login_check
def share_device_follower_allow():
    import share
    return share.share_device_follower_allow()


#获取用户信息
@login_check
def get_user_info():
    user = g.user
    open_id = g.auth.open_id

    user = model.User.get(_id=ObjectId(g.user.id) )
    if not user:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response

    result = dict(
        account = user.account,
        name = user.name,
        avatar = user.avatar,
        mobile = user.mobile,
        email = user.email,
        address = user.address
    )
    return CR(result=result).response


# 更新用户信息
@login_check
def update_user_info():
    kwargs = request.values.to_dict()
    user = model.User.get(_id=ObjectId(g.user.id))
    if not user:
        return ErrorReturn(ErrorDefs.ObjectNotExist).response
    valid_fields = {}
    for k,v in kwargs.items():
        if k in ('name','avatar','mobile','email','address'):
            valid_fields[k] = v

    user.update(**valid_fields)
    return CR().response
