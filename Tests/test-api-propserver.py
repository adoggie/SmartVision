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
        self.box_http = 'http://127.0.0.1:8089'


    @classmethod
    def tearDownClass(self):
        pass

    @classmethod
    def setUpClass(self):
        # 必须使用@classmethod 装饰器,所有test运行前运行一次
       pass

    def test_initdata(self):
        url = '{}/propserver/api/initdata'.format(self.box_http)
        resp = requests.get(url,dict(type = 1))

        print resp.text
        print resp.json()

    def test_query_innerbox_list(self):
        url = '{}/propserver/api/innerbox/list'.format(self.box_http)
        params = dict(
            type = 1
        )
        resp = requests.get(url, params)
        print resp.text
        print resp.json()

    def test_report_event(self):
        url = '{}/propserver/api/device/event'.format(self.box_http)
        params = dict(
            dev_type = 1 ,
            dev_id = '1123',
            event = 'start',
            time = time.time(),
            content = 'this is test ',
            encode = 'plain'
        )
        resp = requests.post(url,params)
        print resp.text
        # return resp.json()['result']

    def test_query_event(self):

        url = '{}/propserver/api/device/event'.format(self.box_http)
        params = dict(
            dev_type = 1,
            dev_id = '1123',
            room_id = '',
            event = 'start',
            start = time.time()-3600*10,
            end = 0
        )
        resp = requests.get(url, params)
        print resp.text
        # print resp.json()
        # return resp.json()['result'].get('token')

    def test_report_status(self):
        url = '{}/propserver/api/device/status'.format(self.box_http)
        params = dict(
            dev_type = 1,
            dev_id = '1123',
            time = time.time(),
            ver = '0.1',
            start = time.time() - 3600,
            elapsed = 1000,
            glore = 'fuck papi..'
        )
        resp = requests.post(url, params)
        print resp.text
        # return resp.json()

    def test_qr_opendoor(self):
        url = '{}/propserver/api/outerbox/opendoor/qr'.format(self.box_http)
        params =dict(
            dev_type = 3,
            dev_id = 'aa123',
            time = time.time(),
            qrcode = 'jjjjjskdfsdfwerwer'
        )
        resp = requests.post(url, params )
        print resp.text
        # return resp.json()

    def test_image_upload(self):
        """"""
        url = '{}/propserver/api/innerbox/video/upload'.format(self.box_http)

        files = [("file", ("file", open("./Slice8.png", "rb"), "image/png"))]
        data = dict(
            dev_type=2,
            dev_id='aabb'
        )
        resp = requests.post(url, data, files=files)
        # resp = requests.post(url, dict(token=token,rand_key='111111'))
        print resp.text
        # return resp.json()


    def test_opendoor(self):
        """物业报警中心发送开启指定房间门禁"""
        url = '{}/propserver/api/innerbox/opendoor'.format(self.box_http)
        params = dict(
            room_id = '2-1-201',
            secret_key = 'qweqweqwe'
        )
        resp = requests.post(url, params)
        print resp.text
        # return resp.json()




if __name__ == '__main__':
    c = MyTest()

    idx = 0
    for _ in range(1):

        idx+=1
        c.test_initdata()
        c.test_query_innerbox_list()
        c.test_report_event()
        c.test_report_status()
        c.test_query_event()
        c.test_qr_opendoor()
        c.test_image_upload()
        c.test_opendoor()

        print idx
        time.sleep(.2)
    # unittest.main()
    # suite = unittest.TestSuite()
    # suite.addTest(MyTest())
    # runner = unittest.TextTestRunner()
    # runner.run(suite)
