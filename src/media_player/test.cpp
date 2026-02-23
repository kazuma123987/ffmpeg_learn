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
        // video_path = "https://d1--ov-gotcha207.bilivideo.com/live-bvc/824090/live_546195_332_c521e483_av1/index.m3u8?expires=1761473062&len=0&oi=1760171076&pt=web&qn=400&trid=10070f0dff7ea4eaf8f740e50a67ab68fde4&bmt=1&sigparams=cdn,expires,len,oi,pt,qn,trid,bmt&cdn=ov-gotcha207&sign=33e5e2e6b821bdcbc97bdb52ff0c8b17&site=1a62d2677fa39422659d515c785b3c22&free_type=0&mid=480171105&sche=ban&bvchls=1&trace=0&isp=other&rg=other&pv=other&flvsk=&strategy_version=latest&deploy_env=prod&hdr_type=0&codec=2&pp=srt&hot_cdn=909773&expected_qn=400&strategy_ids=36,25&long_ab_flag=live_default_longitudinal&ld=yxlg&media_type=0&origin_bitrate=1882&score=62&strategy_type=0&strategy_types=0,1&source=puv3_onetier&long_ab_id=45&strategy_id=36&long_ab_flag_value=test&sk=2c87346267e030818ca0012a5e28fd3c&p2p_type=-1&suffix=av1&info_source=hot_cache&sl=5&vd=bc&src=puv3&order=1";
        // video_path = "../res/tera.mp4"; 
        video_path = "../res/8k_30.mp4"; 
    }
    else
    {
        video_path = argv[1];
    }

    // 创建播放器实例
    VideoPlayer player(video_path.c_str());
    if(player.initResource()<0)
    {
        getchar();
        return -1;    
    }

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
