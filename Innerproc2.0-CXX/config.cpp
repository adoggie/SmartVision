//
// Created by bin zhang on 2019/1/6.
//

#include "config.h"
#include <string>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <stdexcept>

long Config::get(const std::string& name,long def){
	return def ;
}

int Config::get(const std::string& name,int def){
	return def ;
}

std::string Config::get(const std::string& name,const std::string& def){
	auto result = def;
	try {
		auto value = _props.at(name);
		result = boost::lexical_cast<std::string>(value);
	}catch (...){

	}
	return result ;
}

bool Config::get(const std::string& name,bool def){
	return def ;
}

void Config::load(const std::string& filename){
	std::ifstream file(filename);

	if (file.is_open())
	{
		std::string line;
		_props.clear();
		while(std::getline(file, line)){
			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			if(line[0] == '#' || line.empty())
				continue;
			auto delimiterPos = line.find("=");
			auto name = line.substr(0, delimiterPos);
			auto value = line.substr(delimiterPos + 1);
			std::cout << name << " " << value << '\n';
			_props[name] = value;
		}
	}
	else {
		std::cerr << "Couldn't open config file for reading.\n";
	}
}