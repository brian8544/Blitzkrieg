#include "StdAfx.h"
#include "BinkVideoPlayer.h"

#include "..\SFX\SFX.h"
#include "..\GFX\GFXHelper.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

namespace
{
	void TraceFFmpegError( const char *where, int error )
	{
		char message[AV_ERROR_MAX_STRING_SIZE] = {};
		av_strerror( error, message, sizeof(message) );
		NStr::DebugTrace( "FFmpeg: %s failed: %s\n", where, message );
	}

	unsigned short Pack565( unsigned char r, unsigned char g, unsigned char b )
	{
		return static_cast<unsigned short>( ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) );
	}

	unsigned short Pack1555( unsigned char a, unsigned char r, unsigned char g, unsigned char b )
	{
		return static_cast<unsigned short>( ((a >= 128 ? 1 : 0) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3) );
	}

	unsigned short Pack4444( unsigned char a, unsigned char r, unsigned char g, unsigned char b )
	{
		return static_cast<unsigned short>( ((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4) );
	}
}

CBinkVideoPlayer::CBinkVideoPlayer()
	: bMaintainAspect( true ), dwCopyFlags( 0 ), dwPlayFlags( 0 ), bLooped( false ),
	  nLastPlayedFrame( -1 ), bStopped( false ), bPaused( false ),
	  nShadingEffectStart( 17 ), nShadingEffectFinish( 18 ),
	  pFormat( nullptr ), pVideoCodec( nullptr ), pAudioCodec( nullptr ),
	  pVideoFrame( nullptr ), pAudioFrame( nullptr ), pPacket( nullptr ), pAVIO( nullptr ),
	  pSws( nullptr ), pSwr( nullptr ), nVideoStream( -1 ), nAudioStream( -1 ), nAudioChannels( 0 ),
	  nWidth( 0 ), nHeight( 0 ), nNumFrames( 0 ), nLengthMs( 0 ), nCurrentFrame( -1 ),
	  fFrameRate( 0.0 ), fFrameAccumulatorMs( 0.0 ), timeLastUpdate( -1 ),
	  nMemoryPos( 0 ), pAudioInterface( nullptr )
{
	rcDstRect.SetEmpty();
}

CBinkVideoPlayer::~CBinkVideoPlayer()
{
	CloseMovie();
}

void CBinkVideoPlayer::CloseMovie()
{
	if ( pAudioInterface )
		pAudioInterface->EndMovieAudio();
	pAudioInterface = nullptr;

	if ( pSwr ) swr_free( &pSwr );
	if ( pSws ) { sws_freeContext( pSws ); pSws = nullptr; }
	if ( pPacket ) av_packet_free( &pPacket );
	if ( pAudioFrame ) av_frame_free( &pAudioFrame );
	if ( pVideoFrame ) av_frame_free( &pVideoFrame );
	if ( pAudioCodec ) avcodec_free_context( &pAudioCodec );
	if ( pVideoCodec ) avcodec_free_context( &pVideoCodec );

	if ( pFormat )
	{
		// For custom AVIO, avformat_close_input does not own the AVIOContext.
		avformat_close_input( &pFormat );
	}
	if ( pAVIO )
	{
		av_freep( &pAVIO->buffer );
		avio_context_free( &pAVIO );
	}

	buffer.clear();
	frameBGRA.clear();
	nMemoryPos = 0;
	nVideoStream = -1;
	nAudioStream = -1;
	nAudioChannels = 0;
	nWidth = nHeight = nNumFrames = nLengthMs = 0;
	nCurrentFrame = nLastPlayedFrame = -1;
	fFrameRate = 0.0;
	fFrameAccumulatorMs = 0.0;
	timeLastUpdate = -1;
	bPaused = false;
	bStopped = true;
}

int CBinkVideoPlayer::ReadMemoryPacket( void *pOpaque, std::uint8_t *pOut, int nBufferSize )
{
	CBinkVideoPlayer *pThis = static_cast<CBinkVideoPlayer*>( pOpaque );
	if ( !pThis || pThis->nMemoryPos >= pThis->buffer.size() )
		return AVERROR_EOF;
	const std::size_t remaining = pThis->buffer.size() - pThis->nMemoryPos;
	const std::size_t count = std::min<std::size_t>( remaining, static_cast<std::size_t>(nBufferSize) );
	std::memcpy( pOut, pThis->buffer.data() + pThis->nMemoryPos, count );
	pThis->nMemoryPos += count;
	return static_cast<int>( count );
}

std::int64_t CBinkVideoPlayer::SeekMemoryPacket( void *pOpaque, std::int64_t nOffset, int nWhence )
{
	CBinkVideoPlayer *pThis = static_cast<CBinkVideoPlayer*>( pOpaque );
	if ( !pThis )
		return AVERROR( EINVAL );
	if ( nWhence == AVSEEK_SIZE )
		return static_cast<std::int64_t>( pThis->buffer.size() );

	const int baseWhence = nWhence & ~AVSEEK_FORCE;
	std::int64_t base = 0;
	if ( baseWhence == SEEK_CUR ) base = static_cast<std::int64_t>( pThis->nMemoryPos );
	else if ( baseWhence == SEEK_END ) base = static_cast<std::int64_t>( pThis->buffer.size() );
	else if ( baseWhence != SEEK_SET ) return AVERROR( EINVAL );

	const std::int64_t next = base + nOffset;
	if ( next < 0 || next > static_cast<std::int64_t>(pThis->buffer.size()) )
		return AVERROR( EINVAL );
	pThis->nMemoryPos = static_cast<std::size_t>( next );
	return next;
}

bool CBinkVideoPlayer::OpenCodec( int nStreamIndex, AVCodecContext **ppCodec )
{
	if ( !pFormat || nStreamIndex < 0 || !ppCodec )
		return false;
	AVStream *pStream = pFormat->streams[nStreamIndex];
	const AVCodec *pDecoder = avcodec_find_decoder( pStream->codecpar->codec_id );
	if ( !pDecoder )
		return false;

	AVCodecContext *pCodec = avcodec_alloc_context3( pDecoder );
	if ( !pCodec )
		return false;
	int result = avcodec_parameters_to_context( pCodec, pStream->codecpar );
	if ( result >= 0 )
		result = avcodec_open2( pCodec, pDecoder, nullptr );
	if ( result < 0 )
	{
		TraceFFmpegError( "open decoder", result );
		avcodec_free_context( &pCodec );
		return false;
	}
	*ppCodec = pCodec;
	return true;
}

bool CBinkVideoPlayer::ConfigureAudio()
{
	if ( !pAudioCodec || !pAudioInterface )
		return false;

	AVChannelLayout inputLayout = {};
	AVChannelLayout outputLayout = {};
	if ( pAudioCodec->ch_layout.nb_channels > 0 )
		av_channel_layout_copy( &inputLayout, &pAudioCodec->ch_layout );
	else
		av_channel_layout_default( &inputLayout, 2 );
	av_channel_layout_copy( &outputLayout, &inputLayout );

	const int result = swr_alloc_set_opts2( &pSwr,
		&outputLayout, AV_SAMPLE_FMT_FLT, pAudioCodec->sample_rate,
		&inputLayout, pAudioCodec->sample_fmt, pAudioCodec->sample_rate,
		0, nullptr );
	nAudioChannels = outputLayout.nb_channels;
	av_channel_layout_uninit( &outputLayout );
	av_channel_layout_uninit( &inputLayout );
	if ( result < 0 || !pSwr || swr_init( pSwr ) < 0 )
	{
		NStr::DebugTrace( "FFmpeg: failed to initialize movie audio resampler\n" );
		return false;
	}
	return pAudioInterface->BeginMovieAudio( pAudioCodec->sample_rate, nAudioChannels );
}

bool CBinkVideoPlayer::OpenBink( const char *pszFileName, DWORD, DWORD dwFlags )
{
	CloseMovie();
	dwPlayFlags = dwFlags;
	bStopped = false;
	bPaused = false;

	int result = 0;
	if ( dwFlags & IVideoPlayer::PLAY_FROM_MEMORY )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( pszFileName, STREAM_ACCESS_READ );
		if ( pStream == 0 || pStream->GetSize() <= 0 )
			return false;
		buffer.resize( pStream->GetSize() );
		if ( pStream->Read( buffer.data(), static_cast<int>(buffer.size()) ) != static_cast<int>(buffer.size()) )
			return false;

		pFormat = avformat_alloc_context();
		unsigned char *pAVIOBuffer = static_cast<unsigned char*>( av_malloc( 32768 ) );
		if ( !pFormat || !pAVIOBuffer )
			return false;
		pAVIO = avio_alloc_context( pAVIOBuffer, 32768, 0, this, &CBinkVideoPlayer::ReadMemoryPacket, nullptr, &CBinkVideoPlayer::SeekMemoryPacket );
		if ( !pAVIO )
			return false;
		pFormat->pb = pAVIO;
		pFormat->flags |= AVFMT_FLAG_CUSTOM_IO;
		result = avformat_open_input( &pFormat, nullptr, nullptr, nullptr );
	}
	else
	{
		result = avformat_open_input( &pFormat, pszFileName, nullptr, nullptr );
	}
	if ( result < 0 )
	{
		TraceFFmpegError( "avformat_open_input", result );
		CloseMovie();
		return false;
	}
	result = avformat_find_stream_info( pFormat, nullptr );
	if ( result < 0 )
	{
		TraceFFmpegError( "avformat_find_stream_info", result );
		CloseMovie();
		return false;
	}

	nVideoStream = av_find_best_stream( pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0 );
	nAudioStream = av_find_best_stream( pFormat, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0 );
	if ( nVideoStream < 0 || !OpenCodec( nVideoStream, &pVideoCodec ) )
	{
		NStr::DebugTrace( "FFmpeg: movie has no decodable video stream: %s\n", pszFileName );
		CloseMovie();
		return false;
	}
	if ( nAudioStream >= 0 && !OpenCodec( nAudioStream, &pAudioCodec ) )
		nAudioStream = -1;

	pVideoFrame = av_frame_alloc();
	pAudioFrame = av_frame_alloc();
	pPacket = av_packet_alloc();
	if ( !pVideoFrame || !pAudioFrame || !pPacket )
	{
		CloseMovie();
		return false;
	}

	nWidth = pVideoCodec->width;
	nHeight = pVideoCodec->height;
	frameBGRA.resize( static_cast<std::size_t>(nWidth) * static_cast<std::size_t>(nHeight) * 4u );
	pSws = sws_getContext( nWidth, nHeight, pVideoCodec->pix_fmt,
		nWidth, nHeight, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr );
	if ( !pSws )
	{
		CloseMovie();
		return false;
	}

	const AVRational guessed = av_guess_frame_rate( pFormat, pFormat->streams[nVideoStream], nullptr );
	fFrameRate = guessed.num > 0 && guessed.den > 0 ? av_q2d( guessed ) : 25.0;
	if ( pFormat->duration > 0 )
		nLengthMs = static_cast<int>( pFormat->duration * 1000 / AV_TIME_BASE );
	else
		nLengthMs = 0;

	const int64_t streamFrames = pFormat->streams[nVideoStream]->nb_frames;
	nNumFrames = streamFrames > 0 ? static_cast<int>( std::min<int64_t>(streamFrames, 0x7fffffff) ) :
		(nLengthMs > 0 ? static_cast<int>( std::llround( nLengthMs * fFrameRate / 1000.0 ) ) : 0);

	nCurrentFrame = -1;
	nLastPlayedFrame = -1;
	fFrameAccumulatorMs = 0.0;
	timeLastUpdate = -1;
	return true;
}

void CBinkVideoPlayer::DecodeAudioPacket( AVPacket *pAudioPacket )
{
	if ( !pAudioCodec || !pSwr || !pAudioInterface || !pAudioPacket )
		return;
	if ( avcodec_send_packet( pAudioCodec, pAudioPacket ) < 0 )
		return;

	while ( avcodec_receive_frame( pAudioCodec, pAudioFrame ) == 0 )
	{
		const int channels = std::max( nAudioChannels, 1 );
		const int maxFrames = std::max( swr_get_out_samples( pSwr, pAudioFrame->nb_samples ), pAudioFrame->nb_samples );
		std::vector<float> pcm( static_cast<std::size_t>(maxFrames) * static_cast<std::size_t>(channels) );
		unsigned char *output[] = { reinterpret_cast<unsigned char*>( pcm.data() ) };
		const int inputPlanes = av_sample_fmt_is_planar( static_cast<AVSampleFormat>(pAudioFrame->format) ) ?
			std::max( pAudioFrame->ch_layout.nb_channels, 1 ) : 1;
		std::vector<const unsigned char*> input( static_cast<std::size_t>(inputPlanes) );
		for ( int i = 0; i < inputPlanes; ++i )
			input[i] = pAudioFrame->extended_data[i];
		const int frames = swr_convert( pSwr, output, maxFrames, input.data(), pAudioFrame->nb_samples );
		if ( frames > 0 )
			pAudioInterface->SubmitMovieAudio( pcm.data(), static_cast<unsigned int>(frames) );
		av_frame_unref( pAudioFrame );
	}
}

bool CBinkVideoPlayer::DecodeOneVideoFrame()
{
	if ( !pFormat || !pVideoCodec || !pPacket )
		return false;

	for (;;)
	{
		int receive = avcodec_receive_frame( pVideoCodec, pVideoFrame );
		if ( receive == 0 )
		{
			unsigned char *dstData[4] = { frameBGRA.data(), nullptr, nullptr, nullptr };
			int dstLinesize[4] = { nWidth * 4, 0, 0, 0 };
			sws_scale( pSws, pVideoFrame->data, pVideoFrame->linesize, 0, nHeight, dstData, dstLinesize );
			av_frame_unref( pVideoFrame );
			++nCurrentFrame;
			nLastPlayedFrame = nCurrentFrame;
			return true;
		}
		if ( receive != AVERROR(EAGAIN) && receive != AVERROR_EOF )
			return false;

		const int read = av_read_frame( pFormat, pPacket );
		if ( read < 0 )
		{
			avcodec_send_packet( pVideoCodec, nullptr );
			if ( avcodec_receive_frame( pVideoCodec, pVideoFrame ) == 0 )
				continue;
			if ( bLooped || (dwPlayFlags & IVideoPlayer::PLAY_LOOPED) )
				return RestartFromBeginning() && DecodeOneVideoFrame();
			bStopped = true;
			if ( pAudioInterface ) pAudioInterface->EndMovieAudio();
			return false;
		}

		if ( pPacket->stream_index == nAudioStream )
			DecodeAudioPacket( pPacket );
		else if ( pPacket->stream_index == nVideoStream )
			avcodec_send_packet( pVideoCodec, pPacket );
		av_packet_unref( pPacket );
	}
}

bool CBinkVideoPlayer::RestartFromBeginning()
{
	if ( !pFormat )
		return false;
	if ( av_seek_frame( pFormat, nVideoStream, 0, AVSEEK_FLAG_BACKWARD ) < 0 )
		return false;
	avcodec_flush_buffers( pVideoCodec );
	if ( pAudioCodec ) avcodec_flush_buffers( pAudioCodec );
	if ( pSwr ) swr_close( pSwr ), swr_init( pSwr );
	nCurrentFrame = -1;
	nLastPlayedFrame = -1;
	bStopped = false;
	if ( pAudioInterface && pAudioCodec )
	{
		pAudioInterface->EndMovieAudio();
		pAudioInterface->BeginMovieAudio( pAudioCodec->sample_rate,
			std::max( nAudioChannels, 1 ) );
	}
	return true;
}

void CBinkVideoPlayer::SetupRects()
{
	if ( rcDstRect.IsEmpty() || nWidth <= 0 || nHeight <= 0 || images.empty() || images[0].rcSrcRect.IsEmpty() )
		return;

	CTRect<float> destination = rcDstRect;
	if ( bMaintainAspect )
	{
		const float fCoeffX = destination.Width() / static_cast<float>(nWidth);
		const float fCoeffY = destination.Height() / static_cast<float>(nHeight);
		if ( fCoeffX < fCoeffY && std::fabs(fCoeffX - fCoeffY) > 0.001f )
		{
			const float newHeight = nHeight * fCoeffX;
			const float top = destination.y1 + (destination.Height() - newHeight) * 0.5f;
			destination.y1 = top;
			destination.y2 = top + newHeight;
		}
		else if ( fCoeffY < fCoeffX && std::fabs(fCoeffY - fCoeffX) > 0.001f )
		{
			const float newWidth = nWidth * fCoeffY;
			const float left = destination.x1 + (destination.Width() - newWidth) * 0.5f;
			destination.x1 = left;
			destination.x2 = left + newWidth;
		}
	}

	const float fCoeffX = destination.Width() / static_cast<float>(nWidth);
	const float fCoeffY = destination.Height() / static_cast<float>(nHeight);
	for ( CImagesList::iterator it = images.begin(); it != images.end(); ++it )
	{
		it->rcRect.x1 = static_cast<int>( destination.x1 + it->rcSrcRect.x1 * fCoeffX ) - 0.5f;
		it->rcRect.y1 = static_cast<int>( destination.y1 + it->rcSrcRect.y1 * fCoeffY ) - 0.5f;
		it->rcRect.x2 = static_cast<int>( destination.x1 + it->rcSrcRect.x2 * fCoeffX ) - 0.5f;
		it->rcRect.y2 = static_cast<int>( destination.y1 + it->rcSrcRect.y2 * fCoeffY ) - 0.5f;
	}
}

void CBinkVideoPlayer::SetTarget( IGFXTexture *pTexture, IGFX * )
{
	SImagePart image;
	image.pTexture = pTexture;
	images.clear();
	images.push_back( image );
}

void CBinkVideoPlayer::SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect )
{
	rcDstRect = _rcDstRect;
	bMaintainAspect = _bMaintainAspect;
	SetupRects();
}

int CBinkVideoPlayer::Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, ISFX *pSFX )
{
	Stop();
	szFileName = pszFileName ? pszFileName : "";
	pAudioInterface = pSFX;
	bLooped = bLooped || ((dwFlags & IVideoPlayer::PLAY_LOOPED) != 0);

	if ( !OpenBink( pszFileName, 0, dwFlags ) )
		return 0;
	pAudioInterface = pSFX;
	if ( pAudioCodec && pAudioInterface )
		ConfigureAudio();

	if ( images.size() == 1 && images[0].pTexture )
	{
		const int sizeX = Min( images[0].pTexture->GetSizeX(0), nWidth );
		const int sizeY = Min( images[0].pTexture->GetSizeY(0), nHeight );
		images[0].rcSrcRect.Set( 0, 0, sizeX, sizeY );
		images[0].rcDstRect.Set( 0, 0, sizeX, sizeY );
		images[0].rcMaps.Set( 0, 0, float(sizeX) / images[0].pTexture->GetSizeX(0), float(sizeY) / images[0].pTexture->GetSizeY(0) );
	}
	else
	{
		images.clear();
		const bool nonPow2 = (GetGlobalVar( "GFX.Caps.Texture.NonPow2Conditional", 0 ) != 0) || (GetGlobalVar( "GFX.Caps.Texture.NonPow2", 0 ) != 0);
		if ( nonPow2 )
		{
			SImagePart image;
			image.pTexture = pGFX->CreateTexture( nWidth, nHeight, 1, GFXPF_ARGB8888, GFXD_STATIC );
			image.rcSrcRect.Set( 0, 0, nWidth, nHeight );
			image.rcDstRect.Set( 0, 0, nWidth, nHeight );
			image.rcMaps.Set( 0, 0, 1, 1 );
			images.push_back( image );
		}
		else
		{
			const bool squareOnly = GetGlobalVar( "GFX.Caps.Texture.SquareOnly", 0 ) != 0;
			for ( int y = 0; y < nHeight; y += 256 )
			{
				for ( int x = 0; x < nWidth; x += 256 )
				{
					const int sourceWidth = Min( 256, nWidth - x );
					const int sourceHeight = Min( 256, nHeight - y );
					int textureWidth = sourceWidth < 256 ? GetNextPow2( sourceWidth ) : 256;
					int textureHeight = sourceHeight < 256 ? GetNextPow2( sourceHeight ) : 256;
					if ( squareOnly ) textureWidth = textureHeight = Max( textureWidth, textureHeight );
					SImagePart image;
					image.pTexture = pGFX->CreateTexture( textureWidth, textureHeight, 1, GFXPF_ARGB8888, GFXD_STATIC );
					image.rcSrcRect.Set( x, y, x + sourceWidth, y + sourceHeight );
					image.rcDstRect.Set( 0, 0, sourceWidth, sourceHeight );
					image.rcMaps.Set( 0, 0, float(sourceWidth) / textureWidth, float(sourceHeight) / textureHeight );
					images.push_back( image );
				}
			}
		}
	}

	for ( const SImagePart &image : images )
	{
		SSurfaceLockInfo lock;
		image.pTexture->Lock( 0, &lock );
		for ( int y = 0; y < image.pTexture->GetSizeY(0); ++y )
			std::memset( static_cast<char*>(lock.pData) + y * lock.nPitch, 0, lock.nPitch );
		image.pTexture->Unlock( 0 );
	}

	if ( rcDstRect.IsEmpty() )
		rcDstRect.Set( 0, 0, nWidth, nHeight );
	SetupRects();
	if ( DecodeOneVideoFrame() )
		CopyRects();
	return nLengthMs;
}

bool CBinkVideoPlayer::Stop()
{
	CloseMovie();
	return true;
}

bool CBinkVideoPlayer::Pause( bool bPause )
{
	if ( !pFormat )
		return false;
	bPaused = bPause;
	if ( pAudioInterface ) pAudioInterface->PauseMovieAudio( bPause );
	return true;
}

bool CBinkVideoPlayer::Update( const NTimer::STime &time, bool bForcedUpdate )
{
	if ( !pFormat )
		return (dwPlayFlags & IVideoPlayer::PLAY_INFINITE) != 0;
	if ( bStopped || bPaused )
		return IsPlaying() || ((dwPlayFlags & IVideoPlayer::PLAY_INFINITE) != 0);

	bool decoded = false;
	if ( bForcedUpdate )
	{
		decoded = DecodeOneVideoFrame();
	}
	else
	{
		if ( timeLastUpdate < 0 )
			timeLastUpdate = time;
		else
		{
			const double deltaMs = std::max<double>( 0.0, static_cast<double>(time - timeLastUpdate) );
			timeLastUpdate = time;
			fFrameAccumulatorMs += deltaMs;
		}

		const double frameMs = fFrameRate > 0.0 ? 1000.0 / fFrameRate : 40.0;
		int safety = 0;
		while ( fFrameAccumulatorMs >= frameMs && IsPlaying() && safety++ < 8 )
		{
			fFrameAccumulatorMs -= frameMs;
			decoded = DecodeOneVideoFrame() || decoded;
		}
	}
	if ( decoded ) CopyRects();
	return IsPlaying() || ((dwPlayFlags & IVideoPlayer::PLAY_INFINITE) != 0);
}

void CBinkVideoPlayer::CopyRects()
{
	if ( frameBGRA.empty() )
		return;
	for ( const SImagePart &image : images )
	{
		SSurfaceLockInfo lock;
		image.pTexture->Lock( 0, &lock );
		const int format = image.pTexture->GetFormat();
		for ( int y = 0; y < image.rcSrcRect.Height(); ++y )
		{
			const unsigned char *src = frameBGRA.data() + ((image.rcSrcRect.y1 + y) * nWidth + image.rcSrcRect.x1) * 4;
			unsigned char *dst = static_cast<unsigned char*>(lock.pData) + (image.rcDstRect.y1 + y) * lock.nPitch;
			if ( format == GFXPF_ARGB8888 )
			{
				std::memcpy( dst + image.rcDstRect.x1 * 4, src, image.rcSrcRect.Width() * 4 );
			}
			else
			{
				unsigned short *dst16 = reinterpret_cast<unsigned short*>(dst) + image.rcDstRect.x1;
				for ( int x = 0; x < image.rcSrcRect.Width(); ++x )
				{
					const unsigned char b = src[x*4+0], g = src[x*4+1], r = src[x*4+2], a = src[x*4+3];
					if ( format == GFXPF_ARGB0565 ) dst16[x] = Pack565( r, g, b );
					else if ( format == GFXPF_ARGB1555 ) dst16[x] = Pack1555( a, r, g, b );
					else if ( format == GFXPF_ARGB4444 ) dst16[x] = Pack4444( a, r, g, b );
				}
			}
		}
		image.pTexture->Unlock( 0 );
		image.pTexture->AddDirtyRect( 0 );
	}
}

bool CBinkVideoPlayer::Draw( IGFX *pGFX )
{
	pGFX->SetShadingEffect( nShadingEffectStart );
	for ( CImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
	{
		SGFXRect2 rect;
		rect.rect = it->rcRect;
		rect.maps = it->rcMaps;
		rect.color = 0xffffffff;
		rect.specular = 0xff000000;
		rect.fZ = 0;
		pGFX->SetTexture( 0, it->pTexture );
		pGFX->DrawRects( &rect, 1 );
	}
	pGFX->SetShadingEffect( nShadingEffectFinish );
	return true;
}

void CBinkVideoPlayer::Visit( ISceneVisitor *pVisitor, int )
{
	pVisitor->VisitSceneObject( this );
}

bool CBinkVideoPlayer::IsPlaying() const
{
	return pFormat != nullptr && !bStopped;
}

int CBinkVideoPlayer::GetCurrentFrame() const
{
	return IsPlaying() ? nCurrentFrame : -1;
}

bool CBinkVideoPlayer::SetCurrentFrame( const int nFrame )
{
	if ( !pFormat || nFrame < 0 || fFrameRate <= 0.0 )
		return false;
	AVStream *pStream = pFormat->streams[nVideoStream];
	const double seconds = nFrame / fFrameRate;
	const int64_t timestamp = av_rescale_q( static_cast<int64_t>(seconds * AV_TIME_BASE), AVRational{1, AV_TIME_BASE}, pStream->time_base );
	if ( av_seek_frame( pFormat, nVideoStream, timestamp, AVSEEK_FLAG_BACKWARD ) < 0 )
		return false;
	avcodec_flush_buffers( pVideoCodec );
	if ( pAudioCodec ) avcodec_flush_buffers( pAudioCodec );
	if ( pSwr )
	{
		swr_close( pSwr );
		if ( swr_init( pSwr ) < 0 )
			return false;
	}
	nCurrentFrame = nFrame - 1;
	if ( pAudioInterface && pAudioCodec )
	{
		pAudioInterface->EndMovieAudio();
		pAudioInterface->BeginMovieAudio( pAudioCodec->sample_rate,
			std::max( nAudioChannels, 1 ) );
	}
	const bool ok = DecodeOneVideoFrame();
	if ( ok ) CopyRects();
	return ok;
}

int CBinkVideoPlayer::GetLength() const
{
	return nLengthMs;
}

int CBinkVideoPlayer::GetNumFrames() const
{
	return nNumFrames;
}

bool CBinkVideoPlayer::GetMovieSize( CVec2 *pSize ) const
{
	if ( !pFormat || !pSize )
		return false;
	pSize->x = nWidth;
	pSize->y = nHeight;
	return true;
}

int CBinkVideoPlayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rcDstRect );
	saver.Add( 2, &bMaintainAspect );
	saver.Add( 3, &dwCopyFlags );
	saver.Add( 4, &bLooped );
	saver.Add( 5, &nLastPlayedFrame );
	saver.Add( 6, &szFileName );
	saver.Add( 7, &dwPlayFlags );
	saver.Add( 8, &nShadingEffectStart );
	saver.Add( 9, &nShadingEffectFinish );
	saver.Add( 10, &bStopped );
	bool bPlaying = IsPlaying();
	saver.Add( 20, &bPlaying );

	if ( saver.IsReading() )
	{
		buffer.clear();
		if ( bPlaying )
		{
			const int startFrame = nLastPlayedFrame;
			Play( szFileName.c_str(), dwPlayFlags, GetSingleton<IGFX>(), GetSingleton<ISFX>() );
			if ( startFrame > 0 ) SetCurrentFrame( startFrame );
		}
	}
	return 0;
}
