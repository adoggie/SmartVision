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
#include <boost/any.hpp>


typedef  std::map< std::string , boost::any > PropertyMap;

typedef unsigned char * BytePtr;
typedef std::vector<unsigned char> ByteStream;

class  tgObject{
public:
	typedef std::shared_ptr<tgObject> Ptr;
	Ptr data(){
		return data_;
	}
	
	void data(const Ptr& ptr){
		data_ = ptr;
	}
	tgObject(){}
private:
	Ptr data_;
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


enum CallPeerType{
	INNER_BOX = 1,
	INNER_SCREEN = 2,
	OUTER_BOX = 3,
	PROPERTY_APP = 4,
	PROPERTY_CENTER = 5
	
	
};



//}

#endif //INNERPROC_BASE_H
