#include <Windows.h>
#include <assert.h>

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ������ ���� �ܼ� ����
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

	// ������ Ŭ���� ���� �� ���
	WNDCLASS windowClass;
	ZeroMemory(&windowClass, sizeof(WNDCLASS));

	windowClass.lpfnWndProc = WndProc; // �ݹ��Լ� ���
	windowClass.lpszClassName = WINDOW_NAME; // Ŭ���� �̸� ����
	windowClass.hCursor = (HCURSOR)LoadCursor(nullptr, IDC_ARROW); // �⺻ Ŀ�� ����
	windowClass.hInstance = hInstance; // Ŭ���� �ν��Ͻ� ����

	RegisterClass(&windowClass);
	assert(GetLastError() == ERROR_SUCCESS);

	// ���� ����ϰ� �� ȭ�� ũ�� ����
	// ���⼭�� ���÷����� ���� / 2, ���� / 2 ��� ����
	RECT windowRect;
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = GetSystemMetrics(SM_CXSCREEN) >> 1; // ���÷��� �ʺ� / 2
	windowRect.bottom = GetSystemMetrics(SM_CYSCREEN) >> 1; // ���÷��� ���� / 2

	AdjustWindowRect(
		&windowRect, // ���� ���� RECT ����ü �ּ�
		WS_OVERLAPPEDWINDOW, // ũ�⸦ ����� �� ������ ������ ��Ÿ��
		false // �޴� ����
	);
	assert(GetLastError() == ERROR_SUCCESS);

	// ������ ����
	ghWnd = CreateWindow(
		WINDOW_NAME, // Ŭ���� �̸�
		WINDOW_NAME, // ������ �̸�
		WS_OVERLAPPEDWINDOW, // ������ ��Ÿ��
		CW_USEDEFAULT, CW_USEDEFAULT, // (x, y)
		windowRect.right, windowRect.bottom, // �ʺ�, ����
		nullptr, // �θ� ������ ����
		nullptr, // �޴� ����
		hInstance, // �ν��Ͻ� ����
		nullptr // �߰� �޸� ����
	);
	assert(GetLastError() == ERROR_SUCCESS);
	InitializeD3D11(); // ������ ����� ���� D3D �ʱ�ȭ

	// ������ �����ֱ�
	ShowWindow(ghWnd, nCmdShow);
	assert(GetLastError() == ERROR_SUCCESS);

	// PeekMessage�� �̿��� �޽��� ����
	MSG msg;
	while (true)
	{
		if (PeekMessage(
			&msg, // �޽����� ���� �ּ�
			nullptr, // �޽����� ���� ������
			0, // ���� �޽��� ���� �ּڰ�
			0, // ���� �޽��� ���� �ִ�
			PM_REMOVE // �޽��� ó�� ���
		))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg); // �޽��� ���ڷ� �ؼ� �õ�
			DispatchMessage(&msg); // �޽��� ����
		}
		else
		{
			Render();
			Sleep(1000 / 30);
		}
	}

	return (int)msg.wParam;
}

// ������ �ݹ� �Լ�
LRESULT CALLBACK WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY: // ������ ����� �ڿ� ���� �۾�
		DestroyD3D11();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

// ����� ���� ����ü
// ��ġ ���� �ܿ��� �پ��� �����Ͱ� �߰��� �� ����
typedef struct
{
	DirectX::XMFLOAT3 pos; // �ܼ� (x, y, z)�� ǥ���ϱ� ���� float3 ���
} vertex_t;

// D3D11���� ����� ������
// Device: ���ҽ� ����
// DeviceContext: �������� ����� ������ ���� ������ ���
// SwapChain: ���۸� ����̽��� �������ϰ� ���
// RenderTargetView: ���ҽ� �� �߿��� ������ ��� �뵵
static ID3D11Device* spDevice;
static ID3D11DeviceContext* spDeviceContext;
static IDXGISwapChain* spSwapChain;
static ID3D11RenderTargetView* spRenderTargetView;
static ID3D11VertexShader* spVertexShader;
static ID3D11PixelShader* spPixelShader;
static ID3D11Buffer* spVertexBuffer;

void InitializeD3D11()
{
	// ����̽� ���� ���� ����
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if DEBUG || _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif /* Debug */

	// ����ü�ο� ���� ���� �ʱ�ȭ
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// ���� �������� ũ�� ���ϱ�(�۾� ����)
	RECT windowRect;
	GetClientRect(ghWnd, &windowRect);
	assert(GetLastError() == ERROR_SUCCESS);

	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = windowRect.right;
	swapChainDesc.BufferDesc.Height = windowRect.bottom;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // ����� ���� ����
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60; // �ٽ� �׸��� �ֱ��� ����
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1; // �ٽ� �׸��� �ֱ��� �и�
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // ������ �뵵
	swapChainDesc.OutputWindow = ghWnd; // �������� ������� ����� ������
	swapChainDesc.SampleDesc.Count = 1; // �ȼ��� ���ø��ϴ� ��
	swapChainDesc.SampleDesc.Quality = 0; // �̹����� ǰ�� ����
	swapChainDesc.Windowed = true; // â��� ����

	// ����̽�, ����̽� ���ؽ�Ʈ, ����ü�� ����
	// �ֽ� ������ �ٸ� ������� ���� ���� ��
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, // ����� ������
		D3D_DRIVER_TYPE_HARDWARE, // ����� ����̹�
		nullptr, // �����Ͷ������� �ּ�
		creationFlags, // ���� �÷���
		nullptr, // ������ ���� ���� �迭
		0, // ���� ���� �迭�� ����
		D3D11_SDK_VERSION, // D3D SDK ����
		&swapChainDesc, // ����ü�� ���� ����ü, 
		&spSwapChain, // ������ ����ü���� �����͸� ���� �ּ�
		&spDevice, // ������ ����̽��� �����͸� ���� �ּ�
		nullptr, // ������ ���� ������ ���� �ּ�
		&spDeviceContext // ������ ����̽� ���ؽ�Ʈ�� �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// ���� Ÿ������ ����� ���� ���� ����
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = spSwapChain->GetBuffer(
		0, // ����� ���� ��ȣ
		__uuidof(ID3D11Texture2D), // ���۸� �ؼ��� �� ����� �������̽�
		(void**)&pBackBuffer // ���� �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// ���� Ÿ�� �� ����
	hr = spDevice->CreateRenderTargetView(
		pBackBuffer, // ����� ���ҽ� ������
		nullptr, // ���� Ÿ�� �� ���� ����ü ������
		&spRenderTargetView // ������� ���� Ÿ�� ���� �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));
	pBackBuffer->Release();
	pBackBuffer = nullptr;

	// ���� Ÿ���� ���������� OM �ܰ迡 ����
	spDeviceContext->OMSetRenderTargets(
		1, // ���� ���� ��
		&spRenderTargetView, // ���� Ÿ�� ������ �迭
		nullptr // ���� ���ٽ� �� ������
	);

	// ����Ʈ ���� �ʱ�ȭ
	D3D11_VIEWPORT viewPort;
	viewPort.Width = (FLOAT)windowRect.right;
	viewPort.Height = (FLOAT)windowRect.bottom;
	viewPort.MinDepth = 0.f;
	viewPort.MaxDepth = 1.f;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;

	// �� ��Ʈ ����
	spDeviceContext->RSSetViewports(
		1, // ����Ʈ�� ��
		&viewPort // ���� ����ü ������
	);

	// ���̴�, ������ ������ ���� ���� ��ü
	ID3DBlob* pVertexShaderBlob = nullptr;
	ID3DBlob* pPixelShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	// ���� ���̴� ������
	hr = D3DCompileFromFile(
		TEXT("VertexShader.hlsl"), // ���̴� �ڵ� ���� �̸�
		nullptr, // ���̴� ��ũ�θ� �����ϴ� ����ü ������
		nullptr, // ���̴� �����Ϸ��� include ���� ó���� ����ϴ� �������̽� ������
		"main", // ������ �̸�
		"vs_5_0", // ������ ���
		0, // ������ �ɼ�
		0, // ������ �ɼ�2
		&pVertexShaderBlob, // �����ϵ� ���̴� ������ �����͸� ���� �ּ�
		&pErrorBlob // ������ ���� ������ �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// ���� ���̴� ���ҽ� ����
	hr = spDevice->CreateVertexShader(
		pVertexShaderBlob->GetBufferPointer(), // �����ϵ� ���̴� ������ ������
		pVertexShaderBlob->GetBufferSize(), // ���̴� �������� ����
		nullptr, // ���̴� ������ũ ���� �������̽� ������
		&spVertexShader // ���� ���̴� �������̽��� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// �ȼ� ���̴� ������
	hr = D3DCompileFromFile(
		TEXT("PixelShader.hlsl"), // ���̴� �ڵ� ���� �̸�
		nullptr, // ���̴� ��ũ�θ� �����ϴ� ����ü ������
		nullptr, // ���̴� �����Ϸ��� include ���� ó���� ����ϴ� �������̽� ������
		"main", // ������ �̸�
		"ps_5_0", // ������ ���
		0, // ������ �ɼ�
		0, // ������ �ɼ�2
		&pPixelShaderBlob, // �����ϵ� ���̴� ������ �����͸� ���� �ּ�
		&pErrorBlob // ������ ���� ������ �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// �ȼ� ���̴� ���ҽ� ����
	hr = spDevice->CreatePixelShader(
		pPixelShaderBlob->GetBufferPointer(), // �����ϵ� ���̴� ������ ������
		pPixelShaderBlob->GetBufferSize(), // ���̴� �������� ����
		nullptr, // ���̴� ������ũ ���� �������̽� ������
		&spPixelShader // ���� ���̴� �������̽��� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// GPU���� ������ ������ �˷��ֱ� ���� ����ü �ʱ�ȭ
	// ����ü�� ���Ե� �� ��Һ��� �� �۾��� �����ؼ� �Ѱ������
	D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc;

	vertexLayoutDesc.SemanticName = "POSITION"; // �ش� �������� �뵵
	vertexLayoutDesc.SemanticIndex = 0; // �뵵�� ��ĥ ��� ����� ���� ��ȣ
	vertexLayoutDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT; // �Է� ������ ����
	vertexLayoutDesc.InputSlot = 0; // ������ ���� ��ȣ
	vertexLayoutDesc.AlignedByteOffset = 0; // ����ü���� ����� ���� ��ġ(����Ʈ ����)
	vertexLayoutDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA; // �Է� �������� �з���
	vertexLayoutDesc.InstanceDataStepRate = 0; // �ν��Ͻ̿� ����� ���

	// ���������ο� ������ ���̾ƿ� �迭
	D3D11_INPUT_ELEMENT_DESC layoutArr[] = {
		vertexLayoutDesc
	};

	// ���������ο��� ����� ���̾ƿ� ����
	ID3D11InputLayout* pVertexLayout = nullptr;
	hr = spDevice->CreateInputLayout(
		layoutArr, // ���̾ƿ� �迭
		ARRAYSIZE(layoutArr), // �迭�� ����
		pVertexShaderBlob->GetBufferPointer(), // �����ϵ� ���̴� ������ ������
		pVertexShaderBlob->GetBufferSize(), // ���̴� �������� ����
		&pVertexLayout // ������ ���̾ƿ� �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// ���� ���̾ƿ� ����
	spDeviceContext->IASetInputLayout(pVertexLayout);

	pVertexShaderBlob->Release();
	pVertexShaderBlob = nullptr;

	pPixelShaderBlob->Release();
	pPixelShaderBlob = nullptr;

	pVertexLayout->Release();
	pVertexLayout = nullptr;

	// ������ ���� �迭
	vertex_t vertices[3] = {
		DirectX::XMFLOAT3(0.f, 0.5f, 0.f),
		DirectX::XMFLOAT3(0.5f, -0.5f, 0.f),
		DirectX::XMFLOAT3(-0.5f, -0.5f, 0.f)
	};

	// ���� ���ۿ� ���� ���� ����ü �ʱ�ȭ
	D3D11_BUFFER_DESC bufferDesc;

	bufferDesc.ByteWidth = sizeof(vertices); // ������ ����Ʈ ũ��
	bufferDesc.Usage = D3D11_USAGE_DEFAULT; // ������ �뵵
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // ���������ο� ���� ���ε� ����
	bufferDesc.CPUAccessFlags = 0; // CPU�� ���� ����
	bufferDesc.MiscFlags = 0; // ���ҽ��� ���� �ɼ�
	bufferDesc.StructureByteStride = sizeof(vertex_t); // �� ��Һ� ����Ʈ ũ��

	// �ʱ�ȭ�� ���� ���� ���긮�ҽ� ����ü
	D3D11_SUBRESOURCE_DATA vertexSubresource;

	vertexSubresource.pSysMem = vertices; // ������ ������ ������
	vertexSubresource.SysMemPitch = 0; // ���� ������ ���� ���� �ý��� ����Ʈ ��
	vertexSubresource.SysMemSlicePitch = 0; // ���� ������ ���� ���� �ý��� ����Ʈ ��

	// ���� ���� ����
	hr = spDevice->CreateBuffer(
		&bufferDesc, // ���� ���� ����ü ������
		&vertexSubresource, // ���� ���� ���ҽ� ������
		&spVertexBuffer // ������� ���� ���ҽ� �����͸� ���� �ּ�
	);
	assert(SUCCEEDED(hr));

	// ���������ο� ���� ���� ����
	UINT stride = sizeof(vertex_t);
	UINT offset = 0;

	spDeviceContext->IASetVertexBuffers(
		0, // ���� ��ȣ
		1, // ������ ��
		&spVertexBuffer, // ���� ���� �ּ�
		&stride, // ��Һ� ũ�� �迭
		&offset // �� ���ۺ� ���� ������ �迭
	);

	// �ﰢ�� �׸��� ��� ����
	spDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DestroyD3D11()
{
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
	const FLOAT clearColor[4] = { 209 / 255.f, 95 / 255.f, 238 / 255.f, 1.f };

	assert(spRenderTargetView != nullptr);
	spDeviceContext->ClearRenderTargetView(
		spRenderTargetView, // ��� ���� Ÿ�� ��
		clearColor // ä�� ���� ����
	);

	// ���̴� ����
	spDeviceContext->VSSetShader(
		spVertexShader, // ������ ���̴� �������̽� ������
		nullptr, // HLSL Ŭ������ ĸ��ȭ�ϴ� �������̽� ������ �迭
		0 // �迭�� ����
	);

	spDeviceContext->PSSetShader(
		spPixelShader, // ������ ���̴� �������̽� ������
		nullptr, // HLSL Ŭ������ ĸ��ȭ�ϴ� �������̽� ������ �迭
		0 // �迭�� ����
	);

	// ���� �׸���
	spDeviceContext->Draw(
		3, // ������ ��
		0 // ���� ������
	);

	spSwapChain->Present(
		1, // ����ȭ�� ����� �ð�
		0 // ������ ǥ�� �ɼ�
	);
}