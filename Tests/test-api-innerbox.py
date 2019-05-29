#coding:utf-8

import time
import requests

import unittest

# HTMLTestRunner

SUPER_TOKEN ='YTU3NzVlYjktYjQwMi00MGY2LTkxZjktYWMxYjIxZjM4NjNlCg=='

# class MyTest(unittest.TestCase):  # 继承unittest.TestCase
class MyTest:  # 继承unittest.TestCase
    def __init__(self):
        self.setUp()

    def tearDown(self):
        pass

    def setUp(self):
        # 每个测试用例执行之前做操作
        # print('22222')
        self.token = SUPER_TOKEN
        self.box_http = 'http://127.0.0.1:7890'


    @classmethod
    def tearDownClass(self):
        pass

    @classmethod
    def setUpClass(self):
        # 必须使用@classmethod 装饰器,所有test运行前运行一次
       pass

    def test_status(self):
        url = '{}/smartbox/api/status'.format(self.box_http)
        resp = requests.get(url,dict(token=self.token))
        print resp.text
        print resp.json()

    def test_param_set(self):
        url = '{}/smartbox/api/params'.format(self.box_http)
        params = dict(
            token = self.token,
            ip = '127.0.0.1',
            family_ip ='192.168.1.5',
            watchdog_enable = 1,
            alarm_enable = 1,
            reboot = 0,
            call_in_enable = 1,
            save = 1 ,
            seczone_mode = 0,
            initdata = 1,
            online =1
        )
        resp = requests.post(url, params)
        print resp.text
        print resp.json()

    def test_request_code(self):
        url = '{}/smartbox/api/innerscreen/reqcode'.format(self.box_http)
        resp = requests.post(url, dict(token=self.token))
        print resp.text
        return resp.json()['result']

    def test_register_innerscreen(self,reqcode):
        # reqcode ='MTU1Mjk3MDUwOSwxMTIyMzM0NDU1NjYsZDRiZDc5ZWE3NjJlN2Q5MjkzYmVmMDAxOWVkOTljM2I'
        url = '{}/smartbox/api/innerscreen'.format(self.box_http)
        params = dict(
            code = reqcode,
            sn = 'Ds:100211',
            type = 1,
            os = 'linux'
        )
        resp = requests.post(url, params)
        # print resp.text
        print resp.json()
        return resp.json()['result'].get('token')

    def test_seczone_set_password(self,token):
        url = '{}/smartbox/api/seczone/passwd'.format(self.box_http)
        resp = requests.post(url, dict(token=token,old='1111',new='1111'))
        print resp.text
        return resp.json()

    def test_query_seczone_params(self,token):
        url = '{}/smartbox/api/seczone/params'.format(self.box_http)
        resp = requests.get(url, dict(token=token))
        print resp.text
        return resp.json()

    def test_set_seczone_params(self,token):
        """设置防区参数"""
        url = '{}/smartbox/api/seczone/params'.format(self.box_http)
        params = dict(
            token = token,
            passwd = '1111',
            port = 0 ,
            name = '',
            normalstate = 1 ,
            triggertype = 0,
            nursetime = 0 ,
            delaytime = 335,
            alltime = 0,
            online = 1
        )
        resp = requests.post(url, params)
        print resp.text
        return resp.json()

    def test_query_seczone_mode(self,token):
        """查询模式参数"""
        url = '{}/smartbox/api/seczone/mode'.format(self.box_http)
        resp = requests.get(url, dict(token=token))
        print resp.text
        return resp.json()

    def test_set_seczone_mode(self,token):
        """设置模式参数"""
        url = '{}/smartbox/api/seczone/mode'.format(self.box_http)
        resp = requests.post(url, dict(token=token,mode=1,value=0xff))
        print resp.text
        return resp.json()

    def test_opendoor(self, token):
        """远程开门"""
        url = '{}/smartbox/api/emergency/opendoor'.format(self.box_http)
        resp = requests.post(url, dict(token=token,rand_key='111111'))
        print resp.text
        return resp.json()

if __name__ == '__main__':
    c = MyTest()

    idx = 0
    for _ in range(1):

        idx+=1
        # c.test_status()
        # c.test_param_set()
        # break

        code = c.test_request_code()

        token = c.test_register_innerscreen(code)
        # c.test_seczone_set_password(token)
        c.test_query_seczone_params(token)
        break
        c.test_set_seczone_params(token)
        break
        c.test_set_seczone_mode(token)
        c.test_query_seczone_mode(token)
        c.test_opendoor(token)

        print idx
        time.sleep(.2)
    # unittest.main()
    # suite = unittest.TestSuite()
    # suite.addTest(MyTest())
    # runner = unittest.TextTestRunner()
    # runner.run(suite)
