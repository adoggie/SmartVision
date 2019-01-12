//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_CONFIG_H
#define INNERPROC_CONFIG_H

#include "base.h"

class Config{
	PropertyMap _props;
public:
	long get(const std::string& name,long def=0);
	int get(const std::string& name,int def=0);
	std::string get(const std::string& name,const std::string& def="");
	bool get(const std::string& name,bool def=false);

	void load(const std::string& filename);
};

#endif //INNERPROC_CONFIG_H
