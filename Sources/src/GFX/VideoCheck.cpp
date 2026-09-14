#include "StdAfx.h"
#include "VideoCheck.h"

#include <d3d11.h>
#include <dxgi1_4.h>

namespace
{
	std::uint64_t AvailableBudget( IDXGIAdapter3 *pAdapter, DXGI_MEMORY_SEGMENT_GROUP group, std::uint64_t fallback )
	{
		if ( !pAdapter )
			return fallback;
		DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
		if ( FAILED( pAdapter->QueryVideoMemoryInfo( 0, group, &info ) ) )
			return fallback;
		return info.Budget > info.CurrentUsage ? info.Budget - info.CurrentUsage : 0;
	}
}

const wchar_t* STDCALL NVideoCheck::GetAPIName()
{
	return L"Direct3D 11";
}

DWORD STDCALL NVideoCheck::GetAPIVersion()
{
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	ID3D11Device *pDevice = nullptr;
	ID3D11DeviceContext *pContext = nullptr;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_10_0;
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		levels,
		static_cast<UINT>( sizeof(levels) / sizeof(levels[0]) ),
		D3D11_SDK_VERSION,
		&pDevice,
		&featureLevel,
		&pContext );

	if ( pContext ) pContext->Release();
	if ( pDevice ) pDevice->Release();
	if ( FAILED( hr ) )
		return 0;

	switch ( featureLevel )
	{
		case D3D_FEATURE_LEVEL_11_1: return 0x0B01;
		case D3D_FEATURE_LEVEL_11_0: return 0x0B00;
		case D3D_FEATURE_LEVEL_10_1: return 0x0A01;
		case D3D_FEATURE_LEVEL_10_0: return 0x0A00;
		default: return 0;
	}
}

bool STDCALL NVideoCheck::GetVideoMemory( SVideoMemory *pMemory )
{
	if ( !pMemory )
		return false;
	*pMemory = {};

	IDXGIFactory1 *pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1( __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory) );
	if ( FAILED( hr ) || !pFactory )
		return false;

	IDXGIAdapter1 *pAdapter = nullptr;
	hr = pFactory->EnumAdapters1( 0, &pAdapter );
	pFactory->Release();
	if ( FAILED( hr ) || !pAdapter )
		return false;

	DXGI_ADAPTER_DESC1 desc = {};
	pAdapter->GetDesc1( &desc );
	pMemory->local.dwTotal = static_cast<std::uint64_t>( desc.DedicatedVideoMemory );
	pMemory->nonlocal.dwTotal = static_cast<std::uint64_t>( desc.SharedSystemMemory );
	pMemory->texture.dwTotal = pMemory->local.dwTotal + pMemory->nonlocal.dwTotal;

	IDXGIAdapter3 *pAdapter3 = nullptr;
	if ( SUCCEEDED( pAdapter->QueryInterface( __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&pAdapter3) ) ) )
	{
		pMemory->local.dwFree = AvailableBudget( pAdapter3, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, pMemory->local.dwTotal );
		pMemory->nonlocal.dwFree = AvailableBudget( pAdapter3, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, pMemory->nonlocal.dwTotal );
		pMemory->texture.dwFree = pMemory->local.dwFree + pMemory->nonlocal.dwFree;
		pAdapter3->Release();
	}
	else
	{
		pMemory->local.dwFree = pMemory->local.dwTotal;
		pMemory->nonlocal.dwFree = pMemory->nonlocal.dwTotal;
		pMemory->texture.dwFree = pMemory->texture.dwTotal;
	}

	pAdapter->Release();
	return true;
}
