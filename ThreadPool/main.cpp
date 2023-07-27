//
//  main.cpp
//  ThreadPool
//
//  Created by vivi on 2022/10/18.
//

#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.hpp"

using uLong = unsigned long long;
/*
举例：
有些场景，是希望能够获取线程执行任务得返回值得
thread1  1 + ... + 10000
thread2  10001 + ... + 20000
...
main thread:给每个线程分配计算区间，并等待他们算完返回结果，合并最终结果
*/

class MyTask : public Task{
public:
    MyTask(int begin, int end) : begin_(begin), end_(end){
        
    }
    //问题一:怎么设置run函数的返回值，可以表示任意类型
    //Java Object 是所有类型的基类
    //C++17 Any类型
    MyAny run() {
        std::cout << "tid:" << std::this_thread::get_id() << "begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        uLong sum = 0;
        for(uLong i = begin_; i < end_; i++){
            sum += i;
        }
        std::cout << "tid:" << std::this_thread::get_id() << "end!" << std::endl;
        return sum;
    }
    
private:
    int begin_;
    int end_;
};

int main(int argc, const char * argv[]) {
    // insert code here...
    {
        std::cout << "Hello, World!\n";
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        //开始启动线程池
        pool.start(4);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(101,200));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(201,300));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(1,100));
        Result res5 = pool.submitTask(std::make_shared<MyTask>(101,200));
        Result res6 = pool.submitTask(std::make_shared<MyTask>(201,300));
        uLong sum1 = res1.get().cast_<uLong>();
        uLong sum2 = res2.get().cast_<uLong>();
        uLong sum3 = res3.get().cast_<uLong>();
        std::cout << sum1 + sum2 + sum3 << std::endl;
    }
    std::cout << "main over" << std::endl;
    getchar();
    
#if 0
    //问题：ThreadPool对象析构以后，怎么样把线程池相关的线程资源全部回收
    {
        ThreadPool pool;
        //用户自己设置线程池的工作模式
        pool.setMode(PoolMode::MODE_CACHED);
        //开始启动线程池
        pool.start(4);
        //如何设计这里的result机制
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(101,200));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(201,300));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(1,100));
        Result res5 = pool.submitTask(std::make_shared<MyTask>(101,200));
        Result res6 = pool.submitTask(std::make_shared<MyTask>(201,300));
        uLong sum1 = res1.get().cast_<uLong>();
        uLong sum2 = res2.get().cast_<uLong>();
        uLong sum3 = res3.get().cast_<uLong>();
        // Master - Slave线程模型
        // Master线程用来分解任务，然后给各个salve线程分配任务
        // 等待各个slave线程执行完任务，返回结果
        // Master线程合并各个任务结果，输出
        std::cout << sum1+sum2+sum3 << std::endl;
    }
//    int sum = res.get().cast_<int>();
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.submitTask(std::make_shared<MyTask>());
//    pool.start(4);
//    std::this_thread::sleep_for(std::chrono::seconds(5));
    getchar();
#endif
    return 0;
}
 
