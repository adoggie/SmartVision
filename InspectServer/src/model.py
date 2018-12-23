#coding:utf-8

from mantis.fundamental.application.app import instance
from mantis.fundamental.utils.useful import hash_object,object_assign
from bson import ObjectId
from mantis.BlueEarth.types import PositionSource,AlarmSourceType,CoordinateType

# database = None

def get_database():
    service = instance.serviceManager.get('main')
    database = service.get_database()
    return database



class Model(object):
    def __init__(self):
        self._id = None
        self.__collection__ = self.__class__.__name__

    @property
    def id(self):
        return str(self._id)

    @classmethod
    def find(cls,**kwargs):
        clsname = cls.__name__
        coll = get_database()[clsname]
        rs = coll.find(kwargs)
        result =[]
        for r in list(rs):
            obj = cls()
            object_assign(obj,r)
            result.append(obj)
        return result

    def dict(self):
        data = hash_object(self)
        if data.has_key('_id'):
            del data['_id']
        return data

    # @staticmethod
    # def get(self,**kwargs):
    #     pass

    @classmethod
    def collection(cls):
        coll = get_database()[cls.__name__]
        return coll

    @classmethod
    def get(cls,**kwargs):
        obj = cls()
        coll =  get_database()[cls.__name__]
        data = coll.find_one(kwargs)
        if data:
            object_assign(obj,data)
            return obj
        return None

    @classmethod
    def get_or_new(cls, **kwargs):
        obj = cls.get(**kwargs)
        if not obj:
            obj = cls()
            object_assign(obj,kwargs)
        return obj

    def assign(self,data):
        object_assign(self,data)

    def delete(self):
        """删除当前对象"""
        coll = get_database()[self.__collection__]
        coll.delete_one({'_id':self._id})

    def update(self,**kwargs):
        """执行部分更新"""
        coll = get_database()[self.__collection__]
        coll.update_one({'_id':self._id},update={'$set':kwargs},upsert = True)
        return self

    def save(self):
        coll = get_database()[self.__collection__]
        data = hash_object(self, excludes=['_id'])
        if self._id:
            self.update(**data)
        else:
            self._id = coll.insert_one(data).inserted_id
        return self

    @classmethod
    def spawn(cls,data):
        """根据mongo查询的数据集合返回类实例"""

        # 单个对象
        if isinstance(data,dict):
            obj = cls()
            object_assign(obj,data)
            return obj
        # 集合
        rs = []
        for r in data:
            obj = cls()
            object_assign(obj, data)
            rs.append(obj)
        return rs

    @classmethod
    def create(cls,**kwargs):
        obj = cls()
        object_assign(obj,kwargs)
        return obj


class InnberBoxCheck(Model):
    """设备信息"""
    def __init__(self):
        Model.__init__(self)
        self.ip = ''     # 设备唯一编号
        self.port = 0
        self.check_time = 0
        self.check_time_s = ''
        self.pid = 0
        self.systime = ''
        self.version=''
        self.innerinterfaceip=''
        self.outerboxip=''
        self.outerboxip_1 = ''
        self.gateouterboxip = ''
        self.propertyip = ''
        self.streamserverip = ''
        self.callcenterip = ''
        self.alarmcenterip = ''
        self.doorid = ''
        self.appaudiostreamurl = ''
        self.outerboxvideostreamurl=''
        self.outerboxvideostreamurl_1=''
        self.gateouterboxvideostreamurl=''
        self.mem_total=0
        self.mem_used = 0
        self.mem_free = 0
        self.mem_rss = 0
        self.threads = 0
        self.fds = 0

class InnberBoxCheckSnapShot(InnberBoxCheck):
    """设备信息"""
    def __init__(self):
        InnberBoxCheck.__init__(self)
