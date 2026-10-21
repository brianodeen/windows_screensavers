#pragma once

#include "IScreensaver.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

namespace ScreensaverEngine {

using Microsoft::WRL::ComPtr;

// 16-byte aligned standard constant buffer passed to shaders (b0)
struct alignas(16) StandardConstants {
    float resolutionX;
    float resolutionY;
    float time;
    float deltaTime;

    float aspect;
    float speedMultiplier;
    float param1;
    float param2;
};

class ShaderHelper {
public:
    static bool CompileVertexShader(
        ID3D11Device* device,
        const char* hlslSource,
        const char* entryPoint,
        ComPtr<ID3D11VertexShader>& outVS,
        ComPtr<ID3DBlob>* outBytecode = nullptr);

    static bool CompilePixelShader(
        ID3D11Device* device,
        const char* hlslSource,
        const char* entryPoint,
        ComPtr<ID3D11PixelShader>& outPS);

    // Creates a vertexless fullscreen triangle vertex shader (uses SV_VertexID: 3 vertices)
    static bool CreateFullscreenQuadVS(
        ID3D11Device* device,
        ComPtr<ID3D11VertexShader>& outVS);

    template <typename T>
    static bool CreateConstantBuffer(ID3D11Device* device, ComPtr<ID3D11Buffer>& outBuffer) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (sizeof(T) + 15) & ~15;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateBuffer(&desc, nullptr, outBuffer.GetAddressOf()));
    }

    template <typename T>
    static void UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer, const T& data) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &data, sizeof(T));
            context->Unmap(buffer, 0);
        }
    }
};

// Base class for fullscreen procedural shader screensavers (Shadertoy-style in pure HLSL)
class FullscreenShaderEffect : public IScreensaver {
public:
    bool InitFullscreenShader(ID3D11Device* device, const char* pixelShaderHLSL, const char* entryPoint = "PSMain");
    void Update(const FrameContext& frame) override;
    void Render(const FrameContext& frame) override;
    void Shutdown() override;

protected:
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    StandardConstants m_constants = {};
};

} // namespace ScreensaverEngine
