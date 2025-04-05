#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

//=== 线程安全队列模板 ===
template <typename T>
class SafeQueue
{
public:
    SafeQueue(size_t max_size = 1024) : max_size_(max_size) {}

    // 万能引用(可传入左值或右值)
    bool push(T &&item, int timeout_ms = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms > 0)
        {
            if (!not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this]
                                    { return queue_.size() < max_size_; }))
            {
                return false;
            }
        }
        else
        {
            not_full_.wait(lock, [this]
                           { return queue_.size() < max_size_; }); // 队列满时休眠，等待其他线程pop quene时唤醒
        }

        queue_.emplace(std::forward<T>(item));  // 完美转发引用，并使用引用的变量直接在已分配的队列数组中构建对象(避免无意义的临时对象的构造和内存拷贝)
        not_empty_.notify_one();                // 队列push后非空，唤醒其他线程工作以处理非空线程
        return true;
    }

    // 传入左值(变量)
    bool pop(T &item, int timeout_ms = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms > 0)
        {
            if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                     [this]
                                     { return !queue_.empty(); }))
            {
                return false;
            }
        }
        else
        {
            not_empty_.wait(lock, [this]
                            { return !queue_.empty(); }); // 队列空时休眠，等待其他线程push quene时唤醒
        }

        item = std::move(queue_.front()); // 队列最前面的对象的内容搬家到对象item中(避免内存拷贝)
        queue_.pop();                     // 队列最前面的对象出队           
        not_full_.notify_one();
        return true;
    }

    // 新增非阻塞尝试弹出方法
    bool try_pop(T &item)
    {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock() || queue_.empty())
        {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return true;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_; // 队列锁，写入和读取时都需要上锁
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_; // 共享队列，核心成员，访问时需要上锁
    size_t max_size_;
};