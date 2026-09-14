#ifndef __SOUNDENGINE_H__
#define __SOUNDENGINE_H__
#pragma once

#include "SampleSounds.h"
#include "StreamFadeOff.h"
#include <mutex>
#include <unordered_map>
#include <vector>

FMOD::System* GetFMODSystem();
FMOD::Channel* GetFMODChannel( int nChannel );
bool IsFMODChannelPlaying( int nChannel, FMOD::Sound *pExpectedSound = nullptr );

class CSoundEngine : public ISFX
{
	OBJECT_NORMAL_METHODS( CSoundEngine );
	DECLARE_SERIALIZE;

	struct SDriverInfo
	{
		std::string szDriverName;
		bool isHardware3DAccelerated;
		bool supportEAXReverb;
		bool supportA3DOcclusions;
		bool supportA3DReflections;
		bool supportReverb;
	};

	struct SMelodyInfo
	{
		DECLARE_SERIALIZE;
	public:
		std::string szName;
		bool bLooped;
		SMelodyInfo() : bLooped( false ) {}
		void Clear() { szName.clear(); bLooped = false; }
		bool IsValid() const { return !szName.empty(); }
	};

	typedef std::vector<SDriverInfo> CDriversInfo;
	typedef std::unordered_map<ISound*, int, SDefaultPtrHash> CSoundChannelMap;
	typedef std::unordered_map<int, CPtr<ISound> > CChannelSoundMap;
	typedef std::unordered_map<int, FMOD::Channel*> CNativeChannelMap;

	CDriversInfo drivers;
	NTimer::STime timeLastUpdate;
	SMelodyInfo curMelody;
	SMelodyInfo nextMelody;

	FMOD::System *pSystem;
	FMOD::ChannelGroup *pSFXGroup;
	FMOD::ChannelGroup *pMusicGroup;
	FMOD::ChannelGroup *pMovieGroup;
	FMOD::Sound *pStreamingSound;
	FMOD::Channel *pStreamingChannel;
	int nStreamingChannel;
	NTimer::STime timeStreamFinished;

	CSoundChannelMap channelsMap;
	CChannelSoundMap soundsMap;
	CNativeChannelMap nativeChannels;

	float fListenerDistance;
	float fDistanceFactor;
	float fRolloffFactor;
	CVec3 vLastListenerPos;
	bool bInited;
	bool bEnableSFX;
	bool bEnableStreaming;
	bool bSoundCardPresent;
	bool bPaused;
	bool bStreamingPaused;
	BYTE cSFXMasterVolume;
	BYTE cStreamMasterVolume;
	float fStreamCurrentVolume;
	bool bStreamPlaying;
	CStreamFadeOff streamFadeOff;

	// FFmpeg movie audio is decoded to interleaved float PCM and consumed by an FMOD user stream.
	std::mutex movieAudioMutex;
	std::vector<float> movieAudioBuffer;
	std::size_t movieAudioReadOffset;
	int movieAudioChannels;
	int movieAudioSampleRate;
	bool movieAudioActive;
	FMOD::Sound *pMovieSound;
	FMOD::Channel *pMovieChannel;

	void ClearChannels();
	bool SearchDevices();
	void CloseStreaming();
	void ReEnableSounds();
	void UpdateGroupVolumes();
	FMOD::Channel* ResolveChannel( int nChannel ) const;
	int RegisterSound( CBaseSound *pSound, FMOD::Channel *pChannel, bool b3D );
	FMOD_RESULT ReadMoviePCM( void *pData, unsigned int nBytes );

	CSoundEngine();
	virtual ~CSoundEngine() { Done(); }
	void UpdateCameraPos( const CVec3 &vPos );

public:
	bool PlayNextMelody();
	void NotifyMelodyFinished();
	int PlayNativeSample( CBaseSound *pSound, bool b3D, const CVec3 *pPosition );
	void MapSound( ISound *pSound, int nChannel, FMOD::Channel *pNativeChannel );

	virtual BYTE STDCALL GetSFXMasterVolume() const { return cSFXMasterVolume; }
	virtual BYTE STDCALL GetStreamMasterVolume() const { return cStreamMasterVolume; }
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID );
	virtual bool STDCALL IsInitialized();
	virtual bool STDCALL Init( HWND hWnd, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels );
	virtual void STDCALL Done();

	virtual void STDCALL EnableSFX( bool bEnable ) { bEnableSFX = bEnable; ReEnableSounds(); }
	virtual void STDCALL EnableStreaming( bool bEnable ) { bEnableStreaming = bEnable; ReEnableSounds(); }
	virtual bool STDCALL IsSFXEnabled()const { return bEnableSFX && bSoundCardPresent; }
	virtual bool STDCALL IsStreamingEnabled()const { return bEnableStreaming && bSoundCardPresent; }

	virtual void STDCALL SetDistanceFactor( float fFactor );
	virtual void STDCALL SetRolloffFactor( float fFactor );
	virtual void STDCALL SetSFXMasterVolume( float fVolume );
	virtual void STDCALL SetStreamMasterVolume( float fVolume );

	virtual void STDCALL PlayStream( const char *pszFileName, bool bLooped = false, const unsigned int nTimeToFadePrevious = 0 );
	virtual void STDCALL StopStream( const unsigned int nTimeToFade = 0 );
	virtual bool STDCALL IsStreamPlaying() const;
	virtual void STDCALL SetStreamVolume( const float fVolume );
	virtual float STDCALL GetStreamVolume() const;

	virtual bool STDCALL BeginMovieAudio( int nSampleRate, int nChannels );
	virtual void STDCALL SubmitMovieAudio( const float *pInterleavedSamples, unsigned int nFrames );
	virtual void STDCALL PauseMovieAudio( bool bPause );
	virtual void STDCALL EndMovieAudio();

	virtual int STDCALL PlaySample( ISound *pSound, bool bLooped = false, unsigned int nStartPos=0 );
	virtual void STDCALL StopSample( ISound *pSound );
	virtual void STDCALL UpdateSample( ISound *pSound );
	virtual void STDCALL StopChannel( int nChannel );
	virtual void STDCALL Update( interface ICamera *pCamera );
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL PauseStreaming( bool bPause );
	virtual bool STDCALL IsPaused();
	virtual bool STDCALL IsPlaying( ISound *pSound );
	virtual unsigned int STDCALL GetCurrentPosition( ISound *pSound );
	virtual void STDCALL SetCurrentPosition( ISound *pSound, unsigned int pos );

	friend FMOD::System* GetFMODSystem();
	friend FMOD::Channel* GetFMODChannel( int nChannel );
	friend bool IsFMODChannelPlaying( int nChannel, FMOD::Sound *pExpectedSound );
	friend FMOD_RESULT F_CALLBACK BlitzMoviePCMReadCallback( FMOD_SOUND *pSound, void *pData, unsigned int nDataLen );
};

#endif // __SOUNDENGINE_H__
