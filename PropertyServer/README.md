

# PropertyServer
可视对讲2.0版本的物业服务器，提供数据查询、业务服务提供

在新版的智能可视对讲系统中，propserver是对旧版服务的补充。园区内所有设备将直接请求
propserver获得查询信息。

## 主要功能
1. 设备运行配置信息的查询
2. 设备运行数据采集和监控
3. 报警数据采集和响应


## 部署

propserver 是标准的web服务，在并发要求的场景，需要前置nginx作为均衡设备。
后端部署多个propserver 即可。 

## 存储

- 原系统的业务数据在mysql中，propserver启动时会拉取mysql数据并提供给访问用户和设备。
- propserver运行数据存储在mongodb

## 配置

- web port: `18911 / 18912 / 18913`
- mongodb port : `27017`
- nginx port:  `18901`




