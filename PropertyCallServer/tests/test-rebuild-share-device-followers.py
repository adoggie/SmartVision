# coding: utf-8
'''
重建分享设备的参与者列表

读取share-device记录的参与分享用户，添加到ShareDeviceFollower表中

'''
from mantis.fundamental.nosql.mongo import Connection
from mantis.BlueEarth import model
from mantis.fundamental.parser.yamlparser import YamlConfigParser
from mantis.fundamental.utils.timeutils import timestamp_current
from mantis.fundamental.utils.useful import object_assign

from bson import ObjectId

def get_database():
    db = Connection('BlueEarth').db
    return db

model.get_database = get_database

def rebuild_device_followers():

    model.ShareDeviceFollower.collection().delete_many({})

    shares = model.SharedDeviceLink.find()
    # print shares
    for share in shares:
        devices = model.DeviceUserRelation.find(is_share_device=True,share_device_link = share.id)
        for device in devices:
            user = model.User.get(_id=  ObjectId(device.user_id) )
            print '=*'*20
            print share.name
        # print share.user_id
        # print user
        # wxuser = model.WxUser(open_id = user.account)
            follower = model.ShareDeviceFollower.get(share_id=share.id,user_id=user.id)
            if not follower:
                follower = model.ShareDeviceFollower()
                follower.user_id = user.id
                follower.open_id = user.account
                follower.share_id = share.id
                follower.device_id = share.device_id
                follower.create_time = timestamp_current()
                wxuser = model.WxUser.get(open_id=user.account)
                if wxuser:
                    follower.avatar_url = wxuser.avatarUrl
                    follower.nick_name = wxuser.nickName
                print follower.nick_name
                # print follower.dict()
                follower.save()



rebuild_device_followers()


