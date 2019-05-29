#ifndef INNERPROC_SENSOR_H
#define INNERPROC_SENSOR_H

#include "base.h"
#include "config.h"
#include <libserialport.h>

#include <string>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

/**
 *
Host <-> Mcu 通信格式

   A,B,C,D,E,F,G

   A - 消息类型 SensorMessageType (1..6,20)
   B - sensor_type (1..3)
   C - sensor_id (1..3)
   D - feature_name ( 2,3)
   E - feature_value ( 2,3)
   F - flag 控制符，在一次传递多个连续值时用于控制是否连续的消息包 (0 : 最后一项, 1: 不是最后一项)
   G - 包分隔符 '\n'

编码:
 M = A,B,C,D,E 消息内容
 P = HEX( M + CRC16(M) )
 P' = P + G

解码:
 P = P'[:-1]
 P = BIN(P)
 M = P[:-2]
 if CRC16(M) == P[-2:]:
    pass
 else:
    error

室内机MCU上报防区状态
 A = SensorStatusValue(2)
 B = 0
 C = 1-16 防区编号
 D = io
 E = 0/1 开闭状态

室内机下发MCU开门指令

 A = McuValueSet(6)
 B = 0
 C = 0
 D = 0
 E = 0

室内机上报心跳
 A = Heartbeat(20)
 B = C = D = E = 0

 */

//namespace sensor {

#define  SensorID_INVALID   0
#define  SensorType_INVALID 0


    typedef int SensorIdType;
    typedef int MessageValueType;
    enum class SensorType {
        Mcu = 0,
        Replay = 1,
        Temperature = 2,
    };

    enum class MessageType {
        SensorStatusQuery = 1,
        SensorStatusValue = 2,
        SensorValueSet = 3,
        McuStatusQuery = 4,
        McuStatusValue = 5,
        McuValueSet = 6,
        Heartbeat = 20
    };

    struct MessagePayload {
        typedef std::shared_ptr<MessagePayload> Ptr;

        MessagePayload() {
            f = 0;
        }

        virtual ~MessagePayload(){}

        int a;      //消息类型
        int b;      //操作对象类型
        int c;      //对象编号
        std::string d;  //参数名称
        std::string e;  //参数值
        int f;      // 多个参数时标识是否连续

        std::string marshall();
        static std::shared_ptr<MessagePayload> parse(const std::string& text);
        static std::string textEncode(const std::string& text);
//        #define type a
    };

// values(A,B,C)
    struct SensorMessageStatusQuery : MessagePayload {
        SensorMessageStatusQuery() {
            a = (int) MessageType::SensorStatusQuery;
        }
    };

// values(A,B,C,D,E,F)
    struct SensorMessageStatusValue : MessagePayload {
        SensorMessageStatusValue() {
            a = (int) MessageType::SensorStatusValue;
        }
    };

// values(A,B,C,D,E)
    struct SensorMessageValueSet : MessagePayload {
        SensorMessageValueSet() {
            a = (int) MessageType::SensorValueSet;
        }
    };

    struct SensorMessageHeartbeat: MessagePayload {
        SensorMessageHeartbeat() {
            a = (int) MessageType::Heartbeat;
        }
    };


    MessagePayload::Ptr parseSensorMessage(const char *text);

    class Sensor;

    struct SensorAlarmInfo{

    };

    struct ISensorListener {
        virtual void onMessage(std::shared_ptr<MessagePayload> &payload, Sensor *sensor) = 0; // 接收到上行消息
    };


//传感器控制类
    class Sensor {
    public:
        Sensor();

        ~Sensor();

        bool init(const PropertyStringMap &props);

        bool open();

        void close();

        void sendData(const char *data, size_t size);    // 写入数据
        void sendMessage(const std::shared_ptr<MessagePayload>& message);
        void sendSeperator(); //发送包分割符

        void setListener(ISensorListener *listener);

    private:
        void run();
        void onRecv(const char *data, size_t size);
        std::shared_ptr<MessagePayload> parse(const std::string& text);
        std::string pack(const std::string& text);
        std::vector<std::string> data_split();
    private:
        PropertyStringMap cfgs_;
        ISensorListener *listener_;
        struct sp_port *serial_port_;
        std::thread *thread_;
        std::atomic_bool running_;

        std::string buffer_;
        std::vector< std::uint8_t > databuf_;

    };


//};

#endif