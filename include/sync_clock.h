#ifndef __SYNCCLOCK_H
#define __SYNCCLOCK_H

#include <mutex>
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavdevice/avdevice.h"
#include "libswresample/swresample.h"
#ifdef __cplusplus
}
#endif

// 高精度单调时钟（跨平台实现）
static double get_monotonic_time()
{
#ifdef _WIN32
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

static void precise_sleep(double seconds)
{
    using namespace std::chrono;
    static double estimate = 5e-4; // 初始估计误差
    static double mean = 5e-4;
    static double m2 = 0;
    static int64_t count = 1;

    const double target = seconds - estimate;
    const auto start = high_resolution_clock::now();

    while (true)
    {
        const auto now = high_resolution_clock::now();
        const double elapsed =
            duration_cast<nanoseconds>(now - start).count() / 1e9;

        if (elapsed >= target)
            break;

        // 动态调整等待策略
        const double remaining = target - elapsed;
        if (remaining > 0.002)
        { // 2ms以上用系统休眠
            std::this_thread::sleep_for(
                microseconds(static_cast<int64_t>(remaining * 1e6 * 0.8)));
        }
        else
        {                // 短时间用自旋等待
            _mm_pause(); // 插入CPU暂停指令降低功耗
        }
    }

    // 更新误差统计（指数加权移动平均）
    const auto end = high_resolution_clock::now();
    const double actual =
        duration_cast<nanoseconds>(end - start).count() / 1e9;
    const double error = actual - seconds;

    ++count;
    const double delta = error - mean;
    mean += delta / count;
    m2 += delta * (error - mean);
    estimate = mean + sqrt(m2 / (count - 1));
}

struct SyncStats
{
    // 时间差异统计
    double max_diff = 0;    // 最大差异
    double avg_diff = 0;    // 平均差异
    double stddev_diff = 0; // 标准差

    // 帧处理统计
    int64_t total_frames = 0;   // 总帧数
    int64_t dropped_frames = 0; // 丢弃帧数
    int64_t late_frames = 0;    // 延迟显示帧数

    // 性能指标
    double avg_render_time = 0; // 平均渲染时间
    double max_sleep_time = 0;  // 最长等待时间
};

class SyncClock
{
public:
    void update_drift_compensation(double drift)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // drift_ratio_ = drift;
    }
    void update_audio(double pts, int64_t system_time_ns)
    {
        // std::lock_guard<std::mutex> lock(mutex_);
        this->last_audio_pts_ = pts;
    }
    double get_master_clock() const
    {
        // std::lock_guard<std::mutex> lock(mutex_);
        // // 获取当前时间点
        // auto now = std::chrono::high_resolution_clock::now();
        // // 将时间点转换为自纪元（epoch）以来的纳秒数
        // const int64_t now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch().count();
        // const double elapsed = (now_ns - last_update_ns_) / 1e9;
        // return last_audio_pts_ + elapsed * (1.0 + drift_ratio_);
        return last_audio_pts_;
    }

private:
    mutable std::mutex mutex_;
    double last_audio_pts_ = 0;
    double drift_ratio_ = 0; // 时钟漂移补偿
    int64_t last_update_ns_ = 0;
    int64_t last_valid_update_ns_ = 0;
    double last_valid_audio_pts_ = 0;
};

#endif