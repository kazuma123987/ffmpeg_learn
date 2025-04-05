#include "video_player.h"
#include <array>
//=============== 初始化SDL和OpenGL ===============

// 初始化硬件解码器
static AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

std::mutex decode_mux;

static bool hwDecode = true;

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

void VideoPlayer::init_ffmpeg(const char *customPath)
{
    // 为format上下文分配空间
    fmt_ctx = avformat_alloc_context();
    // 打开视频文件
    if (avformat_open_input(&fmt_ctx, customPath, NULL, NULL))
    {
        printf("failed to open the video:%s\n", customPath);
        return;
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
        return;
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
    this->audio_ctx_.out_sample_rate = 48000;
    if (audio_codec_ctx->sample_rate > this->audio_ctx_.out_sample_rate)
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
}

void VideoPlayer::upload_frame(AVFrame *frame)
{
    std::unique_lock<std::mutex> gl_lock(gl_mutex);
    if (!frame || frame->width != video_codec_ctx->width)
    {
        return;
    }

    // // 检查帧格式变化
    // if (last_uploaded_frame &&
    //     (frame->width != last_uploaded_frame->width ||
    //      frame->height != last_uploaded_frame->height ||
    //      frame->format != last_uploaded_frame->format))
    // {
    //     init_gl_resources(frame->width, frame->height); // TODO:之前的纹理的清理
    // }

    // 处理硬件加速表面
    AVFrame *upload_frame = frame;

    // 使用双PBO异步上传
    int next_pbo = (pbo_index.load() + 1) % 2;

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

    pbo_index.store(next_pbo);

    if (upload_frame != frame)
    {
        av_frame_free(&upload_frame);
    }
    last_uploaded_frame = frame;
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

    memcpy(pbo_ptr, data, line_size * height);

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    // 异步上传
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void VideoPlayer::audio_callback(void *userdata, Uint8 *stream, int len)
{
    if (len == 0)
        return;
    VideoPlayer *player = static_cast<VideoPlayer *>(userdata);
    auto &ac = player->audio_ctx_;

    // 清空目标缓冲区
    SDL_memset(stream, 0, len);
    // 从队列获取音频帧
    AVFrame *frame = nullptr;
    if (!player->audio_frame_quene.pop(frame, 5))
    { // 5ms超时
        return;
    }

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

void VideoPlayer::present_frame()
{
    // 执行实际绘制命令
    if (rendererShader)
        renderer(*this->rendererShader);

    // 错误检查
    checkGLError();
}

void VideoPlayer::update_sync_stats(double diff, double render_time)
{
    static std::vector<double> diffs;
    diffs.push_back(diff);

    stats_.total_frames++;
    stats_.max_diff = std::max(stats_.max_diff, fabs(diff));

    // 计算移动平均和标准差
    constexpr double alpha = 0.1;
    stats_.avg_diff = alpha * fabs(diff) + (1 - alpha) * stats_.avg_diff;

    double sum_sq = 0.0;
    for (auto d : diffs)
        sum_sq += d * d;
    stats_.stddev_diff = sqrt(sum_sq / diffs.size() - stats_.avg_diff * stats_.avg_diff);

    // 保留最近1000个样本
    if (diffs.size() > 1000)
        diffs.erase(diffs.begin());
}

// 时钟漂移补偿
void VideoPlayer::update_clock_prediction(double last_diff)
{
    static std::deque<double> diff_history;
    constexpr size_t HISTORY_SIZE = 60; // 保留1秒历史（假设60fps）

    // 维护历史记录
    diff_history.push_back(last_diff);
    if (diff_history.size() > HISTORY_SIZE)
    {
        diff_history.pop_front();
    }

    // 使用线性回归预测漂移趋势
    double sum_x = 0.0, sum_y = 0.0, sum_xx = 0.0, sum_xy = 0.0;
    for (size_t i = 0; i < diff_history.size(); ++i)
    {
        sum_x += i;
        sum_y += diff_history[i];
        sum_xx += i * i;
        sum_xy += i * diff_history[i];
    }

    const double n = diff_history.size();
    const double denominator = n * sum_xx - sum_x * sum_x;
    if (denominator != 0)
    {
        const double slope = (n * sum_xy - sum_x * sum_y) / denominator;
        sync_clock.update_drift_compensation(slope * 0.05); // 应用衰减后的斜率
    }
}

// 平均渲染时间
double VideoPlayer::calculate_avg_render_time() const
{
    constexpr size_t WINDOW_SIZE = 10;
    static std::array<double, WINDOW_SIZE> render_times;
    static size_t index = 0;

    // 获取当前时间点
    static auto lastTime = std::chrono::high_resolution_clock::now();
    // 将时间点转换为自纪元（epoch）以来的纳秒数
    double last_render_time_ = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - lastTime).count();
    lastTime = std::chrono::high_resolution_clock::now();

    // 更新环形缓冲区
    render_times[index] = last_render_time_;
    index = (index + 1) % WINDOW_SIZE;

    // 计算加权平均（最近帧权重更高）
    double sum = 0.0;
    double weight_sum = 0.0;
    for (size_t i = 0; i < WINDOW_SIZE; ++i)
    {
        double weight = 1.0 / (1.0 + i); // 指数衰减权重
        sum += render_times[(index + i) % WINDOW_SIZE] * weight;
        weight_sum += weight;
    }
    return sum / weight_sum;
}

void VideoPlayer::render_video_frame()
{
    constexpr double MAX_AHEAD = 0.001;      // 最大允许提前10ms
    constexpr double MAX_BEHIND = 0.100;     // 最大允许落后100ms
    constexpr double SYNC_THRESHOLD = 0.020; // 同步阈值20ms

    double audio_clock = sync_clock.get_master_clock();
    double diff = last_video_pts_ - audio_clock;

    // 动态调整阈值（基于最近10帧的渲染性能）
    double avg_render_time = calculate_avg_render_time();
    double dynamic_threshold = std::clamp(
        SYNC_THRESHOLD + avg_render_time * 2.0,
        0.005, // 最小5ms
        0.050  // 最大50ms
    );

    if (diff > MAX_AHEAD)
    {
        // 超前过多：精确等待
        double sleep_time = diff;
        precise_sleep(sleep_time);
        return;
    }

    AVFrame *frame = nullptr;
    if (!video_frame_queue.pop(frame, 1000))
    {
        return;
    }

    if (diff < -MAX_BEHIND)
    {
        // 严重落后：直接丢弃
        stats_.dropped_frames++;
        // av_frame_free(&frame);
        // return;
    }
    else if (diff < -dynamic_threshold)
    {
        // 轻微落后：显示但记录
        stats_.late_frames++;
        upload_frame(&(*frame));
        present_frame();
    }
    else
    {
        // 在同步范围内：立即显示
        upload_frame(&(*frame));
        present_frame();
    }

    // // 更新时钟预测模型
    // update_clock_prediction(diff);
    last_video_pts_ = frame->pts * av_q2d(video_time_base);
    av_frame_free(&frame);
}

//=== 解复用线程实现 ===
void VideoPlayer::demux_loop()
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *vframe = av_frame_alloc();
    AVFrame *hw_vFrame = av_frame_alloc();
    AVFrame *aFrame = av_frame_alloc();
    AVFrame *dstFrame = NULL;
    while (running)
    {
        if (av_read_frame(fmt_ctx, packet) < 0)
            break;

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

                // 处理硬件加速表面
                if (hwDecode)
                {
                    // 从GPU显存复制到CPU内存
                    if (av_hwframe_transfer_data(hw_vFrame, vframe, 0) < 0)
                    {
                        std::cerr << "硬件帧转换失败" << std::endl;
                        continue;
                    }
                    dstFrame = av_frame_clone(hw_vFrame);
                    dstFrame->pts = vframe->pts;
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
                handle_decode_error(ret);
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
                    handle_decode_error(ret);
                    break;
                }

                dstFrame = av_frame_clone(aFrame);

                audio_frame_quene.push(std::move(dstFrame));
            }
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    av_frame_free(&vframe);
    av_frame_free(&hw_vFrame);
    av_frame_free(&aFrame);
}

// void VideoPlayer::video_decode_loop()
// {
//     AVPacket *raw_pkt = av_packet_alloc();
//     AVFrame *hw_frame = av_frame_alloc();
//     AVFrame *sw_frame = av_frame_alloc();
//     int ret = 0;

//     while (running)
//     {
//         // 带超时的包获取（防止无限阻塞）
//         AVPacket *pkt = nullptr;
//         if (!video_packet_queue.pop(pkt, 5))
//         { // 50ms超时
//             continue;
//         }

//         // 发送数据包到解码器
//         // std::lock_guard<std::mutex> lock(decode_mux);
//         ret = avcodec_send_packet(video_codec_ctx, pkt);
//         if (ret < 0)
//         {
//             handle_decode_error(ret);
//             continue;
//         }

//         // 接收解码后的帧
//         while (ret >= 0)
//         {
//             ret = avcodec_receive_frame(video_codec_ctx, hw_frame);
//             if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//             {
//                 break;
//             }
//             else if (ret < 0)
//             {
//                 handle_decode_error(ret);
//                 break;
//             }

//             // 处理硬件加速表面
//             if (hw_frame->format == hw_pix_fmt)
//             {
//                 // 从GPU显存复制到CPU内存
//                 if (av_hwframe_transfer_data(sw_frame, hw_frame, 0) < 0)
//                 {
//                     std::cerr << "硬件帧转换失败" << std::endl;
//                     continue;
//                 }
//                 sw_frame->pts = hw_frame->pts;
//             }
//             else
//             {
//                 sw_frame = av_frame_clone(hw_frame);
//             }

//             // 将帧放入渲染队列
//             if (sw_frame && this->video_codec_ctx->width == sw_frame->width && !video_frame_queue.push(ScopedAVFrame(sw_frame)))
//             {
//                 // ++m_stats.dropped_frames_queue_full;
//                 av_frame_unref(sw_frame);
//             }

//             av_frame_unref(hw_frame);
//         }

//         // // 定期更新解码器状态
//         // if (++m_decode_counter % 100 == 0)
//         // {
//         //     check_decoder_health();
//         // }
//     }

//     // 刷新解码器
//     avcodec_send_packet(video_codec_ctx, nullptr);
//     while (ret >= 0)
//     {
//         ret = avcodec_receive_frame(video_codec_ctx, hw_frame);
//         if (ret == AVERROR_EOF)
//             break;
//         // 处理剩余帧...
//     }

//     // 清理资源
//     av_packet_free(&raw_pkt);
//     av_frame_free(&hw_frame);
//     av_frame_free(&sw_frame);
//     av_buffer_unref(&video_codec_ctx->hw_device_ctx);
// }

void VideoPlayer::handle_decode_error(int err)
{
    constexpr int MAX_ERROR_COUNT = 10;
    static int error_count = 0;

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

    // // 使用同步对象避免过早交换
    // static GLsync sync_objects[3] = {0};
    // static int sync_index = 0;

    // if (sync_objects[sync_index])
    // {
    //     glClientWaitSync(sync_objects[sync_index], 0, GL_TIMEOUT_IGNORED);
    //     glDeleteSync(sync_objects[sync_index]);
    // }
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

    // // 插入新的同步对象
    // sync_objects[sync_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    // sync_index = (sync_index + 1) % 3;

    // 交换缓冲区（带垂直同步）
    SDL_GL_SwapWindow(window);
}

//=============== 主事件循环 ===============
void VideoPlayer::run()
{

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // 创建SDL 720
    window = SDL_CreateWindow("Media Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window)
    {
        fprintf(stderr, "\nSDL: could not set video mode:%s - exiting\n", SDL_GetError());
        exit(1);
    }
    gl_context = SDL_GL_CreateContext(window);
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("ERROR::GLAD failed to load the proc\n");
        return;
    }
    // 使能垂直同步
    SDL_GL_SetSwapInterval(1);

    // 编译着色器
    nv12Shader = ResourceManager::loadShader("../shader/nv12.vert", "../shader/nv12.frag", NULL, "nv12");
    nv12Shader.Use().unfm1i("y_tex", 0);
    nv12Shader.unfm1i("uv_tex", 1);
    yuv420Shader = ResourceManager::loadShader("../shader/yuv420.vert", "../shader/yuv420.frag", NULL, "yuv420");
    yuv420Shader.Use().unfm1i("y_tex", 0);
    yuv420Shader.unfm1i("u_tex", 1);
    yuv420Shader.unfm1i("v_tex", 2);

    // 初始化FFmpeg
    init_ffmpeg(this->filename);
    // 初始化纹理
    init_gl_resources(video_codec_ctx->width, video_codec_ctx->height);

    // 创建工作线程
    std::thread demux_thread([this]
                             { demux_loop(); });
    // std::thread video_thread([this]
    //                          { video_decode_loop(); });

    // SDL音频参数
    SDL_AudioSpec spec;
    spec.freq = this->audio_ctx_.out_sample_rate;
    spec.channels = audio_codec_ctx->ch_layout.nb_channels;
    spec.silence = 0;
    spec.samples = this->audio_ctx_.out_nb_samples;
    spec.callback = audio_callback;
    spec.userdata = this; // 这里要使用
    switch (this->audio_ctx_.out_sample_fmt)
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
    this->audio_ctx_.sdl_audio_format = spec.format;
    // 打开SDL音频
    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        throw std::runtime_error("无法打开音频设备");
    }
    SDL_PauseAudio(0);

    // 主渲染循环
    SDL_Event event;
    double last_render_time;
    int target_fps = (float)fmt_ctx->streams[vIndex]->avg_frame_rate.num / fmt_ctx->streams[vIndex]->avg_frame_rate.den + 0.5f;
    while (running)
    {
        // 处理窗口事件
        SDL_PollEvent(&event);
        // {
        if (event.type == SDL_QUIT)
        {
            running = false;
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                glViewport(0, 0, event.window.data1, event.window.data2);
        }
        // }

        // 执行视频渲染
        render_video_frame();
    }

    // 等待线程结束
    running = false;
    demux_thread.join();
    // video_thread.join();
    SDL_CloseAudio();
    SDL_Quit();
}
