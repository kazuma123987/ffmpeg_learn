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

#define USE_SDL_WINDOW
// #define USE_GLFW_WINDOW
// #define USE_FMOD_AUDIO
#define USE_SDL_AUDIO

// 音频参数上下文
struct AudioContext
{
    SwrContext *swr_ctx = nullptr;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S32;
#ifdef USE_SDL_AUDIO
    SDL_AudioFormat sdl_audio_format = AUDIO_S32SYS;
#endif
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
    void init_gl_resources(AVFrame *frame, AVPixelFormat pix_fmt);
    int init_ffmpeg(const char *customPath);
    void upload_frame(AVFrame *frame);
    void upload_plane(Texture &tex, GLenum format,
                      uint8_t *data, int line_size,
                      int width, int height,
                      GLuint pbo, GLuint texID_in_shader,
                      int datatype);
    void present_frame(AVFrame *frame);
    void render_video_frame();
    void demux_loop();
    void handle_decode_error(int err);
    void renderer(Shader &shader);
    int initResource();
    void run();
    // 队列
    SafeQueue<AVFrame *> video_frame_queue{100}; // 最多缓存100帧视频
    SafeQueue<AVFrame *> audio_frame_quene{300}; // 最多缓存300帧音频
    AVRational video_time_base, audio_time_base;

    // 线程控制
    std::atomic<bool> volatile running{true};

    // 同步对象
    std::mutex audio_mutex;
    std::mutex video_pts_mutex;
    std::condition_variable audio_cv;
    std::condition_variable video_cv;
    SyncClock sync_clock;

    AudioContext audio_ctx_;

private:
    // 成员变量
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *video_codec_ctx = nullptr;
    AVCodecContext *audio_codec_ctx = nullptr;
    AVBufferRef *hwCtx = nullptr;
#ifdef USE_SDL_WINDOW
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context = nullptr;
#elif defined(USE_GLFW_WINDOW)
    GLFWwindow *window = nullptr;
#endif
    Shader nv12Shader, yuv420Shader;
    Shader *rendererShader;
    GLuint VAO, VBO;
    Texture image_Y;
    Texture image_U;
    Texture image_V;
    Texture image_UV;
    GLuint pbo_ids[2] = {0, 0};
    std::atomic<int> pbo_index{0};
    std::mutex gl_mutex;
    const char *filename;
    int vIndex, aIndex;
};
#endif
#endif