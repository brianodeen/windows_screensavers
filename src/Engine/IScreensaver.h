#pragma once

#include <d3d11.h>
#include <string>

namespace ScreensaverEngine {

struct FrameContext {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    int width = 1;
    int height = 1;
    float deltaTime = 0.016f;
    float totalTime = 0.0f;
    float speedMultiplier = 1.0f;
};

class IScreensaver {
public:
    virtual ~IScreensaver() = default;

    virtual const char* GetName() const = 0;
    virtual const char* GetDescription() const = 0;

    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
    virtual void Resize(int width, int height) {}
    virtual void Update(const FrameContext& frame) = 0;
    virtual void Render(const FrameContext& frame) = 0;
    virtual void Shutdown() = 0;
};

} // namespace ScreensaverEngine
