1. 条件变量、互斥量和锁:
- std::condition_variable cond; //条件变量
- std::mutex _mutex; //互斥量
- std::unique_lock<std::mutex> _lock(_mutex); // 互斥锁

2. 分析代码
    bool push(T &&item, int timeout_ms = 0)
    {
        // unique_lock构造时自动上锁，离开作用域后自动解锁
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms > 0)
        {
            // 首先解锁，然后等待唤醒，当被唤醒时不满足条件继续休眠等待，相当于外层有个while循环，循环推出的条件是第三个参数
            // 如果被唤醒会自动重新上锁，这样能保护后面的queue_的访问
            // 如果超时后就退出循环，重新上锁并执行后面代码
            if (!not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this]
                                    { return queue_.size() < max_size_; }))
            {
                return false;
            }
        }
        else
        {
            // 首先解锁，然后等待唤醒，当被唤醒时不满足条件继续休眠等待，相当于外层有个while循环，循环推出的条件是第二个参数
            // 如果被唤醒会自动重新上锁，这样能保护后面的queue_的访问
            not_full_.wait(lock, [this]
                           { return queue_.size() < max_size_; }); // 队列满时休眠，等待其他线程pop quene时唤醒
        }

        queue_.emplace(std::forward<T>(item)); // 完美转发引用，并使用引用的变量直接在已分配的队列数组中构建对象(避免无意义的临时对象的构造和内存拷贝)
        not_empty_.notify_one();               // 队列push后非空，唤醒其他线程工作以处理非空线程
        return true;
    }

3. 原子量(std::automic<bool>)
- 原子量能够保证同一时间只能有一个线程访问，防止例如counter++这种读->改->写的操作同时读取的值相同，然后counter的值实际只增加了1
- 原子量被标记为不可优化，即cpu只能从内存中取值(防止从寄存器或缓存取值)
- 原子量是cpu提供的功能，比互斥锁更高效