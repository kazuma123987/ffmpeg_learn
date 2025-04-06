#include "audio_function.h"

static AVCodecContext *pVideoCodecCtx_live = 0; // ffmpeg编码视频上下文pVideoCodecCtx_live

const int time_wait = 5;	   // 防止访问过快，服务器不发数据了
const int master_delay = 500;  // 直播启动时先主动延迟
const int too_fast_wait = 500; // 如果缓冲很不够了，强制停顿
bool first_wait = false;

void audio_callback_live(void *userdata, Uint8 *stream, int len) // 音频回调函数，SDL_OpenAudio使用后会无限循环使用该函数，直到使用SDL_closeAudio，如果不close则SDL_Quit无限阻塞
{

	if (!first_wait)
	{
		Sleep(master_delay + 1000);
		first_wait = true;
		audio_pace = -1;
	}
	if (audio_output_over && read_this_buf->bufsize == 0)
		return; // 如果宣布结束了,且音频播放完了,返回

	while (read_this_buf->bufsize <= 0)
		Sleep(too_fast_wait); // 如果该缓冲块为空，一直等待

	if (video_output_over)
	{
		if ((audio_pace + 2) > decode_audio_pace)
			Sleep(too_fast_wait);
	} // 如果缓冲很不够了，强制停顿
	else
	{
		if ((video_pace + 2) > decode_video_pace)
			Sleep(too_fast_wait);
	} // 如果缓冲很不够了，强制停顿//视频播放播完了，可能是mp3电台节目

	if ((audio_pace + 7) > decode_audio_pace)
		Sleep(5); // 如果缓冲不够了，稍微做出看不出的停顿

	audio_pace = read_this_buf->pts + double(read_this_buf->gap) * (double(read_this_buf->read_buf_index) / double(read_this_buf->bufsize)); ////更新音频播放到的时间

	bool show = false;
	if ((audio_pace < (video_pace - 4)) && (audio_pace > (video_pace - 20))) // 如果是声音追赶画面
		while (audio_pace < video_pace - 2)
		{
			if (!show)
			{
				show = true;
				cout << "开始跳\n";
			}
			SDL_memset(stream, 0, len);
			read_this_buf->bufsize = 0;
			read_count++;
			if (read_count >= bufnumber)
				read_count = 0;

			while (read_this_buf->bufsize == 0)
				Sleep(500); // 如果该缓冲为空，等待

			read_this_buf = &audio_buf[read_count];
			read_this_buf->read_buf_index = 0;
			audio_pace = read_this_buf->pts;
		}
	if (show)
		cout << "跳完了\n";

	// memcpy(stream, audiobuf[read_count]+audio_buf_read_index[read_count], len);//将自己携带信息进行播放,据说memcpy这样直接做会造成失真，用以下两句代替SDL_memset,SDL_MixAudio
	SDL_memset(stream, 0, len);
	if (len + read_this_buf->read_buf_index > single_buf_size)
	{
		cout << "炸了\n";
		audio_output_over = true;
		return;
	}
	if ((len + read_this_buf->read_buf_index) <= read_this_buf->bufsize)
	{
		if ((audio_pace + 2) > decode_audio_pace)
			next_write = true; // 如果还有两秒就到解码缓冲停顿的时间了，发送“解码缓冲开启，继续写”信号

		SDL_MixAudio(stream, read_this_buf->audio_info + read_this_buf->read_buf_index, len, SDL_MIX_MAXVOLUME); // 核心功能句，将帧中解析出来的数据流输入stream并输出
		read_this_buf->read_buf_index += len;																	 // 确定下一段要播放的信息
		if (read_this_buf->read_buf_index == read_this_buf->bufsize)											 // 如果刚好读完，切换内存块
		{
			read_this_buf->bufsize = 0;
			read_count++;
			if (read_count >= bufnumber)
				read_count = 0; // 如果已经是最大内存块了，重新读取最初的内存块
			read_this_buf = &audio_buf[read_count];
			read_this_buf->read_buf_index = 0;
		}
	}
	else
	{
		if (len >= single_buf_size)
		{
			cout << "大于single_buf_size\n";
			return;
		}
		uint8_t temp[single_buf_size * 2] = {0};								  // 创建临时缓存块
		int remain_size = read_this_buf->bufsize - read_this_buf->read_buf_index; // 确认当前读取的缓存块剩余大小
		if (remain_size < 0)
		{
			cout << "remain_size" << remain_size << " read_this_buf->read_buf_index" << read_this_buf->read_buf_index << " read_this_buf->bufsize" << read_this_buf->bufsize << endl;
			Sleep(500);
			return;
		}
		memcpy(temp, read_this_buf->audio_info + read_this_buf->read_buf_index, remain_size); // 将剩余部分内容copy到新创建的临时缓存块里
		read_this_buf->bufsize = 0;
		read_this_buf->read_buf_index = 0; // 该缓存块写入偏移量置0，写服务切换内存块后，切换前的内存块写入偏移量不置0，在这里置0
		read_count++;
		if (read_count >= bufnumber)
			read_count = 0; // 如果已经是最大内存块了，重新读取最初的内存块
		read_this_buf = &audio_buf[read_count];
		memcpy(temp + remain_size, read_this_buf->audio_info, len - remain_size); // 读取新内存块的剩余部分量，即len-remain_size
		SDL_MixAudio(stream, temp, len, SDL_MIX_MAXVOLUME);						  // 将临时缓存块的内容输入音频输出流
		if (write_audio_buff_over && read_this_buf->write_buf_index == 0)
		{
			audio_output_over = true;
			cout << "结束了\n";
		} // 如果音频没帧了且下一块内存内容为空，宣布输出彻底结束output_over=true
		read_this_buf->read_buf_index = len - remain_size; // 确定下一段要播放的信息
	}
}
void process_video_function()
{
	Sleep(master_delay);
	struct SwsContext *pSwsContext = 0;		   // ffmpeg编码数据格式转换
	int numBytes = 0;						   // 解码后的数据长度
	unsigned char *outBuffer = 0;			   // 解码后的数据存放缓存区
	AVFrame *pAVFrameRGB32 = av_frame_alloc(); // ffmpeg单帧缓存转换颜色空间后的缓存
	double fps = pVStream->avg_frame_rate.num * 1.0f / pVStream->avg_frame_rate.den;
	if (fps <= 0)
	{
		cout << fps << endl;
		return;
	} // 帧率
	double interval = 1 * 1000 / fps; // 帧间隔

	// 步骤七：对拿到的原始数据格式进行缩放转换为指定的格式高宽大小  AV_PIX_FMT_YUV420P        pVideoCodecCtx_live->pix_fmt AV_PIX_FMT_RGBA
	pSwsContext = sws_getContext(pVideoCodecCtx_live->width, pVideoCodecCtx_live->height, pVideoCodecCtx_live->pix_fmt, pVideoCodecCtx_live->width, pVideoCodecCtx_live->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
	numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pVideoCodecCtx_live->width, pVideoCodecCtx_live->height, 1);
	outBuffer = (unsigned char *)av_malloc(numBytes);
	av_image_fill_arrays(pAVFrameRGB32->data, pAVFrameRGB32->linesize, outBuffer, AV_PIX_FMT_YUV420P, pVideoCodecCtx_live->width, pVideoCodecCtx_live->height, 1); // pAVFrame32的data指针指向了outBuffer
	CoInitialize(NULL);																																			   // SDL操作，线程无法直接使用主线程show_moive()中初始化后的SDL，加了这句才能使用主线程里初始化后的SDL，下面那句才不会报错
	SDL_Window *pSDLWindow = 0;
	SDL_Renderer *pSDLRenderer = 0;
	SDL_Surface *pSDLSurface = 0;
	SDL_Texture *pSDLTexture = 0;
	SDL_Event event;

	if (pVideoCodecCtx_live->width > my_width)
		set_width = my_width;
	else
		set_width = pVideoCodecCtx_live->width; // 设置sdl屏幕长宽
	if (pVideoCodecCtx_live->height > my_height)
		set_height = my_height;
	else
		set_height = pVideoCodecCtx_live->height;

	pSDLWindow = SDL_CreateWindow("ZasLeonPlayer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, set_width, set_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // 设置显示框大小
	if (!pSDLWindow)
	{
		cout << "Failed SDL_CreateWindow\n";
		return;
	}
	pSDLRenderer = SDL_CreateRenderer(pSDLWindow, -1, 0);
	if (!pSDLRenderer)
	{
		cout << "Failed SDL_CreateRenderer\n";
		return;
	}
	pSDLTexture = SDL_CreateTexture(pSDLRenderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, pVideoCodecCtx_live->width, pVideoCodecCtx_live->height);
	if (!pSDLTexture)
	{
		cout << "Failed SDL_CreateTexture\n";
		return;
	}

	int read_v_count = 0;
	video_output_over = false;
	write_video_buff_over = false;
	while (true)
	{
		if (write_video_buff_over && video_pace >= decode_video_pace)
			break; // 如果视频写服务结束了，且播放完最后一帧画面了，退出解包死循环

		while (video_pace + 2 > decode_video_pace && !write_video_buff_over)
		{
			Sleep(4000);
		} // 如果还没接受到足够多的数据，进行等待

		AVFrame *pAVFrame = vframe[read_v_count];
		{ // 显示该帧图像
			sws_scale(pSwsContext, (const uint8_t *const *)pAVFrame->data, pAVFrame->linesize, 0, pVideoCodecCtx_live->height, pAVFrameRGB32->data, pAVFrameRGB32->linesize);

			SDL_UpdateYUVTexture(pSDLTexture, NULL, pAVFrame->data[0], pAVFrame->linesize[0], pAVFrame->data[1], pAVFrame->linesize[1], pAVFrame->data[2], pAVFrame->linesize[2]);

			SDL_RenderClear(pSDLRenderer);
			// Texture纹理复制到渲染器Renderer
			SDL_Rect sdlRect;
			sdlRect.x = 0;
			sdlRect.y = 0;
			if (pAVFrame->width > my_width)
				sdlRect.w = my_width;
			else
				sdlRect.w = pAVFrame->width; // 更新显示框大小
			if (pAVFrame->height > my_height)
				sdlRect.h = my_height;
			else
				sdlRect.h = pAVFrame->height;

			SDL_RenderCopy(pSDLRenderer, pSDLTexture, 0, &sdlRect);
			SDL_RenderPresent(pSDLRenderer); // 更新Renderer显示
			SDL_PollEvent(&event);			 // 事件处理

			video_pace = double(pAVFrame->best_effort_timestamp) * av_q2d(pVStream->time_base);
			cout << audio_pace << "\t" << video_pace << "\t" << decode_audio_pace << "\t" << decode_video_pace << endl;
		} // 该帧图形显示结束
		// av_frame_free(&pAVFrame);
		av_frame_unref(vframe[read_v_count]);

		read_v_count++;
		if (read_v_count >= bufnumber)
			read_v_count = 0; // 指定到下一个pvpacket视频包进行解码

		while (video_pace > audio_pace && !audio_output_over)
		{
			SDL_PollEvent(&event);
			Sleep(5);
		} // 防止话音不同步，靠死循环等待来同步.这条语句代替了"原实现播放延迟代码"

		if (audio_output_over)
		{
			cout << "s" << int(interval) << endl;
			Sleep(int(interval / float(1000)));
		} // 如果音频播完了视频没播完，正常速度继续播
	} // 视频解码彻底完成，死循环结束
	video_output_over = true;		   // 宣布播放完毕
	SDL_DestroyRenderer(pSDLRenderer); // 销毁渲染器
	SDL_DestroyWindow(pSDLWindow);
	cout << "即将执行SDL_Quit()\n"; // 销毁窗口
}

void show_moive_alive() // 读取网络直播视频
{
	AVFormatContext *pAVFormatContext = avformat_alloc_context(); // ffmpeg的全局上下文，所有ffmpeg操作都需要
	video_pace = -1;
	// ffmpeg相关变量预先定义与分配
	const AVCodec *vCodec = nullptr;		   // ffmpeg编码器
	AVFrame *pAVFrameRGB32 = av_frame_alloc(); // ffmpeg单帧缓存转换颜色空间后的缓存
	struct SwsContext *pSwsContext = 0;		   // ffmpeg编码数据格式转换
	AVDictionary *pAVDictionary = 0;		   // ffmpeg数据字典，用于配置一些编码器属性等
	int ret = 0;							   // 函数执行结果
	char buf[1024] = {0};					   // 函数执行错误后返回信息的缓存
	int videoIndex = -1;					   // 视频流所在的序号
	int numBytes = 0;						   // 解码后的数据长度
	unsigned char *outBuffer = 0;			   // 解码后的数据存放缓存区
	SYSTEMTIME startTime, nextTime;			   // 解码前时间//解码完成时间
	int VideoTimeStamp = 0;					   // 时间戳，防止视频没有自带的pts

	if (!pAVFormatContext || !pAVFrameRGB32)
	{
		cout << "Failed to alloc";
		return;
	}
	//---------------------------------------------------------SDL相关变量预先定义----------------------------------------------------------------
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		cout << "Failed SDL_Init\n";
		return;
	} // 初始化SDL
	//---------------------------------------------------------SDL定义结束------------------------------------------------------------------------

	// 步骤一：注册所有容器和编解码器（也可以只注册一类，如注册容器、注册编码器等）

	avformat_network_init();
	// 步骤二：打开文件(ffmpeg成功则返回0)
	cout << "打开:" << rtspUrl << endl;
	if (avformat_open_input(&pAVFormatContext, rtspUrl, NULL, &pAVDictionary))
	{
		av_strerror(ret, buf, 1024);
		cout << "打开失败,错误编号:" << ret;
		return;
	}
	// 步骤三：探测流媒体信息
	if (avformat_find_stream_info(pAVFormatContext, 0) < 0)
	{
		cout << "avformat_find_stream_info失败\n";
		return;
	}

	//------------------------------------------------视频配置开始---------------------------------------

	for (int i = 0; i < bufnumber; i++)
		vframe[i] = av_frame_alloc();
	double fps;
	double interval;																			   // 画面帧间隔时间
	videoIndex = av_find_best_stream(pAVFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &vCodec, NULL); // 步骤四：提取流信息,提取视频信息
	if (videoIndex < 0)																			   // 没有视频，查询是否有音频
	{
		video_output_over = true;
		cout << "视频没找到!\n";
	}
	else
	{
		// 步骤五：对找到的视频流寻解码器
		pVideoCodecCtx_live = avcodec_alloc_context3(vCodec);
		avcodec_parameters_to_context(pVideoCodecCtx_live, pAVFormatContext->streams[videoIndex]->codecpar);
		pVStream = pAVFormatContext->streams[videoIndex];
		pVideoCodecCtx_live->pkt_timebase = pAVFormatContext->streams[videoIndex]->time_base;

		// 步骤六：打开解码器
		av_dict_set(&pAVDictionary, "probesize", "4", 0);
		av_dict_set(&pAVDictionary, "buffer_size", "1024000", 0); // 设置缓存大小 1024000byte
		av_dict_set(&pAVDictionary, "stimeout", "2000000", 0);	  // 设置超时时间 20s    20000000
		av_dict_set(&pAVDictionary, "max_delay", "30000000", 0);  // 设置最大延时 3s,30000000
		av_dict_set(&pAVDictionary, "rtsp_transport", "udp", 0);  // 设置打开方式 tcp/udp

		if (avcodec_open2(pVideoCodecCtx_live, vCodec, &pAVDictionary))
		{
			cout << "avcodec_open2 failed\n";
			return;
		}

		// 显示视频相关的参数信息（编码上下文）
		cout << "比特率:" << pVideoCodecCtx_live->bit_rate << endl;
		cout << "总时长:" << pVStream->duration * av_q2d(pVStream->time_base) << "秒(" << float(pVStream->duration * av_q2d(pVStream->time_base) / 60.0) << "分钟)\n";
		cout << "总帧数:" << pVStream->nb_frames << endl;
		cout << "格式:" << pVideoCodecCtx_live->pix_fmt << endl; // 格式为0说明是AV_PIX_FMT_YUV420P
		cout << "宽:" << pVideoCodecCtx_live->width << "\t高：" << pVideoCodecCtx_live->height << endl;
		cout << "文件分母:" << pVideoCodecCtx_live->time_base.den << "\t文件分子:" << pVideoCodecCtx_live->time_base.num << endl;
		cout << "帧率分母:" << pVStream->avg_frame_rate.den << "\t帧率分子:" << pVStream->avg_frame_rate.num << endl;
		// 有总时长的时候计算帧率（较为准确）
		fps = pVStream->avg_frame_rate.num * 1.0f / pVStream->avg_frame_rate.den;
		if (fps <= 0)
		{
			cout << fps << endl;
			return;
		} // 帧率
		interval = 1 * 1000 / fps; // 帧间隔
		cout << "平均帧率:" << fps << endl;
		cout << "帧间隔:" << interval << "ms" << endl;
	}
	//------------------------------------------------视频配置结束---------------------------------------

	//------------------------------------------------音频配置开始-----------------------------------------------

	audio_pace = 0;
	decode_audio_pace = 0;		   // 初始速度默认为0
	AudioIndex = -1;			   // 初始没找到音频检索
	write_audio_buff_over = false; // 当前音频输出没有结束
	next_write = false;
	next_read = false;		   // 当前不切换内存写入音频数据
	audio_output_over = false; // 当前输出等待列队没输出完
	for (int i = 0; i < bufnumber; i++)
	{
		memset(audio_buf[i].audio_info, 0, sizeof(audio_buf[i].audio_info)); // 清空所有内存块内容
		audio_buf[i].read_buf_index = 0;
		audio_buf[i].write_buf_index = 0; // 当前读写内存块偏移量置0
		audio_buf[i].bufsize = 0;
		audio_buf[i].pts = 0; // dts和内存块写入大小置为0
	}
	write_this_buf = &audio_buf[0]; // 从第0块内存开始写
	read_this_buf = &audio_buf[0];	// 从第0块内存开始读
	read_count = 0;
	write_count = 0; // 音频的读写内存块都设置为第一块，往第一块里读，往第一块里写

	AVCodecContext *pAudioCodecCtx;			   // 音频上下文
	AVStream *pAStream;						   // 音流
	const AVCodec *pAudioCodec;					   // 音频解码器
	SDL_AudioSpec desired_spec, obtained_spec; // SDL，desired_spec希望定制输出的声音配置 与 obtained_spec获得原始数据的声音配置

	AudioIndex = av_find_best_stream(pAVFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &vCodec, NULL);
	if (AudioIndex < 0)
	{
		audio_output_over = true;
		cout << "无音频\n";
		if (videoIndex < 0 && AudioIndex < 0)
		{
			cout << "视频音频全没找到!\n";
			return;
		}
	}
	else // 找到音频索引了，进行音频配置
	{
		pAudioCodecCtx = avcodec_alloc_context3(NULL); // 此两句话可用老版本概括：AVCodecContext *pAudioCodecCtx = pAFormatContext->streams[audioIndex]->codec;
		avcodec_parameters_to_context(pAudioCodecCtx, pAVFormatContext->streams[AudioIndex]->codecpar);
		pAStream = pAVFormatContext->streams[AudioIndex];
		pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);					 // 找到适合的解码器
		pAudioCodecCtx->pkt_timebase = pAVFormatContext->streams[AudioIndex]->time_base; // 不加这句，会出现：mp3float：Could not update timestamps for skipped samples
		assert(avcodec_open2(pAudioCodecCtx, pAudioCodec, nullptr) >= 0);

		cout << "音频总帧数:" << pAStream->nb_frames << endl;
		cout << "音频总时长:" << pAStream->duration / 10000.0 << "秒\n";
		cout << "音频采样率:" << pAudioCodecCtx->sample_rate << endl;
		cout << "输出格式:" << AUDIO_F32SYS << endl;
		cout << "声音通道数:" << pAudioCodecCtx->ch_layout.nb_channels << endl;

		if (pAudioCodecCtx->sample_rate <= 0) // mp3貌似pAudioCodecCtx->sample_rate都是0？导致输出的声音很慢很奇怪！
			desired_spec.freq = 44100;		  // 该文件采样率数值的确为0，mp3的音频采样率默认设置为44100
		else
			desired_spec.freq = pAudioCodecCtx->sample_rate * pAudioCodecCtx->ch_layout.nb_channels / 2; // 该文件有它自己的采样率,可以通过变化desired_spec.freq的值来改变播放速度,后面加上*x就是翻x倍

		audio_fre = desired_spec.freq;
		desired_spec.format = AUDIO_F32SYS;
		desired_spec.channels = pAudioCodecCtx->ch_layout.nb_channels; // 双声道单声道,高清是6声道
		desired_spec.silence = 0;
		desired_spec.samples = SDL_AUDIO_BUFFER_SIZE;
		desired_spec.callback = audio_callback_live; // 设置音频回调函数为audio_callback_live()

		assert(SDL_OpenAudio(&desired_spec, &obtained_spec) >= 0); // 开启SDL音频播放
		SDL_PauseAudio(0);
	}

	//------------------------------------------------音频配置结束---------------------------------------

	if (videoIndex >= 0)
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)process_video_function, NULL, NULL, NULL); // 如果有视频，开始视频线程部署并播放

	double last_best_effort_timestamp = 0; // 最后一次有效时间戳（音频处理用）
	int lost_time = 0;					   // 丢失有效时间的帧的次数（音频处理用）
	int write_v_count = 0;				   // 写哪一块视频内存

	// 步骤八：开始不断读取一帧数据的数据包
	while (true)
	{
		AVPacket *pPacket = av_packet_alloc();
		GetLocalTime(&startTime); // 获取解码前精确时间
		if (av_read_frame(pAVFormatContext, pPacket) < 0)
			break;

		if (pPacket->stream_index == videoIndex) // 如果当前数据包是视频包，将视频包存入视频包缓存，等待要播放该帧所在包时再解码
		{
			double dts = double(pPacket->dts);
			double timebase = av_q2d(pVStream->time_base);
			decode_video_pace = timebase * dts; // 更新当前解码视频到那个时间点

			avcodec_send_packet(pVideoCodecCtx_live, pPacket);
			while (avcodec_receive_frame(pVideoCodecCtx_live, vframe[write_v_count]) >= 0) // 解码开始
			{
				write_v_count++;
				if (write_v_count >= bufnumber)
					write_v_count = 0;
			}

			Sleep(time_wait);
		}

		if (pPacket->stream_index == AudioIndex) // 如果当前包内信息为音频信息，解码
		{
			avcodec_send_packet(pAudioCodecCtx, pPacket);
			AVFrame *frame = av_frame_alloc();						  // 分配帧空间
			while (avcodec_receive_frame(pAudioCodecCtx, frame) >= 0) // 解码开始
			{
				int uSampleSize = 4;
				int data_size = uSampleSize * pAudioCodecCtx->ch_layout.nb_channels * frame->nb_samples; // 确认播放信息字节数大小
				audio_framesize = data_size;
				write_this_buf->bufsize = data_size;  // 当前内存块数据量大小为data_size大小
				if (frame->best_effort_timestamp > 0) // 可能存在帧时间戳丢失的情况（该值为负值），比如直播，每x帧才有一个有效时间戳，拿此来填补之前损失的
				{
					write_this_buf->pts = double(frame->best_effort_timestamp) * av_q2d(pAVFormatContext->streams[AudioIndex]->time_base); // 确定当前有效时间戳
					double gap = (double(write_this_buf->pts) - double(last_best_effort_timestamp)) / double(lost_time + 1);			   // 计算与之前那次有效时间戳的两帧间差距,并计算和前一帧时间差距
					write_this_buf->gap = gap;																							   // 写入该帧时间差
					last_best_effort_timestamp = write_this_buf->pts;																	   // 更新最后一次有效时间戳
					decode_audio_pace = write_this_buf->pts;																			   // 确认读到哪一帧了

					int k = write_count - 1;
					if (k < 0)
						k = bufnumber - 1;									  // 开始往前几帧无时间戳的帧填补pts和gap
					double this_frame_timestamp = last_best_effort_timestamp; // time_fill为此帧时间戳，则前一帧时间戳为time_fill-gap
					while (audio_buf[k].gap == 0 && lost_time > 0)
					{
						audio_buf[k].pts = double(this_frame_timestamp) - double(gap); // 赋予前一帧时间戳
						audio_buf[k].gap = double(gap);								   // 赋予前一帧时间差距
						this_frame_timestamp -= double(gap);						   // 定位此帧时间戳，下个循环再往前一帧补
						lost_time--;												   // 累计丢失量-1，如果丢失量为0了退出循环
						k--;
						if (k < 0)
							k = bufnumber - 1;
					}
					lost_time = 0; // 丢失量清0
				}
				else // 如果该帧时间戳错误，变为最后一帧正常的时间戳
				{
					lost_time++;									  // 丢失量+1
					write_this_buf->pts = last_best_effort_timestamp; // 先将当前时间戳补为最近一次有效时间戳
					write_this_buf->gap = 0;						  // 与后一帧时间差距为0
				}

				for (int i = 0; i < frame->nb_samples; i++) // 向音频缓存写入该帧内容
				{
					for (int j = 0; j < pAudioCodecCtx->ch_layout.nb_channels; j++)
					{
						if (write_this_buf->write_buf_index < write_this_buf->bufsize) // 如果写入缓存有足够剩余
						{
							memcpy(write_this_buf->audio_info + write_this_buf->write_buf_index, frame->data[j] + uSampleSize * i, uSampleSize); // 正常写入全局内存块，等待音频回调函数调用该内存块读取，回调函数读取后自动播放
							write_this_buf->write_buf_index += uSampleSize;																		 // 缓存块写入偏移量增加

							if (write_this_buf->write_buf_index == write_this_buf->bufsize) // 切换内存条
							{
								write_count++;
								if (write_count == bufnumber)
									write_count = 0;
								write_this_buf = &audio_buf[write_count];
								write_this_buf->write_buf_index = 0; // 写入偏移量置0
								memset(write_this_buf->audio_info, 0, sizeof(write_this_buf->audio_info));

								Sleep(time_wait);
							}
						} // else{cout<<"???"<<write_this_buf->write_buf_index<<"\t"<<write_this_buf->bufsize<<endl;Sleep(5000);}//理论上不会执行到这句话
					}
				} // 音频帧内容写入缓存结束

			} // 解码音频帧packet完成

		} // 一帧音频处理结束
		av_packet_unref(pPacket);
		av_packet_free(&pPacket);
	} // 音视频解码彻底完成，死循环结束
	Sleep(100);

	if (write_count - 1 == -1) // 计算最后一帧的差距时间
		write_this_buf->gap = audio_buf[bufnumber - 1].gap;
	else
		write_this_buf->gap = audio_buf[write_count - 1].gap;

	write_audio_buff_over = true; // 音频解码结束
	write_video_buff_over = true; //
	cout << "音、视频解码（写入缓存服务）结束\n";
	while (!audio_output_over)
	{
		Sleep(100);
	} // 如果音频回调输出还没结束

	cout << "SDL_Audio关闭\n";
	if (AudioIndex >= 0)
		SDL_CloseAudio(); // 音频关闭,不执行这句话，SDL_Quit将阻塞

	while (!video_output_over)
	{
		Sleep(30);
	} // 如果视频没输出完，进行等待
	cout << "释放回收资源,关闭SDL" << endl;
	Sleep(500);
	if (outBuffer)
	{
		av_free(outBuffer);
		outBuffer = 0;
	}
	if (pSwsContext)
	{
		sws_freeContext(pSwsContext);
		pSwsContext = 0;
	}
	if (pAVFrameRGB32)
	{
		av_frame_free(&pAVFrameRGB32);
		pAVFrameRGB32 = 0;
	}
	if (pVideoCodecCtx_live)
	{
		avcodec_close(pVideoCodecCtx_live);
		pVideoCodecCtx_live = 0;
	}
	if (pAVFormatContext)
	{
		avformat_close_input(&pAVFormatContext);
		avformat_free_context(pAVFormatContext);
		pAVFormatContext = 0;
	}
	// SDL_DestroyRenderer(pSDLRenderer);//销毁sdl渲染器,在线程中执行
	// SDL_DestroyWindow(pSDLWindow);cout<<"即将执行SDL_Quit()\n";// 销毁sdl窗口,在线程中执行
	SDL_Quit(); // 退出SDL
}