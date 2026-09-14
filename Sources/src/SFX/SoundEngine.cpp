#include "StdAfx.h"

#include "SoundEngine.h"

#include "SampleSounds.h"
#include "..\Scene\Scene.h"
#include "..\Formats\fmtTerrain.h"

#include <algorithm>
#include <cstring>
#include <list>

namespace
{
	CSoundEngine *g_pSoundEngine = nullptr;

	float ByteVolumeToFloat( BYTE value )
	{
		return static_cast<float>( value ) / 255.0f;
	}

	float Clamp01( float value )
	{
		return std::max( 0.0f, std::min( value, 1.0f ) );
	}

	void TraceFMODError( const char *where, FMOD_RESULT result )
	{
		if ( result != FMOD_OK )
			NStr::DebugTrace( "FMOD: %s failed: %s\n", where, FMOD_ErrorString( result ) );
	}
}

FMOD::System* GetFMODSystem()
{
	return g_pSoundEngine ? g_pSoundEngine->pSystem : nullptr;
}

FMOD::Channel* GetFMODChannel( int nChannel )
{
	return g_pSoundEngine ? g_pSoundEngine->ResolveChannel( nChannel ) : nullptr;
}

bool IsFMODChannelPlaying( int nChannel, FMOD::Sound *pExpectedSound )
{
	FMOD::Channel *pChannel = GetFMODChannel( nChannel );
	if ( !pChannel )
		return false;

	bool playing = false;
	if ( pChannel->isPlaying( &playing ) != FMOD_OK || !playing )
		return false;

	if ( pExpectedSound )
	{
		FMOD::Sound *pCurrent = nullptr;
		if ( pChannel->getCurrentSound( &pCurrent ) != FMOD_OK || pCurrent != pExpectedSound )
			return false;
	}
	return true;
}

class CPlayVisitor : public ISFXVisitor
{
	CSoundEngine *pSFX = nullptr;
public:
	void Init( CSoundEngine *_pSFX ) { pSFX = _pSFX; }
	virtual int STDCALL VisitSound2D( CSound2D *pSound )
	{
		return pSFX ? pSFX->PlayNativeSample( pSound, false, nullptr ) : -1;
	}
	virtual int STDCALL VisitSound3D( CSound3D *pSound, const CVec3 &vPos )
	{
		return pSFX ? pSFX->PlayNativeSample( pSound, true, &vPos ) : -1;
	}
};

static CPlayVisitor thePlayVisitor;

CSoundEngine::CSoundEngine()
	: pSystem( nullptr ), pSFXGroup( nullptr ), pMusicGroup( nullptr ), pMovieGroup( nullptr ),
	  pStreamingSound( nullptr ), pStreamingChannel( nullptr ), nStreamingChannel( -1 ),
	  timeLastUpdate( -1 ), timeStreamFinished( -1 ), fListenerDistance( 0.0f ),
	  fDistanceFactor( 1.0f ), fRolloffFactor( 1.0f ), vLastListenerPos( VNULL3 ),
	  bInited( false ), bEnableSFX( true ), bEnableStreaming( true ), bSoundCardPresent( true ),
	  bPaused( false ), bStreamingPaused( false ), cSFXMasterVolume( 255 ), cStreamMasterVolume( 255 ),
	  fStreamCurrentVolume( 1.0f ), bStreamPlaying( false ), movieAudioReadOffset( 0 ),
	  movieAudioChannels( 0 ), movieAudioSampleRate( 0 ), movieAudioActive( false ),
	  pMovieSound( nullptr ), pMovieChannel( nullptr )
{
}

bool CSoundEngine::SearchDevices()
{
	if ( pSystem )
		return true;

	FMOD_RESULT result = FMOD::System_Create( &pSystem );
	if ( result != FMOD_OK || !pSystem )
	{
		TraceFMODError( "System_Create", result );
		pSystem = nullptr;
		return false;
	}

	int nNumDrivers = 0;
	result = pSystem->getNumDrivers( &nNumDrivers );
	if ( result != FMOD_OK )
	{
		TraceFMODError( "getNumDrivers", result );
		return false;
	}

	drivers.clear();
	drivers.resize( std::max( nNumDrivers, 0 ) );
	for ( int i = 0; i < nNumDrivers; ++i )
	{
		char name[256] = {};
		if ( pSystem->getDriverInfo( i, name, sizeof(name), nullptr, nullptr, nullptr, nullptr ) != FMOD_OK )
			strcpy_s( name, sizeof(name), "FMOD output" );

		SDriverInfo &dr = drivers[i];
		dr.szDriverName = name;
		// These FMOD 3-era capability flags no longer have direct equivalents.
		dr.isHardware3DAccelerated = false;
		dr.supportEAXReverb = false;
		dr.supportA3DOcclusions = false;
		dr.supportA3DReflections = false;
		dr.supportReverb = false;
	}
	return true;
}

bool CSoundEngine::IsInitialized()
{
	return bInited;
}

IRefCount* CSoundEngine::QI( int )
{
	// The old implementation leaked DirectSound through QI(0) for Bink.
	// Movie playback now goes FFmpeg -> ISFX PCM -> FMOD, so no native output handle is exposed.
	return nullptr;
}

bool CSoundEngine::Init( HWND, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels )
{
	Done();

	if ( !SearchDevices() )
		return false;

	bSoundCardPresent = output != SFX_OUTPUT_NO;
	FMOD_RESULT result = pSystem->setOutput( bSoundCardPresent ? FMOD_OUTPUTTYPE_AUTODETECT : FMOD_OUTPUTTYPE_NOSOUND );
	if ( result != FMOD_OK )
		TraceFMODError( "setOutput", result );

	if ( bSoundCardPresent && !drivers.empty() )
	{
		if ( nDriver < 0 || nDriver >= static_cast<int>(drivers.size()) )
			nDriver = 0;
		result = pSystem->setDriver( nDriver );
		if ( result != FMOD_OK )
			TraceFMODError( "setDriver", result );
	}

	if ( nMixRate > 0 )
	{
		result = pSystem->setSoftwareFormat( nMixRate, FMOD_SPEAKERMODE_DEFAULT, 0 );
		if ( result != FMOD_OK )
			TraceFMODError( "setSoftwareFormat", result );
	}

	result = pSystem->init( std::max( nMaxChannels, 32 ), FMOD_INIT_NORMAL, nullptr );
	if ( result != FMOD_OK )
	{
		TraceFMODError( "System::init", result );
		// Preserve the old game's soft-failure semantics for machines with no audio output.
		if ( bSoundCardPresent )
		{
			pSystem->release();
			pSystem = nullptr;
			if ( FMOD::System_Create( &pSystem ) != FMOD_OK || !pSystem ||
				 pSystem->setOutput( FMOD_OUTPUTTYPE_NOSOUND ) != FMOD_OK ||
				 pSystem->init( std::max( nMaxChannels, 32 ), FMOD_INIT_NORMAL, nullptr ) != FMOD_OK )
			{
				Done();
				return false;
			}
			bSoundCardPresent = false;
		}
		else
		{
			Done();
			return false;
		}
	}

	pSystem->createChannelGroup( "SFX", &pSFXGroup );
	pSystem->createChannelGroup( "Music", &pMusicGroup );
	pSystem->createChannelGroup( "Movie", &pMovieGroup );

	fDistanceFactor = 1.0f;
	fRolloffFactor = 1.0f;
	pSystem->set3DSettings( 1.0f, fDistanceFactor, fRolloffFactor );

	fListenerDistance = GetGlobalVar( "Sound.Listener.Distance", 0.0f ) * fWorldCellSize / 2.0f;
	cSFXMasterVolume = static_cast<BYTE>( Clamp01( GetGlobalVar( "Sound.SFXVolume", 100.0f ) / 100.0f ) * 255.0f );
	cStreamMasterVolume = static_cast<BYTE>( Clamp01( GetGlobalVar( "Sound.MusicVolume", 100.0f ) / 100.0f ) * 255.0f );
	UpdateGroupVolumes();

	streamFadeOff.Init();
	timeLastUpdate = -1;
	timeStreamFinished = -1;
	bPaused = false;
	bStreamingPaused = false;
	bStreamPlaying = false;
	g_pSoundEngine = this;
	bInited = true;
	return true;
}

void CSoundEngine::Done()
{
	if ( g_pSoundEngine == this )
		g_pSoundEngine = nullptr;

	nextMelody.Clear();
	curMelody.Clear();
	streamFadeOff.Clear();
	EndMovieAudio();
	CloseStreaming();

	for ( auto &entry : nativeChannels )
	{
		if ( entry.second )
			entry.second->stop();
	}
	channelsMap.clear();
	soundsMap.clear();
	nativeChannels.clear();

	if ( pSFXGroup ) { pSFXGroup->release(); pSFXGroup = nullptr; }
	if ( pMusicGroup ) { pMusicGroup->release(); pMusicGroup = nullptr; }
	if ( pMovieGroup ) { pMovieGroup->release(); pMovieGroup = nullptr; }

	if ( pSystem )
	{
		pSystem->close();
		pSystem->release();
		pSystem = nullptr;
	}

	drivers.clear();
	bInited = false;
	bStreamPlaying = false;
	nStreamingChannel = -1;
}

void CSoundEngine::UpdateGroupVolumes()
{
	if ( pSFXGroup )
		pSFXGroup->setVolume( bEnableSFX ? ByteVolumeToFloat( cSFXMasterVolume ) : 0.0f );
	if ( pMusicGroup )
		pMusicGroup->setVolume( bEnableStreaming ? ByteVolumeToFloat( cStreamMasterVolume ) * fStreamCurrentVolume : 0.0f );
	if ( pMovieGroup )
		pMovieGroup->setVolume( bEnableSFX ? ByteVolumeToFloat( cSFXMasterVolume ) : 0.0f );
}

void CSoundEngine::SetDistanceFactor( float fFactor )
{
	fDistanceFactor = std::max( fFactor, 0.0001f );
	if ( pSystem )
		pSystem->set3DSettings( 1.0f, fDistanceFactor, fRolloffFactor );
}

void CSoundEngine::SetRolloffFactor( float fFactor )
{
	fRolloffFactor = std::max( 0.0f, std::min( fFactor, 10.0f ) );
	if ( pSystem )
		pSystem->set3DSettings( 1.0f, fDistanceFactor, fRolloffFactor );
}

void CSoundEngine::SetSFXMasterVolume( float fVolume )
{
	cSFXMasterVolume = static_cast<BYTE>( Clamp01( fVolume ) * 255.0f );
	UpdateGroupVolumes();
}

void CSoundEngine::SetStreamMasterVolume( float fVolume )
{
	cStreamMasterVolume = static_cast<BYTE>( Clamp01( fVolume ) * 255.0f );
	UpdateGroupVolumes();
}

void CSoundEngine::UpdateCameraPos( const CVec3 &vPos )
{
	if ( !pSystem )
		return;

	FMOD_VECTOR position = { vPos.x, vPos.z, vPos.y };
	FMOD_VECTOR velocity = { 0.0f, 0.0f, 0.0f };
	FMOD_VECTOR forward = { 0.0f, 0.0f, 1.0f };
	FMOD_VECTOR up = { 0.0f, 1.0f, 0.0f };
	pSystem->set3DListenerAttributes( 0, &position, &velocity, &forward, &up );
	vLastListenerPos = vPos;
}

void CSoundEngine::Update( interface ICamera *pCamera )
{
	if ( !pSystem )
		return;

	if ( pCamera )
		UpdateCameraPos( pCamera->GetAnchor() );

	timeLastUpdate = GetSingleton<IGameTimer>()->GetAbsTime();

	if ( pStreamingChannel && bStreamPlaying )
	{
		bool playing = false;
		if ( pStreamingChannel->isPlaying( &playing ) != FMOD_OK || !playing )
			NotifyMelodyFinished();
	}

	if ( (timeStreamFinished != -1) && (timeStreamFinished < timeLastUpdate) && (timeLastUpdate - timeStreamFinished > 15000) )
		PlayNextMelody();

	ClearChannels();
	pSystem->update();

	IScene *pScene = GetSingleton<IScene>();
	if ( pScene && pScene->GetStatSystem() )
		pScene->GetStatSystem()->UpdateEntry( "SFX: num channels:", NStr::Format( "%d", static_cast<int>(nativeChannels.size()) ) );
}

void CSoundEngine::CloseStreaming()
{
	if ( pStreamingChannel )
	{
		pStreamingChannel->stop();
		pStreamingChannel = nullptr;
	}
	if ( pStreamingSound )
	{
		pStreamingSound->release();
		pStreamingSound = nullptr;
	}
	nStreamingChannel = -1;
	bStreamPlaying = false;
	curMelody.Clear();
}

bool CSoundEngine::PlayNextMelody()
{
	if ( !bEnableStreaming || nextMelody.szName.empty() )
		return false;
	const SMelodyInfo next = nextMelody;
	nextMelody.Clear();
	PlayStream( next.szName.c_str(), next.bLooped, 0 );
	return bStreamPlaying;
}

void CSoundEngine::StopStream( const unsigned int nTimeToFade )
{
	if ( nTimeToFade > 0 && bStreamPlaying )
		streamFadeOff.Fade( nTimeToFade );
	else
	{
		CloseStreaming();
		NotifyMelodyFinished();
	}
}

void CSoundEngine::SetStreamVolume( const float fVolume )
{
	fStreamCurrentVolume = Clamp01( fVolume );
	UpdateGroupVolumes();
}

float CSoundEngine::GetStreamVolume() const
{
	return fStreamCurrentVolume;
}

void CSoundEngine::PlayStream( const char *pszFileName, bool bLooped, const unsigned int nTimeToFadePrevious )
{
	if ( !pSystem || !bEnableStreaming || !pszFileName || !*pszFileName )
		return;

	if ( bStreamPlaying && curMelody.IsValid() && curMelody.szName == pszFileName )
		return;

	if ( nTimeToFadePrevious && bStreamPlaying )
	{
		nextMelody.szName = pszFileName;
		nextMelody.bLooped = bLooped;
		if ( !streamFadeOff.IsFading() )
			StopStream( nTimeToFadePrevious );
		return;
	}

	SetStreamVolume( 1.0f );
	CloseStreaming();

	curMelody.szName = pszFileName;
	curMelody.bLooped = bLooped;
	const std::string base = std::string( GetSingleton<IDataStorage>()->GetName() ) + pszFileName;
	const std::string mp3 = base + ".mp3";
	const std::string ogg = base + ".ogg";
	const FMOD_MODE mode = FMOD_2D | FMOD_CREATESTREAM | (bLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	FMOD_RESULT result = pSystem->createStream( mp3.c_str(), mode, nullptr, &pStreamingSound );
	if ( result != FMOD_OK )
		result = pSystem->createStream( ogg.c_str(), mode, nullptr, &pStreamingSound );

	if ( result != FMOD_OK || !pStreamingSound )
	{
		TraceFMODError( "createStream", result );
		curMelody.Clear();
		return;
	}

	result = pSystem->playSound( pStreamingSound, pMusicGroup, bStreamingPaused, &pStreamingChannel );
	if ( result != FMOD_OK || !pStreamingChannel )
	{
		TraceFMODError( "playSound(stream)", result );
		CloseStreaming();
		return;
	}

	pStreamingChannel->getIndex( &nStreamingChannel );
	timeStreamFinished = -1;
	bStreamPlaying = true;
	UpdateGroupVolumes();
}

bool CSoundEngine::PauseStreaming( bool bPause )
{
	bStreamingPaused = bPause;
	if ( pStreamingChannel )
		pStreamingChannel->setPaused( bPause );
	return bPause;
}

bool CSoundEngine::Pause( bool bPause )
{
	bPaused = bPause;
	if ( pSFXGroup )
		pSFXGroup->setPaused( bPause );
	if ( pMovieGroup )
		pMovieGroup->setPaused( bPause );
	return bPause;
}

bool CSoundEngine::IsPaused()
{
	return bPaused;
}

FMOD::Channel* CSoundEngine::ResolveChannel( int nChannel ) const
{
	const CNativeChannelMap::const_iterator it = nativeChannels.find( nChannel );
	return it == nativeChannels.end() ? nullptr : it->second;
}

int CSoundEngine::RegisterSound( CBaseSound *pSound, FMOD::Channel *pChannel, bool b3D )
{
	if ( !pSound || !pChannel )
		return -1;

	int nChannel = -1;
	if ( pChannel->getIndex( &nChannel ) != FMOD_OK || nChannel < 0 )
		return -1;

	const float soundVolume = pSound->GetVolume() >= 0.0f ? Clamp01( pSound->GetVolume() ) : 1.0f;
	pChannel->setVolume( soundVolume );
	if ( !b3D )
		pChannel->setPan( std::max( -1.0f, std::min( pSound->GetPan(), 1.0f ) ) );

	MapSound( pSound, nChannel, pChannel );
	pSound->SetChannel( nChannel );
	return nChannel;
}

int CSoundEngine::PlayNativeSample( CBaseSound *pSound, bool b3D, const CVec3 *pPosition )
{
	if ( !pSystem || !pSound || !pSound->GetSample() )
		return -1;

	CSoundSample *pSample = pSound->GetSample();
	pSample->Set3D( b3D );
	FMOD::Sound *pNativeSound = pSample->GetInternalContainer();
	if ( !pNativeSound )
		return -1;

	FMOD::Channel *pChannel = nullptr;
	FMOD_RESULT result = pSystem->playSound( pNativeSound, pSFXGroup, true, &pChannel );
	if ( result != FMOD_OK || !pChannel )
	{
		TraceFMODError( "playSound(sample)", result );
		return -1;
	}

	if ( b3D && pPosition )
	{
		FMOD_VECTOR position = { pPosition->x, pPosition->y, pPosition->z };
		pChannel->set3DAttributes( &position, nullptr );
	}

	const int nChannel = RegisterSound( pSound, pChannel, b3D );
	if ( nChannel < 0 )
	{
		pChannel->stop();
		return -1;
	}

	pChannel->setPaused( bPaused );
	return nChannel;
}

void CSoundEngine::MapSound( ISound *pSound, int nChannel, FMOD::Channel *pNativeChannel )
{
	channelsMap[pSound] = nChannel;
	soundsMap[nChannel] = pSound;
	nativeChannels[nChannel] = pNativeChannel;
}

void CSoundEngine::ClearChannels()
{
	std::vector<int> finished;
	for ( const auto &entry : soundsMap )
	{
		const int nChannel = entry.first;
		FMOD::Channel *pChannel = ResolveChannel( nChannel );
		bool playing = false;
		if ( !entry.second->IsValid() || !pChannel || pChannel->isPlaying( &playing ) != FMOD_OK || !playing )
		{
			if ( pChannel )
				pChannel->stop();
			finished.push_back( nChannel );
		}
	}

	for ( int nChannel : finished )
	{
		auto soundIt = soundsMap.find( nChannel );
		if ( soundIt != soundsMap.end() )
		{
			channelsMap.erase( soundIt->second );
			soundsMap.erase( soundIt );
		}
		nativeChannels.erase( nChannel );
	}
}

int CSoundEngine::PlaySample( ISound *pSound, bool bLooped, unsigned int nStartPos )
{
	if ( !pSound || !bEnableSFX )
		return -1;

	CBaseSound *pBaseSound = static_cast<CBaseSound*>( pSound );
	if ( !pBaseSound->GetSample() )
		return -1;
	pBaseSound->GetSample()->SetLoop( bLooped );

	thePlayVisitor.Init( this );
	const int nChannel = pSound->Visit( &thePlayVisitor );
	FMOD::Channel *pNativeChannel = ResolveChannel( nChannel );
	if ( pNativeChannel && nStartPos != 0 )
		pNativeChannel->setPosition( nStartPos, FMOD_TIMEUNIT_PCM );
	return nChannel;
}

void CSoundEngine::UpdateSample( ISound *pSound )
{
	const auto it = channelsMap.find( pSound );
	if ( it == channelsMap.end() )
		return;

	FMOD::Channel *pChannel = ResolveChannel( it->second );
	if ( !pChannel )
		return;

	pChannel->setVolume( pSound->GetVolume() >= 0.0f ? Clamp01( pSound->GetVolume() ) : 1.0f );
	FMOD::Sound *pCurrentSound = nullptr;
	FMOD_MODE mode = FMOD_DEFAULT;
	if ( pChannel->getCurrentSound( &pCurrentSound ) == FMOD_OK && pCurrentSound && pCurrentSound->getMode( &mode ) == FMOD_OK && (mode & FMOD_3D) == 0 )
		pChannel->setPan( std::max( -1.0f, std::min( pSound->GetPan(), 1.0f ) ) );
}

void CSoundEngine::StopSample( ISound *pSound )
{
	const auto it = channelsMap.find( pSound );
	if ( it != channelsMap.end() )
		StopChannel( it->second );
}

bool CSoundEngine::IsPlaying( ISound *pSound )
{
	if ( !pSound )
		return false;
	const auto it = channelsMap.find( pSound );
	return it != channelsMap.end() && IsFMODChannelPlaying( it->second );
}

void CSoundEngine::StopChannel( int nChannel )
{
	if ( nChannel < 0 )
		return;
	if ( FMOD::Channel *pChannel = ResolveChannel( nChannel ) )
		pChannel->stop();

	const auto it = soundsMap.find( nChannel );
	if ( it != soundsMap.end() )
	{
		channelsMap.erase( it->second );
		soundsMap.erase( it );
	}
	nativeChannels.erase( nChannel );
}

unsigned int CSoundEngine::GetCurrentPosition( ISound *pSound )
{
	const auto it = channelsMap.find( pSound );
	if ( it == channelsMap.end() )
		return 0;
	FMOD::Channel *pChannel = ResolveChannel( it->second );
	unsigned int position = 0;
	if ( pChannel )
		pChannel->getPosition( &position, FMOD_TIMEUNIT_PCM );
	return position;
}

void CSoundEngine::SetCurrentPosition( ISound *pSound, unsigned int pos )
{
	const auto it = channelsMap.find( pSound );
	if ( it != channelsMap.end() )
	{
		if ( FMOD::Channel *pChannel = ResolveChannel( it->second ) )
			pChannel->setPosition( pos, FMOD_TIMEUNIT_PCM );
	}
}

void CSoundEngine::ReEnableSounds()
{
	if ( !bEnableSFX )
	{
		std::vector<int> channels;
		channels.reserve( nativeChannels.size() );
		for ( const auto &entry : nativeChannels )
			channels.push_back( entry.first );
		for ( int channel : channels )
			StopChannel( channel );
	}
	if ( !bEnableStreaming )
		CloseStreaming();
	UpdateGroupVolumes();
}

void CSoundEngine::NotifyMelodyFinished()
{
	const SMelodyInfo finished = curMelody;
	CloseStreaming();
	if ( nextMelody.IsValid() )
	{
		PlayNextMelody();
	}
	else if ( finished.IsValid() && finished.bLooped )
	{
		PlayStream( finished.szName.c_str(), true, 0 );
	}
	else
	{
		timeStreamFinished = timeLastUpdate;
		bStreamPlaying = false;
	}
}

bool CSoundEngine::IsStreamPlaying() const
{
	if ( !bStreamPlaying || !pStreamingChannel )
		return false;
	bool playing = false;
	return pStreamingChannel->isPlaying( &playing ) == FMOD_OK && playing;
}

FMOD_RESULT F_CALLBACK BlitzMoviePCMReadCallback( FMOD_SOUND *pSound, void *pData, unsigned int nDataLen )
{
	if ( !pSound || !pData )
		return FMOD_ERR_INVALID_PARAM;
	FMOD::Sound *pCppSound = reinterpret_cast<FMOD::Sound*>( pSound );
	void *pUserData = nullptr;
	if ( pCppSound->getUserData( &pUserData ) != FMOD_OK || !pUserData )
	{
		std::memset( pData, 0, nDataLen );
		return FMOD_OK;
	}
	return static_cast<CSoundEngine*>( pUserData )->ReadMoviePCM( pData, nDataLen );
}

FMOD_RESULT CSoundEngine::ReadMoviePCM( void *pData, unsigned int nBytes )
{
	std::lock_guard<std::mutex> lock( movieAudioMutex );
	float *pOut = static_cast<float*>( pData );
	const std::size_t requestedSamples = nBytes / sizeof(float);
	const std::size_t available = movieAudioReadOffset < movieAudioBuffer.size() ? movieAudioBuffer.size() - movieAudioReadOffset : 0;
	const std::size_t toCopy = std::min( requestedSamples, available );

	if ( toCopy )
		std::memcpy( pOut, movieAudioBuffer.data() + movieAudioReadOffset, toCopy * sizeof(float) );
	if ( toCopy < requestedSamples )
		std::memset( pOut + toCopy, 0, (requestedSamples - toCopy) * sizeof(float) );
	movieAudioReadOffset += toCopy;

	if ( movieAudioReadOffset > 32768 && movieAudioReadOffset * 2 > movieAudioBuffer.size() )
	{
		movieAudioBuffer.erase( movieAudioBuffer.begin(), movieAudioBuffer.begin() + movieAudioReadOffset );
		movieAudioReadOffset = 0;
	}
	return FMOD_OK;
}

bool CSoundEngine::BeginMovieAudio( int nSampleRate, int nChannels )
{
	EndMovieAudio();
	if ( !pSystem || nSampleRate <= 0 || nChannels <= 0 )
		return false;

	movieAudioChannels = nChannels;
	movieAudioSampleRate = nSampleRate;
	movieAudioReadOffset = 0;
	movieAudioBuffer.clear();

	FMOD_CREATESOUNDEXINFO exInfo = {};
	exInfo.cbsize = sizeof(exInfo);
	exInfo.numchannels = nChannels;
	exInfo.defaultfrequency = nSampleRate;
	exInfo.format = FMOD_SOUND_FORMAT_PCMFLOAT;
	exInfo.decodebuffersize = static_cast<unsigned int>( std::max( nSampleRate / 20, 1024 ) );
	exInfo.length = static_cast<unsigned int>( nSampleRate * nChannels * sizeof(float) * 60 );
	exInfo.pcmreadcallback = BlitzMoviePCMReadCallback;
	exInfo.userdata = this;

	const FMOD_MODE mode = FMOD_OPENUSER | FMOD_CREATESTREAM | FMOD_2D | FMOD_LOOP_NORMAL;
	FMOD_RESULT result = pSystem->createStream( nullptr, mode, &exInfo, &pMovieSound );
	if ( result != FMOD_OK || !pMovieSound )
	{
		TraceFMODError( "createStream(movie PCM)", result );
		EndMovieAudio();
		return false;
	}

	result = pSystem->playSound( pMovieSound, pMovieGroup, false, &pMovieChannel );
	if ( result != FMOD_OK || !pMovieChannel )
	{
		TraceFMODError( "playSound(movie PCM)", result );
		EndMovieAudio();
		return false;
	}
	movieAudioActive = true;
	return true;
}

void CSoundEngine::SubmitMovieAudio( const float *pInterleavedSamples, unsigned int nFrames )
{
	if ( !movieAudioActive || !pInterleavedSamples || nFrames == 0 || movieAudioChannels <= 0 )
		return;
	const std::size_t nSamples = static_cast<std::size_t>( nFrames ) * static_cast<std::size_t>( movieAudioChannels );
	std::lock_guard<std::mutex> lock( movieAudioMutex );
	movieAudioBuffer.insert( movieAudioBuffer.end(), pInterleavedSamples, pInterleavedSamples + nSamples );
}

void CSoundEngine::PauseMovieAudio( bool bPause )
{
	if ( pMovieChannel )
		pMovieChannel->setPaused( bPause );
}

void CSoundEngine::EndMovieAudio()
{
	movieAudioActive = false;
	if ( pMovieChannel )
	{
		pMovieChannel->stop();
		pMovieChannel = nullptr;
	}
	if ( pMovieSound )
	{
		pMovieSound->release();
		pMovieSound = nullptr;
	}
	std::lock_guard<std::mutex> lock( movieAudioMutex );
	movieAudioBuffer.clear();
	movieAudioReadOffset = 0;
	movieAudioChannels = 0;
	movieAudioSampleRate = 0;
}

int CSoundEngine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		CloseStreaming();
		channelsMap.clear();
		soundsMap.clear();
		nativeChannels.clear();
		timeStreamFinished = timeLastUpdate;
	}

	saver.Add( 1, &fStreamCurrentVolume );
	saver.Add( 2, &bStreamPlaying );
	saver.Add( 3, &bSoundCardPresent );
	saver.Add( 4, &timeLastUpdate );
	saver.Add( 5, &nStreamingChannel );
	saver.Add( 6, &fListenerDistance );
	saver.Add( 7, &vLastListenerPos );
	saver.Add( 9, &streamFadeOff );
	saver.Add( 10, &curMelody );
	saver.Add( 11, &nextMelody );
	saver.Add( 12, &bPaused );
	saver.Add( 13, &bStreamingPaused );

	if ( saver.IsReading() && curMelody.IsValid() )
	{
		const SMelodyInfo melody = curMelody;
		bStreamPlaying = false;
		PlayStream( melody.szName.c_str(), melody.bLooped );
	}
	return 0;
}

int CSoundEngine::SMelodyInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bLooped );
	saver.Add( 2, &szName );
	return 0;
}
