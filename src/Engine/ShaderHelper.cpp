#include "ShaderHelper.h"
#include <windows.h>
#include <cstring>

namespace ScreensaverEngine {

static const char* kFullscreenTriangleVS = R"HLSL(
struct VSOut {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

VSOut VSMain(uint vertexID : SV_VertexID) {
    VSOut output;
    // Generates a single triangle covering [-1, 1] NDC clip space:
    // vertex 0: (-1, -1), uv (0, 1)
    // vertex 1: (-1,  3), uv (0, -1)
    // vertex 2: ( 3, -1), uv (2, 1)
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.uv = grid;
    return output;
}
)HLSL";

bool ShaderHelper::CompileVertexShader(
    ID3D11Device* device,
    const char* hlslSource,
    const char* entryPoint,
    ComPtr<ID3D11VertexShader>& outVS,
    ComPtr<ID3DBlob>* outBytecode) {

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT hr = D3DCompile(
        hlslSource,
        std::strlen(hlslSource),
        "EmbeddedVS.hlsl",
        nullptr,
        nullptr,
        entryPoint,
        "vs_5_0",
        flags,
        0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA("Vertex Shader Compile Error:\n");
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateVertexShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        outVS.GetAddressOf());

    if (SUCCEEDED(hr) && outBytecode) {
        *outBytecode = shaderBlob;
    }

    return SUCCEEDED(hr);
}

bool ShaderHelper::CompilePixelShader(
    ID3D11Device* device,
    const char* hlslSource,
    const char* entryPoint,
    ComPtr<ID3D11PixelShader>& outPS) {

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT hr = D3DCompile(
        hlslSource,
        std::strlen(hlslSource),
        "EmbeddedPS.hlsl",
        nullptr,
        nullptr,
        entryPoint,
        "ps_5_0",
        flags,
        0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA("Pixel Shader Compile Error:\n");
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreatePixelShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        outPS.GetAddressOf());

    return SUCCEEDED(hr);
}

bool ShaderHelper::CreateFullscreenQuadVS(
    ID3D11Device* device,
    ComPtr<ID3D11VertexShader>& outVS) {
    return CompileVertexShader(device, kFullscreenTriangleVS, "VSMain", outVS);
}

bool FullscreenShaderEffect::InitFullscreenShader(
    ID3D11Device* device,
    const char* pixelShaderHLSL,
    const char* entryPoint) {
    if (!ShaderHelper::CreateFullscreenQuadVS(device, m_vs)) {
        return false;
    }
    if (!ShaderHelper::CompilePixelShader(device, pixelShaderHLSL, entryPoint, m_ps)) {
        return false;
    }
    if (!ShaderHelper::CreateConstantBuffer<StandardConstants>(device, m_constantBuffer)) {
        return false;
    }
    return true;
}

void FullscreenShaderEffect::Update(const FrameContext& frame) {
    m_constants.resolutionX = static_cast<float>(frame.width);
    m_constants.resolutionY = static_cast<float>(frame.height);
    m_constants.time = frame.totalTime;
    m_constants.deltaTime = frame.deltaTime;
    m_constants.aspect = (frame.height > 0) ? (static_cast<float>(frame.width) / static_cast<float>(frame.height)) : 1.777f;
    m_constants.speedMultiplier = frame.speedMultiplier;
}

void FullscreenShaderEffect::Render(const FrameContext& frame) {
    auto* ctx = frame.context;
    ShaderHelper::UpdateConstantBuffer(ctx, m_constantBuffer.Get(), m_constants);

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
    ctx->PSSetConstantBuffers(0, 1, cbs);

    ctx->Draw(3, 0);
}

void FullscreenShaderEffect::Shutdown() {
    m_constantBuffer.Reset();
    m_ps.Reset();
    m_vs.Reset();
}

} // namespace ScreensaverEngine
