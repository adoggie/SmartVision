#coding:utf-8

"""
室内机测试主控

"""
import traceback
import gevent

# from gevent import monkey
# monkey.patch_all()
from mantis.fundamental.utils.timeutils import current_datetime_string,timestamp_current
from mantis.fundamental.application.app import instance
import socket,os,os.path
# import app

INNER_ADDR = ('11.41.1.41',18699)
# INNER_ADDR = ('11.41.1.11',18699)
# INNER_ADDR = ('11.41.2.11',18699)
# INNER_ADDR = ('localhost',18699)

def make_salt(random=''):
    import uuid
    return uuid.uuid4().hex

def parseCheckInfo(text):

    lines = text.split('\n')
    lines = map(lambda _:_.strip(),lines)
    lines = filter(lambda  _:_,lines)
    # print lines


    # lines = ['Pid = 108', 'Time = Sat Dec 15 22:36:53 2018',
    #          'App info = version:1.5.0.20181215.14-mc-I,innerinterfaceip:11.41.1.41,outerboxip:11.41.1.1,outerboxip_1:11.41.1.2,gateouterboxip:11.0.0.40propertyip:11.0.0.1,streamserverip:11.0.0.1,callcenterip:11.0.0.3,alarmcenterip:11.0.0.2,doorid:2-1-1-401,appaudiostreamurl:rtmp://%s:1935/hls/%s,outerboxvideostreamurl:rtsp://admin:admin@%s:554/stream1,outerboxvideostreamurl_1:rtsp://admin:admin@%s:554/stream1,gateouterboxvideostreamurl:rtsp://admin:admin@%s:554/stream1',
    #          'Free mem = Mem:          1001          414          587            0            4',
    #          'VmRSS = VmRSS:\t    3136 kB', 'Threads = Threads:\t9', 'Fds = 32']

    data = {}
    for line in lines:
        if line.startswith('Pid'):
            data['pid'] = line.split('=')[1].strip()
        if line.startswith('Time'):
            data['systime'] = line.split('=')[1].strip()
        if line.startswith('App info'):
            params = line.split('=')[1].strip().split(',')
            data['app_info'] = {}
            appinfo = {}
            for param in params:
                many = param.split(':')
                name = many[0]
                value = ':'.join(many[1:])
                appinfo[name] = value
            data.update( appinfo )

        if line.startswith('Free mem'):
            mem = {}
            params = line.split('=')[1]
            fs = params.split()
            fs = map(lambda _: _.strip(), fs)
            total = int(fs[1])
            used = int(fs[2])
            free = int(fs[3])
            mem = dict(mem_total=total, mem_used=used, mem_free=free)
            data.update(mem)

        if line.startswith('VmRSS'):
            params = line.split('=')[1]
            fs = params.split()
            fs = map(lambda _: _.strip(), fs)
            # print fs
            data['mem_rss'] = int(fs[1])
        if line.startswith('Threads'):
            params = line.split('=')[1]
            fs = params.split()
            fs = map(lambda _: _.strip(), fs)
            data['threads'] = int(fs[1])
        if line.startswith('Fds'):
            params = line.split('=')[1]
            data['fds'] = int(params.strip())

    return data

def check_one_host(address):
    print 'enter time:',current_datetime_string()
    print 'check_one_host:',address
    sock = socket.socket()
    data = {}
    try:
        with gevent.Timeout(20) as timeout:
            sock.connect(address)
            sock.sendall('{"method":"system_query_stat"}')
            data = sock.recv(10000)
            data = parseCheckInfo(data)
        return data
    except:
        # traceback.print_exc()
        print 'Error: host({}) not reachable.'.format(str(address))
        return {}
    finally:
        sock.close()


def check_inner_hosts():
    """测试多连接发送 show_version 消息看是否连接延时"""

    print 'create new thread:'
    for _ in range(1000):
        pool = []
        let = gevent.spawn(check_one_host,INNER_ADDR)
        pool.append(let)
        gevent.joinall(pool)
        gevent.sleep(1000)
    gevent.sleep(1000)


class CheckProcessInnerBox(object):
    def __init__(self):
        self.name = ''
        self.cfgs = {}
        self.ip_list = []
        self.running = False
        self.task = None

    def init(self,**cfgs):
        self.cfgs = cfgs

    def start(self):
        self.task = gevent.spawn(self.workThread)

    def stop(self):
        self.running = False
        self.task.join()

    def reload(self):
        iptable = os.path.join(instance.getConfigPath(),self.cfgs.get('iptable'))
        self.ip_list = filter(lambda  _:_ and _[0]!='#',map(lambda _:_.strip(),open(iptable).readlines()))

    def workThread(self):
        import copy
        self.running = True
        while self.running:
            gevent.sleep(self.cfgs.get('wait'),15)
            self.reload()
            ips = copy.copy(self.ip_list)
            pool = map(lambda _:gevent.spawn(self.checkTask,(_,self.cfgs.get('port'))),ips)
            gevent.joinall(pool)

    def checkTask(self,dest):
        from model import InnberBoxCheck,InnberBoxCheckSnapShot
        data = check_one_host(dest)
        if not data:
            return
        data['ip'], data['port'] = dest
        data['check_time_s'] = current_datetime_string()
        data['check_time'] = timestamp_current()

        check = InnberBoxCheck()
        check.assign(data)
        check.save()

        obj = InnberBoxCheckSnapShot.get_or_new(ip=dest[0])
        obj.assign(data)
        obj.save()




if __name__ == '__main__':
    # main()
    check_inner_hosts()