#include<windows.h>
#include<dxgi.h>
#include<direct.h>
#include<d3d12.h>
#include<DirectXMath.h>
#include<string>
#include<WRL.h>
#include<array>
#include<vector>
#include<DirectXColors.h>
#include<DirectXPackedVector.h>
#include<d3dcompiler.h>
#include<string>
#include<iostream>

#include"d3dx12.h"    //辅助函数库，需要另外准备

//链接必要的D3D12库
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DirectX::PackedVector;

LRESULT WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	return DefWindowProc(hwnd, msg, wparam, lparam);    //默认处理消息
}

//顶点格式
struct Vertex {
	XMFLOAT3 Pos;              //位置
	XMFLOAT4 Color;            //颜色
};

class D3DAPP
{
public:
//D3D基础接口
	ComPtr<ID3D12Debug> DebugController;                        //调试层接口

	ComPtr<IDXGIFactory> Factory;                               //工厂
	ComPtr<ID3D12Device> Device;                                //适配器
	ComPtr<ID3D12Fence> Fence;                                  //围栏
	
	ComPtr<ID3D12CommandQueue> CmdQueue;                        //命令队列
	ComPtr<ID3D12CommandAllocator> CmdAllocator;                //命令分配器
	ComPtr<ID3D12GraphicsCommandList> CmdList;                  //命令列表
	
	ComPtr<IDXGISwapChain> SwapChain;                           //交换链

//描述符大小
	UINT RTVSize;                                               //RTV大小
	UINT DSVSize;                                               //DSV大小
	UINT CSUSize;                                               //CBV、SRV、UAV大小

//窗口相关
	UINT ClientWidth;                                           //客户区宽度
	UINT ClientHeight;                                          //客户区高度
	HWND hwnd;                                                  //窗口句柄

	static const UINT SwapChainBufferCount = 2;                 //交换链缓冲区数量
	UINT BackBufferIndex = 0;                                   //当前后台缓冲区的索引

//资源格式
	DXGI_FORMAT RTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;          //渲染目标缓冲区资源格式
	DXGI_FORMAT DSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;       //深度模板缓冲区资源格式

//常量缓冲区源数据
	XMFLOAT4X4 WorldViewProjData;                                          //“世界-观察-投影”变换复合矩阵
	
//描述符、描述符堆、根签名
	ComPtr<ID3D12DescriptorHeap> RTVHeap;                       //RTV堆
	ComPtr<ID3D12DescriptorHeap> DSVHeap;                       //DSV堆
	ComPtr<ID3D12DescriptorHeap> CBVHeap;                       //CBV堆

	ComPtr<ID3D12RootSignature> RootSignature;                  //根签名

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;                  //顶点缓冲区描述符
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;                    //索引缓冲区描述符
	
//D3D资源
	ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];          //RTB数组
	ComPtr<ID3D12Resource> DSBuffer;                                       //DSB，深度模板缓冲区
	
	ComPtr<ID3D12Resource> VerticesUploadBuffer;                           //顶点上传堆
	ComPtr<ID3D12Resource> IndicesUploadBuffer;                            //索引上传堆
	ComPtr<ID3D12Resource> ConstantUploadBuffer;                           //常量上传堆

	ComPtr<ID3D12Resource> VerticesBufferGPU;                              //顶点缓冲区
	ComPtr<ID3D12Resource> IndicesBufferGPU;                               //索引缓冲区

//流水线所需对象
	D3D12_VIEWPORT ViewPort;                                               //视口描述结构体
	D3D12_RECT SciRect;                                                    //裁剪矩形描述结构体

	vector<D3D12_INPUT_ELEMENT_DESC> IAInputElementDesc;                   //顶点结构体元素描述
	D3D12_INPUT_LAYOUT_DESC IAInputDesc;                                   //输入布局描述

	ComPtr<ID3DBlob> SerializedRootSig = nullptr;                          //存放序列化后的根签名数据
	
	ComPtr<ID3DBlob> VSByteCode = nullptr;                                           //存放顶点着色器编译后的字节码
	ComPtr<ID3DBlob> PSByteCode = nullptr;                                           //存放顶点着色器编译后的字节码
	
	D3D12_SHADER_BYTECODE VSByteCodeDesc;                                  //顶点着色器字节码描述
	D3D12_SHADER_BYTECODE PSByteCodeDesc;                                  //像素着色器字节码描述

	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc;                            //流水线状态描述
	ComPtr<ID3D12PipelineState> PSO;

	UINT IndexCount;


//方法
public:
	D3DAPP(HWND hwnd, UINT clientwidth, UINT clientheight);                //初始化

	void OpenDebugLayer();                                      //开启调试层

	//void CheckMSAASupport();                                    //检查

	//初始化阶段
	void CreateCommandObjects();                                //创建命令队列、命令分配器、命令列表
	void CreateSwapChain();                                     //创建交换链
	void CreateRT();                                            //创建渲染目标视图、描述符堆、资源
	void CreateDS();                                            //创建深度模板缓冲区视图、描述符堆、资源
	void SetViewPortAndSciRect();                               //设置视口和裁剪矩形

	//流水线阶段
	void CreateIAInputDesc();                                   //创建输入布局描述
	void CreateVertexBuffer();                                  //创建顶点数据缓冲区
	void CreateIndexBuffer();                                   //创建索引数据缓冲区
	void CaculateWorldViewProjMat();                            //计算“世界-观察-投影”变换复合矩阵
	void CreateConstantBuffer();                                //创建常量缓冲区
	void CreateCBVHeap();                                       //创建常量缓冲区描述符堆，并绑定到命令列表上
	void CreateRootSignature();                                 //创建根签名
	void CompileVertexShader();                                 //编译顶点着色器，并描述着色器字节码
	void CompilePixelShader();                                  //编译像素着色器，并描述着色器字节码
	void CreatePSO();                                           //创建流水线状态
	void Draw();                                                //绘制

	ComPtr<ID3D12Resource> CreateDefaultBuffer(                 //创建资源、上传堆、默认堆，将资源从上传堆复制到默认堆
		ID3D12Device* Device,
		UINT64 ResourceSize,
		const void* ResourceData,
		ID3D12GraphicsCommandList* CmdList,
		ComPtr<ID3D12Resource> &UploadBuffer);
};

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int mCmdShow)
{
	UINT ClientWidth = 800;    //窗口客户区宽度
	UINT ClientHeight = 600;    //高度

	//窗口初始化部分

	const wchar_t class_name[] = L"D3D学习DEMO";

	WNDCLASS wc = {};
	wc.style = CS_VREDRAW | CS_HREDRAW;    //如果改变窗口高/宽度则重绘窗口
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);    //防止背景重绘
	wc.hInstance = hinstance;
	wc.lpszClassName = class_name;
	wc.lpfnWndProc = WinProc;

	RegisterClass(&wc);    //注册窗口

	HWND hwnd = CreateWindowEx(    //创建窗口实例
		0, class_name, L"D3D学习DEMO",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		ClientWidth, ClientHeight, NULL, NULL, hinstance, NULL);

	ShowWindow(hwnd, mCmdShow);    //显示窗口

	//开启控制台输出
	//AllocConsole();
	//FILE* stream1;
	//freopen_s(&stream1, "CONOUT$", "w", stdout);
	//关闭控制台
	//FreeConsole();

	D3DAPP D3DApp(hwnd, ClientWidth, ClientHeight);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

D3DAPP::D3DAPP(HWND Hwnd,UINT clientwidth,UINT clientheight)
{
	//开启控制台输出
	AllocConsole();
	FILE* stream1;
	freopen_s(&stream1, "CONOUT$", "w", stdout);

	//开启调试层
	OpenDebugLayer();
	

	//创建工厂
	CreateDXGIFactory(IID_PPV_ARGS(&Factory));
	
	//创建适配器
	D3D12CreateDevice(0,                                 //指定使用的显示适配器
		D3D_FEATURE_LEVEL_11_0,                          //程序需要硬件支持的最低功能级别
		IID_PPV_ARGS(&Device));

	//创建围栏
	Device->CreateFence(0,                               //标志围栏点，初始值为0
		D3D12_FENCE_FLAG_NONE,                           //围栏类型未指定
		IID_PPV_ARGS(&Fence));

	//检查MSAA
	
	//获得RTV、DSV、CBV&SRV&UAV大小
	RTVSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CSUSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//窗口信息
	hwnd = Hwnd;
	ClientWidth = clientwidth;
	ClientHeight = clientheight;
	
	//检查MSAA支持，暂略
	//枚举适配器，暂略

	CreateCommandObjects();                              //创建命令队列、命令分配器、命令列表
	
	CreateSwapChain();                                   //创建交换链
	
	CreateRT();                                          //创建RTV、RTV堆、RT缓冲区
	
	CreateDS();                                          //创建DSV、DSV堆、DS缓冲区
	
	SetViewPortAndSciRect();                             //设置视口和裁剪矩形
	
	CreateIAInputDesc();                                 //创建输入布局描述
	
	CreateVertexBuffer();                                //创建顶点缓冲区
	
	CreateIndexBuffer();                                 //创建索引缓冲区
	
	CaculateWorldViewProjMat();                          //计算要放入常量缓冲区的复合矩阵
	
	CreateConstantBuffer();                              //创建常量缓冲区
	
	CreateCBVHeap();                                     //创建常量缓冲区描述符堆，并绑定到命令列表上
	
	CreateRootSignature();                               //创建根签名
	
	CompileVertexShader();                               //编译顶点着色器，并描述着色器字节码
	
	CompilePixelShader();                                //编译像素着色器，并描述着色器字节码
	
	CreatePSO();                                         //创建流水线状态

	Draw();                                              //绘制
	
}

void D3DAPP::OpenDebugLayer()
{
	D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController));
	DebugController->EnableDebugLayer();
}

void D3DAPP::CreateCommandObjects()
{
	//创建命令队列
	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};                               //命令队列描述结构体
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;                       //命令列表类型：直接命令列表，指定GPU可执行的命令列表缓冲区，不继承任何GPU状态
	QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;              //命令队列优先级：正常优先级
	QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;                       //标志：默认命令队列
	QueueDesc.NodeMask = 0;                                                //单GPU设置为0

	Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CmdQueue));       //创建命令队列

	//创建命令分配器
	Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,         //命令列表类型：直接命令列表
		IID_PPV_ARGS(&CmdAllocator));

	//创建命令列表
	Device->CreateCommandList(0,                                           //单GPU系统设置为0
		D3D12_COMMAND_LIST_TYPE_DIRECT,                                    //命令列表类型
		CmdAllocator.Get(),                                                //与命令列表关联的命令分配器
		nullptr,                                                           //命令列表渲染流水线初始状态，nullptr为纯初始状态
		IID_PPV_ARGS(CmdList.GetAddressOf()));

	//关闭命令列表
	//CmdList->Close();
}

void D3DAPP::CreateSwapChain()
{	
	//创建并填写交换链描述结构体
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};


	SwapChainDesc.BufferDesc.Width = ClientWidth;                            //分辨率宽度
	SwapChainDesc.BufferDesc.Height = ClientHeight;                          //分辨率高度
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 0;                    //刷新率分母
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;                      //刷新率分子
	SwapChainDesc.BufferDesc.Format = RTFormat;                              //缓冲区显示格式
	SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;       //未指定扫描线顺序
	SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;        //未指定缩放
	SwapChainDesc.SampleDesc.Count = 1;                                      //采样像素个数
	SwapChainDesc.SampleDesc.Quality = 0;                                    //采样质量级别
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = SwapChainBufferCount;                        //交换链数量
	SwapChainDesc.OutputWindow = hwnd;                                       //将深度缓冲区输出的内容绑定到窗口上
	SwapChainDesc.Windowed = true;                                           //窗口模式显示
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;            //可切换窗口全屏模式

	//创建交换链
	Factory->CreateSwapChain(CmdQueue.Get(), &SwapChainDesc, SwapChain.GetAddressOf());
}

void D3DAPP::CreateRT()
{
	//创建描述符堆（RTV Heap）
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc;                                  //描述符堆描述结构体
	
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;                       //描述符堆类型
	RTVHeapDesc.NumDescriptors = SwapChainBufferCount;                       //描述符数量
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RTVHeapDesc.NodeMask = 0;                                                //单显卡为0
	
	Device->CreateDescriptorHeap(&RTVHeapDesc,                               //创建RT描述符堆
		IID_PPV_ARGS(RTVHeap.GetAddressOf()));


	//创建两个RT缓冲区和对应的RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(                             //获得RTV堆首句柄
		RTVHeap->GetCPUDescriptorHandleForHeapStart());
	
	for (UINT i = 0; i < SwapChainBufferCount; i++)                          //依次创建两个RT缓冲区，并把对应RTV填入描述符堆中
	{
		SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i]));          //把交换链和RT缓冲区联系起来
		
		Device->CreateRenderTargetView(                                      //为RT缓冲区创建描述符
			SwapChainBuffer[i].Get(),                                        //RT资源
			nullptr,                                                         //资源中元素数据类型，已指定格式，设为nullptr
			RTVHeapHandle);                                                  //要创建的RTV所在内存区域的CPU描述符句柄
		
		RTVHeapHandle.Offset(                                                //偏移句柄到堆中下一个描述符内存区域的开始
			1,                                                               //偏移数量 
			RTVSize);                                                        //偏移量大小
	}

}

void D3DAPP::CreateDS()
{
	//创建和填写DS缓冲区资源描述结构体
	D3D12_RESOURCE_DESC DSDesc;                                              //DS资源描述结构体
	
	DSDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;                   //资源类型为2D纹理
	DSDesc.Alignment = 0;                                                    //对齐方式：0？
	DSDesc.Width = ClientWidth;                                              //资源宽度
	DSDesc.Height = ClientHeight;                                            //资源高度
	DSDesc.DepthOrArraySize = 1;                                             //资源深度
	DSDesc.MipLevels = 1;                                                    //mipmap层级
	DSDesc.Format = DSFormat;                                                //资源格式
	DSDesc.SampleDesc.Count = 1;
	DSDesc.SampleDesc.Quality = 0;
	DSDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;                            //纹理布局选项：布局未知，可能依赖于适配器
	DSDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;                  //允许为资源创建深度模板视图

	//创建DS缓冲区
	D3D12_CLEAR_VALUE ClearValue;                                            //用于清除资源的优化值
	ClearValue.Format = DSFormat;
	ClearValue.DepthStencil.Depth = 1.0f;
	ClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES DSProperties =                                     //描述将要创建的堆属性：默认堆
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	Device->CreateCommittedResource(                                         //创建DS缓冲区和一个默认堆，并把前者映射到后者上
		&DSProperties,                                                       //堆属性：默认堆
		D3D12_HEAP_FLAG_NONE,                                                //标志：默认值
		&DSDesc,                                                             //放入堆中的资源（DS缓冲区）描述符
		D3D12_RESOURCE_STATE_COMMON,                                         //资源初始状态
		&ClearValue,                                                         //清除资源区域的值
		IID_PPV_ARGS(DSBuffer.GetAddressOf()));

	//创建DSV堆
	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc;                                  //DSV堆描述结构体
	
	DSVHeapDesc.NumDescriptors = 1;
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DSVHeapDesc.NodeMask = 0;

	Device->CreateDescriptorHeap(&DSVHeapDesc,                               //创建DSV堆
		IID_PPV_ARGS(DSVHeap.GetAddressOf()));

	//创建DSV
	Device->CreateDepthStencilView(DSBuffer.Get(),                           //DS资源
		nullptr,                                                             //描述资源格式，已描述则为空指针
		DSVHeap->GetCPUDescriptorHandleForHeapStart());                      //DSV堆首句柄

	//更改DS缓冲区状态
	CD3DX12_RESOURCE_BARRIER DSBufferBarrier =                               //描述DS资源屏障
		CD3DX12_RESOURCE_BARRIER::Transition(
			DSBuffer.Get(),                                                  //DS资源
			D3D12_RESOURCE_STATE_COMMON,                                     //资源转换前状态
			D3D12_RESOURCE_STATE_DEPTH_WRITE);                               //资源转换后状态

	CmdList->ResourceBarrier(                                                //通知驱动同步资源访问
		1,                                                                   //屏障数量
		&DSBufferBarrier);                                                   //屏障描述结构体数组

	/*CmdList->Close();
	ID3D12CommandList* CmdLists[] = { CmdList.Get() };
	CmdQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);*/

}

void D3DAPP::SetViewPortAndSciRect()
{
	//设置视口
	ViewPort.TopLeftX = 0;                                                   //视口左上顶点在后台缓冲区的x坐标
	ViewPort.TopLeftY = 0;                                                   //类上，y坐标
	ViewPort.Width = static_cast<float>(ClientWidth);                        //视口宽度
	ViewPort.Height = static_cast<float>(ClientHeight);                      //视口高度
	ViewPort.MinDepth = 0.0f;                                                //深度值映射区间左端点值
	ViewPort.MaxDepth = 1.0f;                                                //深度值映射区间的右端点值

	CmdList->RSSetViewports(                                                 //视口数量
		1, &ViewPort);                                                       //视口描述结构体数组

	//设置裁剪矩形
	SciRect.left = 0;                                                        //裁剪矩形区左上角x坐标
	SciRect.top = 0;                                                         //左下角y坐标
	SciRect.right = ClientWidth;                                             //右下角x坐标
	SciRect.bottom = ClientHeight;                                           //右下角y坐标
	
	CmdList->RSSetScissorRects(                                              //设置裁剪矩形
		1, &SciRect);                                                        //裁剪矩形数量；裁剪矩形描述结构体数组

}

void D3DAPP::CreateIAInputDesc()
{
	//描述IA阶段输入的顶点结构体的元素的信息
	IAInputElementDesc =
	{
		//描述顶点结构体第一个元素：顶点位置信息
		{"POSITION",                                                         //语义：顶点坐标
		0,                                                                   //附加到语义上的索引以区多个同类语义，单个为0
		DXGI_FORMAT_R32G32B32_FLOAT,                                         //顶点元素类型：3D 32位浮点向量
		0,                                                                   //指定传递元素所用的输入索引槽
		0,                                                                   //特定输入槽中，顶点结构体首地址道元素起始地址的偏移量
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,                          //标志单个输入槽的数据类型，非实例化
		0},                                                                  //与实例化技术有关，非实例化设为0
		//描述顶点结构体第二个元素：顶点颜色信息
		{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};

	//输入布局描述
	IAInputDesc.NumElements = (UINT)IAInputElementDesc.size();                 //顶点结构体元素数量
	IAInputDesc.pInputElementDescs = IAInputElementDesc.data();                //顶点结构体元素描述

}

void D3DAPP::CreateVertexBuffer()
{
	//顶点数据
	array<Vertex, 8> Vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	//创建顶点数据缓冲区
	UINT VerticesSize = (UINT)Vertices.size() * sizeof(Vertex);                //顶点数据大小，以字节为单位
	
	VerticesBufferGPU = CreateDefaultBuffer(Device.Get(),                      //创建顶点缓冲区
		VerticesSize, Vertices.data(), CmdList.Get(),VerticesUploadBuffer);

	//填写顶点缓冲区描述符
	VertexBufferView.BufferLocation =                                          //顶点缓冲区GPU虚拟地址
		VerticesBufferGPU->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = VerticesSize;                               //顶点缓冲区大小
	VertexBufferView.StrideInBytes = (UINT)sizeof(Vertex);                     //单个顶点大小

}

void D3DAPP::CreateIndexBuffer()
{
	//正方体六面顶点索引的原始数据，采用16位类型
	array<std::uint16_t, 36> Indices =    
	{
		//前面
		0, 1, 2,   0, 2, 3,
		//后面
		4, 6, 5,   4, 7, 6,
		//左面
		4, 5, 1,   4, 1, 0,
		//右面
		3, 2, 6,   3, 6, 7,
		//上面
		1, 5, 6,   1, 6, 2,
		//底面
		4, 0, 3,   4, 3, 7
	};

	IndexCount = (UINT)Indices.size();

	//创建索引数据缓冲区
	UINT IndicesSize = (UINT)Indices.size() * sizeof(uint16_t);                //索引数据大小

	IndicesBufferGPU = CreateDefaultBuffer(Device.Get(),                       //创建索引缓冲区
		 IndicesSize, Indices.data(), CmdList.Get(), IndicesUploadBuffer);

	//填写索引缓冲区视描述符
	IndexBufferView.BufferLocation =                                           //索引缓冲区GPU虚拟地址
		IndicesBufferGPU->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R16_UINT;                             //索引格式：16位类型
	IndexBufferView.SizeInBytes = IndicesSize;                                 //索引缓冲区大小

}

void D3DAPP::CaculateWorldViewProjMat()
{
	//计算世界矩阵
	XMFLOAT4X4 WorldMatData(                                                   //世界矩阵数据
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	XMMATRIX WorldMat = XMLoadFloat4x4(&WorldMatData);                     //加载世界矩阵

	//计算观察矩阵
	//摄像机参数
	XMVECTOR CameraPos = XMVectorSet(10.0f, 0.0f, 0.0f, 1.0f);                  //摄像机坐标
	XMVECTOR CameraTarget = XMVectorZero();                                    //摄像机观察点
	XMVECTOR CameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);                   //向上方向向量

	XMMATRIX ViewMat = XMMatrixLookAtLH(CameraPos,                             //观察矩阵
		CameraTarget, CameraUp);

	//计算投影矩阵
	float FovAngelY = 0.25 * XM_PI;                                            //垂直视场角
	float AspectRatio = (float)ClientWidth / (float)ClientHeight;              //宽纵比
	float NearDistance = 1.0f;                                                 //到近平面距离
	float FarDistance = 1000.0f;                                               //到远平面距离

	XMMATRIX ProjMat = XMMatrixPerspectiveFovLH(FovAngelY,                     //世界矩阵
		AspectRatio, NearDistance, FarDistance);

	//全过程整合为复合矩阵
	XMMATRIX WorldViewProjMat = XMMatrixTranspose(                             //计算复合矩阵并转置
		WorldMat * ViewMat * ProjMat);

	XMStoreFloat4x4(&WorldViewProjData, WorldViewProjMat);                     //存储复合矩阵

}

void D3DAPP::CreateConstantBuffer()
{

	UINT CBSize = (UINT)sizeof(WorldViewProjData) + 255 & ~255;                //常量元素的内存大小，必须为256的整数倍
	
	CD3DX12_RESOURCE_DESC ResourceDesc =                                       //常量资源描述结构体
		CD3DX12_RESOURCE_DESC::Buffer(CBSize);

	D3D12_HEAP_PROPERTIES CBHeapProperties =                                   //描述常量上传堆属性
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	Device->CreateCommittedResource(&CBHeapProperties,                         //创建常量上传堆（缓冲区)
		D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(ConstantUploadBuffer.GetAddressOf()));

	//将常量（投影矩阵）复制到上传堆
	void* CBSubresourcePtr = nullptr;                                          //存放常量缓冲区子资源指针
	ConstantUploadBuffer->Map(0, nullptr, &CBSubresourcePtr);                  //获得子资源指针
	memcpy(CBSubresourcePtr, &WorldViewProjData, CBSize);                      //将复合矩阵复制到CB上传堆（缓冲区）

}

void D3DAPP::CreateCBVHeap()
{
	//创建CBV堆
	D3D12_DESCRIPTOR_HEAP_DESC CBVHeapDesc;                                    //描述CB描述符堆
	CBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;                 //描述符堆类型
	CBVHeapDesc.NumDescriptors = 1;                                            //描述符数量
	CBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;             //标志：绑定命令列表上以供着色器参考
	CBVHeapDesc.NodeMask = 0;                                                  //单显卡设置为0

	Device->CreateDescriptorHeap(&CBVHeapDesc, IID_PPV_ARGS(&CBVHeap));        //创建CBV堆
	
	//创建CBV
	D3D12_GPU_VIRTUAL_ADDRESS CBAddress =                                      //常量缓冲区虚拟地址
		ConstantUploadBuffer->GetGPUVirtualAddress();

	UINT CBSize = (UINT)sizeof(WorldViewProjData) + 255 & ~255;                //常量元素的内存大小，必须为256的整数倍

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc;                                   //描述CBV
	CBVDesc.BufferLocation = CBAddress;                                        //子资源地址
	CBVDesc.SizeInBytes = CBSize;                                              //子资源大小

	Device->CreateConstantBufferView(&CBVDesc,                                 //在描述符堆首创建复合矩阵资源的描述符
		CBVHeap->GetCPUDescriptorHandleForHeapStart());

	//将CBV堆设置到命令列表上
	ID3D12DescriptorHeap* CBVHeaps[] = { CBVHeap.Get() };                      //创建CBV堆数组

	CmdList->SetDescriptorHeaps(1,                                             //CBV堆数量
		CBVHeaps);                                                             //CBV堆数组
}

void D3DAPP::CreateRootSignature()
{
	//创建描述符表的描述符范围
	CD3DX12_DESCRIPTOR_RANGE CBVTalbe;                                         //创建描述符范围
	CBVTalbe.Init(                                                             //初始化
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,                                       //描述符范围类型：CBV
		1,                                                                     //描述符数量：1
		0);                                                                    //绑定此描述符区域的基准着色器
	
	//创建根参数，初始化为描述符表
	CD3DX12_ROOT_PARAMETER RootParameters[1];                                  //根参数数组，只有一个根参数

	RootParameters[0].InitAsDescriptorTable(                                   //根参数数组唯一的根参数初始化为描述符表
		1,                                                                     //描述符表数量：1
		&CBVTalbe);                                                            //描述符范围数组

	//描述根签名
	CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc(                                   //描述根签名
		1,                                                                     //根参数数量：1个
		RootParameters,                                                        //根参数数组
		0,                                                                     //静态采样器数量
		nullptr,                                                               //与静态采样器有关
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);         //程序选择输入汇编器（需定义一组顶点缓冲区绑定的输入布局）

	//序列化根签名
	ComPtr<ID3DBlob> ErrorBlob = nullptr;                                      //存放根签名序列化失败的信息

	 D3D12SerializeRootSignature(                                               //序列化根签名
		&RootSigDesc,                                                          //根签名描述结构体地址
		D3D_ROOT_SIGNATURE_VERSION_1,                                          //根签名版本
		SerializedRootSig.GetAddressOf(), ErrorBlob.GetAddressOf());           //返回序列化后的根签名源数据，和错误信息

	//创建根签名
	Device->CreateRootSignature(0,                                             //单GPU设置为0
		SerializedRootSig->GetBufferPointer(),                                 //序列化后根签名数据地址
		SerializedRootSig->GetBufferSize(),                                    //根签名源数据大小
		IID_PPV_ARGS(&RootSignature));

	//还没绑定到命令列表上

}

void D3DAPP::CompileVertexShader()
{
	//编译顶点着色器
	ComPtr<ID3DBlob> Error;                                                    //存放编译后的错误信息

	D3DCompileFromFile(                                                        //编译着色器
		L"shader\\color.hlsl",                                                 //着色器文件名
		NULL,                                                                  //宏定义数组，不用设置为NULL
		NULL,                                                                  //指向处理包含文件的ID3DInclude接口的指针，不用设置为NULL
		"VS",                                                                  //着色器函数名
		"vs_5_0",                                                              //着色器类型版本：5.0版本的顶点着色器
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,                                          //编译器跳过优化阶段
		0,
		&VSByteCode,                                                           //返回编译好的顶点着色器字节码
		&Error);                                                               //返回错误信息

	//描述顶点着色器字节码
	VSByteCodeDesc.pShaderBytecode = VSByteCode->GetBufferPointer();           //顶点着色器字节码内存快地址
	VSByteCodeDesc.BytecodeLength = VSByteCode->GetBufferSize();               //顶点着色器字节码大小

}

void D3DAPP::CompilePixelShader()
{
	//编译像素着色器
	ComPtr<ID3DBlob> Error;
	D3DCompileFromFile(L"shader\\color.hlsl", NULL, NULL, "PS", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PSByteCode, &Error);
	
	//描述像素着色器编译后字节码，用以绑定流水线
	PSByteCodeDesc.pShaderBytecode = PSByteCode->GetBufferPointer();
	PSByteCodeDesc.BytecodeLength = PSByteCode->GetBufferSize();
}

void D3DAPP::CreatePSO()
{
	//VSByteCodeDesc.BytecodeLength = VSByteCode->GetBufferSize();               //顶点着色器字节码大小

	//配置流水线状态对象
	//ZeroMemory(&PSODesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));        //？？？用0填充流水线描述结构体区域
	PSODesc.pRootSignature = RootSignature.Get();                              //绑定根签名
	PSODesc.VS = VSByteCodeDesc;                                               //填入顶点着色器字节码描述结构体，绑定顶点着色器
	PSODesc.PS = PSByteCodeDesc;                                               //类上，像素着色器
	PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);          //使用默认光栅化
	PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                    //混合状态：默认
	PSODesc.SampleMask = UINT_MAX;                                             //多重采样相关，对所有采样点都进行采样
	PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);     //深度、模板状态：默认
	PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;    //图元拓扑类型：三角形
	PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                        //渲染目标格式的类型值数组
	PSODesc.NumRenderTargets = 1;                                              //渲染目标格式数组元素数量
	PSODesc.SampleDesc.Count = 1;                                              //每个像素多重采样数
	PSODesc.SampleDesc.Quality = 0;                                            //多重采样质量
	PSODesc.DSVFormat = DSFormat;                                              //深度模板缓冲区格式
	PSODesc.InputLayout = IAInputDesc;                                         //输入布局描述

	Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PSO));
}

void D3DAPP::Draw()
{
	//命令列表和命令分配器复用

	//绑定流水线状态
	CmdList->SetPipelineState(PSO.Get());

	//后台缓冲区从呈现状态转为渲染目标状态
	CD3DX12_RESOURCE_BARRIER BackBufferBarrier1 =                              //后台缓冲区屏障
		CD3DX12_RESOURCE_BARRIER::Transition(SwapChainBuffer[0].Get(),
		D3D12_RESOURCE_STATE_PRESENT, 
			D3D12_RESOURCE_STATE_RENDER_TARGET);

	CmdList->ResourceBarrier(1, &BackBufferBarrier1);                          //转换屏障


	//资源初始化
	//后台缓冲区初始化，用指定数值（颜色）清除后台缓冲区，背景颜色将呈现为指定颜色
	CmdList->ClearRenderTargetView(                                            //将渲染目标中所有元素设为一个值
		CD3DX12_CPU_DESCRIPTOR_HANDLE(                                         //获得待清除源的描述符堆句柄
			RTVHeap->GetCPUDescriptorHandleForHeapStart(),                     //资源描述符堆句柄
			0,                                                                 //递增的描述符数
			RTVSize),                                                          //每个描述符递增量（包括填充）
		Colors::LightSteelBlue,                                                //用于清除资源的数值（颜色）
		0,                                                                     //下一个参数数组中元素（矩形）的数量
		nullptr);                                                              //用于清除资源视图中矩形的结构体，空指针表示清除整个资源

	//深度模板缓冲区初始化，用指定数值清除深度模板缓冲区，将后台缓冲区像素的深度值指定为1，即最远
	CmdList->ClearDepthStencilView(
		DSVHeap->GetCPUDescriptorHandleForHeapStart(),                         //待清除资源描述符堆句柄
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,                     //标志，表示要清除的数据类型（深度/模板缓冲区）：深度缓冲区和模板缓冲区都清除
		1.0f,                                                                  //用于清除深度缓冲区的值，1.0f表示初始化像素在空间最远端
		0,                                                                     //用于清除模板缓冲区的值
		0,                                                                     //类似后台缓冲区初始化
		nullptr);                                                              //同上

	//指定后台缓冲区和深度模板缓冲区
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE DSVHeapHandle = DSVHeap->GetCPUDescriptorHandleForHeapStart();

	CmdList->OMSetRenderTargets(1,                                             //RTV堆数量
		&RTVHeapHandle,                                                        //RTV堆句柄数组
		TRUE,                                                                  //RTV堆数组存储方式：顺序存储
		&DSVHeapHandle);                                                       //DSV堆句柄地址

	//将CB设置到命令列表上，已在前面完成

	//将根签名绑定到命令列表上
	CmdList->SetGraphicsRootSignature(RootSignature.Get());

	//顶点缓冲区绑定到渲染流水线上
	CmdList->IASetVertexBuffers(0,                                             //绑定缓冲区用的起始输入槽索引
		1,                                                                     //要绑定的顶点缓冲区数量
		&VertexBufferView);                                                    //指向顶点缓冲区描述符的指针

	//索引缓冲区绑定到渲染流水线上
	CmdList->IASetIndexBuffer(&IndexBufferView);

	//指定图元拓扑
	CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);      //三角形列表

	//将根签名和资源绑定
	CmdList->SetGraphicsRootDescriptorTable(0,                                 //绑定到0号寄存器槽
		CBVHeap->GetGPUDescriptorHandleForHeapStart());                        //描述要绑定的资源的GPU句柄

	//以索引绘制
	CmdList->DrawIndexedInstanced(
		(UINT)IndexCount, 1, 0, 0, 0);


	//后台缓冲区从渲染目标状态转为呈现状态
	CD3DX12_RESOURCE_BARRIER BackBufferBarrier2 =CD3DX12_RESOURCE_BARRIER::Transition(
		SwapChainBuffer[0].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	CmdList->ResourceBarrier(1, &BackBufferBarrier2);

	//结束命令记录并提交执行
	CmdList->Close();                                                          //关闭命令列表                       
	ID3D12CommandList* CmdLists[] = { CmdList.Get() };                         //命令列表数组
	CmdQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);               //提交命令列表

	//呈现图像
	SwapChain->Present(0, 0);
}



//辅助函数，将ID3D12Resource资源从上传堆上传到默认堆
ComPtr<ID3D12Resource> D3DAPP::CreateDefaultBuffer(ID3D12Device* Device, 
	UINT64 ResourceSize, const void* ResourceData, 
	ID3D12GraphicsCommandList* CmdList,ComPtr<ID3D12Resource> &UploadBuffer)
{
	CD3DX12_RESOURCE_DESC ResourceDesc =                                     //放入堆中的资源的描述符
		CD3DX12_RESOURCE_DESC::Buffer(ResourceSize);

	//创建资源和上传堆，将资源映射到上传堆
	//D3D12_HEAP_PROPERTIES UploadBufferProperties =                           //描述上传堆属性
		CD3DX12_HEAP_PROPERTIES UploadBufferProperties(D3D12_HEAP_TYPE_UPLOAD);
		
	Device->CreateCommittedResource(                                         //创建堆
		&UploadBufferProperties,                                             //类型：上传堆
		D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,                                   //资源初始状态：上传堆所需的起始状态，
		nullptr, IID_PPV_ARGS(UploadBuffer.GetAddressOf()));
	
	//创建默认堆
	ComPtr<ID3D12Resource> DefaultBuffer;                                    //默认堆接口
	
	D3D12_HEAP_PROPERTIES DefaultBufferProperties =                          //描述默认堆属性
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	
	Device->CreateCommittedResource(                                         //创建堆
		&DefaultBufferProperties,                                            //类型：默认堆
		D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,                                         //资源初始状态：在COPY队列前（之前用于DIRECT/COMPUTE队列时）必须处于此状态，（还有其它作用）
		nullptr, IID_PPV_ARGS(DefaultBuffer.GetAddressOf()));

	//描述要复制到默认缓冲区的数据
	D3D12_SUBRESOURCE_DATA SubResourceDesc = {};
	SubResourceDesc.pData = ResourceData;                                    //指向资源的指针
	SubResourceDesc.RowPitch = ResourceSize;                                 //资源字节数
	SubResourceDesc.SlicePitch = SubResourceDesc.RowPitch;                   //资源字节数（先就这么算）
    
	//将默认堆的资源状态，从默认状态转为复制操作的目标状态
	CD3DX12_RESOURCE_BARRIER DefaultBufferBarrier =                          //返回屏资源屏障的描述
		CD3DX12_RESOURCE_BARRIER::Transition(                                //转换资源的状态
			DefaultBuffer.Get(),                                             //资源地址
			D3D12_RESOURCE_STATE_COMMON,                                     //资源转换前状态
			D3D12_RESOURCE_STATE_COPY_DEST);                                 //资源转换后状态

	CmdList->ResourceBarrier(1,                                              //提交的屏障描述的数量
		&DefaultBufferBarrier);                                              //指向屏障描述数组的指针

    //更新子资源
	UpdateSubresources<1>                                                    //最大子资源数量为1
		(CmdList, DefaultBuffer.Get(), UploadBuffer.Get(),                   //命令列表；复制目标资源；复制源资源
			0, 0, 1,                                                         //到中间资源偏移量；第一个子资源索引，资源中子资源数量
			&SubResourceDesc);                                               //待更新子资源描述结构的地址的数组

    //将默认堆的资源状态，从复制操作的目标状态转为
	CD3DX12_RESOURCE_BARRIER DefaultBufferBarrier2 =                         //返回屏资源屏障的描述
		CD3DX12_RESOURCE_BARRIER::Transition(                                //转换资源的状态
			DefaultBuffer.Get(),                                             //资源地址
			D3D12_RESOURCE_STATE_COPY_DEST,                                  //资源转换前状态
			D3D12_RESOURCE_STATE_GENERIC_READ);                              //资源转换后状态

	CmdList->ResourceBarrier(1,     //提交的屏障描述的数量
		&DefaultBufferBarrier2);    //指向屏障描述数组的指针

	return DefaultBuffer;
}