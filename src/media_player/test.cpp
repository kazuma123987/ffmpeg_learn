#include "video_player.h"
#include <iostream>
#undef main
//=============== 完整调用示例 ===============
int main(int argc, char *argv[])
{
    // 把命令行参数作为视频文件路径传入
    std::string video_path;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_file>" << std::endl;
        // video_path = "../res/360°.mp4"; 
        video_path = "../res/tera.mp4"; 
    }
    else
    {
        video_path = argv[1];
    }

    // 创建播放器实例
    VideoPlayer player(video_path.c_str());

    try
    {
        // 启动播放器主循环
        player.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "播放错误: " << e.what() << std::endl;
    }

    return 0;
}
