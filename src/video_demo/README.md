# FFmpeg_video_demo
题目：C++用FFmpeg和SDL实现音视频同步播放服务，包括直播、点播、播放本地视频
ZasLeonPlayer

代码依旧存在的问题：播放mp3时，显示的图片会失真。。。

环境：

windows 7

Visual Studio 2010

FFmpeg-n4.3.1-30-g666d2fc6e2-win64-gpl-shared-4.3

SDL2-2.0.14

文件说明：

直播流测试：http://ivi.bupt.edu.cn/

具体程序解读请看源码注释和程序结构详解！


对视频实现播放：

对视频帧的解码和播放已经忘了保存源码路径了。。。


对音频实现播放：

音频帧的源码是https://github.com/pc-zhang/ZPlayer ，但他源码有点坑，因为没实现切换播放缓存，导致缓存一用完声音就没法播放了，所以我自己调用了一大堆逻辑控制参数去实现音频缓存切换。另外我的音频是在线程中实现的。

我的原理：设置自定义个缓存块，当缓存块即将用完时，切换到下一个缓存块进行读写音频，这样就能无限循环往复读取音频，再由回调函数播放。



对音频视频同步的实现：

画面与音频对齐。（即当画面播放慢时会快进到和音频同步，画面过快时会直接卡住停下来等待音频跟上。代码参数体现在video_pace会尽可能和audio_pace相同，video_pace过高会等待audio_pace跟上，video_pace过慢画面会快速切换来让video_pace和audio_pace相同）

