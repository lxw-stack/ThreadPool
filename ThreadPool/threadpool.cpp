//
//  threadpool.cpp
//  ThreadPool
//
//  Created by 李笑微 on 2023/2/19.
//

#include "threadpool.hpp"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;//单位：秒
 
//线程池构造
ThreadPool::ThreadPool()
:initThreadSize_(4)
,taskSize_(0)
,idleThreadSize_(0)
,curThreadSize_(0)
,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
,threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
,poolMode_(PoolMode::MODE_FIXED)
,isPoolRunning_(false)
{}

//线程池析构
ThreadPool::~ThreadPool(){
    isPoolRunning_ = false;
    //等待线程池里面所有线程返回  有两种状态 阻塞&正在执行任务中
    notEmpty_.notify_all();
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    exitCond_.wait(lock,[&]()->bool{return threads_.size() == 0;});
    
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode){
    if(checkRunningState())
        return;
    poolMode_ = mode;
}

//设置初始的线程数量
//void ThreadPool::setInitThreadSize(int size){
//    initThreadSize_ = size;
//}

//设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold){
    if(checkRunningState())
        return;
    taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setThreadSizeThreshHold(int threshhold){
    if(checkRunningState())
        return;
    if(poolMode_ == PoolMode::MODE_CACHED){
        threadSizeThreshHold_ = threshhold;
    }
}

//给线程池提交任务 用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp){
    //获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    //线程的通信 等待任务队列有空余
    //用户提交任务，最长阻塞不能超过1s，否则判断提交任务失败返回
//    while(taskQue_.size() == taskQueMaxThreshHold_){
//        notFull_.wait(lock);
//    }//和下面的lambda表达式同义，即阻塞
    if(notFull_.wait_for(lock, std::chrono::seconds(1),[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_;})==false){
        // 表示notFull_等待1s后，条件依然没有满足
        std::cerr << "task queue is full, submit task fail" << std::endl;
//        return task->getResult();线程执行完task，task对象就被析构掉了
        return Result(sp, false);
    }
    //如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;
    //因为新放了任务，任务队列肯定不空，在notEmpty_上通知分配线程执行任务
    notEmpty_.notify_all();
    
    //cached 任务处理比较紧急 场景 小而快,需要根据任务数量和空闲线程数量判断是否需要创建新的线程出来
    if(poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ &&
       curThreadSize_ < threadSizeThreshHold_){
        std::cout << "create new thread" << std::endl;
        
        //创建新线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this, std::placeholders::_1));
        //创建thread线程对象的时候，把线程函数给到thread线程对象
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));//unique_ptr不允许直接拷贝
        threads_[threadId]->start();
        //修改线程个数相关的变量
        curThreadSize_++;
        idleThreadSize_++;
    }
    //返回任务的Result对象
//    return task->getResult();
    return Result(sp);
}

//开启线程池
void ThreadPool::start(int initThreadSize){
    //设置线程池的运行状态
    isPoolRunning_ = true;
    //记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    //创建线程对象
    for(int i = 0; i < initThreadSize; i++){
        //创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));//unique_ptr不允许直接拷贝
    }
    //启动所有线程，std::vector<Thread*> threads_
    for(int i = 0; i < initThreadSize; i++){
        threads_[i]->start();
        idleThreadSize_++;//记录初始空闲线程的数量
    }
}

//定义线程函数 线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadid){
    auto lastTime = std::chrono::high_resolution_clock().now();
    while(isPoolRunning_ == true){ //在这个循环中，线程会一直等待并执行任务队列中的任务。
        std::shared_ptr<Task> task;
        {
            //先获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);//锁默认出当前作用域才释放
            
            std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;
            
            while(taskQue_.size() == 0){
                if(poolMode_ == PoolMode::MODE_CACHED){
                    if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))){
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if(dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_){
                            //开始回收线程
                            //记录线程数量的相关变量的值修改
                            //把线程对象从线程列表容器中删除，没有办法threadFunc 《=》thread对象
                            threads_.erase(threadid);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "tid:" << std::this_thread::get_id() << "exit!" << std::endl;
                            return;
                        }
                    }
                }
                else{
                    notEmpty_.wait(lock);
                }
                //线程池要结束，回收线程资源
                if(isPoolRunning_ == false){
                    threads_.erase(threadid);
                    std::cout << "tid:" << std::this_thread::get_id() << "exit!" << std::endl;
                    exitCond_.notify_all();
                    return;
                }
            }
            //在cached模式下，有可能已经创建了很多线程，空闲时间超过60s，应该把多余的线程回收掉
            //结束回收掉（超过initThreadSize_数量的）
            //当前时间 - 上一次线程执行的时间>60s
            /*if(poolMode_ == PoolMode::MODE_CACHED){
                //每一秒返回一次 怎么区分超时返回？还是有任务待执行返回
                while(taskQue_.size() == 0){
                    //条件变量，超时返回了
                    if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))){
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if(dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_){
                            //开始回收线程
                            //记录线程数量的相关变量的值修改
                            //把线程对象从线程列表容器中删除，没有办法threadFunc 《=》thread对象
                            threads_.erase(threadid);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "tid:" << std::this_thread::get_id() << "exit!" << std::endl;
                            return;
                        }
                    }
                }
            }
            else{
                //等待notEmpty条件
                notEmpty_.wait(lock,[&]()->bool{ return taskQue_.size() > 0;});
            }*/
            idleThreadSize_--;
            
            std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功..." << std::endl;
            
            //不空就从任务队列中取一个任务
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            //如果依然有剩余任务，继续通知其他的线程执行任务
            if(taskQue_.size() > 0){
                notEmpty_.notify_all();
            }
            //取出一个任务应该通知
            notFull_.notify_all();
        }//释放锁
        //当前线程负责执行这个任务
        if(task != nullptr){
//            task->run();//执行任务
            task->exec();//执行任务，把任务的返回值给到Result
        }
        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完的时间
    }
    threads_.erase(threadid);
    std::cout << "tid:" << std::this_thread::get_id() << "exit!" << std::endl;
    exitCond_.notify_all();
}

bool ThreadPool::checkRunningState() const{
    return isPoolRunning_;
}

//线程方法实现
int Thread::generateId_ = 0;
//线程构造
Thread::Thread(ThreadFunc func):func_(func),threadId_(generateId_++)
{}
//线程析构
Thread::~Thread(){
    
}

//启动线程
void Thread::start(){
    //创建一个线程来执行一个线程函数
    std::thread t(func_, threadId_);//C++11来说 线程对象t 和 线程函数func_
    t.detach();//设置分离线程，线程对象和线程函数不相关
}

int Thread::getId() const{
    return threadId_;
}

//Task方法实现
Task::Task():result_(nullptr){
    
}

void Task::exec(){
    if(result_ != nullptr){
        result_->setVal(run());//这里发生多态调用
    }
}

void Task::setResult(Result* res){
    result_ = res;
}
//Result方法的实现
Result::Result(std::shared_ptr<Task> task, bool isVaild)
:isVaild_(isVaild),task_(task){
    task_->setResult(this);
}

MyAny Result::get(){
    if(!isVaild_){
        return "";
    }
    sem_.wait();//如果task任务没执行完，会阻塞用户线程
    return std::move(any_);
}

void Result::setVal(MyAny any){
    //存储task的返回值
    this->any_ =  std::move(any);
    sem_.post();//已经获得任务的返回值，增加信号量资源
}
