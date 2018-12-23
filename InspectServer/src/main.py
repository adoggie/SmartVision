# coding:utf-8


import os,sys
from mantis.fundamental.utils.useful import singleton
from mantis.fundamental.application.app import instance
from mantis.trade.service import TradeService,TradeFrontServiceTraits,ServiceType,ServiceCommonProperty
from optparse import OptionParser
from mantis.fundamental.utils.importutils import import_class
from check_controller import CheckProcessController

class MainService(TradeService):
    def __init__(self,name):
        TradeService.__init__(self,name)
        self.logger = instance.getLogger()
        self.servers = {}
        self.command_controllers ={}

    def init(self, cfgs,**kwargs):
        # self.parseOptions()
        super(MainService,self).init(cfgs)


    def setupFanoutAndLogHandler(self):
        from mantis.trade.log import TradeServiceLogHandler
        self.initFanoutSwitchers(self.cfgs.get('fanout'))
        handler = TradeServiceLogHandler(self)
        self.logger.addHandler(handler)

    def start(self,block=True):
        TradeService.start(self)
        CheckProcessController().init().start()

    def stop(self):
        TradeService.stop(self)

    def initCommandChannels(self):
        pass
        # TradeService.initCommandChannels(self)
        # channel = self.createServiceCommandChannel(CommandChannelTradeAdapterLauncherSub,open=True)
        # self.registerCommandChannel('trade_adapter_launcher',channel)


    def get_database(self):
        conn = instance.datasourceManager.get('mongodb').conn
        return conn['inspect_server']

    def getDbModel(self):
        import model
        return model