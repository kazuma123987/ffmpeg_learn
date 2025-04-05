#ifndef __VIDEO_PLAYER_H
#define __VIDEO_PLAYER_H
//=============== 主框架与SDL集成 ===============
#include <stdio.h>
#include <chrono>
#include <thread>
#include <windows.h>
#include <atomic>
#include <mutex>
#include <locale.h>
#include <numeric>   // for std::accumulate
#include <algorithm> // for std::clamp
#include <iostream>
#include <optional>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"

#include "safe_quene.h"
#include "shader.h"
#include "sound_manager.h"
#include "resource_manager.h"
#include "texture.h"
#include "sync_clock.h"
#include "scoped_avpacket.h"

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

//=== FFmpeg对象包装器 ===

struct AVFrameDeleter
{
    void operator()(AVFrame *frame) const
    {
        av_frame_unref(frame);
        av_frame_free(&frame);
    }
};
using ScopedAVFrame = std::unique_ptr<AVFrame, AVFrameDeleter>;

// 音频参数上下文
struct AudioContext
{
    SwrContext *swr_ctx = nullptr;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S32;
    SDL_AudioFormat sdl_audio_format = AUDIO_S32SYS;
    int frame_size = 0;
    int out_nb_samples = 1024;
    int out_nb_channels = 2;
    int out_sample_rate = 48000;
    std::vector<uint8_t> buffer;
};

class VideoPlayer
{
public:
    VideoPlayer(const char *_filename);
    double calculate_dynamic_threshold() const;
    void init_gl_resources(int width, int height);
    void init_ffmpeg(const char *customPath);
    void upload_frame(AVFrame *frame);
    void upload_plane(Texture &tex, GLenum format,
                      uint8_t *data, int line_size,
                      int width, int height,
                      GLuint pbo, GLuint texID_in_shader);
    void present_frame();
    void update_sync_stats(double diff, double render_time);
    void update_clock_prediction(double last_diff);
    double calculate_avg_render_time() const;
    void render_video_frame();
    void demux_loop();
    void video_decode_loop();
    void handle_decode_error(int err);
    void renderer(Shader &shader);
    void run();
    // 队列
    // SafeQueue<AVPacket *> video_packet_queue{30}; // 最多缓存30个视频包
    // SafeQueue<AVPacket *> audio_packet_queue{100};   // 最多缓存100个音频包
    SafeQueue<AVFrame *> video_frame_queue{100}; // 最多缓存100帧视频
    SafeQueue<AVFrame *> audio_frame_quene{100}; // 

    // 线程控制
    std::atomic<bool> running{true};
    // std::thread demux_thread;
    // std::thread video_decode_thread;
    // std::thread audio_play_thread;

    // 同步对象
    std::mutex audio_mutex;
    std::condition_variable audio_cv;
    SyncClock sync_clock;

private:
    static void audio_callback(void *userdata, Uint8 *stream, int len);
    // 成员变量
    AudioContext audio_ctx_;
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *video_codec_ctx = nullptr;
    AVCodecContext *audio_codec_ctx = nullptr;
    AVBufferRef *hwCtx = nullptr;
    AVRational video_time_base, audio_time_base;
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context = nullptr;
    Shader nv12Shader, yuv420Shader;
    Shader *rendererShader;
    GLuint VAO, VBO;
    Texture image_Y;
    Texture image_U;
    Texture image_V;
    Texture image_UV;
    GLuint pbo_ids[2] = {0, 0};
    std::atomic<int> pbo_index{0};
    AVFrame *last_uploaded_frame = nullptr;
    std::mutex gl_mutex;
    const char *filename;
    int vIndex, aIndex;
    struct SyncStats stats_;
    double last_video_pts_;
};
#endif
#endif