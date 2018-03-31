#pragma once

namespace DX12
{
    ID3D12Device* CreateDevice(bool gpuDebug, bool useWarpDevice, IDXGIFactory4** outFactory);
}