//
//  main.cpp
//  ThreadPool2.0
//
//  Created by vivi on 2023/7/25.
//

#include <iostream>
#include <thread>
#include <future>
#include <functional>
#include "threadpool.hpp"
using namespace std;

/*
 如何让线程提交任务更加方便
 1.pool.submitTask(sum1,1,20);
 pool.submitTask(sum2,1,2,3);
 submitTask：可变参模版编程
 2.我们自己造了一个Result以及相关的类型，代码挺多
 c++11线程库 thread packaged_task(function函数对象) async
 使用future来代替Result节省线程池代码
 */
int sum1(int a, int b){
    this_thread::sleep_for(std::chrono::seconds(5));
    return a + b;
}
int sum2(int a, int b, int c){
    this_thread::sleep_for(std::chrono::seconds(2));
    return a + b + c;
}
int main(int argc, const char * argv[]) {
    // insert code here...
//    packaged_task<int(int, int)> task(sum1);
//    //future <=> Result
//    future<int> res = task.get_future();
//    //    task(10, 20);
//    thread t(std::move(task), 10, 20);
//    t.detach();
//    cout << res.get() << endl;
    ThreadPool pool;
//    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(2);
    future<int> res = pool.submitTask(sum1, 1, 2);
    future<int> res1 = pool.submitTask(sum1, 3, 24);
    future<int> res2 = pool.submitTask(sum1, 5, 6);
    future<int> res3 = pool.submitTask(sum1, 7, 8);
    future<int> res4 = pool.submitTask(sum1, 9, 10);
    std::cout << "Hello, World!\n";
    std::cout << res.get() << endl;
    std::cout << res1.get() << endl;
    std::cout << res2.get() << endl;
    std::cout << res3.get() << endl;
    std::cout << res4.get() << endl;
    return 0;
}
