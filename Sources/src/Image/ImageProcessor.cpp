#include "StdAfx.h"

#include "ImageProcessor.h"

#include "ImageBMP.h"
#include "ImagePNG.h"
#include "ImageTGA.h"
#include "ImageMMP.h"

#include <squish.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::LoadImage( IDataStream *pStream ) const
{
	NI_ASSERT_T( pStream != 0, "Can't load to NULL stream" );
	//
  if ( NImage::RecognizeFormatPNG(pStream) )
    return NImage::LoadImagePNG( pStream );
  else if ( NImage::RecognizeFormatBMP(pStream) )
    return NImage::LoadImageBMP( pStream );
  else if ( NImage::RecognizeFormatTGA(pStream) )
    return NImage::LoadImageTGA( pStream );
	else 
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDDSImage* CImageProcessor::LoadDDSImage( IDataStream *pStream ) const
{
	return NImage::LoadImageDDS( pStream );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImageProcessor::SaveImageAsPNG( IDataStream *pStream, const IImage *pImage ) const
{
	return NImage::SaveImageAsPNG( pStream, pImage );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImageProcessor::SaveImageAsTGA( IDataStream *pStream, const IImage *pImage ) const
{
	return NImage::SaveImageAsTGA( pStream, pImage );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImageProcessor::SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImage ) const
{
	return NImage::SaveImageAsDDS( pStream, pImage );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ImageScale( const CImage *pSrcImg, CImage *pDstImg, EImageScaleMethod method );
IImage* CImageProcessor::CreateScale( const IImage *pImage, float fScaleFactor, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( pImage->GetSizeX()*fScaleFactor, pImage->GetSizeY()*fScaleFactor );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
IImage* CImageProcessor::CreateScale( const IImage *pImage, float fScaleX, float fScaleY, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( pImage->GetSizeX()*fScaleX, pImage->GetSizeY()*fScaleY );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::CreateScaleBySize( const IImage *pImage, int nSizeX, int nSizeY, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( nSizeX, nSizeY );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::CreateMip( const IImage *pImage, int nLevel ) const
{
	return CreateScale( pImage, 1.0 / double( 1UL << nLevel ), ISM_LANCZOS3 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDDSImage* CompressDXTN( const IImage *pImage, EGFXPixelFormat format )
{
	int squishFlags = squish::kColourIterativeClusterFit;
	float perceptualMetric[3] = { 0.2126f, 0.7152f, 0.0722f };
	switch ( format )
	{
		case GFXPF_DXT1: squishFlags |= squish::kDxt1; break;
		case GFXPF_DXT2:
		case GFXPF_DXT3: squishFlags |= squish::kDxt3; break;
		case GFXPF_DXT4:
		case GFXPF_DXT5: squishFlags |= squish::kDxt5; break;
		default: return 0;
	}

	const int width = pImage->GetSizeX();
	const int height = pImage->GetSizeY();
	std::vector<BYTE> rgba( static_cast<size_t>(width) * height * 4 );
	const SColor *src = pImage->GetLFB();
	for ( int i = 0; i < width * height; ++i )
	{
		rgba[i*4+0] = src[i].r;
		rgba[i*4+1] = src[i].g;
		rgba[i*4+2] = src[i].b;
		rgba[i*4+3] = src[i].a;
	}

	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	CImageDDS *pImageDDS = new CImageDDS( width, height, ddsformat );
	std::vector<BYTE> &outdata = pImageDDS->AddEmptyMipLevel();
	outdata.resize( squish::GetStorageRequirements( width, height, squishFlags ) );
	squish::CompressImage( rgba.data(), width, height, outdata.data(), squishFlags, perceptualMetric );
	return pImageDDS;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDDSImage* CompressRGBA( const IImage *pImage, EGFXPixelFormat format )
{
	SPixelConvertInfo pci;
	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	switch ( format )
	{
		case GFXPF_ARGB8888:
			pci.InitMaskInfo( 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff );
			break;
		case GFXPF_ARGB1555:
			pci.InitMaskInfo( 0x00008000, 0x00007c00, 0x000003e0, 0x0000001f );
			break;
		case GFXPF_ARGB4444:
			pci.InitMaskInfo( 0x0000f000, 0x00000f00, 0x000000f0, 0x0000000f );
			break;
		case GFXPF_ARGB0565:
			pci.InitMaskInfo( 0x00000000, 0x0000f800, 0x000007e0, 0x0000001f );
			break;
		default:
			return false;
	}
	//
	int nSizeX = pImage->GetSizeX();
	int nSizeY = pImage->GetSizeY();
	int nBPP = ::GetBPP( format );
	
	CImageDDS *pImageMMP = new CImageDDS( nSizeX, nSizeY, ddsformat );
	std::vector<BYTE> &outdata = pImageMMP->AddEmptyMipLevel();
	outdata.resize( nSizeX * nSizeY * nBPP / 8 );

	const DWORD *pSrc = reinterpret_cast<const DWORD*>( pImage->GetLFB() );
	//std::vector<BYTE> buffer( nSizeX * nSizeY * nBPP / 8 );
	if ( nBPP == 16 )
	{
		WORD *pDst = reinterpret_cast<WORD*>( &( outdata[0] ) );
		for ( int i=0; i<nSizeX*nSizeY; ++i, ++pDst )
			*pDst = pci.ComposeColorSlow( pSrc[i] );
		//pImageMMP->AddMipLevel( &(outdata[0]), outdata.size() );
		return pImageMMP;
	}
	else if ( nBPP == 32 )
	{
		DWORD *pDst = reinterpret_cast<DWORD*>( &( outdata[0] ) );
		memcpy( pDst, pSrc, nSizeX*nSizeY*nBPP/8 );
		//pImageMMP->AddMipLevel( &(buffer[0]), buffer.size() );
		return pImageMMP;
	}
	// unsuccessfull load - destroy image
	delete pImageMMP;
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDDSImage* CImageProcessor::Compress( const IImage *pImage, EGFXPixelFormat format ) const
{
	if ( (format >= GFXPF_DXT1) && (format <= GFXPF_DXT5) )
		return CompressDXTN( pImage, format );
	else if ( (format >= GFXPF_ARGB8888) || (format <= GFXPF_ARGB0565) )
		return CompressRGBA( pImage, format );
	// CRAP{ still not all formats are realized
	else
		return 0;
	// CRAP}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::Decompress( const IDDSImage *pImage ) const
{
	if ( pImage->GetGFXFormat() == GFXPF_ARGB8888 ) 
	{
		CImage *pDstImage = new CImage( pImage->GetSizeX(0), pImage->GetSizeY(0) );
		memcpy( pDstImage->GetLFB(), pImage->GetLFB(), pImage->GetSizeX(0)*pImage->GetSizeY(0)*sizeof(SColor) );
		return pDstImage;
	}
	//
	//
	//
	const int width = pImage->GetSizeX( 0 );
	const int height = pImage->GetSizeY( 0 );
	int squishFlags = 0;
	switch ( pImage->GetGFXFormat() )
	{
		case GFXPF_DXT1: squishFlags = squish::kDxt1; break;
		case GFXPF_DXT2:
		case GFXPF_DXT3: squishFlags = squish::kDxt3; break;
		case GFXPF_DXT4:
		case GFXPF_DXT5: squishFlags = squish::kDxt5; break;
		default:
			return 0;
	}

	std::vector<BYTE> rgba( static_cast<size_t>(width) * height * 4 );
	squish::DecompressImage( rgba.data(), width, height, pImage->GetLFB(0), squishFlags );
	std::vector<DWORD> outdata( static_cast<size_t>(width) * height );
	for ( int i = 0; i < width * height; ++i )
	{
		const SColor color( rgba[i*4+3], rgba[i*4+0], rgba[i*4+1], rgba[i*4+2] );
		outdata[i] = color.color;
	}
	CImage *pDstImage = new CImage( width, height, outdata );
	return pDstImage;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// generate mip-levels and compress
IDDSImage* CImageProcessor::GenerateAndCompress( const IImage *pSrcImage, EGFXPixelFormat format, int nNumMipLevels ) const
{
	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	CImageDDS *pResultMMP = new CImageDDS( pSrcImage->GetSizeX(), pSrcImage->GetSizeY(), ddsformat );

	CPtr<IDDSImage> pMMP = Compress( pSrcImage, format );
	pResultMMP->AddMipLevels( pMMP );
	for ( int i=1; i<nNumMipLevels; ++i )
	{
		CPtr<IImage> pScaled = CreateMip( pSrcImage, i );
		CPtr<IDDSImage> pMMP = Compress( pScaled, format );
		pResultMMP->AddMipLevels( pMMP );
	}
	//
	return pResultMMP;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::CreateImage( int nSizeX, int nSizeY )
{
	return new CImage( nSizeX, nSizeY );
}
IImage* CImageProcessor::CreateImage( int nSizeX, int nSizeY, void *pData )
{
	CImage *pImage = new CImage( nSizeX, nSizeY );
	memcpy( pImage->GetLFB(), pData, nSizeX*nSizeY*4 );
	return pImage;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CImageProcessor::RestoreImage( IImage *pImage, const SColor &bg )
{
	// c0 * alpha + bg * (1 - alpha) = c1  => c0 = ( c1 - bg * (1 - alpha) ) / alpha
	//                                        c0 = c0, if alpha == 0
	SColor *pColors = pImage->GetLFB();
	float fBGr = float( bg.r ), fBGg = float( bg.g ), fBGb = float( bg.b );
	for ( int i=0; i<pImage->GetSizeX()*pImage->GetSizeY(); ++i )
	{
		if ( pColors[i].a != 0 )
		{
			float fAlpha = float( pColors[i].a ) / 255.0f;
			float fValue = ( float(pColors[i].r) - fBGr * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].r = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
			fValue = ( float(pColors[i].g) - fBGg * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].g = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
			fValue = ( float(pColors[i].b) - fBGb * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].b = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::GenerateImage( int nSizeX, int nSizeY, int nType )
{
	IImage *pImage = 0;
	switch ( nType )
	{
		case IGT_WHITE:
			pImage = CreateImage( nSizeX, nSizeY );
			pImage->Set( bit_cast<SColor>( 0xffffffff ) );
			break;
		case IGT_BLACK:
			pImage = CreateImage( nSizeX, nSizeY );
			pImage->Set( bit_cast<SColor>( 0xff000000 ) );
			break;
		case IGT_CHECKER:
			pImage = CreateImage( nSizeX, nSizeY );
			for ( int i=0; i<nSizeY; ++i )
			{
				SColor *pColors = pImage->GetLine( i );
				bool bOddY = ( ( i / (nSizeY / 16) ) & 1 ) != 0;
				for ( int j=0; j<nSizeX; ++j )
				{
					bool bOddX = ( ( j / (nSizeX / 16) ) & 1 ) != 0;
					pColors[j] = ( bOddX == bOddY ) ? 0xffffffff : 0xff000000;
				}
			}
			break;
		case IGT_SHADOW_INDEX1:
			NI_ASSERT_T( 0, "still not realized" );
			break;
		case IGT_SHADOW_INDEX2:
			NI_ASSERT_T( 0, "still not realized" );
			break;
	}
	return pImage;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline BYTE GetGammaCorrection( BYTE val, float fBrightness, float fPower, float fA, float fB )
{
  const float fVal = float( val ) / 255.0f;
  const float fGammaValue = pow( fVal, fPower );
  const float fContrastValue = Clamp( fA*fGammaValue + fB, 0.0f, 1.0f );
  const float fResult = Clamp( fContrastValue + fBrightness, 0.0f, 1.0f );
	return BYTE( fResult * 255.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CImageProcessor::CreateGammaCorrection( IImage *pSrc, float fBrightness, float fContrast, float fGamma )
{
	if ( (fBrightness == 0) && (fContrast == 0) && (fGamma == 0) )
		return CreateImage( pSrc->GetSizeX(), pSrc->GetSizeY(), pSrc->GetLFB() );
	//
	IImage *pDst = CreateImage( pSrc->GetSizeX(), pSrc->GetSizeY() );
  // build ramp from the brightness, contrast and gamma values
  // y = a*x + b
  // 
  fBrightness = Clamp( fBrightness, -1.0f, 1.0f ) * 0.5f; // to avoid complete dark and complete white values
  fContrast = Clamp( fContrast, -1.0f, 1.0f ) * 0.5f;
  fGamma = Clamp( fGamma, -1.0f, 1.0f ) * 0.5f;
  // calculate equation params for Y = A*X + B
  // contrast: a*x + b
  // если contrast < 0, то a = 1/a (наклон <45 градусов)
  float fA = 1.0f + 4.0f*fabs( fContrast );
  if ( fContrast < 0 )
    fA = 1.0f / fA;
  float fB = 0.5f*( 1.0f - fA );
  // gamma: x^power
  float fPower = 1;
  {
    if ( fGamma > 0 )
      fPower = 1.0f / ( 5.0f*fGamma + 1 );
    else if ( fGamma < 0 )
      fPower = 1.0f / ( 0.5f*fGamma + 1 );
  }
  // brightness: x + b
  // 
	for ( int i = 0; i != pSrc->GetSizeY(); ++i )
	{
		SColor *pDstColor = pDst->GetLine( i );
		SColor *pSrcColor = pSrc->GetLine( i );
		for ( int j = 0; j != pSrc->GetSizeX(); ++j )
		{
			pDstColor[j].a = pSrcColor[j].a;
			pDstColor[j].r = GetGammaCorrection( pSrcColor[j].r, fBrightness, fPower, fA, fB );
			pDstColor[j].g = GetGammaCorrection( pSrcColor[j].g, fBrightness, fPower, fA, fB );
			pDstColor[j].b = GetGammaCorrection( pSrcColor[j].b, fBrightness, fPower, fA, fB );
		}
	}
	//
	return pDst;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
