# coding:utf-8


import os,sys
import json
from mantis.fundamental.utils.useful import singleton
from mantis.fundamental.application.app import instance
from mantis.trade.service import TradeService,TradeFrontServiceTraits,ServiceType,ServiceCommonProperty
from optparse import OptionParser
from mantis.fundamental.utils.importutils import import_class
from adatper import PropAppConnectionAdapter,ExternalAppConnectionAdapter

class MainService(TradeService):
    def __init__(self,name):
        TradeService.__init__(self,name)
        self.logger = instance.getLogger()
        self.servers = {}
        self.command_controllers ={}
        self.outerbox_list={} # id:ip
        self.innerbox_list={} # ip:{doorid,ip}

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
        ExternalAppConnectionAdapter().init().start()
        PropAppConnectionAdapter().init().start()

        self.queryOuterBoxList()
        self.queryInnerBoxList()

    def stop(self):
        TradeService.stop(self)
        ExternalAppConnectionAdapter().stop()
        PropAppConnectionAdapter().stop()

    def getPropAppConnectionAdapter(self):
        return PropAppConnectionAdapter()

    def initCommandChannels(self):
        pass
        # TradeService.initCommandChannels(self)
        # channel = self.createServiceCommandChannel(CommandChannelTradeAdapterLauncherSub,open=True)
        # self.registerCommandChannel('trade_adapter_launcher',channel)

    def get_database(self):
        conn = instance.datasourceManager.get('mongodb').conn
        return conn['ismart-vision']

    def getOuterBoxList(self):
        return self.outerbox_list.keys()

    def getInnerBoxList(self):
        return self.innerbox_list.values()

    def getVideoStreamPushAddressOfOuterBox(self,ipaddr):
        return self.getConfig().get('stream_push_address').get('audio_url').format(ipaddr)

    def getAudioStreamPushAddressOfOuterBox(self,ipaddr):
        return self.getConfig().get('stream_push_address').get('outerbox_video_url').format(ipaddr)

    def getRoomIdByInnerBoxIp(self,ipaddr):
        ip_room = self.innerbox_list.get(ipaddr)
        if ip_room:
            return ip_room.get('door_id')
        return ''

    def getInnerBoxIpByRoomId(self,room_id):
        return ''

    def queryOuterBoxList(self):
        from conn import ShortConnectionOutgoing

        ipaddr = self.getConfig().get("gatebox_ipaddr")
        req = json.dumps( dict(method='ipaddr_query_of_all_outerbox',params={'ipaddr':ipaddr}) )
        # req = json.dumps( dict(method='ipaddr_query_of_outerbox',params={'ipaddr':ipaddr}) )
        # req = json.dumps( dict(method='ipaddr_query_of_outerbox',params={}) )
        ipaddr,port = self.getConfig().get("property_server").split(':')
        issue = ShortConnectionOutgoing(ipaddr,int(port))
        data = issue.sendAndRecv(req,60)
        if data:
            obj = json.loads(data)
            if obj.get('result') == 'ipconf_of_outerbox':
                values = obj.get('values',[])
                self.outerbox_list = values

        """ 极度不正规
         {
            "result": "ipconf_of_outerbox",
            "gateouterbox": "11.0.0.40",
            "outerbox_f1": "11.41.1.1",
            "outerbox_x1": "11.41.1.2",
            "streamserver": "11.0.0.1"
        }
        """

    def queryInnerBoxList(self):
        from conn import ShortConnectionOutgoing
        # req = json.dumps(dict(method='ipaddr_query_of_all_innerbox'))
        ipaddr = self.getConfig().get("gatebox_ipaddr")

        req = json.dumps(dict(method='ipaddr_query_of_innerbox',params={'ipaddr':ipaddr}))

        ipaddr, port = self.getConfig().get("property_server").split(':')
        issue = ShortConnectionOutgoing(ipaddr, int(port))
        data = issue.sendAndRecv(req, 60)
        obj = json.loads(data)
        if obj.get('result') == 'ipconf_of_innerbox':
            values = obj.get('values', [])
            self.innerbox_list = values