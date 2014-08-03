//--------------------------------------------------------------------------------------
// File: PixelQuad.cpp
//
// This application demonstrates texturing
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include "resource.h"

#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};

struct CBChangeOnResize
{
    XMMATRIX mProjection;
};

struct CBChangesEveryFrame
{
    D3DXMATRIX mView;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                  g_hInst = NULL;
HWND                       g_hWnd = NULL;
D3D_DRIVER_TYPE            g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL          g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*              g_pd3dDevice = NULL;
ID3D11DeviceContext*       g_pImmediateContext = NULL;
IDXGISwapChain*            g_pSwapChain = NULL;
ID3D11RenderTargetView*    g_pRenderTargetView = NULL;
ID3D11Texture2D*           g_pDepthStencil = NULL;
ID3D11DepthStencilView*    g_pDepthStencilView = NULL;
ID3D11VertexShader*        g_pSceneVertexShader = NULL;
ID3D11PixelShader*         g_pScenePixelShader = NULL;
ID3D11InputLayout*         g_pSceneVertexLayout = NULL;
ID3D11PixelShader*         g_pSceneDepthPixelShader = NULL;
ID3D11VertexShader*        g_pVisVertexShader = NULL;
ID3D11PixelShader*         g_pVisPixelShader = NULL;
ID3D11InputLayout*         g_pVisVertexLayout = NULL;
ID3D11Buffer*              g_pQuadVertexBuffer = NULL;
ID3D11Buffer*              g_pQuadIndexBuffer = NULL;
ID3D11Buffer*              g_pCBChangeOnResize = NULL;
ID3D11Buffer*              g_pCBChangesEveryFrame = NULL;
ID3D11Texture1D*           g_pDummyBuffer = NULL;
ID3D11ShaderResourceView*  g_pDummySRV = NULL;
ID3D11SamplerState*        g_pSamplerLinear = NULL;
XMMATRIX                   g_View;
XMMATRIX                   g_Projection;

ID3D11RasterizerState*     g_sceneRS = NULL;
ID3D11DepthStencilState*   g_sceneDS = NULL;

ID3D11DepthStencilState*   g_sceneDepthDS = NULL;

ID3D11RasterizerState*     g_visRS = NULL;

ID3D11Texture2D*           g_pLockBuffer = NULL;
ID3D11UnorderedAccessView* g_pLockUAV = NULL;
ID3D11ShaderResourceView*  g_pLockSRV = NULL;

ID3D11Texture2D*           g_pOverdrawBuffer = NULL;
ID3D11UnorderedAccessView* g_pOverdrawUAV = NULL;
ID3D11ShaderResourceView*  g_pOverdrawSRV = NULL;

ID3D11Texture2D*           g_pLiveCountBuffer = NULL;
ID3D11UnorderedAccessView* g_pLiveCountUAV = NULL;
ID3D11ShaderResourceView*  g_pLiveCountSRV = NULL;

ID3D11Texture1D*           g_pLiveStatsBuffer = NULL;
ID3D11UnorderedAccessView* g_pLiveStatsUAV = NULL;
ID3D11ShaderResourceView*  g_pLiveStatsSRV = NULL;

CDXUTSDKMesh g_Mesh;
CModelViewerCamera g_Camera;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();

    return (int)msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"WindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 1024, 1024 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"WindowClass", L"Quad Shading", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                          NULL);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
                               dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
        if (pErrorBlob != NULL)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        if (pErrorBlob) pErrorBlob->Release();
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();

    return S_OK;
}


HRESULT InitQuadBuffers()
{
    HRESULT hr;

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
        { XMFLOAT3(-1.0f,-1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f,-1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3( 1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
    };

    // Create vertex buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pQuadVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Create index buffer
    WORD indices[] =
    {
        0, 2, 1,
        1, 2, 3
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * 6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pQuadIndexBuffer);
    if (FAILED(hr))
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
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
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
                                           D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
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
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    ID3DBlob* pVSBlob = NULL;
    ID3DBlob* pPSBlob = NULL;

    // Compile the vertex shader
    hr = CompileShaderFromFile(L"QuadShading.fx", "SceneVS", "vs_5_0", &pVSBlob);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pSceneVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
                                         pVSBlob->GetBufferSize(), &g_pSceneVertexLayout);
    if (FAILED(hr))
        return hr;

    hr = CompileShaderFromFile(L"QuadShading.fx", "ScenePS", "ps_5_0", &pPSBlob);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pScenePixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return hr;

    hr = CompileShaderFromFile(L"QuadShading.fx", "SceneDepthPS", "ps_5_0", &pPSBlob);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pSceneDepthPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return hr;

    hr = CompileShaderFromFile(L"QuadShading.fx", "VisVS", "vs_5_0", &pVSBlob);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVisVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
                                         pVSBlob->GetBufferSize(), &g_pVisVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return hr;

    hr = CompileShaderFromFile(L"QuadShading.fx", "VisPS", "ps_5_0", &pPSBlob);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pVisPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return hr;

    InitQuadBuffers();

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    // Create the constant buffers
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CBChangeOnResize);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCBChangeOnResize);
    if (FAILED(hr))
        return hr;

    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCBChangesEveryFrame);
    if (FAILED(hr))
        return hr;

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc;  
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
    if (FAILED(hr))
        return hr;

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 5000.0f);

    CBChangeOnResize cbChangesOnResize;
    cbChangesOnResize.mProjection = XMMatrixTranspose(g_Projection);
    g_pImmediateContext->UpdateSubresource(g_pCBChangeOnResize, 0, NULL, &cbChangesOnResize, 0, 0);

    D3D11_TEXTURE2D_DESC desc2D;
    D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;

    DWORD uavWidth  = width  >> 1;
    DWORD uavHeight = height >> 1;

    // Create fragment count buffer
    ZeroMemory(&desc2D, sizeof(D3D11_TEXTURE2D_DESC));
    desc2D.ArraySize = 1;
    desc2D.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc2D.Usage  = D3D11_USAGE_DEFAULT;
    desc2D.Format = DXGI_FORMAT_R32_UINT;
    desc2D.Width  = uavWidth;
    desc2D.Height = uavHeight;
    desc2D.MipLevels = 1;
    desc2D.SampleDesc.Count   = 1;
    desc2D.SampleDesc.Quality = 0;
    g_pd3dDevice->CreateTexture2D(&desc2D, NULL, &g_pLockBuffer);

    descSRV.Format = desc2D.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    descSRV.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(g_pLockBuffer, &descSRV, &g_pLockSRV);

    descUAV.Format = desc2D.Format;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    descUAV.Texture2D.MipSlice = 0;
    g_pd3dDevice->CreateUnorderedAccessView(g_pLockBuffer, &descUAV, &g_pLockUAV);

    g_pd3dDevice->CreateTexture2D(&desc2D, NULL, &g_pOverdrawBuffer);
    g_pd3dDevice->CreateShaderResourceView (g_pOverdrawBuffer, &descSRV, &g_pOverdrawSRV);
    g_pd3dDevice->CreateUnorderedAccessView(g_pOverdrawBuffer, &descUAV, &g_pOverdrawUAV);

    g_pd3dDevice->CreateTexture2D(&desc2D, NULL, &g_pLiveCountBuffer);
    g_pd3dDevice->CreateShaderResourceView (g_pLiveCountBuffer, &descSRV, &g_pLiveCountSRV);
    g_pd3dDevice->CreateUnorderedAccessView(g_pLiveCountBuffer, &descUAV, &g_pLiveCountUAV);

    DWORD statsWidth = 4;

    D3D11_TEXTURE1D_DESC desc1D;
    ZeroMemory(&desc1D, sizeof(D3D11_TEXTURE1D_DESC));
    desc1D.ArraySize = 1;
    desc1D.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc1D.Usage  = D3D11_USAGE_DEFAULT;
    desc1D.Format = DXGI_FORMAT_R32_UINT;
    desc1D.Width  = statsWidth;
    desc1D.MipLevels = 1;

    descSRV.Format = desc1D.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    descSRV.Texture1D.MipLevels = 1;
    descSRV.Texture1D.MostDetailedMip = 0;

    descUAV.Format = desc1D.Format;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
    descUAV.Texture1D.MipSlice = 0;

    g_pd3dDevice->CreateTexture1D(&desc1D, NULL, &g_pLiveStatsBuffer);
    g_pd3dDevice->CreateShaderResourceView (g_pLiveStatsBuffer, &descSRV, &g_pLiveStatsSRV);
    g_pd3dDevice->CreateUnorderedAccessView(g_pLiveStatsBuffer, &descUAV, &g_pLiveStatsUAV);

    desc1D.Width = 1;
    g_pd3dDevice->CreateTexture1D(&desc1D, NULL, &g_pDummyBuffer);
    hr = g_pd3dDevice->CreateShaderResourceView(g_pDummyBuffer, &descSRV, &g_pDummySRV);
    if (FAILED(hr))
        return hr;

    D3D11_RASTERIZER_DESC descRS;
    descRS.FillMode = D3D11_FILL_SOLID;
    descRS.CullMode = D3D11_CULL_BACK;
    descRS.FrontCounterClockwise = FALSE;
    descRS.DepthBias = 0;
    descRS.DepthBiasClamp = 0.0f;
    descRS.SlopeScaledDepthBias = 0.0f;
    descRS.DepthClipEnable = TRUE;
    descRS.ScissorEnable = FALSE;
    descRS.MultisampleEnable = FALSE;
    descRS.AntialiasedLineEnable = FALSE;
    g_pd3dDevice->CreateRasterizerState(&descRS, &g_sceneRS);

    D3D11_DEPTH_STENCIL_DESC descDS;
    descDS.DepthEnable = TRUE;
    descDS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    descDS.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    descDS.StencilEnable = FALSE;
    descDS.StencilReadMask = 0;
    descDS.StencilWriteMask = 0;
    descDS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    descDS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    descDS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    descDS.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    descDS.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    descDS.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    descDS.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    descDS.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    g_pd3dDevice->CreateDepthStencilState(&descDS, &g_sceneDS);

    descDS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    g_pd3dDevice->CreateDepthStencilState(&descDS, &g_sceneDepthDS);

    g_Mesh.Create(g_pd3dDevice, L"hebe.sdkmesh");

    D3DXVECTOR3 vecAt = g_Mesh.GetMeshBBoxCenter(0);
    D3DXVECTOR3 vecEye = vecAt - D3DXVECTOR3(0, 0, 16.0f);

    float fAspectRatio = float(width)/height;
    g_Camera.SetProjParams(D3DX_PI/4, fAspectRatio, 0.1f, 5000.0f);
    g_Camera.SetWindow(width, height);
    g_Camera.SetButtonMasks(0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON);
    g_Camera.SetViewParams(&vecEye, &vecAt);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pDummyBuffer) g_pDummyBuffer->Release();
    if (g_pDummySRV) g_pDummySRV->Release();
    if (g_pCBChangeOnResize) g_pCBChangeOnResize->Release();
    if (g_pCBChangesEveryFrame) g_pCBChangesEveryFrame->Release();
    if (g_pQuadVertexBuffer) g_pQuadVertexBuffer->Release();
    if (g_pQuadIndexBuffer) g_pQuadIndexBuffer->Release();
    if (g_pSceneVertexShader) g_pSceneVertexShader->Release();
    if (g_pScenePixelShader) g_pScenePixelShader->Release();
    if (g_pSceneVertexLayout) g_pSceneVertexLayout->Release();
    if (g_pSceneDepthPixelShader) g_pSceneDepthPixelShader->Release();
    if (g_pVisVertexShader) g_pVisVertexShader->Release();
    if (g_pVisPixelShader) g_pVisPixelShader->Release();
    if (g_pVisVertexLayout) g_pVisVertexLayout->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    if (g_pLockBuffer) g_pLockBuffer->Release();
    if (g_pLockUAV) g_pLockUAV->Release();
    if (g_pLockSRV) g_pLockSRV->Release();

    if (g_pOverdrawBuffer) g_pOverdrawBuffer->Release();
    if (g_pOverdrawUAV) g_pOverdrawUAV->Release();
    if (g_pOverdrawSRV) g_pOverdrawSRV->Release();

    if (g_pLiveCountBuffer) g_pLiveCountBuffer->Release();
    if (g_pLiveCountUAV) g_pLiveCountUAV->Release();
    if (g_pLiveCountSRV) g_pLiveCountSRV->Release();

    if (g_pLiveStatsBuffer) g_pLiveStatsBuffer->Release();
    if (g_pLiveStatsUAV) g_pLiveStatsUAV->Release();
    if (g_pLiveStatsSRV) g_pLiveStatsSRV->Release();

    if (g_sceneRS) g_sceneRS->Release();
    if (g_sceneDS) g_sceneDS->Release();

    if (g_sceneDepthDS) g_sceneDepthDS->Release();

    if (g_visRS) g_visRS->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    g_Camera.HandleMessages(hWnd, message, wParam, lParam);

    switch (message)
    {
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


//--------------------------------------------------------------------------------------
// RenderMesh Allows the app to render individual meshes of an sdkmesh
// and override the primitive topology
//--------------------------------------------------------------------------------------
void RenderMesh(
    ID3D11DeviceContext* deviceContext,
    CDXUTSDKMesh* pDXUTMesh, UINT uMesh,
    UINT uDiffuseSlot = INVALID_SAMPLER_SLOT,
    UINT uNormalSlot = INVALID_SAMPLER_SLOT,
    UINT uSpecularSlot = INVALID_SAMPLER_SLOT)
{
    if (0 < pDXUTMesh->GetOutstandingBufferResources())
    {
        return;
    }

    SDKMESH_MESH* pMesh = pDXUTMesh->GetMesh(uMesh);

    UINT Strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    UINT Offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11Buffer* pVB[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

    if (pMesh->NumVertexBuffers > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
    {
        return;
    }

    for (UINT64 i = 0; i < pMesh->NumVertexBuffers; i++)
    {
        pVB[i] = pDXUTMesh->GetVB11(uMesh, (UINT)i);
        Strides[i] = pDXUTMesh->GetVertexStride(uMesh, (UINT)i);
        Offsets[i] = 0;
    }

    ID3D11Buffer* pIB = pDXUTMesh->GetIB11(pMesh->IndexBuffer);
    DXGI_FORMAT ibFormat = pDXUTMesh->GetIBFormat11(pMesh->IndexBuffer);

    deviceContext->IASetVertexBuffers(0, pMesh->NumVertexBuffers, pVB, Strides, Offsets);
    deviceContext->IASetIndexBuffer(pIB, ibFormat, 0);

    SDKMESH_SUBSET* pSubset = NULL;

    for (UINT uSubset = 0; uSubset < pMesh->NumSubsets; uSubset++)
    {
        pSubset = pDXUTMesh->GetSubset(uMesh, uSubset);

        UINT IndexCount  = (UINT)pSubset->IndexCount;
        UINT IndexStart  = (UINT)pSubset->IndexStart;
        UINT VertexStart = (UINT)pSubset->VertexStart;

        deviceContext->DrawIndexed(IndexCount, IndexStart, VertexStart);
    }
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    // Update our time
    static float t = 0.0f;
    if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
    {
        t += (float)XM_PI * 0.0125f;
    }
    else
    {
        static DWORD dwTimeStart = 0;
        DWORD dwTimeCur = GetTickCount();
        if (dwTimeStart == 0)
            dwTimeStart = dwTimeCur;
        t = (dwTimeCur - dwTimeStart) / 1000.0f;

        g_Camera.FrameMove(t);
    }

    //
    // Clear the back buffer
    //
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Clear the fragment count buffer
    UINT clearValueUINT[1];
    clearValueUINT[0] = 0xffffffff;
    g_pImmediateContext->ClearUnorderedAccessViewUint(g_pLockUAV, clearValueUINT);
    clearValueUINT[0] = 0x00000000;
    g_pImmediateContext->ClearUnorderedAccessViewUint(g_pOverdrawUAV,  clearValueUINT);
    g_pImmediateContext->ClearUnorderedAccessViewUint(g_pLiveCountUAV, clearValueUINT);
    g_pImmediateContext->ClearUnorderedAccessViewUint(g_pLiveStatsUAV, clearValueUINT);

    //
    // Update variables that change once per frame
    //
    CBChangesEveryFrame cb;
    D3DXMatrixTranspose(&cb.mView, g_Camera.GetViewMatrix());
    g_pImmediateContext->UpdateSubresource(g_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0);

    //
    // Render the mesh
    //
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->IASetInputLayout(g_pSceneVertexLayout);

    g_pImmediateContext->VSSetShader(g_pSceneVertexShader, NULL, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pCBChangeOnResize);
    g_pImmediateContext->VSSetConstantBuffers(1, 1, &g_pCBChangesEveryFrame);
    g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pCBChangeOnResize);
    g_pImmediateContext->PSSetConstantBuffers(1, 1, &g_pCBChangesEveryFrame);
    g_pImmediateContext->PSSetShader(g_pSceneDepthPixelShader, NULL, 0);

    // Depth pass
    g_pImmediateContext->RSSetState(g_sceneRS);
    g_pImmediateContext->OMSetDepthStencilState(g_sceneDepthDS, 0);
    g_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, g_pDepthStencilView, 0, 0, NULL, NULL);

    for (UINT i = 0; i < g_Mesh.GetNumMeshes(); i++)
        RenderMesh(g_pImmediateContext, &g_Mesh, i, 2);

    // Fragments pass
    g_pImmediateContext->PSSetShader(g_pScenePixelShader, NULL, 0);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pDummySRV);
    g_pImmediateContext->PSSetShaderResources(1, 1, &g_pDummySRV);

    ID3D11UnorderedAccessView* pUAVs[4];
    pUAVs[0] = g_pLockUAV;
    pUAVs[1] = g_pOverdrawUAV;
    pUAVs[2] = g_pLiveCountUAV;
    pUAVs[3] = g_pLiveStatsUAV;
    g_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, g_pDepthStencilView, 0, 4, pUAVs, NULL);

    g_pImmediateContext->OMSetDepthStencilState(g_sceneDS, 0);

    for (UINT i = 0; i < g_Mesh.GetNumMeshes(); i++)
        RenderMesh(g_pImmediateContext, &g_Mesh, i, 2);

    g_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &g_pRenderTargetView, g_pDepthStencilView, 0, 0, NULL, NULL);

    //
    // Render the quad
    //
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pQuadVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetIndexBuffer(g_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->IASetInputLayout(g_pVisVertexLayout);

    g_pImmediateContext->VSSetShader(g_pVisVertexShader, NULL, 0);
    g_pImmediateContext->PSSetShader(g_pVisPixelShader,  NULL, 0);

    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pOverdrawSRV);
    g_pImmediateContext->PSSetShaderResources(1, 1, &g_pLiveStatsSRV);

    g_pImmediateContext->DrawIndexed(6, 0, 0);

    //
    // Present our back buffer to our front buffer
    //
    g_pSwapChain->Present(0, 0);
}
