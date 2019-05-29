// util/hex.h



#pragma once

#include <string>
#include <sstream>
#include <vector>

namespace utils {
    typedef std::vector<std::uint8_t > ByteArray;

    //can't use hex namespace because it conflicts with hex iostream function
    inline int fromHex( char c ) {
        if ( '0' <= c && c <= '9' )
            return c - '0';
        if ( 'a' <= c && c <= 'f' )
            return c - 'a' + 10;
        if ( 'A' <= c && c <= 'F' )
            return c - 'A' + 10;
//        assert( false );
        return 0xff;
    }

    inline std::uint8_t fromHex( const char *c ) {
        return (std::uint8_t)(( fromHex( c[ 0 ] ) << 4 ) | fromHex( c[ 1 ] ));
    }

    inline ByteArray convertHex2Bin(const char* data,size_t size){
        ByteArray array;
        array.reserve(size/2);
        const char * cur ;
        for(size_t n=0 ;n< size/2;n++){
            cur = data + n*2;
            auto byte = fromHex(cur);
            array[n] = byte;
        }
        return array;
    }

    inline std::string toHex(const void* inRaw, int len) {
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

    inline std::string toHexLower(const void* inRaw, int len) {
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
