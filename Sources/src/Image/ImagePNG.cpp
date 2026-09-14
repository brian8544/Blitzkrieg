#include "StdAfx.h"
#include "ImagePNG.h"

#include <png.h>

#include <csetjmp>
#include <vector>

namespace
{
	void PNGReadFunction( png_structp png, png_bytep data, png_size_t length )
	{
		IDataStream *pStream = static_cast<IDataStream*>( png_get_io_ptr( png ) );
		if ( !pStream || pStream->Read( data, static_cast<int>(length) ) != static_cast<int>(length) )
			png_error( png, "Read error" );
	}

	void PNGWriteFunction( png_structp png, png_bytep data, png_size_t length )
	{
		IDataStream *pStream = static_cast<IDataStream*>( png_get_io_ptr( png ) );
		if ( !pStream || pStream->Write( data, static_cast<int>(length) ) != static_cast<int>(length) )
			png_error( png, "Write error" );
	}

	void PNGFlushFunction( png_structp )
	{
	}
}

bool NImage::RecognizeFormatPNG( IDataStream *pStream )
{
	if ( !pStream )
		return false;
	png_byte signature[8] = {};
	const int count = pStream->Read( signature, sizeof(signature) );
	pStream->Seek( -count, STREAM_SEEK_CUR );
	return count == sizeof(signature) && png_sig_cmp( signature, 0, sizeof(signature) ) == 0;
}

CImage* NImage::LoadImagePNG( IDataStream *pStream )
{
	if ( !pStream )
		return nullptr;

	png_structp png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
	if ( !png )
		return nullptr;
	png_infop info = png_create_info_struct( png );
	if ( !info )
	{
		png_destroy_read_struct( &png, nullptr, nullptr );
		return nullptr;
	}

	if ( setjmp( png_jmpbuf( png ) ) )
	{
		png_destroy_read_struct( &png, &info, nullptr );
		return nullptr;
	}

	png_set_read_fn( png, pStream, PNGReadFunction );
	png_read_info( png, info );

	const png_uint_32 width = png_get_image_width( png, info );
	const png_uint_32 height = png_get_image_height( png, info );
	int colorType = png_get_color_type( png, info );
	int bitDepth = png_get_bit_depth( png, info );

	if ( bitDepth == 16 )
		png_set_strip_16( png );
	if ( colorType == PNG_COLOR_TYPE_PALETTE )
		png_set_palette_to_rgb( png );
	if ( colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8 )
		png_set_expand_gray_1_2_4_to_8( png );
	if ( png_get_valid( png, info, PNG_INFO_tRNS ) )
		png_set_tRNS_to_alpha( png );
	if ( colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb( png );
	if ( (colorType & PNG_COLOR_MASK_ALPHA) == 0 && !png_get_valid( png, info, PNG_INFO_tRNS ) )
		png_set_add_alpha( png, 0xff, PNG_FILLER_AFTER );

	png_set_interlace_handling( png );
	png_read_update_info( png, info );

	const png_size_t rowBytes = png_get_rowbytes( png, info );
	std::vector<png_byte> rgba( rowBytes * height );
	std::vector<png_bytep> rows( height );
	for ( png_uint_32 y = 0; y < height; ++y )
		rows[y] = rgba.data() + y * rowBytes;
	png_read_image( png, rows.data() );
	png_read_end( png, info );

	std::vector<DWORD> image( static_cast<std::size_t>(width) * height );
	for ( png_uint_32 y = 0; y < height; ++y )
	{
		const png_byte *row = rows[y];
		for ( png_uint_32 x = 0; x < width; ++x )
		{
			const png_byte *pixel = row + x * 4;
			image[y * width + x] = (DWORD(pixel[3]) << 24) | (DWORD(pixel[0]) << 16) | (DWORD(pixel[1]) << 8) | DWORD(pixel[2]);
		}
	}

	png_destroy_read_struct( &png, &info, nullptr );
	return new CImage( static_cast<int>(width), static_cast<int>(height), image );
}

bool NImage::SaveImageAsPNG( IDataStream *pStream, const IImage *pImage )
{
	if ( !pStream || !pImage || pImage->GetSizeX() <= 0 || pImage->GetSizeY() <= 0 )
		return false;

	png_structp png = png_create_write_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
	if ( !png )
		return false;
	png_infop info = png_create_info_struct( png );
	if ( !info )
	{
		png_destroy_write_struct( &png, nullptr );
		return false;
	}

	if ( setjmp( png_jmpbuf( png ) ) )
	{
		png_destroy_write_struct( &png, &info );
		return false;
	}

	png_set_write_fn( png, pStream, PNGWriteFunction, PNGFlushFunction );
	png_set_IHDR( png, info,
		static_cast<png_uint_32>(pImage->GetSizeX()), static_cast<png_uint_32>(pImage->GetSizeY()),
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
	png_write_info( png, info );

	const int width = pImage->GetSizeX();
	const int height = pImage->GetSizeY();
	std::vector<png_byte> rgba( static_cast<std::size_t>(width) * height * 4 );
	std::vector<png_bytep> rows( height );
	const SColor *colors = pImage->GetLFB();
	for ( int y = 0; y < height; ++y )
	{
		rows[y] = rgba.data() + static_cast<std::size_t>(y) * width * 4;
		for ( int x = 0; x < width; ++x )
		{
			const SColor &color = colors[y * width + x];
			png_byte *pixel = rows[y] + x * 4;
			pixel[0] = color.r;
			pixel[1] = color.g;
			pixel[2] = color.b;
			pixel[3] = color.a;
		}
	}

	png_write_image( png, rows.data() );
	png_write_end( png, info );
	png_destroy_write_struct( &png, &info );
	return true;
}
