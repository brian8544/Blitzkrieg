#include "StdAfx.h"
#include "D3D8Runtime.h"

namespace
{
using TDirect3DCreate8 = IDirect3D8 *(WINAPI *)(UINT);

HMODULE LoadAppLocalD3D8()
{
    wchar_t path[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])));
    if ( length == 0 || length >= (sizeof(path) / sizeof(path[0])) )
        return nullptr;

    wchar_t *fileName = path;
    for ( wchar_t *p = path; *p != L'\0'; ++p )
    {
        if ( *p == L'\\' || *p == L'/' )
            fileName = p + 1;
    }

    static constexpr wchar_t kShimName[] = L"d3d8.dll";
    const size_t prefixLength = static_cast<size_t>( fileName - path );
    if ( prefixLength + (sizeof(kShimName) / sizeof(kShimName[0])) > (sizeof(path) / sizeof(path[0])) )
        return nullptr;

    wcscpy_s( fileName, (sizeof(path) / sizeof(path[0])) - prefixLength, kShimName );
    return ::LoadLibraryW( path );
}

struct SD3D8Runtime
{
    HMODULE module = LoadAppLocalD3D8();
    TDirect3DCreate8 create = module != nullptr
        ? reinterpret_cast<TDirect3DCreate8>( ::GetProcAddress(module, "Direct3DCreate8") )
        : nullptr;

};

SD3D8Runtime &GetRuntime()
{
    static SD3D8Runtime runtime;
    return runtime;
}
}

namespace ND3D8Runtime
{
IDirect3D8 *Create()
{
    const auto create = GetRuntime().create;
    return create != nullptr ? create( 220u ) : nullptr;
}

bool IsAvailable()
{
    return GetRuntime().create != nullptr;
}
}
