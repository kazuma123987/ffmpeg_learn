#include "video_player.h"
#include <array>
//=============== 初始化SDL和OpenGL ===============

// 初始化硬件解码器
static AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

static bool hwDecode = true;
static std::atomic<double> video_pts(0.0), audio_pts(0.0);
void setFPS(void *window)
{
    static int fpsCount = 0;
    fpsCount++;
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto curTime = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(curTime - lastTime).count() >= 1000)
    {
        lastTime = curTime;
#ifdef USE_SDL_WINDOW
        SDL_SetWindowTitle((SDL_Window *)window, ("Media Player   FPS:" + std::to_string(fpsCount)).c_str());
#elif defined(USE_GLFW_WINDOW)
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

#ifdef USE_FMOD_AUDIO
FMOD_RESULT F_CALLBACK audio_callback(FMOD_SOUND *sound, void *data, unsigned int len)
{
    if (len == 0)
        return FMOD_OK;

    uint8_t **stream = (uint8_t **)&data;
    memset(stream[0], 0, len);
    // 从队列获取音频帧
    AVFrame *frame = nullptr;
    void *userdata = nullptr;
    FMOD_Sound_GetUserData(sound, &userdata);
    VideoPlayer *player = static_cast<VideoPlayer *>(userdata);
    if (!player->audio_frame_quene.pop(frame, 5))
    { // 5ms超时
        return FMOD_RESULT_FORCEINT;
    }

    auto &ac = player->audio_ctx_;
    uint8_t *out_data[1] = {ac.buffer.data()};
    ac.buffer.resize(ac.buffer.capacity());
    swr_convert(ac.swr_ctx, out_data, player->audio_ctx_.out_nb_samples,
                const_cast<const uint8_t **>(frame->data),
                frame->nb_samples);
    // 更新同步时钟（加权平均）
    static int64_t lastframepts = 0;
    double frame_pts = lastframepts * av_q2d(player->audio_time_base);
    lastframepts = frame->pts;
    // 获取当前时间点
    auto now = std::chrono::high_resolution_clock::now();
    // 将时间点转换为自纪元（epoch）以来的纳秒数
    const int64_t now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch().count();
    player->sync_clock.update_audio(frame_pts, now_ns); // 平滑系数0.2
    audio_pts = frame->pts * av_q2d(player->audio_time_base);
    if(audio_pts >= video_pts)
    {
        player->video_cv.notify_one();
    }

    av_frame_free(&frame);
    // 计算可拷贝数据量
    const int copy_bytes = (ac.buffer.size() < len ? ac.buffer.size() : len);
    if (copy_bytes > 0)
    {
        // 混合到输出流
        memcpy(stream[0], ac.buffer.data(), copy_bytes);

        // 更新缓冲区
        ac.buffer.erase(ac.buffer.begin(), ac.buffer.begin() + copy_bytes);
        stream[0] += copy_bytes;
        len -= copy_bytes;
    }
    return FMOD_OK;
}
#elif defined(USE_SDL_AUDIO)
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    if (len == 0)
        return;
    // 清空目标缓冲区
    SDL_memset(stream, 0, len);

    // 从队列获取音频帧
    AVFrame *frame = nullptr;
    VideoPlayer *player = static_cast<VideoPlayer *>(userdata);
    if (!player->audio_frame_quene.pop(frame, 5))
    { // 5ms超时
        return;
    }

    auto &ac = player->audio_ctx_;
    uint8_t *out_data[1] = {ac.buffer.data()};
    ac.buffer.resize(ac.buffer.capacity());
    swr_convert(ac.swr_ctx, out_data, player->audio_ctx_.out_nb_samples,
                const_cast<const uint8_t **>(frame->data),
                frame->nb_samples);
    // 更新同步时钟（加权平均）
    static int64_t lastframepts = 0;
    double frame_pts = lastframepts * av_q2d(player->audio_time_base);
    lastframepts = frame->pts;
    // 获取当前时间点
    auto now = std::chrono::high_resolution_clock::now();
    // 将时间点转换为自纪元（epoch）以来的纳秒数
    const int64_t now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch().count();
    player->sync_clock.update_audio(frame_pts, now_ns); // 平滑系数0.2
    audio_pts = frame->pts * av_q2d(player->audio_time_base);
    if(audio_pts >= video_pts)
    {
        player->video_cv.notify_one();
    }

    av_frame_free(&frame);
    // 计算可拷贝数据量
    const int copy_bytes = (ac.buffer.size() < len ? ac.buffer.size() : len);
    if (copy_bytes > 0)
    {
        // 混合到输出流
        SDL_MixAudioFormat(stream, ac.buffer.data(), ac.sdl_audio_format,
                           copy_bytes, SDL_MIX_MAXVOLUME);

        // 更新缓冲区
        ac.buffer.erase(ac.buffer.begin(), ac.buffer.begin() + copy_bytes);
        stream += copy_bytes;
        len -= copy_bytes;
    }
}
#endif

#ifdef USE_FMOD_AUDIO
static SoundManager sound(32);
#endif

static void audio_play(VideoPlayer *player)
{
#ifdef USE_FMOD_AUDIO
    // 设置FMOD参数
    FMOD_CREATESOUNDEXINFO exinfo;
    memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    uint8_t ltpcmlen = 0U;
    switch (player->audio_ctx_.out_sample_fmt)
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
    exinfo.defaultfrequency = player->audio_ctx_.out_sample_rate;
    exinfo.decodebuffersize = player->audio_ctx_.out_nb_samples;
    exinfo.length = player->audio_ctx_.out_sample_rate * ltpcmlen * player->audio_ctx_.out_nb_channels * 5; // '5'代表5秒
    exinfo.numchannels = player->audio_ctx_.out_nb_channels;
    exinfo.pcmreadcallback = audio_callback;
    exinfo.userdata = player;
    // SoundManager *sound = new SoundManager(32);
    sound.loadSound(NULL, "bgm", FMOD_CREATESTREAM | FMOD_OPENUSER | FMOD_LOOP_NORMAL, &exinfo);
    sound.playSound("bgm", 0);
#elif defined(USE_SDL_AUDIO)
    // SDL音频参数
    SDL_AudioSpec spec;
    spec.freq = player->audio_ctx_.out_sample_rate;
    spec.channels = player->audio_ctx_.out_nb_channels;
    spec.silence = 0;
    spec.samples = player->audio_ctx_.out_nb_samples;
    spec.callback = audio_callback;
    spec.userdata = player;
    switch (player->audio_ctx_.out_sample_fmt)
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
    // 设置为0表示开始播放
    SDL_PauseAudio(0);
#endif
}

static void framesizecallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

VideoPlayer::VideoPlayer(const char *_filename)
{
    this->filename = _filename;
}

// 初始化OpenGL资源
void VideoPlayer::init_gl_resources(int width, int height)
{
    std::lock_guard<std::mutex> gl_lock(gl_mutex);

    // 创建YUV纹理
    image_Y.setFormat(GL_R8, GL_RED);
    image_U.setFormat(GL_R8, GL_RED);
    image_V.setFormat(GL_R8, GL_RED);
    image_UV.setFormat(GL_RG8, GL_RG);
    image_Y.Generate(width, height, NULL, GL_UNSIGNED_BYTE, false);
    image_U.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);
    image_V.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);
    image_UV.Generate(width / 2, height / 2, NULL, GL_UNSIGNED_BYTE, false);

    // 创建PBO
    glGenBuffers(2, pbo_ids);
    for (int i = 0; i < 2; ++i)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER,
                     width * height * 3 / 2, // YUV420P大小
                     nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

int VideoPlayer::init_ffmpeg(const char *customPath)
{
    // 为format上下文分配空间
    fmt_ctx = avformat_alloc_context();
    // 打开视频文件
    if (avformat_open_input(&fmt_ctx, customPath, NULL, NULL))
    {
        printf("failed to open the video:%s\n", customPath);
        return -1;
    }

    // 查找流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
        printf("failed to find stream info\n");

    // 查找流索引
    const AVCodec *vCodec = NULL, *aCodec = NULL;
    vIndex = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vCodec, 0);
    if (vIndex == -1)
        printf("failed to find a video stream\n");
    aIndex = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &aCodec, 0);
    if (aIndex == -1)
        printf("failed to find a audio stream\n");
    video_time_base = fmt_ctx->streams[vIndex]->time_base;
    audio_time_base = fmt_ctx->streams[aIndex]->time_base;
    av_dump_format(fmt_ctx, vIndex, NULL, 0);
    printf("video decoder name:%s\n", vCodec->name);
    printf("audio decoder name:%s\n", aCodec->name);

    // 根据选中的编解码器为编解码器上下文分配空间
    video_codec_ctx = avcodec_alloc_context3(vCodec);
    video_codec_ctx->thread_count = 8; // 线程数

    //*############################开启硬件加速############################
    // 1.查找设备
    const char *hwtypename = "d3d11va";
    AVHWDeviceType hwType = av_hwdevice_find_type_by_name(hwtypename);
    if (hwType == AV_HWDEVICE_TYPE_NONE)
    {
        printf("failed to find the %s device\n", hwtypename);
        return -2;
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
        video_codec_ctx->get_format = custom_get_format;
        // 4.创建硬件上下文
        if (av_hwdevice_ctx_create(&this->hwCtx, hwType, NULL, NULL, 0) < 0)
            printf("failed to create device context\n");
        video_codec_ctx->hw_device_ctx = av_buffer_ref(this->hwCtx);
    }
    else
    {
    }
    //*##############################################################

    // 赋值编解码上下文并打开编解码器
    AVCodecParameters *vCodecPar = fmt_ctx->streams[vIndex]->codecpar;
    avcodec_parameters_to_context(video_codec_ctx, vCodecPar);
    avcodec_open2(video_codec_ctx, vCodec, NULL);

    // open decoder
    audio_codec_ctx = avcodec_alloc_context3(aCodec);
    AVCodecParameters *par = fmt_ctx->streams[aIndex]->codecpar;
    avcodec_parameters_to_context(audio_codec_ctx, par);
    if (avcodec_open2(audio_codec_ctx, aCodec, NULL) < 0)
        printf("failed to open the audio decoder\n");
    // 设置转换参数
    this->audio_ctx_.out_nb_samples = 1024;
    this->audio_ctx_.out_nb_channels = audio_codec_ctx->ch_layout.nb_channels;
    this->audio_ctx_.out_sample_rate = audio_codec_ctx->sample_rate;
    this->audio_ctx_.out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    this->audio_ctx_.out_sample_fmt = AV_SAMPLE_FMT_S32;
    AVChannelLayout in_ch_layout = {};
    av_channel_layout_default(&in_ch_layout, audio_codec_ctx->ch_layout.nb_channels);
    // 设置转换上下文
    swr_alloc_set_opts2(
        &this->audio_ctx_.swr_ctx, &this->audio_ctx_.out_ch_layout, this->audio_ctx_.out_sample_fmt, this->audio_ctx_.out_sample_rate,
        &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, 0, NULL);
    swr_init(this->audio_ctx_.swr_ctx);
    this->audio_ctx_.buffer.resize(av_samples_get_buffer_size(NULL, this->audio_ctx_.out_nb_channels, this->audio_ctx_.out_nb_samples, this->audio_ctx_.out_sample_fmt, 1));
    return 0;
}

void VideoPlayer::upload_frame(AVFrame *frame)
{
    std::unique_lock<std::mutex> gl_lock(gl_mutex);
    if (!frame || frame->width != video_codec_ctx->width)
    {
        return;
    }

    // 处理硬件加速表面
    AVFrame *upload_frame = frame;

    // 使用双PBO异步上传
    int next_pbo = (pbo_index + 1) % 2;

    // 上传Y分量
    upload_plane(image_Y, GL_RED, upload_frame->data[0],
                 upload_frame->linesize[0],
                 frame->width, frame->height,
                 pbo_ids[next_pbo], 0);

    // 上传UV分量（处理不同格式）
    switch (upload_frame->format)
    {
    case AV_PIX_FMT_YUV420P:
        yuv420Shader.Use();
        rendererShader = &yuv420Shader;
        upload_plane(image_U, GL_RED, upload_frame->data[1],
                     upload_frame->linesize[1],
                     frame->width / 2, frame->height / 2,
                     pbo_ids[next_pbo], 1);
        upload_plane(image_V, GL_RED, upload_frame->data[2],
                     upload_frame->linesize[2],
                     frame->width / 2, frame->height / 2,
                     pbo_ids[next_pbo], 2);
        break;
    case AV_PIX_FMT_NV12:
        nv12Shader.Use();
        rendererShader = &nv12Shader;
        upload_plane(image_UV, GL_RG, upload_frame->data[1],
                     upload_frame->linesize[1],
                     frame->width / 2, frame->height / 2,
                     pbo_ids[next_pbo], 1);
        break;
    default:
        throw std::runtime_error("Unsupported pixel format");
    }

    pbo_index= next_pbo;

    if (upload_frame != frame)
    {
        av_frame_free(&upload_frame);
    }

    gl_lock.unlock();
}

// 通用平面上传函数
void VideoPlayer::upload_plane(Texture &tex, GLenum format,
                               uint8_t *data, int line_size,
                               int width, int height,
                               GLuint pbo, GLuint texID_in_shader)
{
    glActiveTexture(GL_TEXTURE0 + texID_in_shader);
    tex.Bind();

    // 映射PBO内存
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    uint8_t *pbo_ptr = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, line_size * height, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

    if(pbo_ptr == nullptr)
    {
        return;
    }

    memcpy(pbo_ptr, data, line_size * height);

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    // 异步上传
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void VideoPlayer::present_frame()
{
    // 执行实际绘制命令
    if (rendererShader)
    {
        renderer(*this->rendererShader);
    }

    // 错误检查
    checkGLError();
}

void VideoPlayer::render_video_frame()
{
    AVFrame *frame = nullptr;

    if (!video_frame_queue.pop(frame, 1000))
    {
        return;
    }

    video_pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(video_time_base);

    std::unique_lock<std::mutex> video_lock(this->video_pts_mutex);
    this->video_cv.wait(video_lock,
    [this]()
    {
        return video_pts <= audio_pts || !running;
    });
    video_lock.unlock(); // 手动释放锁
    // 在同步范围内：立即显示
    upload_frame(&(*frame));
    present_frame();
    av_frame_free(&frame);
}

//=== 解复用线程实现 ===
void VideoPlayer::demux_loop()
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *vframe = av_frame_alloc();
    AVFrame *aFrame = av_frame_alloc();
    AVFrame *dstFrame = nullptr;
    while (running)
    {
        if (av_read_frame(fmt_ctx, packet) < 0)
        {
            break;
        }

        // 分支处理前克隆数据包
        if (packet->stream_index == vIndex)
        {
            int ret = avcodec_send_packet(video_codec_ctx, packet);
            if (ret < 0)
            {
                handle_decode_error(ret);
                continue;
            }

            // 接收解码后的帧
            while (ret >= 0)
            {
                ret = avcodec_receive_frame(video_codec_ctx, vframe);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    handle_decode_error(ret);
                    break;
                }

                // 从GPU显存复制到CPU内存
                if (vframe->format == hw_pix_fmt)
                {
                    // 这里hw_vFrame一定要重新分配内存
                    AVFrame *hw_vFrame = av_frame_alloc();
                    if (av_hwframe_transfer_data(hw_vFrame, vframe, 0) < 0)
                    {
                        std::cerr << "硬件帧转换失败" << std::endl;
                        av_frame_free(&hw_vFrame);
                        continue;
                    }
                    hw_vFrame->pts = vframe->pts;
                    hw_vFrame->pkt_dts = vframe->pkt_dts;
                    hw_vFrame->duration = vframe->duration;
                    hw_vFrame->best_effort_timestamp = vframe->best_effort_timestamp;
                    dstFrame = av_frame_clone(hw_vFrame);
                    av_frame_free(&hw_vFrame);
                }
                else
                {
                    dstFrame = av_frame_clone(vframe);
                }

                video_frame_queue.push(std::move(dstFrame));
            }
        }
        else if (packet->stream_index == aIndex)
        {
            int ret = avcodec_send_packet(this->audio_codec_ctx, packet);

            if (ret < 0)
            {
                continue;
            }

            // 接收解码后的帧
            while (ret >= 0)
            {
                ret = avcodec_receive_frame(this->audio_codec_ctx, aFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    break;
                }

                dstFrame = av_frame_clone(aFrame);

                this->audio_frame_quene.push(std::move(dstFrame));
            }
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    av_frame_free(&vframe);
    av_frame_free(&aFrame);
}

void VideoPlayer::handle_decode_error(int err)
{

    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err);

    std::cerr << "解码错误: " << err_buf << std::endl;
}

// 渲染
void VideoPlayer::renderer(Shader &shader)
{
    // glClear(GL_COLOR_BUFFER_BIT);
    // // 确保在OpenGL上下文线程
    // SDL_GL_MakeCurrent(window, gl_context);

    // 使用同步对象避免过早交换
    static GLsync sync_objects[3] = {0};
    static int sync_index = 0;

    if (sync_objects[sync_index])
    {
        glClientWaitSync(sync_objects[sync_index], 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(sync_objects[sync_index]);
    }
    setFPS(window);
    shader.Use();
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
        isFirst = 0;
    }
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // 插入新的同步对象
    sync_objects[sync_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    sync_index = (sync_index + 1) % 3;

    // 交换缓冲区（带垂直同步）
#ifdef USE_SDL_WINDOW
    SDL_GL_SwapWindow(window);
#elif defined(USE_GLFW_WINDOW)
    glfwSwapBuffers(window);
#endif
}

int VideoPlayer::initResource()
{
    int ret = 0;
#if defined(USE_SDL_WINDOW) || defined(USE_SDL_AUDIO)
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        ret = -1;
        goto FAIL;
    }
#endif
#ifdef USE_SDL_WINDOW
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // 创建SDL 720
    window = SDL_CreateWindow("Media Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window)
    {
        fprintf(stderr, "\nSDL: could not set video mode:%s - exiting\n", SDL_GetError());
        ret = -2;
        goto FAIL;
    }
    gl_context = SDL_GL_CreateContext(window);
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("ERROR::GLAD failed to load the proc\n");
        ret = -3;
        goto FAIL;
    }
    // 使能垂直同步
    SDL_GL_SetSwapInterval(1);

#elif defined(USE_GLFW_WINDOW)

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // glfwWindowHint(GLFW_RESIZABLE, false);
    window = glfwCreateWindow(2560, 1440, "Media Player", NULL, NULL);
    if (window == NULL)
    {
        printf("failed to create window\n");
        glfwTerminate();
        ret = -2;
        goto FAIL;
    }
    // // 获取主显示器的视频模式
    // const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    // // 切换到全屏模式
    // glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("ERROR::GLAD failed to load the proc\n");
        ret = -3;
        goto FAIL;
    }
    glfwSetFramebufferSizeCallback(window, framesizecallback);
#endif

    // 编译着色器
    nv12Shader = ResourceManager::loadShader("../shader/nv12.vert", "../shader/nv12.frag", NULL, "nv12");
    nv12Shader.Use().unfm1i("y_tex", 0);
    nv12Shader.unfm1i("uv_tex", 1);
    yuv420Shader = ResourceManager::loadShader("../shader/yuv420.vert", "../shader/yuv420.frag", NULL, "yuv420");
    yuv420Shader.Use().unfm1i("y_tex", 0);
    yuv420Shader.unfm1i("u_tex", 1);
    yuv420Shader.unfm1i("v_tex", 2);

    // 初始化FFmpeg
    if(init_ffmpeg(this->filename)<0)
    {
        ret = -4;
        goto FAIL;
    }
    // 初始化纹理
    init_gl_resources(video_codec_ctx->width, video_codec_ctx->height);
FAIL:
    return ret;
}

//=============== 主事件循环 ===============
void VideoPlayer::run()
{
    // 创建工作线程
    std::thread demux_thread([this]
                             { demux_loop(); });
    // std::thread au_demux_thread(au_demux_loop, this->filename, this);

    audio_play(this);

    // 主渲染循环
    while (running)
    {
        // 处理窗口事件
#ifdef USE_SDL_WINDOW
        SDL_Event event;
        SDL_PollEvent(&event);
#elif defined(USE_GLFW_WINDOW)
        glfwPollEvents();
#endif

#ifdef USE_SDL_WINDOW
        if (event.type == SDL_QUIT)
        {
            running = false;
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                glViewport(0, 0, event.window.data1, event.window.data2);
        }
#elif defined(USE_GLFW_WINDOW)
        if (glfwWindowShouldClose(window))
        {
            running = false;
            break;
        }
#endif

        // 执行视频渲染
        render_video_frame();

#ifdef USE_FMOD_AUDIO
        sound.update();
#endif
    }

    // 等待线程结束
    running = false;
    this->audio_frame_quene.clear();
    this->video_frame_queue.clear();
    demux_thread.join();
    // au_demux_thread.join();
    // video_thread.join();
    avformat_free_context(this->fmt_ctx);
    swr_free(&this->audio_ctx_.swr_ctx);
    avcodec_free_context(&this->audio_codec_ctx);
    avcodec_free_context(&this->video_codec_ctx);
#ifdef USE_SDL_AUDIO
    SDL_CloseAudio();
#endif
#if defined(USE_SDL_WINDOW) || defined(USE_SDL_AUDIO)
    SDL_Quit();
#endif
}
