#ifndef __SCOPED_AVPACKET_H
#define __SCOPED_AVPACKET_H

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

class ScopedAVPacket
{
public:
    explicit ScopedAVPacket() : pkt(av_packet_alloc()) {}

    // 禁用拷贝构造
    ScopedAVPacket(const ScopedAVPacket &) = delete;
    ScopedAVPacket &operator=(const ScopedAVPacket &) = delete;

    // 移动语义
    ScopedAVPacket(ScopedAVPacket &&other) noexcept : pkt(other.pkt)
    {
        other.pkt = nullptr;
    }

    ScopedAVPacket &operator=(ScopedAVPacket &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            pkt = other.pkt;
            other.pkt = nullptr;
        }
        return *this;
    }

    ~ScopedAVPacket() { reset(); }

    void reset()
    {
        if (pkt)
        {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }

    AVPacket *get() const { return pkt; }

private:
    AVPacket *pkt = nullptr;
};

#endif
