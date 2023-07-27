## ThreadPool
### 涉及技术：
- C++、线程间通信、可变参数模板、lambda表达式；
- g++编译动态库，GDB多线程调试；
- 基于condition_ vatiable和mutex实现生产者和消费者线程之间的通信；
- 使用future类型异步访问消费者线程执行完任务后的返回值；
- 线程池支持线程数量固定和动态调整双模式；
### 特性：
- 支持任意数量参数的函数传递
- 哈希表和队列管理线程对象和任务
- 支持线程池双模式切换
### ThreadPool
> 此目录下编译好的动态库。 需要用户继承任务基类，重写run方法。
#### 编译
```bash
git clone git@github.com:lxw-stack/ThreadPool.git

cd ThreadPool/ThreadPool

g++ -fPIC -shared threadpool.cpp -o libpool.so -std=c++17
```
#### 使用方法
```bash
mv libpool.so /usr/local/lib/

mv threadpool.h /usr/local/include/

# 编译时连接动态库生成a.out
g++ main.cpp -std=c++17 -lpool -lpthread

cd /etc/ld.so.conf.d/

vim mylib.conf # 在其中加入刚才动态库的路径/usr/local/lib/

ldconfig # 将动态库刷新到 /etc/ld.so.cahce中

# 执行a.out
./a.out

```
#### 使用示例
```cpp
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
    return 0;
}
```
### ThreadPool2.0
> 使用可变参数模板，支持任意数量参数的任务函数加入线程池任务队列
#### 使用方法
> Header only. 直接包含头文件即可
#### 使用示例
```cpp
ThreadPool pool;
pool.start(2);
auto f = [](int a, int b) -> int {
    int sum = 0;
    for (int i = a; i <= b; ++i) {
        sum += i;
    }
    return sum;
};
std::future<int> r2 = pool.submitTask(f, 1, 100);

std::cout << r1.get() << std::endl;
```
