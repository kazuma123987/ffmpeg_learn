#if 1
#include "checkhdr.h"
#include <chrono>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <iostream>
#include <thread>
#include <windows.h>
#include <cstdio>

// 窗口类名和标题
LPCSTR CLASS_NAME = "HDRDemoClass";
LPCSTR WINDOW_TITLE = "Hardware HDR Red Demo";
const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

// 全局变量
IDXGISwapChain3 *pSwapChain = nullptr;
ID3D11Device *pDevice = nullptr;
ID3D11DeviceContext *pContext = nullptr;
ID3D11RenderTargetView *pRTV = nullptr;
ID3D11VertexShader *pVertexShader = nullptr;
ID3D11PixelShader *pPixelShader = nullptr;
ID3D11InputLayout *pInputLayout = nullptr;
ID3D11Buffer *pVertexBuffer = nullptr; // 全局或成员
ID3D11RasterizerState *pRasterState = nullptr;

// 顶点结构（全屏四边形）
struct Vertex {
  float x, y; // 位置
};

// 全屏四边形顶点数据
Vertex quadVertices[] = {
    {-1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};

// 顶点着色器代码（简单传递位置）
const char *vertexShaderSource = R"(
    struct VS_INPUT {
        float2 pos : POSITION;
    };
    struct PS_INPUT {
        float4 pos : SV_POSITION;
    };
    PS_INPUT main(VS_INPUT input) {
        PS_INPUT output;
        output.pos = float4(input.pos, 0.0f, 1.0f);
        return output;
    }
)";

// 像素着色器代码（输出HDR红色，亮度为10.0尼特）
const char *pixelShaderSource = R"(
    struct PS_INPUT {
        float4 pos : SV_POSITION;
    };
    float4 main(PS_INPUT input) : SV_Target {
        // 输出HDR红色（亮度10.0，超过LDR的1.0范围）
        // 在HDR10中，此值会被映射到实际物理亮度
        return float4(0.3f, 0.0f, 0.0f, 1.0f);
    }
)";

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

// 初始化DirectX和HDR交换链
bool InitD3D(HWND hwnd) {
  // 1. 创建DXGI工厂（支持HDR的1.4版本）
  IDXGIFactory4 *pFactory = nullptr;
  if (CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory)) != S_OK) {
    std::cerr << "Failed to create DXGI factory" << std::endl;
    return false;
  }

  // 2. 创建D3D设备和上下文
  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  if (D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                        &featureLevel, 1, D3D11_SDK_VERSION, &pDevice, nullptr,
                        &pContext) != S_OK) {
    std::cerr << "Failed to create D3D device" << std::endl;
    pFactory->Release();
    return false;
  }

  // 3. 配置HDR交换链
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width = WIDTH;
  swapChainDesc.Height = HEIGHT;
  swapChainDesc.Format =
      DXGI_FORMAT_R10G10B10A2_UNORM; // HDR常用格式（10位色深）
  swapChainDesc.Stereo = FALSE;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = 2; // 双缓冲
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 现代交换效果
  swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  swapChainDesc.Flags = 0;

  // 4. 创建交换链
  IDXGISwapChain1 *pTempSwapChain = nullptr;
  if (pFactory->CreateSwapChainForHwnd(pDevice, hwnd, &swapChainDesc, nullptr,
                                       nullptr, &pTempSwapChain) != S_OK) {
    std::cerr << "Failed to create swap chain" << std::endl;
    pDevice->Release();
    pContext->Release();
    pFactory->Release();
    return false;
  }

  // 5. 升级到SwapChain3以支持HDR色彩空间设置
  pTempSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain));
  pTempSwapChain->Release();

  // 6. 检查显示器是否支持HDR
  if (IsHdrOn()) {
    std::cout << "HDR is active. Proceeding with HDR setup." << std::endl;
    // 设置HDR色彩空间（ST.2084 + P2020，HDR10标准）
    pSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
  } else {
    std::cout << "HDR is not active. Display may still show SDR content."
              << std::endl;
    // 设置SDR色彩空间（sRGB）
    pSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
  }

  pFactory->Release();

  // 6. 创建渲染目标视图
  ID3D11Texture2D *pBackBuffer = nullptr;
  if (pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)) != S_OK) {
    std::cerr << "Failed to get back buffer" << std::endl;
    return false;
  }

  if (pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRTV) != S_OK) {
    std::cerr << "Failed to create RTV" << std::endl;
    pBackBuffer->Release();
    return false;
  }
  pBackBuffer->Release();

  // 7. 编译并创建着色器
  ID3DBlob *pVSBlob = nullptr;
  ID3DBlob *pErrorBlob = nullptr;
  if (D3DCompile(vertexShaderSource, strlen(vertexShaderSource), nullptr,
                 nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVSBlob,
                 &pErrorBlob) != S_OK) {
    std::cerr << "VS Compile Error: " << (char *)pErrorBlob->GetBufferPointer()
              << std::endl;
    return false;
  }

  if (pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                  pVSBlob->GetBufferSize(), nullptr,
                                  &pVertexShader) != S_OK) {
    std::cerr << "Failed to create vertex shader" << std::endl;
    return false;
  }

  // 输入布局（匹配顶点结构）
  D3D11_INPUT_ELEMENT_DESC inputDesc[] = {{"POSITION", 0,
                                           DXGI_FORMAT_R32G32_FLOAT, 0, 0,
                                           D3D11_INPUT_PER_VERTEX_DATA, 0}};
  if (pDevice->CreateInputLayout(inputDesc, 1, pVSBlob->GetBufferPointer(),
                                 pVSBlob->GetBufferSize(),
                                 &pInputLayout) != S_OK) {
    std::cerr << "Failed to create input layout" << std::endl;
    return false;
  }
  pVSBlob->Release();

  // 编译像素着色器
  ID3DBlob *pPSBlob = nullptr;
  if (D3DCompile(pixelShaderSource, strlen(pixelShaderSource), nullptr, nullptr,
                 nullptr, "main", "ps_5_0", 0, 0, &pPSBlob,
                 &pErrorBlob) != S_OK) {
    std::cerr << "PS Compile Error: " << (char *)pErrorBlob->GetBufferPointer()
              << std::endl;
    return false;
  }

  if (pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                 pPSBlob->GetBufferSize(), nullptr,
                                 &pPixelShader) != S_OK) {
    std::cerr << "Failed to create pixel shader" << std::endl;
    return false;
  }
  pPSBlob->Release();

  // 8. 设置渲染状态
  pContext->OMSetRenderTargets(1, &pRTV, nullptr);
  D3D11_VIEWPORT viewport = {};
  viewport.Width = WIDTH;
  viewport.Height = HEIGHT;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  pContext->RSSetViewports(1, &viewport);

  // 创建光栅化状态（禁用剔除）
  D3D11_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D11_FILL_SOLID;
  rasterDesc.CullMode = D3D11_CULL_NONE;    // 不剔除任何三角形
  rasterDesc.FrontCounterClockwise = FALSE; // 不影响，因为不剔除
  pDevice->CreateRasterizerState(&rasterDesc, &pRasterState);
  pContext->RSSetState(pRasterState);

  // 创建顶点缓冲区（一次）
  D3D11_BUFFER_DESC vbDesc = {};
  vbDesc.ByteWidth = sizeof(quadVertices);
  vbDesc.Usage = D3D11_USAGE_IMMUTABLE; // 数据不变，使用 IMMUTABLE 提高性能
  vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vbData = {quadVertices};
  if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbData, &pVertexBuffer))) {
    std::cerr << "Failed to create vertex buffer" << std::endl;
    return false;
  }

  return true;
}

// Render 简化
void Render() {
  // 重新绑定渲染目标视图（确保有效）
  pContext->OMSetRenderTargets(1, &pRTV, nullptr);

  // 清除（可选）
  float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
  pContext->ClearRenderTargetView(pRTV, clearColor);

  // 设置渲染管线（保持不变）
  pContext->IASetInputLayout(pInputLayout);
  pContext->VSSetShader(pVertexShader, nullptr, 0);
  pContext->PSSetShader(pPixelShader, nullptr, 0);

  UINT stride = sizeof(Vertex), offset = 0;
  pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  pContext->Draw(4, 0);

  HRESULT hr = pSwapChain->Present(1, 0);
  if (FAILED(hr)) {
    std::cerr << "Present failed: " << std::hex << hr << std::endl;
    // 可根据错误码采取行动，如设备丢失时重建
  }
}

// 释放资源
void Cleanup() {
  if (pVertexBuffer)
    pVertexBuffer->Release();
  if (pRasterState)
    pRasterState->Release();
  if (pRTV)
    pRTV->Release();
  if (pPixelShader)
    pPixelShader->Release();
  if (pVertexShader)
    pVertexShader->Release();
  if (pInputLayout)
    pInputLayout->Release();
  if (pContext)
    pContext->Release();
  if (pDevice)
    pDevice->Release();
  if (pSwapChain)
    pSwapChain->Release();
}

void frame_counter() {
  static int frameCount = 0;
  static auto lastTime = std::chrono::steady_clock::now();
  frameCount++;

  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
  if (elapsed >= 1) {
    printf("\rFPS: %d", frameCount);
    frameCount = 0;
    lastTime = now;
  }
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {

  AllocConsole();
  FILE *fDummy;
  freopen_s(&fDummy, "CONOUT$", "w", stdout);
  freopen_s(&fDummy, "CONOUT$", "w", stderr);
  // 注册窗口类
  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  RegisterClass(&wc);

  // 创建窗口
  HWND hwnd = CreateWindowEx(0, CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
                             nullptr, nullptr, hInstance, nullptr);

  if (!hwnd)
    return 0;
  ShowWindow(hwnd, nCmdShow);

  // 初始化DirectX和HDR
  if (!InitD3D(hwnd)) {
    Cleanup();
    return 0;
  }

  MSG msg = {};
  while (true) {
    // 处理所有待处理的消息
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        // 退出循环
        return 0;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    frame_counter(); // 可选：显示FPS

    // 没有消息时，执行渲染
    Render();

    // 可以加一点小的 sleep 避免 CPU 100% 占用（但垂直同步 Present(1,0)
    // 本身会等待） Sleep(1); // 可选，但通常 Present 已经做了等待
  }

  Cleanup();
  return 0;
}

#else
#include "local_stream.h"

#undef main // 不加这句貌似会和sdl里main冲突出错
int main() {

  switch (movie_type) {
  case liveshow:
    // show_moive_alive(); // 直播TS流
    break;
  case local:
    show_moive(); // 本地影片
    break;
  case vod:
    // show_moive_vod(); // 网络点播
    break;
  }
  cout << "彻底结束\n";

  int k;
  cin >> k;

  return 0;
}
#endif