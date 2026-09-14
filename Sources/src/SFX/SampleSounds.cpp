#include "StdAfx.h"
#include "SampleSounds.h"
#include "SoundEngine.h"

void CSoundSample::Close()
{
	if ( sample )
		sample->release();
	sample = nullptr;
}

void CSoundSample::SetSample( FMOD::Sound *_sample )
{
	Close();
	sample = _sample;
	if ( sample )
	{
		sample->setMode( nMode | (bLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) );
		sample->set3DMinMaxDistance( fMinDistance, 1000000000.0f );
	}
}

void CSoundSample::SwapData( ISharedResource *pResource )
{
	CSoundSample *pRes = dynamic_cast<CSoundSample*>( pResource );
	NI_ASSERT_TF( pRes != 0, NStr::Format("shared resource is not a \"%s\"", typeid(*this).name()), return );
	std::swap( sample, pRes->sample );
}

void CSoundSample::Set3D( bool b3D )
{
	nMode &= ~(FMOD_2D | FMOD_3D);
	nMode |= b3D ? FMOD_3D : FMOD_2D;
	if ( sample )
		sample->setMode( nMode | (bLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) );
}

void CSoundSample::SetLoop( bool bEnable )
{
	bLooped = bEnable;
	if ( sample )
		sample->setMode( nMode | (bLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) );
}

void CSoundSample::SetMinDistance( float _fMinDistance )
{
	fMinDistance = _fMinDistance;
	if ( sample )
		sample->set3DMinMaxDistance( fMinDistance, 1000000000.0f );
}

bool CSoundSample::Load( const bool bPreLoad )
{
	if ( sample != nullptr || bPreLoad )
		return true;

	FMOD::System *pSystem = GetFMODSystem();
	if ( !pSystem )
		return false;

	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 || pStream->GetSize() <= 0 )
		return false;

	const int nSize = pStream->GetSize();
	std::vector<char> buffer( nSize );
	const int nCheck = pStream->Read( buffer.data(), nSize );
	NI_ASSERT_SLOW_TF( nCheck == nSize, "Read size doesn't match requested size", return false );

	FMOD_CREATESOUNDEXINFO exInfo = {};
	exInfo.cbsize = sizeof(exInfo);
	exInfo.length = static_cast<unsigned int>( nSize );

	FMOD::Sound *pNewSample = nullptr;
	const FMOD_MODE mode = nMode | FMOD_OPENMEMORY | (bLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
	const FMOD_RESULT result = pSystem->createSound( buffer.data(), mode, &exInfo, &pNewSample );
	if ( result != FMOD_OK )
	{
		NStr::DebugTrace( "FMOD: failed to load sound '%s': %s\n", szStreamName.c_str(), FMOD_ErrorString(result) );
		return false;
	}

	SetSample( pNewSample );
	return true;
}

bool CBaseSound::IsPlaying()
{
	return IsFMODChannelPlaying( nChannel, pSample ? pSample->GetInternalContainer() : nullptr );
}

void CBaseSound::SetLooping( bool bEnable, int nStart, int nEnd )
{
	FMOD::Sound *pSound = pSample->GetInternalContainer();
	if ( !pSound )
		return;
	pSample->SetLoop( bEnable );
	if ( nStart >= 0 && nEnd >= 0 )
		pSound->setLoopPoints( static_cast<unsigned int>(nStart), FMOD_TIMEUNIT_PCM,
			static_cast<unsigned int>(nEnd), FMOD_TIMEUNIT_PCM );
}

unsigned int CBaseSound::GetLenght()
{
	unsigned int length = 0;
	if ( FMOD::Sound *pSound = pSample->GetInternalContainer() )
		pSound->getLength( &length, FMOD_TIMEUNIT_PCM );
	return length;
}

unsigned int CBaseSound::GetSampleRate()
{
	float frequency = 44000.0f;
	if ( FMOD::Sound *pSound = pSample->GetInternalContainer() )
		pSound->getDefaults( &frequency, nullptr );
	return static_cast<unsigned int>( frequency );
}

int CSound2D::Visit( interface ISFXVisitor *pVisitor )
{
	return pVisitor->VisitSound2D( this );
}

int CSound2D::Play()
{
	FMOD::System *pSystem = GetFMODSystem();
	FMOD::Sound *pSound = GetSample()->GetInternalContainer();
	if ( !pSystem || !pSound )
		return -1;
	FMOD::Channel *pChannel = nullptr;
	if ( pSystem->playSound( pSound, nullptr, false, &pChannel ) != FMOD_OK || !pChannel )
		return -1;
	int index = -1;
	pChannel->getIndex( &index );
	SetChannel( index );
	return index;
}

int CSound3D::Visit( interface ISFXVisitor *pVisitor )
{
	return pVisitor->VisitSound3D( this, vPos );
}

void CSound3D::SetPosition( const CVec3 &vPos3 )
{
	// Engine coordinates are X/right, Y/forward, Z/up. FMOD uses X/right, Y/up, Z/forward.
	vPos.Set( vPos3.x, vPos3.z, vPos3.y );
	if ( FMOD::Channel *pChannel = GetFMODChannel( GetChannel() ) )
	{
		FMOD_VECTOR position = { vPos.x, vPos.y, vPos.z };
		pChannel->set3DAttributes( &position, nullptr );
	}
}

int CSound3D::Play()
{
	FMOD::System *pSystem = GetFMODSystem();
	FMOD::Sound *pSound = GetSample()->GetInternalContainer();
	if ( !pSystem || !pSound )
		return -1;
	FMOD::Channel *pChannel = nullptr;
	if ( pSystem->playSound( pSound, nullptr, true, &pChannel ) != FMOD_OK || !pChannel )
		return -1;
	FMOD_VECTOR position = { vPos.x, vPos.y, vPos.z };
	pChannel->set3DAttributes( &position, nullptr );
	pChannel->setPaused( false );
	int index = -1;
	pChannel->getIndex( &index );
	SetChannel( index );
	return index;
}
