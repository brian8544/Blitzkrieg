#ifndef __D3D8RUNTIME_H__
#define __D3D8RUNTIME_H__

struct IDirect3D8;

namespace ND3D8Runtime
{
IDirect3D8 *Create();
bool IsAvailable();
}

#endif // __D3D8RUNTIME_H__
