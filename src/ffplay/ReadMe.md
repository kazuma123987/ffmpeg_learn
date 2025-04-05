# <center>多线程注意事项</center>
1. 线程1使用std::condition_variable cv;cv.notify_one()5次后，线程2也只会被唤醒一次，只会运行一次循环，C语言则是线程1调用5次SDL_CondSignal函数线程2也只会被唤醒一次，可以在唤醒后处理所有的队列里的东西
2. 条件变量的作用是用于休眠和唤醒，即std::condition_variable cv;cv.wait进行休眠，另一个线程要用到此线程后再cv.notify_one()唤醒此线程，以节省性能
3. 互斥锁std::mutex作用是保护共享资源，上锁和解锁之间的时间一定要非常短，此外注意所有线程上锁和解锁顺序要一致，否则会死锁