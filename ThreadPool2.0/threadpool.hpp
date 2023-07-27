//
//  threadpool.hpp
//  ThreadPool2.0
//
//  Created by vivi on 2023/7/26.
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
#include <thread>
#include <unordered_map>
#include <future>

const int TASK_MAX_THRESHHOLD = 2;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;//单位：秒

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
    Thread(ThreadFunc func):func_(func),threadId_(generateId_++)
    {}
    //线程析构
    ~Thread() = default;
    //启动线程
    void start(){
        //创建一个线程来执行一个线程函数
        std::thread t(func_, threadId_);//C++11来说 线程对象t 和 线程函数func_
        t.detach();//设置分离线程，线程对象和线程函数不相关
    }
    int getId() const{
        return threadId_;
    }
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_; //保存线程id
};
int Thread::generateId_ = 0;

//线程池类型
class ThreadPool{
public:
    //线程池构造
    ThreadPool()
    :initThreadSize_(4)
    ,taskSize_(0)
    ,idleThreadSize_(0)
    ,curThreadSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
    ,poolMode_(PoolMode::MODE_FIXED)
    ,isPoolRunning_(false)
    {}
    
    ~ThreadPool(){
        isPoolRunning_ = false;
        //等待线程池里面所有线程返回  有两种状态 阻塞&正在执行任务中
        //    notEmpty_.notify_all();
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(lock,[&]()->bool{return threads_.size() == 0;});
    }

    //设置线程池的工作模式
    void setMode(PoolMode mode){
        if(checkRunningState())
            return;
        poolMode_ = mode;
    }

    //设置初始的线程数量
    //void setInitThreadSize(int size);

    //设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(int threshhold){
        if(checkRunningState())
            return;
        taskQueMaxThreshHold_ = threshhold;
    }

    //设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threshhold){
        if(checkRunningState())
            return;
        if(poolMode_ == PoolMode::MODE_CACHED){
            threadSizeThreshHold_ = threshhold;
        }
    }

    //给线程池提交任务
    //使用可变参模版编程，让submitTask可以接收任意任务函数和任意数量的参数
    //pool.submitTask(sum1,1,20);
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>{
        //打包任务，放入任务队列里
//        using RType = decltype(func(args)...));
        using RType = decltype(func(std::forward<Args>(args)...));
        auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();
        //获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        //用户提交任务，最长阻塞不能超过1s，否则判断提交任务失败返回
        if(notFull_.wait_for(lock, std::chrono::seconds(1),[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_;})==false){
            // 表示notFull_等待1s后，条件依然没有满足
            std::cerr << "task queue is full, submit task fail" << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType{return RType();});
            (*task)();
            return task->get_future();
        }
        //如果有空余，把任务放入任务队列中
//        using Task = std::function<void()>;
        taskQue_.emplace([task](){(*task)();});//执行下面的任务，(*task)解引用后就是个packaged_task
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
        return result;
    }
    
    //开启线程池
    void start(int initThreadSize = std::thread::hardware_concurrency()){//hardware_concurrency本机cpu核数量
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

    ThreadPool(const ThreadPool&) = delete;//=delete表示这个成员函数不能被再调用，const ThreadPool&为拷贝构造函数，即禁止拷贝线程池
    ThreadPool& operator=(const ThreadPool&) = delete;//禁止重载赋值

private:
    //定义线程函数
    void threadFunc(int threadid){
        auto lastTime = std::chrono::high_resolution_clock().now();
        //所有任务必须执行完成，线程池才可以回收所有线程资源
        for(;;){ //在这个循环中，线程会一直等待并执行任务队列中的任务。
            Task task;
            {
                //先获取锁
                std::unique_lock<std::mutex> lock(taskQueMtx_);//锁默认出当前作用域才释放

                std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;
                //锁+双重判断
                while(taskQue_.size() == 0){
                    //线程池要结束，回收线程资源
                    if(!isPoolRunning_){
                        threads_.erase(threadid);
                        std::cout << "tid:" << std::this_thread::get_id() << "exit!" << std::endl;
                        exitCond_.notify_all();
                        return;
                    }
                    //在cached模式下，有可能已经创建了很多线程，空闲时间超过60s，应该把多余的线程回收掉
                    //结束回收掉（超过initThreadSize_数量的）
                    //当前时间 - 上一次线程执行的时间>60s
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
                }

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
                task();//执行任务，把任务的返回值给到Result
            }
            idleThreadSize_++;
            lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完的时间
        }
    }

    bool checkRunningState() const{
        return isPoolRunning_;
    }

private:
    //    std::vector<std::unique_ptr<Thread>> threads_; //线程列表
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    size_t initThreadSize_; //初始的线程数量
    int threadSizeThreshHold_; //线程数量上限阈值
    std::atomic_int curThreadSize_;//记录当前线程池里面的线程总数量
    std::atomic_int idleThreadSize_;//记录空闲线程的数量

    //Task任务=》函数对象
    using Task = std::function<void()>;
    std::queue<Task> taskQue_;//任务队列
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
