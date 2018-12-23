#coding:utf-8

"""
室内机测试主控

"""
import traceback
import gevent

from mantis.fundamental.utils.timeutils import current_datetime_string
from mantis.fundamental.utils.useful import singleton
from mantis.fundamental.application.app import instance
from mantis.fundamental.utils.importutils import import_class


@singleton
class CheckProcessController(object):
    def __init__(self):
        self.cfgs = {}
        self.processes = []

    def init(self):
        main = instance.serviceManager.get('main')
        self.cfgs = main.getConfig().get('checkProcesses')
        for cfg in self.cfgs:
            cls = import_class( cfg.get('class') )
            checkp = cls()
            checkp.init(**cfg)
            self.registerProcess(checkp)

        return self

    def start(self):
        for ck in self.processes:
            ck.start()

    def stop(self):
        for ck in self.processes:
            ck.stop()

    def registerProcess(self,checkp):
        self.processes.append(checkp)


