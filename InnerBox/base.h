//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_BASE_H
#define INNERPROC_BASE_H

#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>


#include <boost/any.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


typedef  std::map< std::string , boost::any > PropertyMap;
typedef  std::map< std::string , std::string > PropertyStringMap;

typedef unsigned char * BytePtr;
typedef std::vector<unsigned char> ByteStream;

class  Object{
public:
	typedef std::shared_ptr<Object> Ptr;
	Ptr data(){
		return data_;
	}
	
	void data(const Ptr& ptr){
		data_ = ptr;
	}
	Object(){}
private:

protected:
	Ptr data_;
	std::condition_variable cv_;
	std::recursive_mutex rmutex_;
	std::mutex			mutex_;
};

//
//class Config{
//	std::map< std::string , std::string > _props;
//public:
//	long get(const std::string& name,long def=0);
//	int get(const std::string& name,int def=0);
//	std::string get(const std::string& name,std::string def="");
//	bool get(const std::string& name,bool def=false);
//
//	void load(const std::string& filename);
//};
//
////namespace smart {
//
//

#define SCOPED_LOCK  std::lock_guard<std::mutex> lock(mutex_);

enum CallPeerType{
	INNER_BOX = 1,
	INNER_SCREEN = 2,
	OUTER_BOX = 3,
	PROPERTY_APP = 4,
	PROPERTY_CENTER = 5

};

//围墙机(A)，单元机(B)，岗亭机(C)

#define OUTER_BOX_A 	'A'
#define OUTER_BOX_B 	'B'
#define OUTER_BOX_C 	'C'

#ifdef _LINUX
#define SECZONE_FILE "/home/innerbox/seczone.txt"
#define SECZONE_USER_FILE "/home/innerbox/seczone.user"
#define SETTINGS_FILE "/home/innerbox/settings.txt"
#define SETTINGS_USER_FILE "/home/innerbox/settings.user"

#else
#define SECZONE_FILE "seczone.txt"
#define SECZONE_USER_FILE "seczone.user"
#define SETTINGS_FILE "settings.txt"
#define SETTINGS_USER_FILE "settings.user"
#endif



//}

#endif //INNERPROC_BASE_H
