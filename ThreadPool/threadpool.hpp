//
//  threadpool.hpp
//  ThreadPool
//
//  Created by 李笑微 on 2023/2/19.
//

#ifndef threadpool_hpp
#define threadpool_hpp

#include <stdio.h>
#include <vector>
#include <queue>
#include <memory>//智能指针
#include <atomic>//atomic_int
#include <mutex>
#include <condition_variable>


//任务抽象基类
class Task{
public:
    //用户可以自定义任务类型，从Task继承，重写run方法，实现自定义任务处理
    virtual void run() = 0;//=0纯虚函数，目的是为了不能实例化对象
};
//线程池支持的模式
enum class PoolMode{
    MODE_FIXED, //固定数量的线程
    MODE_CACHED, //线程数量可动态增长
};
//线程类型
class Thread{
public:
private:
};
//线程池类型
class ThreadPool{
public:
    ThreadPool();
    ~ThreadPool();
 
    //设置线程池的工作模式
    void setMode(PoolMode mode);
    
    //设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(int threshhold);
    
    //给线程池提交任务
    void submitTask(std::shared_ptr<Task> sp);
    
    //开启线程池
    void start();
    
    ThreadPool(const ThreadPool&) = delete;//=delete表示这个成员函数不能被再调用，const ThreadPool&为拷贝构造函数，即禁止拷贝线程池
    ThreadPool& operator=(const ThreadPool&) = delete;//禁止重载
private:
    std::vector<Thread*> threads_; //线程列表
    size_t initThreadSize_; //初始的线程数量
    
    std::queue<std::shared_ptr<Task>> taskQue_;//任务队列，Task*不能保证用户传进来的任务周期足够长，智能指针可以确保在完成任务后自动释放内存
    std::atomic_int taskSize_;//任务数量，保证原子操作，保证线程安全
    int  taskQueMaxThreshHold_; //任务队列数量上限阈值
    
    std::mutex taskQueMtx_; //保证任务队列的线程安全
    std::condition_variable notFull_; //表示任务队列不满
    std::condition_variable notEmpty_; //表示任务队列不空
    
    PoolMode poolMode_;//当前线程池的工作模式
};

#endif /* threadpool_hpp */
