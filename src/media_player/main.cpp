#include <stdio.h>
#include <chrono>
#include <thread>
#include <windows.h>
#include <atomic>
#include <mutex>
#include <locale.h>
#include <condition_variable>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"
#include "util.h"

#include "shader.h"
#include "sound_manager.h"
#include "resource_manager.h"
#include "texture.h"

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
// N卡使用独显运行
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement;
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
// 函数声明
static enum AVPixelFormat custom_get_format(AVCodecContext *vCodecCtx, const enum AVPixelFormat *pix_fmt);
void render(Shader &shader);
inline void frame_sleep(int duration, int duration_first, int fps);
void setFPS(void *window);
void glfw_init_window(int width, int height);
void sdl_init_window(int width, int height);
void read_audio_data(void *udata, Uint8 *stream, int len);
FMOD_RESULT F_CALLBACK fmod_read_data(FMOD_SOUND * /*sound*/, void *data, unsigned int datalen);
void au_decode(const char *path);
void framesizecallback(GLFWwindow *window, int width, int height);
// 静态全局变量
static uint8_t *audio_chunk; // 指向PCM数据的头部
static uint32_t audio_len;   // PCM数据的总长度
static uint8_t *audio_pos;   // 指向PCM数据已经被读取到的位置
static double video_pts, audio_pts;
static AVRational *video_timebase = nullptr;
static AVRational *audio_timebase = nullptr;
// 非静态全局变量
const int SCREEN_WIDTH = 1600;
const int SCREEN_HEIGHT = 900;
GLFWwindow *window = NULL;
SDL_Window *sdl_window = NULL;
SDL_Event sdlEvent = {0};
bool buttons[8] = {0};
glm::vec2 cursorPos = glm::vec2(0);
bool musicHasPaused = false;
static enum AVPixelFormat hw_pix_fmt;
GLuint VAO, VBO;
// 多线程原子量
std::atomic<bool> start_or_quit(false);
std::mutex pause_mutex;
std::mutex au_pts_mutex;
std::condition_variable au_pts_cond;
std::mutex au_read_mutex;
std::condition_variable au_read_cond;
// define
#define USE_SDL
// #define USE_PBO
#ifdef main
#undef main
#endif
#if 0
int main(int argc, char *argv[])
{
    wchar_t tmpPath[FILENAME_MAX / 2] = {0};
    char customPath[FILENAME_MAX] = {0};
    char videoPath[] = "../res/gura.mp4";
    // char videoPath[] = "../res/360°.mp4";
    // char videoPath[] = "../res/tera.mp4";
    // glfw_init_window();
    if (argc < 2)
        strcpy(customPath, videoPath);
    else
    {
#ifndef WIN32
        setlocale(LC_CTYPE, "");
        char_to_wchar(argv[1], tmpPath, sizeof(tmpPath), 0);
        setlocale(LC_CTYPE, "utf8");
        wchar_to_char(tmpPath, customPath, sizeof(customPath), 65001);
        setlocale(LC_CTYPE, "C");
#else
        char_to_wchar(argv[1], tmpPath, sizeof(tmpPath), 0);
        wchar_to_char(tmpPath, customPath, sizeof(customPath), 65001);
#endif
    }

    //*---------FFMPEG显示部分---------*//

    // 为format上下文分配空间
    AVFormatContext *fmtCtx = avformat_alloc_context();
    // 打开视频文件
    if (avformat_open_input(&fmtCtx, customPath, NULL, NULL))
    {
        printf("failed to open the video:%s\n", customPath);
        return 1;
    }

    // 查找流信息
    if (avformat_find_stream_info(fmtCtx, NULL) < 0)
        printf("failed to find stream info\n");

    // 查找流索引
    int vIndex = -1;
    const AVCodec *vCodec = NULL;
    vIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &vCodec, 0);
    if (vIndex == -1)
        printf("failed to find a video stream\n");
    video_timebase = &fmtCtx->streams[vIndex]->time_base;
    av_dump_format(fmtCtx, vIndex, NULL, 0);
    printf("decoder name:%s\n", vCodec->name);

    // 根据选中的编解码器为编解码器上下文分配空间
    AVCodecContext *vCodecCtx = avcodec_alloc_context3(vCodec);
    vCodecCtx->thread_count = 8; // 线程数

    //*############################开启硬件加速############################
    AVBufferRef *hwCtx = NULL;
    bool hwDecode = true;
    // 1.查找设备
    const char *hwtypename = "d3d11va";
    AVHWDeviceType hwType = av_hwdevice_find_type_by_name(hwtypename);
    if (hwType == AV_HWDEVICE_TYPE_NONE)
    {
        printf("failed to find the %s device\n", hwtypename);
        return 1;
    }
    // 2.获取硬件像素格式
    for (int i = 0;; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(vCodec, i);
        if (config == NULL)
        {
            printf("Decoder %s don't support the device type:%s\n", vCodec->name, av_hwdevice_get_type_name(hwType));
            hwDecode = false;
            break;
        }
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == hwType)
        {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }

    if (hwDecode)
    {
        // 3设置编解码器获取硬件像素格式的回调函数
        vCodecCtx->get_format = custom_get_format;
        // 4.创建硬件上下文
        if (av_hwdevice_ctx_create(&hwCtx, hwType, NULL, NULL, 0) < 0)
            printf("failed to create device context\n");
        vCodecCtx->hw_device_ctx = av_buffer_ref(hwCtx);
    }
    else
    {
    }
    //*##############################################################

    // 赋值编解码上下文并打开编解码器
    AVCodecParameters *vCodecPar = fmtCtx->streams[vIndex]->codecpar;
    avcodec_parameters_to_context(vCodecCtx, vCodecPar);
    avcodec_open2(vCodecCtx, vCodec, NULL);

    // 为packet及其数据区分配空间
    AVPacket *packet = av_packet_alloc();
    av_new_packet(packet, vCodecCtx->width * vCodecCtx->height);

    // 为帧分配空间
    AVFrame *yuvFrame = av_frame_alloc();
    AVFrame *hwFrame = av_frame_alloc();
    AVFrame *tmpFrame = NULL; // 临时指针,用于指向yuvFrame或者hwFrame
    // 计算纹理宽高
    int width = vCodecCtx->width;
    int height = vCodecCtx->height;
// 创建窗口
#ifdef USE_SDL
    sdl_init_window(SCREEN_WIDTH, SCREEN_HEIGHT);
#else
    glfw_init_window(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

    Shader nv12Shader = ResourceManager::loadShader("../shader/nv12.vert", "../shader/nv12.frag", NULL, "nv12");
    nv12Shader.Use().unfm1i("y_tex", 0);
    nv12Shader.unfm1i("uv_tex", 1);
    Shader yuv420Shader = ResourceManager::loadShader("../shader/yuv420.vert", "../shader/yuv420.frag", NULL, "yuv420");
    yuv420Shader.Use().unfm1i("y_tex", 0);
    yuv420Shader.unfm1i("u_tex", 1);
    yuv420Shader.unfm1i("v_tex", 2);

    // 创建YUV纹理
    Texture image_Y(GL_R8, GL_RED);
    Texture image_U(GL_R8, GL_RED);
    Texture image_V(GL_R8, GL_RED);
    Texture image_UV(GL_RG8, GL_RG);
#define USE_PBO
#ifndef USE_PBO
    image_Y.Generate(width, height, NULL, GL_UNSIGNED_BYTE, true);
    image_U.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, true);
    image_V.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, true);
    image_UV.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, true);
#else
    image_Y.Generate(width, height, NULL, GL_UNSIGNED_BYTE, false);
    image_U.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);
    image_V.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);
    image_UV.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);
    // 使用PBO异步传输
    GLuint pbo_ids[2];

    glGenBuffers(2, pbo_ids);
    for (int i = 0; i < 2; ++i)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER,
                     width * height * 3 / 2, // YUV420P大小
                     nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
    // 创建音频解码线程
    std::thread decode(au_decode, customPath);
    // 渲染视频
    int count = 0;
    int fps = (float)fmtCtx->streams[vIndex]->avg_frame_rate.num / fmtCtx->streams[vIndex]->avg_frame_rate.den + 0.5f;
    int duration = 1000 / fps;
    int duration_first = duration + 1000 % fps;
    while (1)
    {
        pause_mutex.lock();
#ifdef USE_SDL
        SDL_PollEvent(&sdlEvent);
#else
        glfwPollEvents();
#endif
        pause_mutex.unlock();
#ifdef USE_SDL
        if (sdlEvent.type == SDL_QUIT)
        {
            start_or_quit = false;
            break;
        }
        else if (sdlEvent.type == SDL_WINDOWEVENT)
        {
            if (sdlEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                glViewport(0, 0, sdlEvent.window.data1, sdlEvent.window.data2);
        }
#else
        if (glfwWindowShouldClose(window))
        {
            start_or_quit = false;
            break;
        }
#endif
        if (av_read_frame(fmtCtx, packet) >= 0)
        {
            if (packet->stream_index == vIndex)
            {
                count++;
                if (avcodec_send_packet(vCodecCtx, packet) == 0)
                {
                    while (avcodec_receive_frame(vCodecCtx, yuvFrame) == 0) // while循环统一线程
                    {
                        if (hwDecode)
                        {
                            if (av_hwframe_transfer_data(hwFrame, yuvFrame, 0) < 0)
                                printf("failed to transfer data from gpu to memory\n");
                            tmpFrame = hwFrame;
#ifndef USE_PBO
                            // Y分量
                            glActiveTexture(GL_TEXTURE0);
                            image_Y.Bind();
                            glTexImage2D(GL_TEXTURE_2D, 0, image_Y.internalFormat, image_Y.width, image_Y.height, 0, image_Y.imageFormat, GL_UNSIGNED_BYTE, tmpFrame->data[0]);
                            // UV分量
                            glActiveTexture(GL_TEXTURE1);
                            image_UV.Bind();
                            glTexImage2D(GL_TEXTURE_2D, 0, image_UV.internalFormat, image_UV.width, image_UV.height, 0, image_UV.imageFormat, GL_UNSIGNED_BYTE, tmpFrame->data[1]);
#else
                            nv12Shader.Use();
                            static int pbo_index = 0;
                            // Y分量
                            glActiveTexture(GL_TEXTURE0);
                            image_Y.Bind();
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[pbo_index]);
                            uint8_t *pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
                            memcpy(pbo_ptr, tmpFrame->data[0], width * height);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, 0);
                            // UV分量
                            glActiveTexture(GL_TEXTURE1);
                            image_UV.Bind();
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[pbo_index]);
                            pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height / 2, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
                            memcpy(pbo_ptr, tmpFrame->data[1], width * height / 2);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RG, GL_UNSIGNED_BYTE, 0);
                            pbo_index = (pbo_index + 1) % 2;
#endif
                        }
                        else
                        {
                            yuv420Shader.Use();
                            tmpFrame = yuvFrame;
#ifdef USE_PBO
                            static int pbo_index = 0;
                            // Y分量
                            glActiveTexture(GL_TEXTURE0);
                            image_Y.Bind();
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[pbo_index]);
                            uint8_t *pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
                            memcpy(pbo_ptr, tmpFrame->data[0], width * height);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, 0);
                            pbo_index = (pbo_index + 1) % 2;
                            // U分量
                            glActiveTexture(GL_TEXTURE1);
                            image_U.Bind();
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[pbo_index]);
                            pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height / 4, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
                            memcpy(pbo_ptr, tmpFrame->data[1], width * height / 4);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, 0);
                            // V分量
                            glActiveTexture(GL_TEXTURE2);
                            image_V.Bind();
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[pbo_index]);
                            pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height / 4, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
                            memcpy(pbo_ptr, tmpFrame->data[2], width * height / 4);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, 0);
                            pbo_index = (pbo_index + 1) % 2;
#else
                            glActiveTexture(GL_TEXTURE0);
                            image_Y.Bind();
                            glTexImage2D(GL_TEXTURE_2D, 0, image_Y.internalFormat, image_Y.width, image_Y.height, 0, image_Y.imageFormat, GL_UNSIGNED_BYTE, tmpFrame->data[0]);
                            glActiveTexture(GL_TEXTURE1);
                            image_U.Bind();
                            glTexImage2D(GL_TEXTURE_2D, 0, image_U.internalFormat, image_U.width, image_U.height, 0, image_U.imageFormat, GL_UNSIGNED_BYTE, tmpFrame->data[1]);
                            glActiveTexture(GL_TEXTURE2);
                            image_V.Bind();
                            glTexImage2D(GL_TEXTURE_2D, 0, image_V.internalFormat, image_V.width, image_V.height, 0, image_V.imageFormat, GL_UNSIGNED_BYTE, tmpFrame->data[2]);
#endif
                        }
                        static int isFirst = 1;
                        if (!isFirst)
                        {
                            video_pts = yuvFrame->best_effort_timestamp * av_q2d(*video_timebase);
                            std::unique_lock<std::mutex> au_pts_lock(au_pts_mutex);
                            while (video_pts > audio_pts)
                            {
                                au_pts_cond.wait(au_pts_lock, []()
                                                 { return video_pts <= audio_pts; });
                            }
                            render(hwDecode ? nv12Shader : yuv420Shader);
                            // frame_sleep(duration, duration_first, fps);
#ifdef USE_SDL
                            setFPS(sdl_window);
#else
                            setFPS(window);
#endif
#ifdef USE_SDL
                            SDL_GL_SwapWindow(sdl_window);
#else
                            glfwSwapBuffers(window);
                            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                            glClear(GL_COLOR_BUFFER_BIT);
#endif
                        }
                        if (isFirst)
                        {
                            start_or_quit = true;
                            isFirst = 0;
                        }
                    }
                }
            }
            av_packet_unref(packet); // 清空数据区
        }
        else
        {
            start_or_quit = false;
            break; // 如果不能读取帧了就退出循环
        }
    }
    decode.join();
    printf("total frames:%d\n", count);
    // 释放内存
    av_packet_free(&packet);
    avcodec_free_context(&vCodecCtx);
    avformat_free_context(fmtCtx);
    av_frame_free(&yuvFrame);
    av_frame_free(&hwFrame);

    ResourceManager::Clear();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
#ifdef USE_PBO
    glDeleteBuffers(2, pbo_ids);
#endif
// 关闭窗口
#ifdef USE_SDL
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
#else
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return 0;
}
// 渲染
void render(Shader &shader)
{
    static int isFirst = 1;
    if (isFirst)
    {
        float vertices[] =
            {
                -1.0f, -1.0f, 0.0f, 1.0f, // 左下
                1.0f, -1.0f, 1.0f, 1.0f,  // 右下
                -1.0f, 1.0f, 0.0f, 0.0f,  // 左上
                1.0f, 1.0f, 1.0f, 0.0f    // 右上
            };
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
        glBindVertexArray(0);
        isFirst = 0;
    }
    shader.Use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}
// 保持帧间隔不变
inline void frame_sleep(int duration, int duration_first, int fps)
{
    static int fpsCount = 0;
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto fpsTime = std::chrono::milliseconds(fpsCount ? duration : duration_first);
    while (std::chrono::high_resolution_clock::now() - lastTime < fpsTime)
        ;
    // std::this_thread::sleep_for(lastTime - std::chrono::high_resolution_clock::now() + fpsTime);
    lastTime = std::chrono::high_resolution_clock::now();
    fpsCount = (fpsCount + 1) % fps;
}
// 设置帧率
void setFPS(void *window)
{
    static int fpsCount = 0;
    fpsCount++;
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto curTime = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(curTime - lastTime).count() >= 1000)
    {
        lastTime = curTime;
#ifdef USE_SDL
        SDL_SetWindowTitle((SDL_Window *)window, ("Media Player   FPS:" + std::to_string(fpsCount)).c_str());
#else
        glfwSetWindowTitle((GLFWwindow *)window, ("Media Player   FPS:" + std::to_string(fpsCount)).c_str());
#endif
        fpsCount = 0;
    }
}
// 用于编解码上下文的get_format回调函数
static enum AVPixelFormat custom_get_format(AVCodecContext *vCodecCtx, const enum AVPixelFormat *pix_fmt)
{
    const enum AVPixelFormat *ptr;
    for (ptr = pix_fmt; *ptr != -1; ptr++)
    {
        if (*ptr == hw_pix_fmt)
            return *ptr;
    }
    printf("failed to get hw_format\n");
    return AV_PIX_FMT_NONE;
}
// 初始化glfw
void glfw_init_window(int width, int height)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    //*---------GLFW初始化部分---------*//
    // 如果GLFW_CONTEXT_VERSION_MAJOR为4,GLFW_CONTEXT_VERSION_MINOR为6,表示opengl4.6版本
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // glfwWindowHint(GLFW_RESIZABLE, false);
    window = glfwCreateWindow(width, height, "Media Player", NULL, NULL);
    if (window == NULL)
    {
        printf("failed to create window\n");
        glfwTerminate();
        return;
    }
    // // 获取主显示器的视频模式
    // const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    // // 切换到全屏模式
    // glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("ERROR::GLAD failed to load the proc\n");
        return;
    }
    glfwSetFramebufferSizeCallback(window, framesizecallback);
}
void sdl_init_window(int width, int height)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // 创建SDL Window
    sdl_window = SDL_CreateWindow("Media Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!sdl_window)
    {
        fprintf(stderr, "\nSDL: could not set video mode:%s - exiting\n", SDL_GetError());
        exit(1);
    }
    SDL_GL_CreateContext(sdl_window);
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("ERROR::GLAD failed to load the proc\n");
        return;
    }
    // SDL_Renderer *renderer = SDL_CreateRenderer(sdl_window, -1, 0);
}
/**
 * @brief 音频设备需要更多数据的时候会调用该回调函数
 * @param udata:用户数据,即spec.userdata
 * @param stream:数据缓冲区,用于写入音频数据
 * @param len:写入数据缓冲区的数据大小
 */
void read_audio_data(void *udata, Uint8 *stream, int len)
{
    // 首先使用SDL_memset()将stream中的数据设置为0
    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;
    len = (len > (int)audio_len ? audio_len : len);

    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
    if (audio_len == 0)
    {
        au_read_cond.notify_one();
    }
}
FMOD_RESULT F_CALLBACK fmod_read_data(FMOD_SOUND * /*sound*/, void *data, unsigned int datalen)
{
    memset(data, 0, datalen);
    if (audio_len == 0)
        return FMOD_OK;
    datalen = (datalen > audio_len ? audio_len : datalen);
    memcpy(data, audio_pos, datalen);
    audio_pos += datalen;
    audio_len -= datalen;
    if (audio_len == 0)
    {
        au_read_cond.notify_one();
    }
    return FMOD_OK;
}
/**
 * @brief 音频解码线程
 * @param path 音视频的utf-8路径
 * @retval none
 */
void au_decode(const char *path)
{
    // avformat
    AVFormatContext *fmtCtx = avformat_alloc_context();
    if (avformat_open_input(&fmtCtx, path, NULL, NULL))
        printf("failed to open the path:%s\n", path);
    if (avformat_find_stream_info(fmtCtx, NULL) < 0)
        printf("no stream info in path:%s\n", path);
    // find decoder
    int aIndex = -1;
    const AVCodec *aDecoder = NULL;
    aIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &aDecoder, 0);
    if (aIndex == -1)
        printf("failed to find the best stream in func:%s,line:%d", __FUNCTION__, __LINE__);
    audio_timebase = &fmtCtx->streams[aIndex]->time_base;
    // open decoder
    AVCodecContext *aCodecCtx = avcodec_alloc_context3(aDecoder);
    AVCodecParameters *par = fmtCtx->streams[aIndex]->codecpar;
    avcodec_parameters_to_context(aCodecCtx, par);
    if (avcodec_open2(aCodecCtx, aDecoder, NULL) < 0)
        printf("failed to open the audio decoder\n");
    // 设置转换参数
    int out_nb_samples = 1024;
    int out_nb_channels = aCodecCtx->ch_layout.nb_channels;
    int out_sample_rate = aCodecCtx->sample_rate;
    if (aCodecCtx->sample_rate > out_sample_rate)
        out_sample_rate = aCodecCtx->sample_rate;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVSampleFormat out_sample_format = AV_SAMPLE_FMT_S32;
    AVChannelLayout in_ch_layout = {};
    av_channel_layout_default(&in_ch_layout, aCodecCtx->ch_layout.nb_channels);
    // 设置转换上下文
    SwrContext *swrCtx = NULL;
    swr_alloc_set_opts2(
        &swrCtx, &out_ch_layout, out_sample_format, out_sample_rate,
        &aCodecCtx->ch_layout, aCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
    swr_init(swrCtx);
    // 分配容器空间
    AVFrame *aFrame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    int bufferSize = av_samples_get_buffer_size(NULL, out_nb_channels, out_nb_samples, out_sample_format, 1);
    unsigned char *buffer = (unsigned char *)av_malloc(bufferSize);
#ifndef USE_SDL
    // 设置FMOD参数
    FMOD_CREATESOUNDEXINFO exinfo;
    memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    uint8_t ltpcmlen = 0U;
    switch (out_sample_format)
    {
    case AV_SAMPLE_FMT_U8:
        exinfo.format = FMOD_SOUND_FORMAT_PCM8;
        ltpcmlen = 1U;
        break;
    case AV_SAMPLE_FMT_S16:
        exinfo.format = FMOD_SOUND_FORMAT_PCM16;
        ltpcmlen = 2U;
        break;
    case AV_SAMPLE_FMT_S32:
        exinfo.format = FMOD_SOUND_FORMAT_PCM32;
        ltpcmlen = 4U;
        break;
    case AV_SAMPLE_FMT_FLT:
        exinfo.format = FMOD_SOUND_FORMAT_PCMFLOAT;
        ltpcmlen = 4U;
        break;
    default:
        exinfo.format = FMOD_SOUND_FORMAT_NONE;
        break;
    }
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    exinfo.defaultfrequency = out_sample_rate;
    exinfo.decodebuffersize = out_nb_samples;
    exinfo.length = out_sample_rate * ltpcmlen * out_nb_channels * 5; // '5'代表5秒
    exinfo.numchannels = out_nb_channels;
    exinfo.pcmreadcallback = fmod_read_data;
    SoundManager *sound = new SoundManager(32);
    sound->loadSound(NULL, "bgm", FMOD_CREATESTREAM | FMOD_OPENUSER | FMOD_LOOP_NORMAL, &exinfo);
    while (!start_or_quit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    sound->playSound("bgm", 0);
#else
    // SDL音频参数
    SDL_AudioSpec spec;
    spec.freq = out_sample_rate;
    spec.channels = out_nb_channels;
    spec.silence = 0;
    spec.samples = out_nb_samples;
    spec.callback = read_audio_data;
    spec.userdata = NULL;
    switch (out_sample_format)
    {
    case AV_SAMPLE_FMT_S16:
        spec.format = AUDIO_S16SYS;
        break;
    case AV_SAMPLE_FMT_S32:
        spec.format = AUDIO_S32SYS;
        break;
    case AV_SAMPLE_FMT_FLT:
        spec.format = AUDIO_F32SYS;
        break;
    default:
        break;
    }
    // 打开SDL音频
    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        printf("can't open audio.\n");
        return;
    }
    while (!start_or_quit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // 设置为0表示开始播放
    SDL_PauseAudio(0);
#endif

    while (start_or_quit)
    {
#ifndef USE_SDL
        sound->pauseGroup(0, true);
#else
        SDL_PauseAudio(true);
#endif
        pause_mutex.lock();
#ifndef USE_SDL
        sound->pauseGroup(0, false);
#else
        SDL_PauseAudio(false);
#endif
        pause_mutex.unlock();
        if (av_read_frame(fmtCtx, packet) >= 0)
        {
            if (packet->stream_index == aIndex)
            {
                avcodec_send_packet(aCodecCtx, packet);
                while (avcodec_receive_frame(aCodecCtx, aFrame) == 0)
                {
                    swr_convert(
                        swrCtx, &buffer, out_nb_samples,
                        (const uint8_t **)aFrame->data, aFrame->nb_samples);
                    audio_pts = aFrame->pts * av_q2d(*audio_timebase);
                    au_pts_cond.notify_one();
                }
                std::unique_lock<std::mutex> au_read_lock(au_read_mutex);
                while (audio_len > 0)
                {
                    au_read_cond.wait(au_read_lock, []()
                                      { return audio_len == 0; });
                }
                audio_chunk = buffer;
                audio_len = bufferSize;
                audio_pos = audio_chunk;
#ifndef USE_SDL
                sound->update();
#endif
            }
            av_packet_unref(packet);
        }
    }

    // 清除空间
#ifndef USE_SDL
    delete sound;
#endif
    avformat_free_context(fmtCtx);
    avcodec_free_context(&aCodecCtx);
    av_frame_free(&aFrame);
    av_packet_free(&packet);
    av_free(buffer);
    swr_free(&swrCtx);
}
void framesizecallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}
#endif
