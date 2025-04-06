#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <windows.h>

#pragma warning(disable : 4819) // 会有警告：libavutil\rational.h : warning C4819: The file contains a character that cannot be represented in the current code page (936). Save the file in Unicode format to prevent data loss
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "postproc.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
extern "C"
{
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "SDL2/SDL.h"
#include <SDL2/SDL_thread.h>
};

using namespace std;

void blank() {} //-0-------------------------------拦截符-------------------------------------------------------

#define vod 1	   // 点播
#define liveshow 2 // 现场直播
#define local 3	   // 本地影片

// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\杀戮都市：OBD日语中字1280高清[www.66ys.tv].rmvb";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\Wildlife.wmv";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\我们都是超能力者！.mp4";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\warriors.mp3";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\Kalimba.mp3";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\hc\\123123.mp4";int movie_type=local;
// char rtspUrl[] = "F:\\JY\\www.NewXing.com\\movie\\Don Diablo - BBC Radio Essential Mix 2017   Future House & Deep House Music Mix.mp3";int movie_type=local;
// char rtspUrl[] = "123123.mp4";int movie_type=local;
// char rtspUrl[] = "http://ivi.bupt.edu.cn/hls/dfhd.m3u8";int movie_type=liveshow;//高清
// char rtspUrl[] = "http://ivi.bupt.edu.cn/hls/dftv.m3u8";int movie_type=liveshow;
// char rtspUrl[] = "https://vip.w.xk.miui.com/10d7ff4c9368d411b254bb5c62d6e849";
// char rtspUrl[] = "../res/tera.mp4";
char rtspUrl[] = "../res/gura.mp4";
int movie_type = local;
// http://1251316161.vod2.myqcloud.com/5f6ddb64vodsh1251316161/c841de715285890814995493210/ALat0G5iwIQA.mp4
// char rtspUrl[] = "https://sk.cri.cn/887.m3u8";int movie_type=liveshow;//hitfm电台mp3节目
// char rtspUrl[] = "http://10.125.39.201:10080/frecord/warriors.mp3";int movie_type=vod;//

#define SDL_AUDIO_BUFFER_SIZE 1024
const int my_width = 1280; // 自己电脑分辨率，超过则在显示时进行限制
const int my_height = 720;
static int set_width, set_height;
//---------------------------------------------------对音频输出的逻辑控制----------------------------

const int bufnumber = 1000;													// 设置音频内存块个数，视频帧包个数
const int single_buf_size = 65535 * 2;										// 单帧音频的最大容量,wmv的单个音帧很大很大！
static bool next_write, next_read;											// 音频写入内存服务可以继续了（写服务是一下子写完当前内存块，进行等待。播放功能发送该指令next_write（你可以先写下一块了）后才进行下一块内存块的写入）
static bool audio_output_over;												// 音频输出结束了
static double video_pace, audio_pace, decode_audio_pace, decode_video_pace; // 确认当前进度，保证视频音频同步
static int AudioIndex, VideoIndex;											// 全局确认该帧是否是音频帧或视频帧
bool write_audio_buff_over;													// 音频是否结束，true为结束，false为没结束
static int audio_framesize, audio_fre;										// 单帧音频数据大小,音频采样率
struct Audio_buf
{
	uint8_t audio_info[single_buf_size];
	int bufsize;		 // 该帧音频大小
	int write_buf_index; // 写块偏移量
	int read_buf_index;	 // 读块偏移量
	double pts;
	double gap; // 和后一时间的差额
};
static Audio_buf audio_buf[bufnumber];
static Audio_buf *read_this_buf;
static Audio_buf *write_this_buf;
static int write_count, read_count; // 记录当前在读或写的内存块

//--------------------对视频的逻辑控制参数（直播、点播采用，本地流不用）-----------------
static bool video_output_over;	   // 视频输出是否结束
static bool write_video_buff_over; // 写入视频缓存服务是否结束（视频解码是否结束）
AVFrame *vframe[bufnumber];
static AVStream *pVStream = 0; // ffmpeg流信息，在确认想要的流信息后再进行赋值
							   //--------------------视频逻辑控制参数结束------------------------------------------------