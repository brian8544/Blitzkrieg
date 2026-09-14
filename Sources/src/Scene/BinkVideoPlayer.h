#ifndef __BINKVIDEOPLAYER_H__
#define __BINKVIDEOPLAYER_H__
#pragma once

#include <cstddef>
#include <cstdint>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVIOContext;
struct SwsContext;
struct SwrContext;

struct SImagePart
{
	CPtr<IGFXTexture> pTexture;
	CTRect<long> rcSrcRect;
	CTRect<long> rcDstRect;
	CTRect<float> rcMaps;
	CTRect<float> rcRect;
	SImagePart() : rcSrcRect( 0, 0, 0, 0 ), rcDstRect( 0, 0, 0, 0 ), rcMaps( 0, 0, 0, 0 ), rcRect( 0, 0, 0, 0 ) {}
};
typedef std::vector<SImagePart> CImagesList;

// Kept under the historical name because Blitzkrieg assets are .bik files.
// The proprietary Bink SDK is no longer used; FFmpeg owns demuxing/decoding.
class CBinkVideoPlayer : public CTRefCount<IVideoPlayer>
{
	OBJECT_SERVICE_METHODS( CBinkVideoPlayer );
	DECLARE_SERIALIZE;

	CImagesList images;
	CTRect<float> rcDstRect;
	bool bMaintainAspect;
	DWORD dwCopyFlags;
	DWORD dwPlayFlags;
	bool bLooped;
	int nLastPlayedFrame;
	bool bStopped;
	bool bPaused;
	int nShadingEffectStart;
	int nShadingEffectFinish;
	std::vector<char> buffer;
	std::string szFileName;

	AVFormatContext *pFormat;
	AVCodecContext *pVideoCodec;
	AVCodecContext *pAudioCodec;
	AVFrame *pVideoFrame;
	AVFrame *pAudioFrame;
	AVPacket *pPacket;
	AVIOContext *pAVIO;
	SwsContext *pSws;
	SwrContext *pSwr;
	int nVideoStream;
	int nAudioStream;
	int nAudioChannels;
	int nWidth;
	int nHeight;
	int nNumFrames;
	int nLengthMs;
	int nCurrentFrame;
	double fFrameRate;
	double fFrameAccumulatorMs;
	NTimer::STime timeLastUpdate;
	std::size_t nMemoryPos;
	std::vector<unsigned char> frameBGRA;
	ISFX *pAudioInterface;

	bool OpenBink( const char *pszFileName, DWORD dwOpenFlags, DWORD dwFlags );
	void CloseMovie();
	bool OpenCodec( int nStreamIndex, AVCodecContext **ppCodec );
	bool ConfigureAudio();
	bool DecodeOneVideoFrame();
	void DecodeAudioPacket( AVPacket *pAudioPacket );
	bool RestartFromBeginning();
	void CopyRects();
	void SetupRects();

	static int ReadMemoryPacket( void *pOpaque, std::uint8_t *pBuffer, int nBufferSize );
	static std::int64_t SeekMemoryPacket( void *pOpaque, std::int64_t nOffset, int nWhence );

public:
	CBinkVideoPlayer();
	virtual ~CBinkVideoPlayer();
	virtual void STDCALL SetTarget( interface IGFXTexture *pTexture, IGFX *pGFX );
	virtual void STDCALL SetDstRect( const RECT &_rcDstRect, bool bMaintainAspect );
	virtual void STDCALL SetLoopMode( bool _bLooped ) { bLooped = _bLooped; }
	virtual int STDCALL GetCurrentFrame() const;
	virtual bool STDCALL SetCurrentFrame( const int nFrame );
	virtual void SetShadingEffect( const int nEffect, bool bStart )
	{
		if ( bStart ) nShadingEffectStart = nEffect;
		else nShadingEffectFinish = nEffect;
	}
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForcedUpdate );
	virtual int STDCALL Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, interface ISFX *pSFX );
	virtual bool STDCALL Stop();
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL IsPlaying() const;
	virtual int STDCALL GetLength() const;
	virtual int STDCALL GetNumFrames() const;
	virtual bool STDCALL GetMovieSize( CVec2 *pSize ) const;
	virtual bool STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
};

#endif // __BINKVIDEOPLAYER_H__
