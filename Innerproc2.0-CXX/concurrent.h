//
// Created by bin zhang on 2019/1/9.
//

#ifndef INNERPROC_CONCURRENT_H
#define INNERPROC_CONCURRENT_H

#include <mutex>
#include <queue>
#include <boost/thread.hpp>

using namespace std;
using namespace boost;

template  <typename Data>
class ConcurrentQueue
{
private:
	queue<Data> the_queue;								//标准库队列
	mutable mutex the_mutex;							//boost互斥锁
	condition_variable the_condition_variable;			//boost条件变量

public:
	
	//存入新的任务
	void push(Data const& data)
	{
		mutex::scoped_lock lock(the_mutex);				//获取互斥锁
		the_queue.push(data);							//向队列中存入数据
		lock.unlock();									//释放锁
		the_condition_variable.notify_one();			//通知正在阻塞等待的线程
	}
	
	//检查队列是否为空
	bool empty() const
	{
		mutex::scoped_lock lock(the_mutex);
		return the_queue.empty();
	}
	
	//取出
	Data wait_and_pop()
	{
		mutex::scoped_lock lock(the_mutex);
		
		while (the_queue.empty())						//当队列为空时
		{
			the_condition_variable.wait(lock);			//等待条件变量通知
		}
		
		Data popped_value = the_queue.front();			//获取队列中的最后一个任务
		the_queue.pop();								//删除该任务
		return popped_value;							//返回该任务
	}
	
};



#endif //INNERPROC_CONCURRENT_H
