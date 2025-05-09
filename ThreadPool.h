#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;  // usage: std::function<返回类型(参数类型1, 参数类型2, ...)>
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); }); // 等待直到线程池有任务
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());  // 取任务
                        this->tasks.pop();
                    }

                    task(); // 执行任务
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<std::invoke_result_t<F, Args...>>    // 相当于 C++11::  typename std::result_of<F(Args...)>::type                             
    {                                                   // 编译期推导F(Args...)的返回类型 注意不是f(args...) 这个是运行期的对象参数
    using return_type = std::invoke_result_t<F, Args>;
    // C++11::   using return_type = typename std::result_of<F(Args...)>::type;

                                                                        // packaged_task 封装函数相当于把参数解决掉了
    auto task = std::make_shared< std::packaged_task<return_type()> >(  // 类似于是 function<return_type()> 只不过可以用 get_future()
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)      // 初始化 packaged_task
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });  // 这个 lambda式 符合 void() 的签名 所以可以加入 std::queue< std::function<void()> >
                                                // lambda式 为了解决 packaged_task<return_type()> 的返回值
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif
