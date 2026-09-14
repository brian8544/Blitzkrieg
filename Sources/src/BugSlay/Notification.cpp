#include "StdAfx.h"
#include <cstring>
static const char LOCAL_FILE[] = __FILE__;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Notification.h"

#include <string>
#include <list>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* DXErrorToString( HRESULT hErrorCode );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommonException : public ICommonException
{
private:
	const std::string szErrorString;
	const DWORD dwErrorCode;
public:
	CCommonException( const char *pszString, DWORD dwCode )
		: szErrorString( pszString ), dwErrorCode( dwCode ) {  }
	CCommonException( const char *pszString )
		: szErrorString( pszString ), dwErrorCode( 0 ) {  }
	CCommonException( DWORD dwCode )
		: dwErrorCode( dwCode ) {  }

	virtual const char* GetString() const { return szErrorString.c_str(); }
	virtual DWORD GetCode() const { return dwErrorCode; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGuardException : public IGuardException
{
private:
  std::list<std::string> szErrors;
  mutable std::string szErrorString;
  mutable bool bStringChanged;
public:
  CGuardException( const char *pszString )
    : bStringChanged( true ) { szErrors.push_back( pszString ); }

	virtual const char* GetString() const 
  { 
    if ( bStringChanged )
    {
			char buff[2048];
			buff[0] = 0;
      for ( std::list<std::string>::const_iterator pos = szErrors.begin(); pos != szErrors.end(); ++pos )
			{
				strcat_s( buff, pos->c_str() );
				strcat_s( buff, " <= " );
			}
			strcat_s( buff, "WinMain" );
      szErrorString = buff;
      bStringChanged = false;
    }
    return szErrorString.c_str(); 
  }
  virtual void Append( const char *pszFormat, ... )
  {
    char buffer[512];
    va_list va;
	  // compose error string
    va_start( va, pszFormat );
    vsprintf_s( buffer, pszFormat, va );
    va_end( va );
    //
    szErrors.push_back( buffer );
    bStringChanged = true;
  }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::list<NotificationShowMBox> notifyError;
static std::list<NotificationShowMBox> notifyWarning;
static std::list<NotificationShowMBox> notifyReport;
void AddNotifyErrorShowMBoxFunction( NotificationShowMBox funct )
{
	notifyError.push_back( funct );
}
void AddNotifyWarningShowMBoxFunction( NotificationShowMBox funct )
{
	notifyWarning.push_back( funct );
}
void AddNotifyReportShowMBoxFunction( NotificationShowMBox funct )
{
	notifyReport.push_back( funct );
}
void RemoveNotifyErrorShowMBoxFunction( NotificationShowMBox funct )
{
	notifyError.remove( funct );
}
void RemoveNotifyWarningShowMBoxFunction( NotificationShowMBox funct )
{
	notifyWarning.remove( funct );
}
void RemoveNotifyReportShowMBoxFunction( NotificationShowMBox funct )
{
	notifyReport.remove( funct );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BreakHere()
{
#ifdef _DEBUG
	DebugBreak();
#endif // _DEBUG
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ShowError( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyError.begin(); pos != notifyError.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "ERROR!", pszString, MB_ABORTRETRYIGNORE | MB_ICONERROR ) );
	return nRetVal;
}
int ShowWarning( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyWarning.begin(); pos != notifyWarning.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "Warning!", pszString, MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION ) );
	return nRetVal;
}
int ShowReport( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyReport.begin(); pos != notifyReport.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "Report", pszString, MB_OK ) );
	return nRetVal;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowExceptionHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, "(0x%X) ", dxrval );
	strcat_s( buffer, "\n" );
	strcat_s( buffer, buff1 );
	strcat_s( buffer, DXErrorToString(dxrval) );
	//
	CCommonException* pException = new CCommonException( buffer, dxrval );
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowException( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	CCommonException* pException = new CCommonException( buffer );
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowGuardException( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	CGuardException* pException = new CGuardException( buffer );
	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// error notification
bool ReportErrorHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, "(0x%X) ", dxrval );
	strcat_s( buffer, "\n" );
	strcat_s( buffer, buff1 );
	strcat_s( buffer, DXErrorToString(dxrval) );
	//
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	return false;
}
bool ReportError( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// warning notification
bool ReportWarningHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, "(0x%X) ", dxrval );
	strcat_s( buffer, "\n" );
	strcat_s( buffer, buff1 );
	strcat_s( buffer, DXErrorToString(dxrval) );
	//
	ShowWarning( buffer );

	return false;
}
bool ReportWarning( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	ShowWarning( buffer );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// just plain notification
bool ReportInfoHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, "(0x%X) ", dxrval );
	strcat_s( buffer, "\n" );
	strcat_s( buffer, buff1 );
	strcat_s( buffer, DXErrorToString(dxrval) );
	//
	ShowReport( buffer );

	return false;
}
bool ReportInfo( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
  vsprintf_s( buffer, pszFormat, va );
  va_end( va );
	//
	ShowReport( buffer );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// converts DirectX error code to the string
const char* DXErrorToString( HRESULT hErrorCode )
{
	static thread_local char message[512] = {};
	message[0] = '\0';
	const DWORD length = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		static_cast<DWORD>( hErrorCode ),
		0,
		message,
		static_cast<DWORD>( sizeof(message) ),
		nullptr );
	if ( length == 0 )
		return "Unrecognized HRESULT.";
	while ( message[0] != '\0' )
	{
		const std::size_t n = std::strlen( message );
		if ( n == 0 || (message[n - 1] != '\r' && message[n - 1] != '\n') )
			break;
		message[n - 1] = '\0';
	}
	return message;
}
