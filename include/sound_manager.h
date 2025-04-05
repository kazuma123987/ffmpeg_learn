#ifndef __SOUND__MANAGER__H
#define __SOUND__MANAGER__H
#include <unordered_map>
#include <string>
#include <vector>
#include "fmod/fmod.h"
// Fmod每个channel只能播放一个sound,channel有各种模式可供设置(循环播放模式、3D播放模式等)
// 一个sound可以同时在多个channel上播放,fmod的一切资源都由fmod_system管理
// 方案:sounds由一个哈希表管理,channel由channelgroup管理,环境声和音效由不同的channelgroup管理
// 放大音效或音乐可通过FMOD_ChannelGroup_SetVolume实现
class SoundManager
{
public:
    std::unordered_map<std::string, FMOD_SOUND *> sounds;
    std::vector<FMOD_CHANNEL *> channels;
    std::vector<FMOD_BOOL> channelIsPlaying;
    std::vector<FMOD_CHANNELGROUP *> groups;//0为背景音,1为音效
    int usedChannelCounts;
    SoundManager(int maxChannels, FMOD_INITFLAGS flags = FMOD_INIT_NORMAL, void *extraDriverData = NULL);
    ~SoundManager();
    void loadSound(const char *path, const char *name, FMOD_MODE soundMode = FMOD_DEFAULT,FMOD_CREATESOUNDEXINFO *exinfo=NULL);
    void playSound(const char *name,int groupIndex=0);
    void setSoundMode(std::string soundName, FMOD_MODE mode);
    void stopGroup(int groupIndex);
    void pauseGroup(int groupIndex, bool paused);
    void setGroupVolume(int groupIndex, float volume = 0.5f); // 0.0f-1.0f
    void setChannel3DPos(int channelIndex, float x, float y, float z);
    void update();
    void destroy();
private:
    FMOD_SYSTEM *system;
};
#endif
//*FMOD_RESULT FMOD_System_Init(FMOD_SYSTEM *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata)
//*FMOD_INITFLAGS:
// FMOD_INIT_NORMAL:提供了FMOD默认选项
// FMOD_INIT_STREAM_FROM_UPDATE：此选项将流式数据来源于 FMOD_System_Update 函数的调用线程，而不是在单独的工作线程中处理。这可以减少线程间的同步开销。
// FMOD_INIT_3D_RIGHTHANDED：此选项将 FMOD 的 3D 坐标系设置为右手坐标系。
// FMOD_INIT_CHANNEL_LOWPASS：此选项在音频系统中为每个音频通道添加一个低通滤波器。模拟远离音源时的声音衰减效果。
// FMOD_INIT_VOL0_BECOMES_VIRTUAL：此选项将音量为 0 的声音通道转换为虚拟通道，以节省系统资源。