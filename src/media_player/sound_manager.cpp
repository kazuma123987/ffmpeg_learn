#include "sound_manager.h"
SoundManager::SoundManager(int maxChannels, FMOD_INITFLAGS flags, void *extraDriverData)
{
    // 初始化成员变量
    this->system = NULL;
    this->usedChannelCounts = 0;
    this->channels.emplace_back((FMOD_CHANNEL *)NULL);
    this->channelIsPlaying.emplace_back((FMOD_BOOL)0);
    for (int i = 0; i < 2; i++)
        this->groups.emplace_back((FMOD_CHANNELGROUP *)NULL);
    // 初始化FMOD系统
    FMOD_RESULT result = FMOD_System_Create(&this->system, FMOD_VERSION);
    if (result != FMOD_OK)
        printf("failed tho create fmod system\n");
    result = FMOD_System_Init(this->system, maxChannels, flags, extraDriverData);
    if (result != FMOD_OK)
    {
        printf("failed to init fmod system\n");
        FMOD_System_Release(this->system);
    }
    // 初始化FMOD通道组
    result = FMOD_System_CreateChannelGroup(system, "bgMusic", &groups[0]);
    if (result != FMOD_OK)
        printf("failed to create bgMusic group\n");
    result = FMOD_System_CreateChannelGroup(system, "effectMusic", &groups[1]);
    if (result != FMOD_OK)
        printf("failed to create effectMusic group\n");
}
SoundManager::~SoundManager()
{
    this->destroy();
}
void SoundManager::loadSound(const char *path, const char *name, FMOD_MODE soundMode,FMOD_CREATESOUNDEXINFO *exinfo)
{
    sounds[name] = NULL;
    FMOD_RESULT result = FMOD_System_CreateSound(this->system, path, soundMode, exinfo, &sounds[name]);
    if (result != FMOD_OK)
        printf("failed to create the sound:%s\n", name);
}
void SoundManager::playSound(const char *name, int groupIndex)
{
    for (size_t i = 0, size = channelIsPlaying.size(); i < size; i++)
    {
        if (!channelIsPlaying[i])
        {
            FMOD_RESULT result = FMOD_System_PlaySound(system, sounds[name], groups[groupIndex], false, &channels[i]);
            channelIsPlaying[i]=true;//别忘了更新
            if (result == FMOD_OK)
            {
                usedChannelCounts++;                           // 每次播放成功音乐时会增加已用channels的大小
                if (usedChannelCounts == (int)channels.size()) // channels的大小会始终是已用channels大小+1
                {
                    channels.emplace_back((FMOD_CHANNEL *)NULL);
                    channelIsPlaying.emplace_back(false);
                }
            }
            else
                printf("failed to play sound:%s\n", name);
            break;//找到了要播放的频道就能break了
        }
    }
}
void SoundManager::setSoundMode(std::string soundName, FMOD_MODE mode)
{
    FMOD_RESULT result = FMOD_Sound_SetMode(sounds[soundName], mode);
    if (result != FMOD_OK)
        printf("failed to set the sound mode of the sound:%s\n", soundName.c_str());
}
void SoundManager::stopGroup(int groupIndex)
{
    FMOD_RESULT result = FMOD_ChannelGroup_Stop(groups[groupIndex]);
    if (result != FMOD_OK)
        printf("failed to stop group%d\n", groupIndex);
}
void SoundManager::pauseGroup(int groupIndex, bool paused)
{
    FMOD_RESULT result = FMOD_ChannelGroup_SetPaused(groups[groupIndex], paused);
    if (result != FMOD_OK)
        printf("failed to pause group%d\n", groupIndex);
}
void SoundManager::setGroupVolume(int groupIndex, float volume) // 0.0f-1.0f
{
    FMOD_RESULT result = FMOD_ChannelGroup_SetVolume(groups[groupIndex], volume);
    if (result != FMOD_OK)
        printf("failed to set group%d's volume\n", groupIndex);
}
void SoundManager::setChannel3DPos(int channelIndex, float x, float y, float z)
{
    FMOD_VECTOR vector;
    vector.x = x;
    vector.y = y;
    vector.z = z;
    FMOD_Channel_Set3DAttributes(channels[channelIndex], &vector, NULL);
}
void SoundManager::update()
{
    // 遍历channels查看有哪些在使用,以及更新已用channels数量的值
    this->usedChannelCounts = 0;
    for (size_t i = 0; i < channels.size(); i++)
    {
        FMOD_Channel_IsPlaying(channels[i], &(channelIsPlaying[i]));
        if (channelIsPlaying[i])
            usedChannelCounts++;
    }
    FMOD_System_Update(this->system);
}
void SoundManager::destroy()
{
    for (size_t i = 0; i < channels.size(); i++)
        FMOD_Channel_Stop(channels[i]);
    for (auto &sound : sounds)
        FMOD_Sound_Release(sound.second);
    for (size_t i = 0; i < groups.size(); i++)
        FMOD_ChannelGroup_Release(this->groups[i]);
    FMOD_System_Release(this->system);
}