

## PropertyAppAgent 物业话机服务代理


```bash
    python appserver.py 
```

## 设计 

1. server定时发送18699端口的状态检查包，维持多个app是否在线，是否通话中 
2. 呼叫连接到达时，选择一个空闲的app进行转发，如遇忙碌或不可达则返回忙线消息
3. 呼叫挂断消息到达时，将挂断消息广播给所有当前活跃的app