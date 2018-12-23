#--coding:utf-8--

import traceback
from datetime import datetime
from logging import getLogger

import gevent
import gevent.socket
from 	gevent.server import StreamServer
import 	gevent.event
import gevent.ssl

from mantis.fundamental.utils.importutils import import_class
from mantis.fundamental.application.app import instance

class SocketClientIdentifier(object):
	def __init__(self):
		self.unique_id = '' 	# 可以是连接设备的唯一设备标识
		self.props = {}

class SocketConnection(object):
	def __init__(self,sock,consumer,address,server=None):
		self.server = server
		self.sock = sock
		self.consumer = consumer
		self.datetime = None
		self.client_id = SocketClientIdentifier()
		self.recv_timout = 0
		self.address =  address

	def getAddress(self):
		return 'RpcConnectionSocket:'+str(self.sock.getsockname())

	def setTimeout(self,timeout):
		self.recv_timout = timeout

	def open(self):
		self.datetime = datetime.now()
		return True

	def close(self):
		if self.sock:
			self.sock.close()
		self.sock = None

	def sendData(self,data):
		self.sock.sendall(data)
		# instance.getLogger().debug( 'sent >> ' + self.hex_dump(data) )
		instance.getLogger().debug( 'sent >> ' + data)

	def hex_dump(self, bytes):
		dump = ' '.join(map(hex, map(ord, bytes)))
		return dump

	def recv(self):
		if self.recv_timout:
			self.sock.settimeout(self.recv_timout)
		while True:
			try:
				d = self.sock.recv(1000)
				if not d:
					break
			except:
				# traceback.print_exc()
				break
			try:
				self.consumer.onData(d,self)
			except:
				instance.getLogger().error(traceback.format_exc())
				# traceback.print_exc()
		instance.getLogger().debug( 'socket disconnected!' )
		self.sock = None

class DataConsumer(object):
	def __init__(self,accumulator,handler):
		self.accumulator = accumulator
		self.handler = handler

	def onData(self,bytes):
		messages = self.accumulator.enqueue(bytes)
		for message in messages:
			self.handler.handle(message)

class Server(object):
	def __init__(self):
		self.cfgs = None
		self.conns = []
		self.server = None

	@property
	def name(self):
		return self.cfgs.get('name')

	def init(self,cfgs):
		self.cfgs = cfgs
		return self

	def stop(self):
		self.server.stop()

	def start(self):
		ssl = self.cfgs.get('ssl')
		if ssl:
			self.server = StreamServer((self.cfgs.get('host'),self.cfgs.get('port')),
										self._service,keyfile=self.cfgs.get('keyfile'),
				certfile=self.cfgs.get('certfile'))
		else:
			self.server = StreamServer((self.cfgs.get('host'),self.cfgs.get('port')), self._service)

		print 'socket server started!'
		self.server.start() #serve_forever() , not block

	def _service(self,sock,address):
		# cfgs = self.cfgs.get('accumulator')
		# accCls = import_class(cfgs.get('class'))
		# acc = accCls().init(cfgs)

		handler = self.cfgs.get('handler')
		# handlerCls = import_class(cfgs.get('class'))
		# handler = handlerCls().init(cfgs)

		# consumer = DataConsumer(acc,handler)
		conn = SocketConnection(sock,handler,address,self)
		self.addConnection(conn)
		# handler.setConnection(conn)
		# handler.setAccumulator(acc)
		handler.onConnected(conn,address)
		conn.recv()
		self.removeConnection(conn)
		handler.onDisconnected(conn)

	def sendMessage(self,m):
		pass

	def addConnection(self,conn):
		self.conns.append(conn)

	def removeConnection(self,conn):
		self.conns.remove(conn)