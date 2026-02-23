#include "checkhdr.h"
// 链接所需的系统库
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

// 检查指定显示器的HDR状态
bool CheckAdvancedColorState(LUID adapterId, UINT32 targetId, bool& supported, bool& enabled) {
    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo = {};
    colorInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
    colorInfo.header.size = sizeof(colorInfo);
    colorInfo.header.adapterId = adapterId;
    colorInfo.header.id = targetId;

    LONG result = DisplayConfigGetDeviceInfo(&colorInfo.header);
    if (result == ERROR_SUCCESS) {
        supported = (colorInfo.advancedColorSupported == 1);
        enabled = (colorInfo.advancedColorEnabled == 1);
        return true;
    }
    return false;
}

// 检查主显示器是否已开启HDR
bool IsHdrOn() {
    UINT32 pathCount = 0, modeCount = 0;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;

    // 1. 获取所需的数组大小
    LONG result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    // 2. 分配内存并获取活动路径信息
    paths.resize(pathCount);
    modes.resize(modeCount);
    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    // 3. 遍历每个活动路径，检查其对应的显示器
    for (const auto& path : paths) {
        bool supported = false, enabled = false;
        if (CheckAdvancedColorState(path.targetInfo.adapterId, path.targetInfo.id, supported, enabled)) {
            // 只要找到一个启用了高级颜色的显示器，就认为 HDR 已打开
            if (enabled) {
                std::cout << "HDR is ENABLED on this monitor." << std::endl;
                return true;
            } else {
                std::cout << "HDR is supported but NOT enabled on this monitor." << std::endl;
            }
        }
    }
    return false; // 未找到启用了HDR的显示器
}