// This began life as Tutorial07 from MSDN.
// MS has copyrights on whatever subset of this code still somewhat looks like theirs.

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

#include <vector>

#include "image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};

static HINSTANCE hInstance_ = nullptr;
static HWND hwnd_ = nullptr;

static D3D_DRIVER_TYPE driverType_ = D3D_DRIVER_TYPE_NULL;
static D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_11_0;
static ID3D11Device * device_ = nullptr;
static ID3D11Device1 * device1_ = nullptr;
static ID3D11DeviceContext * context_ = nullptr;
static ID3D11DeviceContext1 * context1_ = nullptr;
static IDXGISwapChain * swapChain_ = nullptr;
static IDXGISwapChain1 * swapChain1_ = nullptr;
static ID3D11RenderTargetView * renderTarget_ = nullptr;
static ID3D11Texture2D * depthBuffer_ = nullptr;
static ID3D11DepthStencilView * depthBufferView_ = nullptr;
static ID3D11VertexShader * vertexShader_ = nullptr;
static ID3D11PixelShader * pixelShader_ = nullptr;
static ID3D11InputLayout * vertexLayout_ = nullptr;
static ID3D11Buffer * vertexBuffer_ = nullptr;
static ID3D11Buffer * indexBuffer_ = nullptr;
static ID3D11SamplerState * sampler_ = nullptr;

static const int TEXTURE_COUNT = 1;
static ID3D11ShaderResourceView * textures_[TEXTURE_COUNT];
static int currentTextureIndex_ = 0;

static bool hdrActive_ = false;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message) {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

static HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = "mshdrWindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
    if (!RegisterClassEx(&wcex) )
        return E_FAIL;

    hInstance_ = hInstance;
    RECT rc = { 0, 0, 1920, 1080 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindow("mshdrWindowClass", "mshdr", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd_)
        return E_FAIL;

    ShowWindow(hwnd_, nCmdShow);
    return S_OK;
}

static HRESULT CompileShaderFromFile(WCHAR * szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob ** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob * pErrorBlob = nullptr;
    hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if (FAILED(hr) ) {
        if (pErrorBlob) {
            OutputDebugStringA(reinterpret_cast<const char *>( pErrorBlob->GetBufferPointer() ) );
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();

    return S_OK;
}

static HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_DRIVER_TYPE driverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };
        UINT numDriverTypes = ARRAYSIZE(driverTypes);

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        UINT numFeatureLevels = ARRAYSIZE(featureLevels);

        for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
            driverType_ = driverTypes[driverTypeIndex];
            hr = D3D11CreateDevice(nullptr, driverType_, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &device_, &featureLevel_, &context_);

            if (hr == E_INVALIDARG) {
                // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
                hr = D3D11CreateDevice(nullptr, driverType_, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1, D3D11_SDK_VERSION, &device_, &featureLevel_, &context_);
            }

            if (SUCCEEDED(hr))
                break;
        }
        if (FAILED(hr) )
            return hr;

        // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
        IDXGIFactory1 * dxgiFactory = nullptr;
        {
            IDXGIDevice * dxgiDevice = nullptr;
            hr = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgiDevice) );
            if (SUCCEEDED(hr)) {
                IDXGIAdapter * adapter = nullptr;
                hr = dxgiDevice->GetAdapter(&adapter);
                if (SUCCEEDED(hr)) {
                    hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&dxgiFactory) );
                    adapter->Release();
                }
                dxgiDevice->Release();
            }
        }
        if (FAILED(hr))
            return hr;

        // Create swap chain
        IDXGIFactory2 * dxgiFactory2 = nullptr;
        hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&dxgiFactory2) );
        if (dxgiFactory2) {
            // DirectX 11.1 or later
            hr = device_->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void **>(&device1_) );
            if (SUCCEEDED(hr)) {
                (void)context_->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void **>(&context1_) );
            }

            DXGI_SWAP_CHAIN_DESC1 sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.Width = width;
            sd.Height = height;
            sd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.BufferCount = 1;

            hr = dxgiFactory2->CreateSwapChainForHwnd(device_, hwnd_, &sd, nullptr, nullptr, &swapChain1_);
            if (SUCCEEDED(hr)) {
                hr = swapChain1_->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void **>(&swapChain_) );
            }

            dxgiFactory2->Release();
        } else {
            // DirectX 11.0 systems
            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 1;
            sd.BufferDesc.Width = width;
            sd.BufferDesc.Height = height;
            sd.BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 60;
            sd.BufferDesc.RefreshRate.Denominator = 1;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hwnd_;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;

            hr = dxgiFactory->CreateSwapChain(device_, &sd, &swapChain_);
        }

        if (FAILED(hr))
            return hr;

        IDXGISwapChain3 * swapChain3 = nullptr;
        hr = swapChain_->QueryInterface(IID_PPV_ARGS(&swapChain3));
        if(FAILED(hr)) {
            return hr;
        }
        hr = swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
        if(FAILED(hr)) {
            hdrActive_ = false;
        } else {
            hdrActive_ = true;
        }
        swapChain3->Release();

        // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
        dxgiFactory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
        dxgiFactory->Release();

        // Create a render target view
        ID3D11Texture2D * pBackBuffer = nullptr;
        hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>( &pBackBuffer ) );
        if (FAILED(hr) )
            return hr;

        hr = device_->CreateRenderTargetView(pBackBuffer, nullptr, &renderTarget_);
        pBackBuffer->Release();
        if (FAILED(hr) )
            return hr;

        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC descDepth;
        ZeroMemory(&descDepth, sizeof(descDepth) );
        descDepth.Width = width;
        descDepth.Height = height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDepth.SampleDesc.Count = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = device_->CreateTexture2D(&descDepth, nullptr, &depthBuffer_);
        if (FAILED(hr) )
            return hr;

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        ZeroMemory(&descDSV, sizeof(descDSV) );
        descDSV.Format = descDepth.Format;
        descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = device_->CreateDepthStencilView(depthBuffer_, &descDSV, &depthBufferView_);
        if (FAILED(hr) )
            return hr;

        context_->OMSetRenderTargets(1, &renderTarget_, depthBufferView_);

        // Setup the viewport
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        context_->RSSetViewports(1, &vp);
    }

    // Compile the vertex shader
    ID3DBlob * pVSBlob = nullptr;
    hr = CompileShaderFromFile(L"data\\shaders.fx", "VS", "vs_4_0", &pVSBlob);
    if (FAILED(hr) ) {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
        return hr;
    }

    // Create the vertex shader
    hr = device_->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &vertexShader_);
    if (FAILED(hr) ) {
        pVSBlob->Release();
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = device_->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &vertexLayout_);
    pVSBlob->Release();
    if (FAILED(hr) )
        return hr;

    // Set the input layout
    context_->IASetInputLayout(vertexLayout_);

    // Compile the pixel shader
    ID3DBlob * pPSBlob = nullptr;
    hr = CompileShaderFromFile(L"data\\shaders.fx", "PS", "ps_4_0", &pPSBlob);
    if (FAILED(hr) ) {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = device_->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pixelShader_);
    pPSBlob->Release();
    if (FAILED(hr) )
        return hr;

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd) );
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData) );
    InitData.pSysMem = vertices;
    hr = device_->CreateBuffer(&bd, &InitData, &vertexBuffer_);
    if (FAILED(hr) )
        return hr;

    // Set vertex buffer
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);

    // Create index buffer
    // Create vertex buffer
    WORD indices[] =
    {
        0, 1, 2,
        0, 3, 1,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( WORD ) * 6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = device_->CreateBuffer(&bd, &InitData, &indexBuffer_);
    if (FAILED(hr) )
        return hr;

    // Set index buffer
    context_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Load all textures
    for (int textureIndex = 0; textureIndex < TEXTURE_COUNT; ++textureIndex) {
        Image image;
        char filename[MAX_PATH];
        sprintf(filename, "data\\%d.bmp", textureIndex);
        if(!image.load(filename) || (image.depth() != 16)) {
            continue;
        }

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = static_cast<UINT>(image.width());
        desc.Height = static_cast<UINT>(image.height());
        desc.MipLevels = static_cast<UINT>(1);
        desc.ArraySize = static_cast<UINT>(1);
        desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = (const void *)image.pixels();
        initData.SysMemPitch = image.width() * image.bpp();
        initData.SysMemSlicePitch = static_cast<UINT>(image.bytes());

        ID3D11Texture2D * tex = nullptr;
        hr = device_->CreateTexture2D(&desc, &initData, &tex);
        if (SUCCEEDED(hr) && (tex != nullptr)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            memset(&SRVDesc, 0, sizeof(SRVDesc));
            SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;

            hr = device_->CreateShaderResourceView(tex, &SRVDesc, &textures_[textureIndex]);
            if (FAILED(hr)) {
                tex->Release();
                return hr;
            }
        }
        tex->Release();
    }

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc) );
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sampDesc, &sampler_);
    if (FAILED(hr) )
        return hr;

    return S_OK;
}

static void CleanupDevice()
{
    if (context_)
        context_->ClearState();

    if (sampler_)
        sampler_->Release();

    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        if (textures_[i])
            textures_[i]->Release();
    }

    if (vertexBuffer_)
        vertexBuffer_->Release();
    if (indexBuffer_)
        indexBuffer_->Release();
    if (vertexLayout_)
        vertexLayout_->Release();
    if (vertexShader_)
        vertexShader_->Release();
    if (pixelShader_)
        pixelShader_->Release();
    if (depthBuffer_)
        depthBuffer_->Release();
    if (depthBufferView_)
        depthBufferView_->Release();
    if (renderTarget_)
        renderTarget_->Release();
    if (swapChain1_)
        swapChain1_->Release();
    if (swapChain_)
        swapChain_->Release();
    if (context1_)
        context1_->Release();
    if (context_)
        context_->Release();
    if (device1_)
        device1_->Release();
    if (device_)
        device_->Release();
}

static void Render()
{
    context_->ClearRenderTargetView(renderTarget_, Colors::MidnightBlue);
    context_->ClearDepthStencilView(depthBufferView_, D3D11_CLEAR_DEPTH, 1.0f, 0);

    context_->VSSetShader(vertexShader_, nullptr, 0);
    context_->PSSetShader(pixelShader_, nullptr, 0);
    context_->PSSetShaderResources(0, 1, &textures_[currentTextureIndex_]);
    context_->PSSetSamplers(0, 1, &sampler_);
    context_->DrawIndexed(6, 0, 0);

    swapChain_->Present(0, 0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice())) {
        CleanupDevice();
        return 0;
    }

    char windowTitle[1024];
    sprintf(windowTitle, "mshdr - HDR: %s",
        hdrActive_ ? "Active" : "Inactive"
    );
    SetWindowText(hwnd_, windowTitle);

    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render();
        }
    }

    CleanupDevice();

    return (int)msg.wParam;
}
