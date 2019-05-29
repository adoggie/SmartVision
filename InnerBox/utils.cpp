//
// Created by scott on 2019-03-12.
//

#include "utils.h"


#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>

namespace utils {

    std::string generateUUID() {
//    boost::uuids::uuid _uuid = boost::uuids::random_generator()();
//     std::string uuid = boost::uuids::to_string(_uuid);
        std::string uuid = boost::lexical_cast<std::string>(std::time(NULL));

        return uuid;
    }

    char *get_local_ip(char *address, size_t size, int count) {
        const char *cmd = "/system/bin/busybox ip addr | /system/bin/busybox grep inet | /system/bin/busybox grep -v 127 | /system/bin/busybox grep -v inet6 | busybox awk '{print $2}'| busybox awk -F '/' '{print $1}' > /data/user/local.txt";
        int i = 0;
        memset(address, 0, size);
        for (i = 0; i < count; i++) {
            printf("== Try LocalIP Detecting.. %d \n", i + 1);
            int ret = system(cmd);

            FILE *fp = fopen("/data/user/local.txt", "r");
//    FILE * fp = fopen("/system/xbin/local.txt","r");
            if (!fp) {
                sleep(1);
                continue;
            }
            if (fp) {
                fread(address, 1, size, fp);
                if (strlen(address) == 0) {
                    sleep(1);
                    continue;
                }
                printf("== Read local address: %s\n", address);
                fclose(fp);
                break;
            }
        }

        if (strlen(address)) {
            char last_char = address[strlen(address) - 1];
            if ((last_char >= '0' && last_char <= '9') || last_char == '.') {

            } else {
                address[strlen(address) - 1] = '\0';
            }
        }
        return address;
    }

    std::string getIpAddress(const std::string& eth){
        std::string address;
//    char address[40];
//    get_local_ip(address, sizeof(address), 60); // scott
        return address;
    }





    int fromHex( char c ) {
        if ( '0' <= c && c <= '9' )
            return c - '0';
        if ( 'a' <= c && c <= 'f' )
            return c - 'a' + 10;
        if ( 'A' <= c && c <= 'F' )
            return c - 'A' + 10;
//        assert( false );
        return 0xff;
    }

    std::uint8_t fromHex( const char *c ) {
        return (std::uint8_t)(( fromHex( c[ 0 ] ) << 4 ) | fromHex( c[ 1 ] ));
    }

    ByteArray convertHex2Bin(const char* data,size_t size){
        ByteArray array;
        array.resize(size/2);
        const char * cur ;
        for(size_t n=0 ;n< size/2;n++){
            cur = data + n*2;
            auto byte = fromHex(cur);
            array[n] = byte;
        }
        return array;
    }

    std::string toHex(const void* inRaw, int len) {
        static const char hexchars[] = "0123456789ABCDEF";

        std::stringstream out;

//        StringBuilder out;
        const char* in = reinterpret_cast<const char*>(inRaw);
        for (int i=0; i<len; ++i) {
            char c = in[i];
            char hi = hexchars[(c & 0xF0) >> 4];
            char lo = hexchars[(c & 0x0F)];

            out << hi << lo;
        }

        return out.str();
    }

    std::string toHexLower(const void* inRaw, int len) {
        static const char hexchars[] = "0123456789abcdef";

        std::stringstream out;
        const char* in = reinterpret_cast<const char*>(inRaw);
        for (int i=0; i<len; ++i) {
            char c = in[i];
            char hi = hexchars[(c & 0xF0) >> 4];
            char lo = hexchars[(c & 0x0F)];

            out << hi << lo;
        }

        return out.str();
    }
}
