//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_CODEC_H
#define INNERPROC_CODEC_H


#include "message.h"

struct DataConsumer{

};

struct DataAccumulator{
	void enqueue(const BytePtr data,size_t size);
	MessageList_t getMessageList();
};



#endif //INNERPROC_CODEC_H
