#include "sensor.h"
#include <unistd.h> // for sleep function
#include <libserialport.h>

#include "app.h"
#include "sensor_message.h"
#include "base64.h"
#include "crc16.h"
#include "utils.h"

#define SP '\n'
#define SPS "\n"

#define  CRC_BIT_SIZE  2

/*
* libserialport
* https://gist.github.com/Nixes/78e401234e66aa131547d7b78135271c
*
*
* */

// namespace sensor {

//	 MessagePayload::Ptr parseMessage(const char *data, size_t size) {
//
//
//	 }

bool Sensor::init(const PropertyStringMap &props) {
    cfgs_ = props;
    return true;
}

bool Sensor::open() {
    std::string name;
    name = cfgs_["port"];    // 打开串口
//    name = "/dev/ttyS1";
    int baudrate = boost::lexical_cast<int>(cfgs_["baudrate"].c_str());
    Application::instance()->getLogger().info(
            (boost::format("== uart open: %s %d") % name.c_str()%baudrate).str()
    );

    if(0){ // for test

        // python:
        //  '322C31332C302C302C312C30E357'.decode('hex')

//        std::string text = "322C31332C302C302C302C30D467\n";
        std::string text = "322C31332C302C302C312C30E357\n";
//        std::string text = "322C31332C302C302C312C30E357\n";
        onRecv(text.c_str(),text.size());
        MessagePayload p;
        p.marshall();
    }



    sp_return error = sp_get_port_by_name(name.c_str(), &serial_port_);

    if (error != SP_OK) {
        Application::instance()->getLogger().error("sp_get_port_by_name() failed.");
        return false;
    }
    error = sp_open(serial_port_, SP_MODE_READ_WRITE);
    if (error != SP_OK) {
        Application::instance()->getLogger().error("sp_open() failed.");
        return false;
    }


    sp_set_baudrate(serial_port_, baudrate);

    thread_ = new std::thread(&Sensor::run, this);

    return true;
}

void Sensor::close() {
    running_ = false;
}

Sensor::Sensor() {
    listener_ = NULL;
    running_ = false;
}

Sensor::~Sensor() {

}

// 发送下行串口消息
void Sensor::sendData(const char *data, size_t size) {
    sp_nonblocking_write(serial_port_, data, size);
}

void Sensor::setListener(ISensorListener *listener) {
    listener_ = listener;
}

void Sensor::run() {
    running_ = true;
    sp_return error;
    int ret = 0;
    char buffer[200];

    Logger &logger = Application::instance()->getLogger();
    puts("==Sensor run Thread started ..");
    while (running_) {
//        logger.debug("sensor read blocking  ...");
        ret = (int) sp_blocking_read(serial_port_, buffer, (size_t) sizeof(buffer), 1000);

        if (ret < 0) {
            logger.error("InnerSensor Read Error.");
            break;//  error
        }
        if (ret > 0) {
            logger.debug((boost::format(" << %d bytes read in.") % ret).str());
            onRecv(buffer, (size_t) ret);
        }
    }

    logger.debug("InnerSensor Read Thread Exiting..");
}



std::vector<std::string> Sensor::data_split(){
    std::vector<std::string> lines;
    while(true) {
        auto itr = std::find(databuf_.begin(), databuf_.end(), SP);
        if (itr == databuf_.end()) {
            break;
        }
        std::string text(databuf_.begin(),itr) ;
        databuf_.erase(databuf_.begin(),itr+1);
        lines.push_back(text);
    }
    return lines;
}

void Sensor::onRecv(const char *data, size_t size) {
    std::vector<std::string> lines;
    std::string text;

//    bool keep_last = false;
//    if (data[size - 1] != SP) {
//        keep_last = true;
//    }
    buffer_ = std::string(data, size);
    Application::instance()->getLogger().error("<< buffer: " + buffer_);

    databuf_.resize(databuf_.size() + size);
    memcpy(&databuf_[databuf_.size() - size],data,size);

    lines = data_split();


//    boost::split(lines, buffer_, boost::is_any_of((SPS)));

//    if (keep_last) {
//        text = lines[lines.size() - 1];
//        lines.pop_back();
//        buffer_ = text;
//    }

// parse json message
    MessagePayload::Ptr message;
    for (auto &line : lines) {
        if (listener_) {
//			 message = parseSensorMessage(line.c_str());
            Application::instance()->getLogger().debug("parse sensor message:" + line);
            message = MessagePayload::parse(line);
            if (message && listener_) {
                listener_->onMessage(message, this);
            }
        }
    }
}


std::string Sensor::pack(const std::string &text) {
    return "";
}


void Sensor::sendMessage(const std::shared_ptr<MessagePayload> &message) {
    if (message) {
        std::string text = message->marshall();
        sendData(text.c_str(), text.size());
        sendData(SPS, 1);
    }
}

void Sensor::sendSeperator(){
    sendData(SPS, 1);
}

//消息解码
std::string MessagePayload::marshall() {
    boost::format fmt("%d,%d,%d,%s,%s,%d");
    fmt % a % b % c % d.c_str() % e.c_str() % f;
    std::string text = fmt.str();
    std::string result = textEncode(text);
    return result;
}


std::string MessagePayload::textEncode(const std::string& text){
    { // for test
//        text = "2,13,0,0,0,0";
    }
    uint16_t crc = crc16(text.c_str(), text.length());

    crc = (crc >> 8 & 0xff) | (crc << 8 & 0xff00); // 字节序保持与单片机一致(little endian)

    size_t out_size, in_size;
    char *out_data, *in_data;

    in_size = text.length() + CRC_BIT_SIZE;
    in_data = (char *) malloc(in_size);

    memcpy(in_data, text.c_str(), text.length());
    memcpy(in_data + text.length(), &crc, sizeof(crc));

    std::string result = utils::toHex(in_data, in_size);
    free(in_data);
    return result;
}
/*
//消息解码
std::string MessagePayload::marshall() {
boost::format fmt("%d,%d,%d,%s,%s,%d");
fmt % a % b % c % d.c_str() % e.c_str() % f;
std::string text =  fmt.str();
uint16_t crc = crc16(text.c_str(),text.length());

size_t out_size, in_size ;
char* out_data, * in_data ;

in_size = text.length()+ CRC_BIT_SIZE;
out_size = Base64::EncodedLength( in_size );
out_data = (char*) malloc(out_size);

in_data = (char*)malloc( in_size );

memcpy(in_data,text.c_str(),text.length());
memcpy(in_data + text.length(),&crc, sizeof(crc));


Base64::Encode(in_data,in_size,out_data,out_size);

std::string result(out_data,out_size);

free(in_data);
free(out_data);

return result;
}
*/

//解析来自mcu串口上行的消息
std::shared_ptr<MessagePayload> MessagePayload::parse(const std::string &text) {

    std::shared_ptr<MessagePayload> empty;
    Logger &logger = Application::instance()->getLogger();

    Application::instance()->getLogger().debug("MessagePayload::parse " + text);

//# hex
    if (text.length() % 2) {
        logger.error("MCU Message Package Invalid Length.");
        return empty; // 必须为偶数字节
    }

    auto bytes = utils::convertHex2Bin(text.c_str(), text.length());
    char *data = (char *) &bytes[0];
    size_t size;
    size = bytes.size();

    uint16_t crc1, crc2;
    memcpy(&crc1, data + size - CRC_BIT_SIZE, CRC_BIT_SIZE);
    crc1 = (crc1<<8 &0xff00) | (crc1>>8 & 0xff);
    crc2 = crc16(data, size - CRC_BIT_SIZE);
    if (crc1 != crc2) {
//		free(data);
        logger.error("MCU Message Package Invalid: CRC error.");
        return empty;
    }

    std::string M(data, size - CRC_BIT_SIZE);
    Application::instance()->getLogger().debug("sensor raw:"+M);
    std::vector<std::string> fields;
    boost::split(fields, M, boost::is_any_of((",")));

    if (fields.size() != 6) { //  A,B,C,D,E,F
        logger.error("MCU Message Package Invalid: Fields Num is not 6.");
        return empty;
    }

    std::shared_ptr<MessagePayload> message = std::make_shared<MessagePayload>();
    try {
        message->a = boost::lexical_cast<int>(fields.at(0));
        message->b = boost::lexical_cast<int>(fields.at(1));
        message->c = boost::lexical_cast<int>(fields.at(2));
        message->d = boost::lexical_cast<std::string>(fields.at(3));
        message->e = boost::lexical_cast<std::string>(fields.at(4));
        message->f = boost::lexical_cast<int>(fields.at(5));
    } catch (boost::bad_lexical_cast &e) {
        logger.error("MCU Message Package Invalid: Field Value Case Error. M:" + M);
        return empty;
    }

    return message;
}


/*
//解析来自mcu串口上行的消息
std::shared_ptr<MessagePayload> MessagePayload::parse(const std::string& text) {

std::shared_ptr<MessagePayload> empty;

size_t size = Base64::DecodedLength(text);
char *data = (char*)malloc(size);
memset(data, 0, size);

if (!Base64::Decode(text.c_str(), (size_t) text.length(), data, size)) {
    free(data);
    return empty;
}

uint16_t crc1, crc2;
memcpy(&crc1, data + size - CRC_BIT_SIZE, CRC_BIT_SIZE);

crc2 = crc16(data, size - CRC_BIT_SIZE);
if (crc1 != crc2) {
    free(data);
    return empty;
}

std::string M(data, size - CRC_BIT_SIZE);
free(data);

std::vector<std::string> fields;
boost::split(fields, M, boost::is_any_of((",")));

if (fields.size() != 6) { //  A,B,C,D,E,F
    return empty;
}

std::shared_ptr<MessagePayload> message = std::make_shared<MessagePayload>();
try {
    message->a = boost::lexical_cast<int>(fields.at(0));
    message->b = boost::lexical_cast<int>(fields.at(1));
    message->c = boost::lexical_cast<int>(fields.at(2));
    message->d = boost::lexical_cast<std::string>(fields.at(3));
    message->e = boost::lexical_cast<std::string>(fields.at(4));
    message->f = boost::lexical_cast<int>(fields.at(5));
} catch (boost::bad_lexical_cast &e) {
    return empty;
}

return message;
}
*/