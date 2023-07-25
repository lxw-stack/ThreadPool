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
#include <functional>
#include <unordered_map>

//Any类型：可以接收任意数据的类型
class MyAny{
public:
    MyAny() = default;
    ~MyAny() = default;
    MyAny(const MyAny&) = delete;
    MyAny& operator = (const MyAny&) = delete;
    MyAny(MyAny&&) = default;
    MyAny& operator = (MyAny&&) = default;
    //这个构造类型可以让Any接收任意其他数据
    template<typename T>
    MyAny(T data) : base_(std::make_unique<MyDerive<T>>(data)){
        
    }
    //这个方法能把Any对象里面存储的data数据提取出来
    template<typename T>
    T cast_(){
        //我们怎么从base_找到它所指向的Derive对象，从它里面取出data成员变量
        //基类指针转换成派生类指针  RTTI
        MyDerive<T>* pd = dynamic_cast<MyDerive<T>*>(base_.get());
        if(pd == nullptr){
            throw "type is unmatch!";
        }
        else
            return pd->data_;
    }
private:
    //基类类型
    class MyBase{
    public:
        virtual ~MyBase() = default;
    };
    //派生类类型
    template<typename T>
    class MyDerive : public MyBase{
    public:
        MyDerive(T data) : data_(data){
    
        }
        T data_;
    };
private:
    std::unique_ptr<MyBase> base_;
};

class Semaphore{
public:
    Semaphore(int limit = 0):resLimit(limit){
        
    }
    ~Semaphore() = default;
    //获取一个信号资源量
    void wait(){
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,[&]()->bool {return resLimit > 0;});
        resLimit--;
    }
    //增加一个信号资源量
    void post(){
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit++;
        cond_.notify_all();
    }
private:
    int resLimit;
    std::mutex mtx_;
    std::condition_variable cond_;
};

class Task;

//实现接收提交到线程池的task任务执行完成后的返回值类型Result
class Result{
public:
    Result(std::shared_ptr<Task> task, bool isVaild = true);
    ~Result() = default;
    //问题一:setVal方法，获取任务执行完的返回值
    void setVal(MyAny any);
    //问题二：get方法，用户调用这个方法获取task的返回值
    MyAny get();
private:
    MyAny any_;//存储返回值
    Semaphore sem_;//线程通信信号量
    std::shared_ptr<Task> task_;//指向对应获取返回值的对象
    std::atomic_bool isVaild_;//返回值是否有效
};

//任务抽象基类
class Task{
public:
    Task();
    ~Task() = default;
    void exec();
    void setResult(Result* res);
    //用户可以自定义任务类型，从Task继承，重写run方法，实现自定义任务处理
    virtual MyAny run() = 0;//=0纯虚函数，目的是为了不能实例化对象
private:
    Result* result_;//不能都用智能指针，智能指针交叉引用会导致无法释放，内存泄漏
};
//线程池支持的模式
enum class PoolMode{
    MODE_FIXED, //固定数量的线程
    MODE_CACHED, //线程数量可动态增长
};
//线程类型
class Thread{
public:
    //线程函数对象类型
    using ThreadFunc = std::function<void(int)>;
    //线程构造
    Thread(ThreadFunc func);
    //线程析构
    ~Thread();
    //启动线程
    void start();
    int getId() const;
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_; //保存线程id
};

/*
 example:
 ThreadPool pool
 pool.start(4);
 class MyTask : public Task{
    public:
        void run() {//线程代码}
 };
 pool.submitTask(std::make_shared<MyTask>())
 */

//线程池类型
class ThreadPool{
public:
    ThreadPool();
    ~ThreadPool();
 
    //设置线程池的工作模式
    void setMode(PoolMode mode);
    
    //设置初始的线程数量
    //void setInitThreadSize(int size);
    
    //设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(int threshhold);
    
    //设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threshhold);
    
    //给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sp);
    
    //开启线程池
    void start(int initThreadSize = 4);
    
    ThreadPool(const ThreadPool&) = delete;//=delete表示这个成员函数不能被再调用，const ThreadPool&为拷贝构造函数，即禁止拷贝线程池
    ThreadPool& operator=(const ThreadPool&) = delete;//禁止重载赋值
    
private:
    //定义线程函数
    void threadFunc(int threadid);
    
    bool checkRunningState() const;
private:
//    std::vector<std::unique_ptr<Thread>> threads_; //线程列表
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    size_t initThreadSize_; //初始的线程数量
    int threadSizeThreshHold_; //线程数量上限阈值
    std::atomic_int curThreadSize_;//记录当前线程池里面的线程总数量
    std::atomic_int idleThreadSize_;//记录空闲线程的数量
    
    std::queue<std::shared_ptr<Task>> taskQue_;//任务队列，Task*不能保证用户传进来的任务周期足够长，智能指针可以确保在完成任务后自动释放内存
    std::atomic_int taskSize_;//任务数量，保证原子操作，保证线程安全
    int taskQueMaxThreshHold_; //任务队列数量上限阈值
    
    std::mutex taskQueMtx_; //保证任务队列的线程安全
    std::condition_variable notFull_; //表示任务队列不满
    std::condition_variable notEmpty_; //表示任务队列不空
    std::condition_variable exitCond_;//等待线程资源全部回收
    
    PoolMode poolMode_;//当前线程池的工作模式
    
    std::atomic_bool isPoolRunning_;//表示当前线程池的启动状态
};

#endif /* threadpool_hpp */
