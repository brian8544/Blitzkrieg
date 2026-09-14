#include "StdAfx.h"

#include "DataTreeXML.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	pugi::xml_node FindPath( pugi::xml_node root, const char *pszPath )
	{
		if ( !root || !pszPath || !*pszPath )
			return pugi::xml_node();

		std::string path( pszPath );
		size_t pos = 0;
		pugi::xml_node node = root;
		while ( pos < path.size() )
		{
			const size_t slash = path.find( '/', pos );
			const std::string part = path.substr( pos, slash == std::string::npos ? std::string::npos : slash - pos );
			if ( !part.empty() )
				node = node.child( part.c_str() );
			if ( !node || slash == std::string::npos )
				break;
			pos = slash + 1;
		}
		return node;
	}

	bool ReadStream( IDataStream *pStream, std::vector<char> *pData )
	{
		if ( !pStream || !pData )
			return false;
		const int pos = pStream->GetPos();
		const int size = pStream->GetSize();
		if ( pos < 0 || size < pos )
			return false;
		pData->resize( static_cast<size_t>( size - pos ) );
		return pData->empty() || pStream->Read( pData->data(), static_cast<int>( pData->size() ) ) == static_cast<int>( pData->size() );
	}

	class CDataStreamXMLWriter : public pugi::xml_writer
	{
		IDataStream *pStream;
	public:
		explicit CDataStreamXMLWriter( IDataStream *pStream ) : pStream( pStream ) {}
		virtual void write( const void *data, size_t size )
		{
			if ( pStream && size )
				pStream->Write( data, static_cast<int>( size ) );
		}
	};

	std::wstring UTF8ToWide( const char *pszText )
	{
		if ( !pszText || !*pszText )
			return std::wstring();
		const int count = MultiByteToWideChar( CP_UTF8, 0, pszText, -1, nullptr, 0 );
		if ( count <= 1 )
			return std::wstring();
		std::wstring result( static_cast<size_t>( count ), L'\0' );
		MultiByteToWideChar( CP_UTF8, 0, pszText, -1, result.data(), count );
		result.resize( static_cast<size_t>( count - 1 ) );
		return result;
	}

	std::string WideToUTF8( const wchar_t *pszText )
	{
		if ( !pszText || !*pszText )
			return std::string();
		const int count = WideCharToMultiByte( CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr );
		if ( count <= 1 )
			return std::string();
		std::string result( static_cast<size_t>( count ), '\0' );
		WideCharToMultiByte( CP_UTF8, 0, pszText, -1, result.data(), count, nullptr, nullptr );
		result.resize( static_cast<size_t>( count - 1 ) );
		return result;
	}
}

void InitCOM()
{
	// MSXML/COM was removed. Kept to avoid touching old callers unnecessarily.
}

CDataTreeXML::CDataTreeXML( IDataTree::EAccessMode _eMode )
	: eMode( _eMode )
{
}

CDataTreeXML::~CDataTreeXML()
{
	if ( !IsReading() && pStream )
		SaveDocument();
}

bool CDataTreeXML::SaveDocument()
{
	if ( !pStream )
		return false;
	pStream->Seek( 0, STREAM_SEEK_SET );
	pStream->SetSize( 0 );
	CDataStreamXMLWriter writer( pStream );
	xmlDocument.save( writer, "  ", pugi::format_default, pugi::encoding_utf8 );
	pStream->Flush();
	return true;
}

bool CDataTreeXML::Open( IDataStream *_pStream, DTChunkID idBaseNode )
{
	pStream = _pStream;
	if ( !pStream )
		return false;

	if ( IsReading() )
	{
		std::vector<char> data;
		// assertions are compiled out of final builds
		if ( !ReadStream( pStream, &data ) )
		{
			NI_ASSERT_T( false, "Can't read XML data stream" );
			return false;
		}
		const pugi::xml_parse_result result = xmlDocument.load_buffer( data.data(), data.size(), pugi::parse_default, pugi::encoding_auto );
		if ( !result )
			return false;
		nodes.push_back( xmlDocument );
		xmlCurrNode = xmlDocument;
		return StartChunk( idBaseNode ) != 0;
	}

	return StartChunk( idBaseNode ) != 0;
}

int CDataTreeXML::StartChunk( DTChunkID idChunk )
{
	if ( !idChunk || idChunk[0] == '\0' )
		return -1;

	if ( IsReading() )
	{
		if ( xmlCurrNode )
			nodes.push_back( xmlCurrNode );
		if ( !xmlCurrNode )
			return 0;
		xmlCurrNode = FindPath( xmlCurrNode, idChunk );
		if ( !xmlCurrNode )
		{
			FinishChunk();
			return 0;
		}
		return 1;
	}

	if ( xmlCurrElement )
		elements.push_back( xmlCurrElement );
	if ( elements.empty() )
		xmlCurrElement = xmlDocument.append_child( idChunk );
	else
		xmlCurrElement = elements.back().append_child( idChunk );
	return xmlCurrElement ? 1 : 0;
}

void CDataTreeXML::FinishChunk()
{
	if ( IsReading() )
	{
		if ( nodes.empty() )
			xmlCurrNode = pugi::xml_node();
		else
		{
			xmlCurrNode = nodes.back();
			nodes.pop_back();
		}
	}
	else
	{
		if ( elements.empty() )
			xmlCurrElement = pugi::xml_node();
		else
		{
			xmlCurrElement = elements.back();
			elements.pop_back();
		}
	}
}

int CDataTreeXML::StartContainerChunk( DTChunkID idChunk )
{
	const std::string chunkName = (!idChunk || idChunk[0] == '\0') ? "data" : idChunk;
	if ( IsReading() )
	{
		pugi::xml_node base = FindPath( xmlCurrNode, chunkName.c_str() );
		if ( !base )
			return 0;
		nodes.push_back( xmlCurrNode );
		nodelists.push_back( SNodesList() );
		for ( pugi::xml_node item : base.children( "item" ) )
			nodelists.back().nodes.push_back( item );
		return 1;
	}

	if ( xmlCurrElement )
		elements.push_back( xmlCurrElement );
	pugi::xml_node base = xmlCurrElement ? xmlCurrElement.append_child( chunkName.c_str() ) : xmlDocument.append_child( chunkName.c_str() );
	arrbases.push_back( base );
	return base ? 1 : 0;
}

void CDataTreeXML::FinishContainerChunk()
{
	FinishChunk();
	if ( IsReading() )
	{
		if ( !nodelists.empty() )
			nodelists.pop_back();
	}
	else if ( !arrbases.empty() )
		arrbases.pop_back();
}

bool CDataTreeXML::SetChunkCounter( int nCount )
{
	if ( IsReading() )
	{
		if ( nodelists.empty() || nCount < 0 || static_cast<size_t>( nCount ) >= nodelists.back().nodes.size() )
		{
			xmlCurrNode = pugi::xml_node();
			return false;
		}
		nodelists.back().nCurrElement = nCount;
		xmlCurrNode = nodelists.back().nodes[static_cast<size_t>( nCount )];
		return true;
	}

	if ( arrbases.empty() )
		return false;
	xmlCurrElement = arrbases.back().append_child( "item" );
	return !!xmlCurrElement;
}

int CDataTreeXML::CountChunks( DTChunkID idChunk )
{
	if ( !IsReading() || !xmlCurrNode )
		return 0;
	const std::string chunkName = (!idChunk || idChunk[0] == '\0') ? "data" : idChunk;
	pugi::xml_node base = FindPath( xmlCurrNode, chunkName.c_str() );
	int count = 0;
	for ( pugi::xml_node item : base.children( "item" ) )
		++count;
	return count;
}

int CDataTreeXML::GetChunkSize()
{
	return IsReading() && xmlCurrNode ? static_cast<int>( std::strlen( xmlCurrNode.text().get() ) ) : 0;
}

bool CDataTreeXML::RawData( void *pData, int nSize )
{
	if ( !pData || nSize < 0 )
		return false;
	if ( IsReading() )
	{
		if ( !xmlCurrNode )
			return false;
		const char *text = xmlCurrNode.text().get();
		if ( static_cast<int>( std::strlen( text ) ) != nSize * 2 )
			return false;
		int decoded = 0;
		NStr::StringToBin( text, pData, &decoded );
		return decoded == nSize;
	}

	std::string text( static_cast<size_t>( nSize ) * 2, '\0' );
	NStr::BinToString( pData, nSize, text.data() );
	xmlCurrElement.text().set( text.c_str() );
	return true;
}

bool CDataTreeXML::StringData( char *pData )
{
	if ( !pData )
		return false;
	if ( IsReading() )
	{
		if ( !xmlCurrNode )
			return false;
		const char *text = xmlCurrNode.text().get();
		std::memcpy( pData, text, std::strlen(text) + 1 );
		return true;
	}
	if ( !xmlCurrElement )
		return false;
	xmlCurrElement.text().set( pData );
	return true;
}

bool CDataTreeXML::StringData( WORD *pData )
{
	if ( !pData )
		return false;
	if ( IsReading() )
	{
		if ( !xmlCurrNode )
			return false;
		const std::wstring text = UTF8ToWide( xmlCurrNode.text().get() );
		std::memcpy( pData, text.c_str(), (text.size() + 1) * sizeof(wchar_t) );
		return true;
	}
	if ( !xmlCurrElement )
		return false;
	const std::string text = WideToUTF8( reinterpret_cast<const wchar_t*>( pData ) );
	xmlCurrElement.text().set( text.c_str() );
	return true;
}

bool CDataTreeXML::GetTextValue( DTChunkID idChunk, std::string *pValue ) const
{
	if ( !xmlCurrNode || !idChunk || !pValue )
		return false;
	if ( pugi::xml_attribute attr = xmlCurrNode.attribute( idChunk ) )
	{
		*pValue = attr.value();
		return true;
	}
	if ( pugi::xml_node node = FindPath( xmlCurrNode, idChunk ) )
	{
		*pValue = node.text().get();
		return true;
	}
	return false;
}

bool CDataTreeXML::DataChunk( DTChunkID idChunk, int *pData )
{
	if ( !pData )
		return false;
	if ( IsReading() )
	{
		std::string value;
		if ( !GetTextValue( idChunk, &value ) )
			return false;
		return sscanf_s( value.c_str(), "%i", pData ) == 1;
	}
	if ( !xmlCurrElement )
		return false;
	xmlCurrElement.append_attribute( idChunk ).set_value( *pData );
	return true;
}

bool CDataTreeXML::DataChunk( DTChunkID idChunk, double *pData )
{
	if ( !pData )
		return false;
	if ( IsReading() )
	{
		std::string value;
		if ( !GetTextValue( idChunk, &value ) )
			return false;
		return sscanf_s( value.c_str(), "%lg", pData ) == 1;
	}
	if ( !xmlCurrElement )
		return false;
	xmlCurrElement.append_attribute( idChunk ).set_value( *pData );
	return true;
}
