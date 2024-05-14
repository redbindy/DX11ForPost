#include <Windows.h>
#include <assert.h>

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 디버깅용 정적 콘솔 연결
#pragma comment(linker, "/entry:wWinMainCRTStartup /subsystem:console")

LRESULT CALLBACK WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam);
void InitializeD3D11();
void DestroyD3D11();
void Render();

HINSTANCE ghInstance;
HWND ghWnd;

const TCHAR* WINDOW_NAME = TEXT("DX11 Sample");

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					  _In_opt_ HINSTANCE hPrevInstance,
					  _In_ LPWSTR    lpCmdLine,
					  _In_ int       nCmdShow)
{
	ghInstance = hInstance;

	// 윈도우 클래스 정의 및 등록
	WNDCLASS windowClass;
	ZeroMemory(&windowClass, sizeof(WNDCLASS));

	windowClass.lpfnWndProc = WndProc; // 콜백함수 등록
	windowClass.lpszClassName = WINDOW_NAME; // 클래스 이름 지정
	windowClass.hCursor = (HCURSOR)LoadCursor(nullptr, IDC_ARROW); // 기본 커서 지정
	windowClass.hInstance = hInstance; // 클래스 인스턴스 지정

	RegisterClass(&windowClass);
	assert(GetLastError() == ERROR_SUCCESS);

	// 실제 사용하게 될 화면 크기 지정
	// 여기서는 디스플레이의 가로 / 2, 세로 / 2 사용 예정
	RECT windowRect;
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = GetSystemMetrics(SM_CXSCREEN) >> 1; // 디스플레이 너비 / 2
	windowRect.bottom = GetSystemMetrics(SM_CYSCREEN) >> 1; // 디스플레이 높이 / 2

	AdjustWindowRect(
		&windowRect, // 값을 받을 RECT 구조체 주소
		WS_OVERLAPPEDWINDOW, // 크기를 계산할 때 참고할 윈도우 스타일
		false // 메뉴 여부
	);
	assert(GetLastError() == ERROR_SUCCESS);

	// 윈도우 생성
	ghWnd = CreateWindow(
		WINDOW_NAME, // 클래스 이름
		WINDOW_NAME, // 윈도우 이름
		WS_OVERLAPPEDWINDOW, // 윈도우 스타일
		CW_USEDEFAULT, CW_USEDEFAULT, // (x, y)
		windowRect.right, windowRect.bottom, // 너비, 높이
		nullptr, // 부모 윈도우 지정
		nullptr, // 메뉴 지정
		hInstance, // 인스턴스 지정
		nullptr // 추가 메모리 지정
	);
	assert(GetLastError() == ERROR_SUCCESS);
	InitializeD3D11(); // 윈도우 만들고 나서 D3D 초기화

	// 윈도우 보여주기
	ShowWindow(ghWnd, nCmdShow);
	assert(GetLastError() == ERROR_SUCCESS);

	// PeekMessage를 이용한 메시지 루프
	MSG msg;
	while (true)
	{
		if (PeekMessage(
			&msg, // 메시지를 받을 주소
			nullptr, // 메시지를 받을 윈도우
			0, // 받을 메시지 범위 최솟값
			0, // 받을 메시지 범위 최댓값
			PM_REMOVE // 메시지 처리 방법
		))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg); // 메시지 문자로 해석 시도
			DispatchMessage(&msg); // 메시지 전달
		}
		else
		{
			Render();
			Sleep(1000 / 30);
		}
	}

	return (int)msg.wParam;
}

// 윈도우 콜백 함수
LRESULT CALLBACK WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY: // 윈도우 종료시 자원 해제 작업
		DestroyD3D11();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

// 번거로움 피하기
using namespace DirectX;

// 사용할 정점 구조체
// 위치 정보 외에도 다양한 데이터가 추가될 수 있음
typedef struct
{
	XMFLOAT3 pos; // 단순 (x, y, z)를 표현하기 위해 float3 사용
	// XMFLOAT4 color; // Red, Green, Blue, Alpha를 표현하기 위한 float4
	XMFLOAT3 normal; // 법선 벡터
} vertex_t;

typedef struct
{
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX projection;

	XMMATRIX transfrom; // 정점 변환용 변수
	XMMATRIX normalTransform;

	XMFLOAT4 vLightDir;
	XMFLOAT4 vLightColor;
} constant_buffer_t;

// D3D11에서 사용할 변수들
// Device: 리소스 생성
// DeviceContext: 렌더링에 사용할 정보를 담을 데이터 덩어리
// SwapChain: 버퍼를 디바이스에 렌더링하고 출력
// RenderTargetView: 리소스 뷰 중에서 렌더링 대상 용도
// Texture2D: 리소스 중 2D 버퍼 용도
// DepthStencilView: 리소스 뷰 중에서 깊이/스텐실 추적 용도
static ID3D11Device* spDevice;
static ID3D11DeviceContext* spDeviceContext;
static IDXGISwapChain* spSwapChain;
static ID3D11RenderTargetView* spRenderTargetView;
static ID3D11Texture2D* spDepthStencilBuffer;
static ID3D11DepthStencilView* spDepthStencilView;

// VertexShader: 정점 쉐이더 리소스 인터페이스
// PixelShader: 픽셀 쉐이더 리소스 인터페이스
// VertexBuffer: 정점 버퍼 리소스 인터페이스
// IndexBuffer: 인덱스 버퍼 리소스 인터페이스
static ID3D11VertexShader* spVertexShader;
static ID3D11PixelShader* spPixelShader;
static ID3D11Buffer* spVertexBuffer;
static ID3D11Buffer* spIndexBuffer;

// ConstantBuffer: 쉐이더의 전역변수용 리소스 인터페이스
static constant_buffer_t sConstantBuffer;
static ID3D11Buffer* spConstantBuffer;

void InitializeD3D11()
{
	// 디바이스 생성 관련 정보
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if DEBUG || _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif /* Debug */

	// 스왑체인에 대한 정보 초기화
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// 현재 윈도우의 크기 구하기(작업 영역)
	RECT windowRect;
	GetClientRect(ghWnd, &windowRect);
	assert(GetLastError() == ERROR_SUCCESS);

	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = windowRect.right;
	swapChainDesc.BufferDesc.Height = windowRect.bottom;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 사용할 색상 포맷
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60; // 다시 그리는 주기의 분자
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1; // 다시 그리는 주기의 분모
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 버퍼의 용도
	swapChainDesc.OutputWindow = ghWnd; // 렌더링한 결과물을 출력할 윈도우
	swapChainDesc.SampleDesc.Count = 1; // 픽셀당 샘플링하는 수
	swapChainDesc.SampleDesc.Quality = 0; // 이미지의 품질 수준
	swapChainDesc.Windowed = true; // 창모드 설정

	// 디바이스, 디바이스 컨텍스트, 스왑체인 생성
	// 최신 문서는 다른 방식으로 생성 권장 중
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, // 어댑터 포인터
		D3D_DRIVER_TYPE_HARDWARE, // 사용할 드라이버
		nullptr, // 레스터라이저의 주소
		creationFlags, // 생성 플래그
		nullptr, // 지원할 버전 정보 배열
		0, // 버전 정보 배열의 길이
		D3D11_SDK_VERSION, // D3D SDK 버전
		&swapChainDesc, // 스왑체인 정보 구조체, 
		&spSwapChain, // 생성된 스왑체인의 포인터를 받을 주소
		&spDevice, // 생성된 디바이스의 포인터를 받을 주소
		nullptr, // 생성된 버전 정보를 받을 주소
		&spDeviceContext // 생성된 디바이스 컨텍스트의 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 렌더 타겟으로 사용할 버퍼 갖고 오기
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = spSwapChain->GetBuffer(
		0, // 사용할 버퍼 번호
		__uuidof(ID3D11Texture2D), // 버퍼를 해석할 때 사용할 인터페이스
		(void**)&pBackBuffer // 버퍼 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 렌더 타겟 뷰 생성
	hr = spDevice->CreateRenderTargetView(
		pBackBuffer, // 사용할 리소스 포인터
		nullptr, // 렌더 타겟 뷰 정보 구조체 포인터
		&spRenderTargetView // 만들어진 렌터 타겟 뷰의 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));
	pBackBuffer->Release();
	pBackBuffer = nullptr;

	// 깊이/스텐실 버퍼 정보 초기화
	D3D11_TEXTURE2D_DESC depthBufferDesc;

	depthBufferDesc.Width = windowRect.right; // 텍스쳐 너비
	depthBufferDesc.Height = windowRect.bottom; // 텍스쳐 높이
	depthBufferDesc.MipLevels = 1; // mipmap 수준(level)의 최대 수
	depthBufferDesc.ArraySize = 1; // 텍스쳐 배열에서 텍스쳐의 수
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 텍스쳐의 데이터 형식
	depthBufferDesc.SampleDesc.Count = 1; // 픽셀당 샘플링하는 수
	depthBufferDesc.SampleDesc.Quality = 0; // 이미지 품질 수준
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT; // 텍스쳐의 용도
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL; // 파이프라인에 뭘로 바인딩 할지
	depthBufferDesc.CPUAccessFlags = 0; // CPU의 접근 권한
	depthBufferDesc.MiscFlags = 0; // 리소스에 대한 옵션

	// 깊이/스텐실용 리소스 생성
	hr = spDevice->CreateTexture2D(
		&depthBufferDesc, // 텍스쳐 정보 구조체
		nullptr,  // 서브리소스 데이터
		&spDepthStencilBuffer // 생성된 텍스쳐의 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 깊이/스텐실 뷰 정보 초기화
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

	depthStencilViewDesc.Format = depthBufferDesc.Format; // 뷰의 데이터 형식
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D; // 리소스의 자료형
	depthStencilViewDesc.Flags = 0; // 텍스쳐가 읽기 전용인지를 나타내는 플래그
	depthStencilViewDesc.Texture2D.MipSlice = 0; // 첫 번째로 사용할 mipmap 수준의 색인

	// 깊이/스텐실 뷰 생성
	hr = spDevice->CreateDepthStencilView(
		spDepthStencilBuffer, // 사용할 리소스 포인터
		&depthStencilViewDesc, // 깊이/스텐실 정보 구조체
		&spDepthStencilView // 깊이/스텐실 뷰 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 렌더 타겟을 파이프라인 OM 단계에 설정
	spDeviceContext->OMSetRenderTargets(
		1, // 넣을 뷰의 수
		&spRenderTargetView, // 렌터 타겟 포인터 배열
		spDepthStencilView // 깊이 스텐실 뷰 포인터
	);

	// 뷰포트 정보 초기화
	D3D11_VIEWPORT viewPort;
	viewPort.Width = (FLOAT)windowRect.right;
	viewPort.Height = (FLOAT)windowRect.bottom;
	viewPort.MinDepth = 0.f;
	viewPort.MaxDepth = 1.f;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;

	// 뷰 포트 설정
	spDeviceContext->RSSetViewports(
		1, // 뷰포트의 수
		&viewPort // 정보 구조체 포인터
	);

	// 쉐이더, 컴파일 오류를 받을 이진 개체
	ID3DBlob* pVertexShaderBlob = nullptr;
	ID3DBlob* pPixelShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	// 정점 쉐이더 컴파일
	hr = D3DCompileFromFile(
		TEXT("VertexShader.hlsl"), // 쉐이더 코드 파일 이름
		nullptr, // 쉐이더 매크로를 정의하는 구조체 포인터
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // 쉐이더 컴파일러가 include 파일 처리에 사용하는 인터페이스 포인터
		"main", // 진입점 이름
		"vs_5_0", // 컴파일 대상
		0, // 컴파일 옵션
		0, // 컴파일 옵션2
		&pVertexShaderBlob, // 컴파일된 쉐이더 데이터 포인터를 받을 주소
		&pErrorBlob // 컴파일 에러 데이터 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 정점 쉐이더 리소스 생성
	hr = spDevice->CreateVertexShader(
		pVertexShaderBlob->GetBufferPointer(), // 컴파일된 쉐이더 데이터 포인터
		pVertexShaderBlob->GetBufferSize(), // 쉐이더 데이터의 길이
		nullptr, // 쉐이더 동적링크 관련 인터페이스 포인터
		&spVertexShader // 정점 쉐이더 인터페이스를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 픽셀 쉐이더 컴파일
	hr = D3DCompileFromFile(
		TEXT("PixelShader.hlsl"), // 쉐이더 코드 파일 이름
		nullptr, // 쉐이더 매크로를 정의하는 구조체 포인터
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // 쉐이더 컴파일러가 include 파일 처리에 사용하는 인터페이스 포인터
		"main", // 진입점 이름
		"ps_5_0", // 컴파일 대상
		0, // 컴파일 옵션
		0, // 컴파일 옵션2
		&pPixelShaderBlob, // 컴파일된 쉐이더 데이터 포인터를 받을 주소
		&pErrorBlob // 컴파일 에러 데이터 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 픽셀 쉐이더 리소스 생성
	hr = spDevice->CreatePixelShader(
		pPixelShaderBlob->GetBufferPointer(), // 컴파일된 쉐이더 데이터 포인터
		pPixelShaderBlob->GetBufferSize(), // 쉐이더 데이터의 길이
		nullptr, // 쉐이더 동적링크 관련 인터페이스 포인터
		&spPixelShader // 정점 쉐이더 인터페이스를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// GPU에게 정점의 정보를 알려주기 위한 구조체 초기화
	// 구조체에 포함된 각 요소별로 이 작업을 진행해서 넘겨줘야함
	D3D11_INPUT_ELEMENT_DESC vertexLayoutDescForPos;

	vertexLayoutDescForPos.SemanticName = "POSITION"; // 해당 데이터의 용도
	vertexLayoutDescForPos.SemanticIndex = 0; // 용도가 겹칠 경우 사용할 색인 번호
	vertexLayoutDescForPos.Format = DXGI_FORMAT_R32G32B32_FLOAT; // 입력 데이터 형식
	vertexLayoutDescForPos.InputSlot = 0; // 버퍼의 슬롯 번호
	vertexLayoutDescForPos.AlignedByteOffset = 0; // 구조체에서 요소의 시작 위치(바이트 단위)
	vertexLayoutDescForPos.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA; // 입력 데이터의 분류법
	vertexLayoutDescForPos.InstanceDataStepRate = 0; // 인스턴싱에 사용할 배수

	//D3D11_INPUT_ELEMENT_DESC vertexLayoutDescForColor;

	//vertexLayoutDescForColor.SemanticName = "COLOR"; // 해당 데이터의 용도
	//vertexLayoutDescForColor.SemanticIndex = 0; // 용도가 겹칠 경우 사용할 색인 번호
	//vertexLayoutDescForColor.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 입력 데이터 형식
	//vertexLayoutDescForColor.InputSlot = 0; // 버퍼의 슬롯 번호
	//vertexLayoutDescForColor.AlignedByteOffset = sizeof(vertex_t::pos); // 구조체에서 요소의 시작 위치(바이트 단위)
	//vertexLayoutDescForColor.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA; // 입력 데이터의 분류법
	//vertexLayoutDescForColor.InstanceDataStepRate = 0; // 인스턴싱에 사용할 배수

	D3D11_INPUT_ELEMENT_DESC vertexLayoutDescForNormal;

	vertexLayoutDescForNormal.SemanticName = "NORMAL"; // 해당 데이터의 용도
	vertexLayoutDescForNormal.SemanticIndex = 0; // 용도가 겹칠 경우 사용할 색인 번호
	vertexLayoutDescForNormal.Format = DXGI_FORMAT_R32G32B32_FLOAT; // 입력 데이터 형식
	vertexLayoutDescForNormal.InputSlot = 0; // 버퍼의 슬롯 번호
	vertexLayoutDescForNormal.AlignedByteOffset = sizeof(vertex_t::pos); // 구조체에서 요소의 시작 위치(바이트 단위)
	vertexLayoutDescForNormal.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA; // 입력 데이터의 분류법
	vertexLayoutDescForNormal.InstanceDataStepRate = 0; // 인스턴싱에 사용할 배수

	// 파이프라인에 전송할 레이아웃 배열
	D3D11_INPUT_ELEMENT_DESC layoutArr[] = {
		vertexLayoutDescForPos,
		// vertexLayoutDescForColor
		vertexLayoutDescForNormal
	};

	// 파이프라인에서 사용할 레이아웃 생성
	ID3D11InputLayout* pVertexLayout = nullptr;
	hr = spDevice->CreateInputLayout(
		layoutArr, // 레이아웃 배열
		ARRAYSIZE(layoutArr), // 배열의 길이
		pVertexShaderBlob->GetBufferPointer(), // 컴파일된 쉐이더 데이터 포인터
		pVertexShaderBlob->GetBufferSize(), // 쉐이더 데이터의 길이
		&pVertexLayout // 생성된 레이아웃 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 정점 레이아웃 설정
	spDeviceContext->IASetInputLayout(pVertexLayout);

	pVertexShaderBlob->Release();
	pVertexShaderBlob = nullptr;

	pPixelShaderBlob->Release();
	pPixelShaderBlob = nullptr;

	pVertexLayout->Release();
	pVertexLayout = nullptr;

	// 전송할 정점 배열
	vertex_t vertices[] = {
		{ { 1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f } }, // 정면 
		{ { 1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f } },
		{ { -1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f } },
		{ { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f } },

		{ { 1.f, 1.f, -1.f }, { 1.f, 0.f, 0.f } }, // 우측 
		{ { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f } },
		{ { 1.f, 1.f, 1.f }, { 1.f, 0.f, 0.f } },
		{ { 1.f, -1.f, 1.f }, { 1.f, 0.f, 0.f } },

		{ { 1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f } }, // 뒷면 
		{ { 1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f } },
		{ { -1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f } },
		{ { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f } },

		{ { -1.f, 1.f, -1.f }, { -1.f, 0.f, 0.f } }, // 좌측 
		{ { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f } },
		{ { -1.f, 1.f, 1.f }, { -1.f, 0.f, 0.f } },
		{ { -1.f, -1.f, 1.f }, { -1.f, 0.f, 0.f } },

		{ { 1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f } }, // 윗면 
		{ { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f } },
		{ { 1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f } },
		{ { -1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f } },

		{ { 1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f } }, // 아랫면
		{ { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f } },
		{ { 1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f } },
		{ { -1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f } },
	};

	// 정점 버퍼에 대한 정보 구조체 초기화
	D3D11_BUFFER_DESC bufferDesc;

	bufferDesc.ByteWidth = sizeof(vertices); // 버퍼의 바이트 크기
	bufferDesc.Usage = D3D11_USAGE_DEFAULT; // 버퍼의 용도
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 파이프라인에 뭘로 바인딩 할지
	bufferDesc.CPUAccessFlags = 0; // CPU의 접근 권한
	bufferDesc.MiscFlags = 0; // 리소스에 대한 옵션
	bufferDesc.StructureByteStride = sizeof(vertex_t); // 각 요소별 바이트 크기

	// 초기화할 정점 버퍼 서브리소스 구조체
	D3D11_SUBRESOURCE_DATA vertexSubresource;

	vertexSubresource.pSysMem = vertices; // 전송할 데이터 포인터
	vertexSubresource.SysMemPitch = 0; // 다음 행으로 가기 위한 시스템 바이트 수
	vertexSubresource.SysMemSlicePitch = 0; // 다음 면으로 가기 위한 시스템 바이트 수

	// 정점 버퍼 생성
	hr = spDevice->CreateBuffer(
		&bufferDesc, // 버퍼 정보 구조체 포인터
		&vertexSubresource, // 정점 서브 리소스 포인터
		&spVertexBuffer // 만들어진 버퍼 리소스 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 파이프라인에 정점 버퍼 설정
	UINT stride = sizeof(vertex_t);
	UINT offset = 0;

	spDeviceContext->IASetVertexBuffers(
		0, // 슬롯 번호
		1, // 버퍼의 수
		&spVertexBuffer, // 정점 버퍼 주소
		&stride, // 요소별 크기 배열
		&offset // 각 버퍼별 시작 오프셋 배열
	);

	// 삼각형 그리는 방법 설정
	spDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 인덱스 데이터 정의
	WORD indice[] = {
		0, 1, 2, // 정면
		2, 1, 3,

		4, 6, 7, // 우측면
		7, 5, 4, 

		8, 10, 9, // 뒷면
		10, 11, 9, 

		12, 13, 15, // 좌측면
		15, 14, 12, 

		16, 17, 19, // 윗면
		19, 18, 16, 

		20, 22, 21, // 아랫면
		21, 22, 23
	};

	// 인덱스 버퍼에 대한 정보 구조체 초기화
	D3D11_BUFFER_DESC indexBufferDesc;

	indexBufferDesc.ByteWidth = sizeof(indice); // 버퍼의 바이트 크기
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT; // 버퍼의 용도
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 파이프라인에 뭘로 바인딩 할지
	indexBufferDesc.CPUAccessFlags = 0; // CPU의 접근 권한
	indexBufferDesc.MiscFlags = 0; // 리소스에 대한 옵션
	indexBufferDesc.StructureByteStride = sizeof(WORD); // 각 요소별 바이트 크기

	// 초기화할 인덱스 버퍼 서브리소스 구조체
	D3D11_SUBRESOURCE_DATA indexSubresource;

	indexSubresource.pSysMem = indice; // 전송할 데이터 포인터
	indexSubresource.SysMemPitch = 0; // 다음 행으로 가기 위한 시스템 바이트 수
	indexSubresource.SysMemSlicePitch = 0; // 다음 면으로 가기 위한 시스템 바이트 수

	// 인덱스 버퍼 생성
	hr = spDevice->CreateBuffer(
		&indexBufferDesc, // 버퍼 정보 구조체 포인터
		&indexSubresource, // 인덱스 서브 리소스 포인터
		&spIndexBuffer // 만들어진 버퍼 리소스 포인터를 받을 주소
	);
	assert(SUCCEEDED(hr));

	// 파이프라인에 인덱스 버퍼 설정
	spDeviceContext->IASetIndexBuffer(
		spIndexBuffer, // 설정할 인덱스 버퍼 포인터
		DXGI_FORMAT_R16_UINT, // 인덱스 버퍼의 형식
		0 // 시작 오프셋
	);

	// 회전에 사용할 변수 초기화
	sConstantBuffer.transfrom = XMMatrixIdentity();
	sConstantBuffer.normalTransform = XMMatrixIdentity();

	// 월드 변환 행렬 초기화
	sConstantBuffer.world = XMMatrixIdentity();

	// 뷰 변환 행렬 초기화
	XMVECTOR eyePosition = XMVectorSet(0.f, 3.f, -5.f, 0.f);
	XMVECTOR focusPosition = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMVECTOR upDirection = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	sConstantBuffer.view = XMMatrixLookAtLH(
		eyePosition, // 눈(= 카메라)의 위치
		focusPosition, // 초점의 위치(= 시선의 위치)
		upDirection // 카메라의 위쪽 방향
	);

	sConstantBuffer.view = XMMatrixTranspose(sConstantBuffer.view);

	// 투영 변환 행렬 초기화
	sConstantBuffer.projection = XMMatrixPerspectiveFovLH(
		XM_PIDIV2, // 하양식 시야각(= y축과 평행한 시야각)
		windowRect.right / (float)windowRect.bottom, // 종횡비 비율 값 = 너비 / 높이
		0.01f, // 절두체 근평면의 위치
		100.f // 절두체 원평면의 위치
	);

	sConstantBuffer.projection = XMMatrixTranspose(sConstantBuffer.projection);

	// 광원 정보 초기화
	sConstantBuffer.vLightColor = { 1.f, 1.f, 1.f, 1.f };
	XMVECTOR lightDir = XMVector4Normalize(eyePosition - focusPosition);
	XMStoreFloat4(&sConstantBuffer.vLightDir, lightDir);

	// 상수 버퍼에 대한 정보 구조체 초기화
	D3D11_BUFFER_DESC constantBufferDesc;

	constantBufferDesc.ByteWidth = sizeof(sConstantBuffer); // 버퍼의 바이트 크기
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT; // 버퍼의 용도
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // 파이프라인에 뭘로 바인딩 할지
	constantBufferDesc.CPUAccessFlags = 0; // CPU의 접근 권한
	constantBufferDesc.MiscFlags = 0; // 리소스에 대한 옵션
	constantBufferDesc.StructureByteStride = sizeof(XMMATRIX); // 각 요소별 바이트 크기

	// 초기화할 상수 버퍼 서브리소스 구조체
	D3D11_SUBRESOURCE_DATA constantBufferSubresource;

	constantBufferSubresource.pSysMem = &sConstantBuffer; // 전송할 데이터 포인터
	constantBufferSubresource.SysMemPitch = 0; // 다음 행으로 가기 위한 시스템 바이트 수
	constantBufferSubresource.SysMemSlicePitch = 0; // 다음 면으로 가기 위한 시스템 바이트 수

	// 인덱스 버퍼 생성
	hr = spDevice->CreateBuffer(
		&constantBufferDesc, // 버퍼 정보 구조체 포인터
		&constantBufferSubresource, // 상수 버퍼 서브 리소스 포인터
		&spConstantBuffer // 만들어진 버퍼 리소스 포인터를 받을 주소
	);

	// 상수 버퍼 설정
	spDeviceContext->VSSetConstantBuffers(
		0, // 시작 슬롯
		1, // 설정할 버퍼의 수
		&spConstantBuffer // 설정할 버퍼 리소스 배열
	);

	spDeviceContext->PSSetConstantBuffers(
		0, // 시작 슬롯
		1, // 설정할 버퍼의 수
		&spConstantBuffer // 설정할 버퍼 리소스 배열
	);


}

void DestroyD3D11()
{
	if (spDepthStencilView != nullptr)
	{
		spDepthStencilView->Release();
		spDepthStencilView = nullptr;
	}

	if (spDepthStencilBuffer != nullptr)
	{
		spDepthStencilBuffer->Release();
		spDepthStencilBuffer = nullptr;
	}

	if (spConstantBuffer != nullptr)
	{
		spConstantBuffer->Release();
		spConstantBuffer = nullptr;
	}

	if (spIndexBuffer != nullptr)
	{
		spIndexBuffer->Release();
		spIndexBuffer = nullptr;
	}

	if (spVertexBuffer != nullptr)
	{
		spVertexBuffer->Release();
		spVertexBuffer = nullptr;
	}

	if (spPixelShader != nullptr)
	{
		spPixelShader->Release();
		spPixelShader = nullptr;
	}

	if (spVertexShader != nullptr)
	{
		spVertexShader->Release();
		spVertexShader = nullptr;
	}

	if (spRenderTargetView != nullptr)
	{
		spRenderTargetView->Release();
		spRenderTargetView = nullptr;
	}

	if (spSwapChain != nullptr)
	{
		spSwapChain->Release();
		spSwapChain = nullptr;
	}

	if (spDeviceContext != nullptr)
	{
		spDeviceContext->ClearState();
		spDeviceContext->Release();
		spDeviceContext = nullptr;
	}

	if (spDevice != nullptr)
	{
		spDevice->Release();
		spDevice = nullptr;
	}
}

void Render()
{
	const FLOAT clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.f };

	assert(spRenderTargetView != nullptr);
	spDeviceContext->ClearRenderTargetView(
		spRenderTargetView, // 대상 렌더 타겟 뷰
		clearColor // 채워 넣을 색상
	);

	spDeviceContext->ClearDepthStencilView(
		spDepthStencilView, // 대상 깊이/스텐실 뷰
		D3D11_CLEAR_DEPTH, // 지울 데이터 형식
		1.f, // 채워 넣을 깊이값
		UINT8_MAX // 채워 넣을 스텐실 값
	);

	// 쉐이더 설정
	spDeviceContext->VSSetShader(
		spVertexShader, // 설정할 쉐이더 인터페이스 포인터
		nullptr, // HLSL 클래스를 캡슐화하는 인터페이스 포인터 배열
		0 // 배열의 길이
	);

	spDeviceContext->PSSetShader(
		spPixelShader, // 설정할 쉐이더 인터페이스 포인터
		nullptr, // HLSL 클래스를 캡슐화하는 인터페이스 포인터 배열
		0 // 배열의 길이
	);

	// 회전 행렬용 정적 각도 값
	const float DELTA_THETA = XM_PI / 36;
	static float theta = 0;

	theta += DELTA_THETA;
	// 다음 변환 가중치 계산
	constant_buffer_t constantBufferForCenterCube = sConstantBuffer;
	XMMATRIX rotationTransform = XMMatrixRotationY(theta);

	// HLSL 계산 방식으로 인한 전치
	constantBufferForCenterCube.transfrom = XMMatrixTranspose(rotationTransform);
	constantBufferForCenterCube.normalTransform = constantBufferForCenterCube.transfrom;

	// 바인딩한 리소스 업데이트
	spDeviceContext->UpdateSubresource(
		spConstantBuffer, // 업데이트할 서브리소스 포인터
		0, // 업데이트할 서브리소스 번호
		nullptr, // 서브리소스 선택 박스 포인터
		&constantBufferForCenterCube, // 업데이트에 사용할 데이터
		0, // 다음 행까지의 시스템 메모리 바이트 수
		0 // 다음 깊이까지의 시스템 메모리 바이트 수
	);

	spDeviceContext->DrawIndexed(
		36, // 그릴 인덱스의 수
		0, // 첫 인덱스를 읽을 위치
		0 // 정점 버퍼로부터 정점을 읽기 전에 각 인덱스에 더할 값
	);

	constant_buffer_t constantBufferForOrbitCube = sConstantBuffer;

	XMMATRIX scaleTransform = XMMatrixScaling(
		0.5f, // x축 방향 확대/축소 값
		0.5f, // y축 방향 확대/축소 값
		0.5f // z축 방향 확대/축소 값
	);

	// XMMATRIX rotationOrbitCube = XMMatrixRotationY(theta);
	XMMATRIX rotationOrbitCube = XMMatrixRotationY(theta * 2.f);

	XMMATRIX translationTransform = XMMatrixTranslation(
		3.f, // x축 변위
		0.f, // y축 변위
		0.f // z축 변위
	);

	XMMATRIX orbitTransform = XMMatrixRotationY(-theta);

	constantBufferForOrbitCube.transfrom = XMMatrixTranspose(scaleTransform * rotationOrbitCube * translationTransform * orbitTransform);
	constantBufferForOrbitCube.normalTransform = XMMatrixTranspose(rotationTransform);

	// constantBufferForOrbitCube.transfrom = XMMatrixTranspose(scaleTransform * rotationOrbitCube * translationTransform * orbitTransform * scaleTransform);
	// constantBufferForOrbitCube.transfrom = XMMatrixTranspose(scaleTransform * rotationOrbitCube * translationTransform * orbitTransform * translationTransform);

	// 바인딩한 리소스 업데이트
	spDeviceContext->UpdateSubresource(
		spConstantBuffer, // 업데이트할 서브리소스 포인터
		0, // 업데이트할 서브리소스 번호
		nullptr, // 서브리소스 선택 박스 포인터
		&constantBufferForOrbitCube, // 업데이트에 사용할 데이터
		0, // 다음 행까지의 시스템 메모리 바이트 수
		0 // 다음 깊이까지의 시스템 메모리 바이트 수
	);

	spDeviceContext->DrawIndexed(
		36, // 그릴 인덱스의 수
		0, // 첫 인덱스를 읽을 위치
		0 // 정점 버퍼로부터 정점을 읽기 전에 각 인덱스에 더할 값
	);

	spSwapChain->Present(
		1, // 동기화에 사용할 시간
		0 // 프레임 표현 옵션
	);
}