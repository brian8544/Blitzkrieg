#include "StdAfx.h"

#include "Font.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
inline int GetStrLen( const TYPE *pszString )
{
	int nCounter = 0;
	while ( *pszString++ != 0 )
		++nCounter;
	return nCounter;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CFont::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	//saver.Add( 1, &format );
	saver.Add( 2, &pTexture );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFont::SwapData( ISharedResource *pResource )
{
	CFont *pRes = dynamic_cast<CFont*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CFont", return );
	//
	std::swap( format, pRes->format );
	//std::swap( pTexture, pRes->pTexture );
	std::swap( baseFormat, pRes->baseFormat );
	std::swap( pBaseTexture, pRes->pBaseTexture );
	scaledFonts.clear();
	nPreparedScalePercent = 100;
	pTexture = pBaseTexture;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// fill geometry data for one string.
// NOTE: no special characters available
bool CFont::FillGeometryData( const char *pszString, float sx, const float sy, 
                              const DWORD dwColor, const DWORD dwSpecular, 
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	// check for string length
	const int nStrLen = GetStrLen( pszString );
	// check for string length
	if ( nStrLen == 0 )
		return sx;                       // can't render zero-length string
	vertices.clear();
	indices.clear();
	vertices.reserve( nStrLen*4 );
  indices.reserve( nStrLen*6 );
	// visit all characters and create TL vertices (6 for each)
	CTextNoClipVisitor visitor( vertices, indices, format.metrics.nHeight, dwColor, dwSpecular );
	VisitText( pszString, pszString + nStrLen, sx, sy, visitor );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFont::FillGeometryData( const wchar_t *pszString, float sx, const float sy, 
                              const DWORD dwColor, const DWORD dwSpecular, 
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	// check for string length
	const int nStrLen = GetStrLen( pszString );
	// check for string length
	if ( nStrLen == 0 )
		return sx;                       // can't render zero-length string
	vertices.clear();
	indices.clear();
	vertices.reserve( nStrLen*4 );
  indices.reserve( nStrLen*6 );
	// visit all characters and create TL vertices (6 for each)
	CTextNoClipVisitor visitor( vertices, indices, format.metrics.nHeight, dwColor, dwSpecular );
	VisitText( pszString, pszString + nStrLen, sx, sy, visitor );
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	const int FONT_ATLAS_PADDING = 2;

	inline int FontScaleNextPow2( int nValue )
	{
		int nResult = 1;
		while ( nResult < nValue )
			nResult <<= 1;
		return nResult;
	}

	struct SRuntimeGlyph
	{
		WORD wChar;
		ABC abc;
		int nWidth;
		int nX, nY;
		SRuntimeGlyph() : wChar( 0 ), nWidth( 0 ), nX( 0 ), nY( 0 ) { Zero( abc ); }
	};
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFont::BuildScaledFont( const int nScalePercent, SScaledFontData *pData )
{
	if ( pData == 0 || pBaseTexture == 0 || nScalePercent <= 100 )
		return false;

	const float fScale = float(nScalePercent) / 100.0f;
	const int nRequestedHeight = Max( 1, int(baseFormat.metrics.nHeight * fScale + 0.5f) );
	const std::string szFaceName = baseFormat.szFaceName.empty() ? "Times New Roman" : baseFormat.szFaceName;

	HDC hDC = CreateCompatibleDC( 0 );
	if ( hDC == 0 )
		return false;

	if ( baseFormat.szFaceName.empty() )
	{
		HFONT hValidationFont = CreateFontA( baseFormat.metrics.nHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			baseFormat.metrics.cCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, VARIABLE_PITCH, szFaceName.c_str() );
		if ( hValidationFont == 0 )
		{
			DeleteDC( hDC );
			return false;
		}
		HFONT hOldValidationFont = (HFONT)SelectObject( hDC, hValidationFont );
		TEXTMETRIC tmValidation;
		Zero( tmValidation );
		bool bMetricsMatch = GetTextMetrics( hDC, &tmValidation ) != FALSE;
		if ( bMetricsMatch )
		{
			bMetricsMatch = abs(tmValidation.tmHeight - baseFormat.metrics.nHeight) <= 1 &&
				abs(tmValidation.tmAscent - baseFormat.metrics.nAscent) <= 1 &&
				abs(tmValidation.tmDescent - baseFormat.metrics.nDescent) <= 1 &&
				abs(tmValidation.tmAveCharWidth - baseFormat.metrics.nAveCharWidth) <= 1;
		}
		const WORD samples[] = { 'A', 'M', 'W', 'i', '0' };
		int nCompared = 0, nMatched = 0;
		for ( int i = 0; bMetricsMatch && i != sizeof(samples)/sizeof(samples[0]); ++i )
		{
			SFontFormat::CCharacterMap::const_iterator it = baseFormat.chars.find( samples[i] );
			if ( it == baseFormat.chars.end() )
				continue;
			ABC abc;
			if ( !GetCharABCWidthsW(hDC, samples[i], samples[i], &abc) )
				continue;
			++nCompared;
			const SFontFormat::SCharDesc &ch = it->second;
			if ( abs(int(ch.fA) - int(abc.abcA)) <= 1 && abs(int(ch.fB) - int(abc.abcB)) <= 1 && abs(int(ch.fC) - int(abc.abcC)) <= 1 )
				++nMatched;
		}
		SelectObject( hDC, hOldValidationFont );
		DeleteObject( hValidationFont );
		if ( !bMetricsMatch || (nCompared >= 3 && nMatched < nCompared - 1) )
		{
			DeleteDC( hDC );
			return false;
		}
	}

	HFONT hFont = CreateFontA( nRequestedHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		baseFormat.metrics.cCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY, VARIABLE_PITCH, szFaceName.c_str() );
	if ( hFont == 0 )
	{
		DeleteDC( hDC );
		return false;
	}

	HFONT hOldFont = (HFONT)SelectObject( hDC, hFont );
	char szActualFace[LF_FACESIZE];
	memset( szActualFace, 0, sizeof(szActualFace) );
	GetTextFaceA( hDC, LF_FACESIZE, szActualFace );
	if ( szActualFace[0] != 0 && _stricmp(szActualFace, szFaceName.c_str()) != 0 )
	{
		SelectObject( hDC, hOldFont );
		DeleteObject( hFont );
		DeleteDC( hDC );
		return false; // Do not silently replace an original game font with a Windows fallback face.
	}

	TEXTMETRIC tm;
	Zero( tm );
	if ( !GetTextMetrics(hDC, &tm) || tm.tmHeight <= 0 )
	{
		SelectObject( hDC, hOldFont );
		DeleteObject( hFont );
		DeleteDC( hDC );
		return false;
	}

	std::vector<SRuntimeGlyph> glyphs;
	glyphs.reserve( baseFormat.chars.size() );
	for ( SFontFormat::CCharacterMap::const_iterator it = baseFormat.chars.begin(); it != baseFormat.chars.end(); ++it )
	{
		SRuntimeGlyph glyph;
		glyph.wChar = it->first;
		if ( !GetCharABCWidthsW(hDC, glyph.wChar, glyph.wChar, &glyph.abc) )
		{
			SIZE size;
			Zero( size );
			WCHAR wc = glyph.wChar;
			if ( !GetTextExtentPoint32W(hDC, &wc, 1, &size) )
				continue;
			glyph.abc.abcA = 0;
			glyph.abc.abcB = size.cx;
			glyph.abc.abcC = 0;
		}
		glyph.nWidth = Max( 1, int(glyph.abc.abcB) + Max(int(glyph.abc.abcC), 0) );
		glyphs.push_back( glyph );
	}
	if ( glyphs.empty() )
	{
		SelectObject( hDC, hOldFont );
		DeleteObject( hFont );
		DeleteDC( hDC );
		return false;
	}

	int nAtlasWidth = FontScaleNextPow2( Max(64, int(pBaseTexture->GetSizeX(0) * fScale + 0.5f)) );
	int nAtlasHeight = FontScaleNextPow2( Max(64, int(pBaseTexture->GetSizeY(0) * fScale + 0.5f)) );
	for ( ;; )
	{
		int x = FONT_ATLAS_PADDING;
		int y = 0;
		bool bFits = true;
		for ( int i = 0; i != glyphs.size(); ++i )
		{
			if ( x + glyphs[i].nWidth + FONT_ATLAS_PADDING > nAtlasWidth )
			{
				x = FONT_ATLAS_PADDING;
				y += tm.tmHeight;
			}
			if ( y + tm.tmHeight > nAtlasHeight )
			{
				bFits = false;
				break;
			}
			glyphs[i].nX = x;
			glyphs[i].nY = y;
			x += glyphs[i].nWidth + FONT_ATLAS_PADDING * 2;
		}
		if ( bFits )
			break;
		if ( nAtlasHeight <= nAtlasWidth )
			nAtlasHeight <<= 1;
		else
			nAtlasWidth <<= 1;
		if ( nAtlasWidth > 4096 || nAtlasHeight > 4096 )
		{
			SelectObject( hDC, hOldFont );
			DeleteObject( hFont );
			DeleteDC( hDC );
			return false;
		}
	}

	BITMAPINFO bmi;
	Zero( bmi );
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = nAtlasWidth;
	bmi.bmiHeader.biHeight = -nAtlasHeight; // top-down: texture V coordinates match GDI Y coordinates
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	BYTE *pBitmapBits = 0;
	HBITMAP hBitmap = CreateDIBSection( hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, 0, 0 );
	if ( hBitmap == 0 || pBitmapBits == 0 )
	{
		SelectObject( hDC, hOldFont );
		DeleteObject( hFont );
		DeleteDC( hDC );
		return false;
	}
	HBITMAP hOldBitmap = (HBITMAP)SelectObject( hDC, hBitmap );
	memset( pBitmapBits, 0, nAtlasWidth * nAtlasHeight * 4 );
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, RGB(255, 255, 255) );
	SetTextAlign( hDC, TA_TOP | TA_LEFT | TA_NOUPDATECP );

	pData->format = baseFormat; // keep original metrics, ABC spacing, wrapping and line heights in logical UI pixels
	for ( int i = 0; i != glyphs.size(); ++i )
	{
		const SRuntimeGlyph &glyph = glyphs[i];
		WCHAR wc = glyph.wChar;
		TextOutW( hDC, glyph.nX - glyph.abc.abcA, glyph.nY, &wc, 1 );

		SFontFormat::CCharacterMap::iterator fmtIt = pData->format.chars.find( glyph.wChar );
		if ( fmtIt != pData->format.chars.end() )
		{
			SFontFormat::SCharDesc &ch = fmtIt->second;
			ch.x1 = float(glyph.nX + 0.5f) / nAtlasWidth;
			ch.y1 = float(glyph.nY + 0.5f) / nAtlasHeight;
			ch.x2 = float(glyph.nX + glyph.nWidth + 0.5f) / nAtlasWidth;
			ch.y2 = float(glyph.nY + tm.tmHeight + 0.5f) / nAtlasHeight;
		}
	}

	CPtr<IGFXTexture> pScaledTexture = GetSingleton<IGFX>()->CreateTexture( nAtlasWidth, nAtlasHeight, 1, GFXPF_ARGB8888, GFXD_STATIC );
	if ( pScaledTexture != 0 )
	{
		SSurfaceLockInfo lockinfo;
		if ( pScaledTexture->Lock(0, &lockinfo) )
		{
			for ( int y = 0; y != nAtlasHeight; ++y )
			{
				DWORD *pDst = (DWORD*)((BYTE*)lockinfo.pData + y * lockinfo.nPitch);
				const BYTE *pSrc = pBitmapBits + y * nAtlasWidth * 4;
				for ( int x = 0; x != nAtlasWidth; ++x )
				{
					const DWORD c = pSrc[x * 4];
					pDst[x] = (c << 24) | (c << 16) | (c << 8) | c;
				}
			}
			pScaledTexture->Unlock( 0 );
		}
		else
			pScaledTexture = 0;
	}

	SelectObject( hDC, hOldBitmap );
	SelectObject( hDC, hOldFont );
	DeleteObject( hBitmap );
	DeleteObject( hFont );
	DeleteDC( hDC );

	if ( pScaledTexture == 0 )
		return false;
	pData->nScalePercent = nScalePercent;
	pData->pTexture = pScaledTexture;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFont::PrepareForScale( const float fScale )
{
	int nScalePercent = int(fScale * 100.0f + 0.5f);
	if ( nScalePercent <= 112 )
		nScalePercent = 100;
	else if ( nScalePercent <= 137 )
		nScalePercent = 125;
	else if ( nScalePercent <= 175 )
		nScalePercent = 150;
	else
		nScalePercent = 200;

	if ( nPreparedScalePercent == nScalePercent )
		return;

	if ( nScalePercent == 100 )
	{
		format = baseFormat;
		pTexture = pBaseTexture;
		nPreparedScalePercent = 100;
		return;
	}

	for ( int i = 0; i != scaledFonts.size(); ++i )
	{
		if ( scaledFonts[i].nScalePercent == nScalePercent )
		{
			format = scaledFonts[i].format;
			pTexture = scaledFonts[i].pTexture.GetPtr();
			nPreparedScalePercent = nScalePercent;
			return;
		}
	}

	SScaledFontData data;
	if ( BuildScaledFont(nScalePercent, &data) )
	{
		scaledFonts.push_back( data );
		format = scaledFonts.back().format;
		pTexture = scaledFonts.back().pTexture.GetPtr();
		nPreparedScalePercent = nScalePercent;
		return;
	}

	format = baseFormat;
	pTexture = pBaseTexture;
	nPreparedScalePercent = 100;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CFont::EstimateTextWidth( const char *pszString ) const
{
  return format.metrics.nAveCharWidth * static_cast<int>( strlen( pszString ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFont::Load( const bool bPreLoad )
{
	SFontFormat localformat;
	{
		std::string szStreamName = GetSharedResourceFullName();
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT_T( pStream != 0, NStr::Format("Can't open stream \"%s\" to load font", szStreamName.c_str()) );
		if ( pStream == 0 )
			return false;
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &localformat );
	}

	CPtr<IGFXTexture> pTexture = GetSingleton<ITextureManager>()->GetTexture( (szSharedResourceName + "\\1").c_str() );
	return Init( localformat, pTexture );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
