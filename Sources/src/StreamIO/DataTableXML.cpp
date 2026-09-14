#include "StdAfx.h"

#include "DataTableXML.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	bool ReadStream( IDataStream *pStream, std::vector<char> *pData )
	{
		if ( !pStream || !pData )
			return false;
		const int start = pStream->GetPos();
		const int size = pStream->GetSize();
		if ( start < 0 || size < start )
			return false;
		pData->resize( static_cast<size_t>( size - start ) );
		const bool ok = pData->empty() || pStream->Read( pData->data(), static_cast<int>( pData->size() ) ) == static_cast<int>( pData->size() );
		pStream->Seek( start, STREAM_SEEK_SET );
		return ok;
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

	int AddToBuffer( const std::string &name, char* &pBuffer, int bufferSize, int totalSize )
	{
		if ( !pBuffer || totalSize < 0 || totalSize + static_cast<int>( name.size() ) + 2 > bufferSize )
			return 0;
		std::memcpy( pBuffer, name.c_str(), name.size() + 1 );
		pBuffer += name.size() + 1;
		return static_cast<int>( name.size() ) + 1;
	}

	void CollectEntryNames( pugi::xml_node row, const std::string &prefix, std::vector<std::string> *pNames )
	{
		for ( pugi::xml_attribute attr : row.attributes() )
			pNames->push_back( prefix + attr.name() );

		for ( pugi::xml_node child : row.children() )
		{
			if ( child.type() != pugi::node_element )
				continue;
			const std::string childName = prefix + child.name();
			bool hasStructuredChildren = false;
			for ( pugi::xml_node nested : child.children() )
			{
				if ( nested.type() == pugi::node_element )
				{
					hasStructuredChildren = true;
					break;
				}
			}
			if ( child.first_attribute() || hasStructuredChildren )
				CollectEntryNames( child, childName + ".", pNames );
			else
				pNames->push_back( childName );
		}
	}
}

CDataTableXML::CDataTableXML()
	: bModified( false )
{
}

CDataTableXML::~CDataTableXML()
{
	if ( bModified && pStream )
		SaveDocument();
}

bool CDataTableXML::SaveDocument()
{
	if ( !pStream )
		return false;
	pStream->Seek( 0, STREAM_SEEK_SET );
	pStream->SetSize( 0 );
	CDataStreamXMLWriter writer( pStream );
	xmlDocument.save( writer, "  ", pugi::format_default, pugi::encoding_utf8 );
	pStream->Flush();
	bModified = false;
	return true;
}

bool CDataTableXML::Open( IDataStream *_pStream, const char *pszBaseNode )
{
	pStream = _pStream;
	if ( !pStream || !pszBaseNode || !*pszBaseNode )
		return false;

	std::vector<char> data;
	if ( ReadStream( pStream, &data ) && !data.empty() )
	{
		const pugi::xml_parse_result result = xmlDocument.load_buffer( data.data(), data.size(), pugi::parse_default, pugi::encoding_auto );
		if ( result )
			xmlRootNode = xmlDocument.child( pszBaseNode );
	}

	if ( !xmlRootNode )
	{
		xmlDocument.reset();
		xmlRootNode = xmlDocument.append_child( pszBaseNode );
		bModified = true;
	}
	return !!xmlRootNode;
}

std::string CDataTableXML::MakeName( const char *pszRow, const char *pszEntry ) const
{
	std::string name = pszRow ? pszRow : "";
	if ( pszEntry && *pszEntry )
	{
		if ( !name.empty() )
			name += '/';
		name += pszEntry;
	}
	std::replace( name.begin(), name.end(), '.', '/' );
	return name;
}

pugi::xml_node CDataTableXML::FindNode( const std::string &path ) const
{
	pugi::xml_node node = xmlRootNode;
	size_t pos = 0;
	while ( node && pos < path.size() )
	{
		const size_t slash = path.find( '/', pos );
		const std::string part = path.substr( pos, slash == std::string::npos ? std::string::npos : slash - pos );
		if ( !part.empty() )
			node = node.child( part.c_str() );
		if ( slash == std::string::npos )
			break;
		pos = slash + 1;
	}
	return node;
}

pugi::xml_node CDataTableXML::EnsureNode( const std::string &path )
{
	pugi::xml_node node = xmlRootNode;
	size_t pos = 0;
	while ( node && pos < path.size() )
	{
		const size_t slash = path.find( '/', pos );
		const std::string part = path.substr( pos, slash == std::string::npos ? std::string::npos : slash - pos );
		if ( !part.empty() )
		{
			pugi::xml_node child = node.child( part.c_str() );
			if ( !child )
				child = node.append_child( part.c_str() );
			node = child;
		}
		if ( slash == std::string::npos )
			break;
		pos = slash + 1;
	}
	return node;
}

bool CDataTableXML::GetValue( const std::string &name, std::string *pValue ) const
{
	if ( !pValue || name.empty() )
		return false;
	const size_t slash = name.rfind( '/' );
	const std::string parentPath = slash == std::string::npos ? std::string() : name.substr( 0, slash );
	const std::string leaf = slash == std::string::npos ? name : name.substr( slash + 1 );
	pugi::xml_node parent = parentPath.empty() ? xmlRootNode : FindNode( parentPath );
	if ( !parent )
		return false;
	if ( pugi::xml_attribute attr = parent.attribute( leaf.c_str() ) )
	{
		*pValue = attr.value();
		return true;
	}
	if ( pugi::xml_node child = parent.child( leaf.c_str() ) )
	{
		*pValue = child.text().get();
		return true;
	}
	return false;
}

void CDataTableXML::SetValue( const char *pszRow, const char *pszEntry, const char *pszValue )
{
	const std::string rowPath = MakeName( pszRow, nullptr );
	pugi::xml_node row = EnsureNode( rowPath );
	if ( !row )
		return;

	const std::string entryPath = MakeName( "", pszEntry );
	const size_t slash = entryPath.rfind( '/' );
	if ( slash == std::string::npos )
	{
		pugi::xml_attribute attr = row.attribute( entryPath.c_str() );
		if ( !attr )
			attr = row.append_attribute( entryPath.c_str() );
		attr.set_value( pszValue ? pszValue : "" );
	}
	else
	{
		pugi::xml_node parent = row;
		const std::string parentPath = entryPath.substr( 0, slash );
		size_t pos = 0;
		while ( pos < parentPath.size() )
		{
			const size_t next = parentPath.find( '/', pos );
			const std::string part = parentPath.substr( pos, next == std::string::npos ? std::string::npos : next - pos );
			pugi::xml_node child = parent.child( part.c_str() );
			if ( !child ) child = parent.append_child( part.c_str() );
			parent = child;
			if ( next == std::string::npos ) break;
			pos = next + 1;
		}
		const std::string leaf = entryPath.substr( slash + 1 );
		pugi::xml_node valueNode = parent.child( leaf.c_str() );
		if ( !valueNode ) valueNode = parent.append_child( leaf.c_str() );
		valueNode.text().set( pszValue ? pszValue : "" );
	}
	SetModified();
}

int CDataTableXML::GetRowNames( char *pszBuffer, int nBufferSize )
{
	if ( !pszBuffer || nBufferSize < 2 || !xmlRootNode )
		return 0;
	char *out = pszBuffer;
	int total = 0;
	for ( pugi::xml_node child : xmlRootNode.children() )
	{
		if ( child.type() != pugi::node_element ) continue;
		const int added = AddToBuffer( child.name(), out, nBufferSize, total );
		if ( !added ) break;
		total += added;
	}
	if ( total + 1 > nBufferSize ) return 0;
	*out = '\0';
	return total + 1;
}

int CDataTableXML::GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize )
{
	if ( !pszBuffer || nBufferSize < 2 )
		return 0;
	pugi::xml_node row = FindNode( MakeName( pszRow, nullptr ) );
	if ( !row )
	{
		pszBuffer[0] = '\0';
		pszBuffer[1] = '\0';
		return 1;
	}
	std::vector<std::string> names;
	CollectEntryNames( row, "", &names );
	char *out = pszBuffer;
	int total = 0;
	for ( const std::string &name : names )
	{
		const int added = AddToBuffer( name, out, nBufferSize, total );
		if ( !added ) return 0;
		total += added;
	}
	if ( total + 1 > nBufferSize ) return 0;
	*out = '\0';
	return total + 1;
}

void CDataTableXML::ClearRow( const char *pszRowName )
{
	const std::string path = MakeName( pszRowName, nullptr );
	const size_t slash = path.rfind( '/' );
	pugi::xml_node parent = slash == std::string::npos ? xmlRootNode : FindNode( path.substr( 0, slash ) );
	const std::string leaf = slash == std::string::npos ? path : path.substr( slash + 1 );
	if ( parent && parent.remove_child( leaf.c_str() ) )
		SetModified();
}

int CDataTableXML::GetInt( const char *pszRow, const char *pszEntry, int defval )
{
	std::string value;
	return GetValue( MakeName( pszRow, pszEntry ), &value ) ? std::atoi( value.c_str() ) : defval;
}

double CDataTableXML::GetDouble( const char *pszRow, const char *pszEntry, double defval )
{
	std::string value;
	return GetValue( MakeName( pszRow, pszEntry ), &value ) ? std::atof( value.c_str() ) : defval;
}

const char* CDataTableXML::GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize )
{
	if ( !pszBuffer || nBufferSize <= 0 )
		return nullptr;
	std::string value;
	if ( !GetValue( MakeName( pszRow, pszEntry ), &value ) )
		value = defval ? defval : "";
	if ( static_cast<int>( value.size() ) + 1 > nBufferSize )
		return nullptr;
	std::memcpy( pszBuffer, value.c_str(), value.size() + 1 );
	return pszBuffer;
}

int CDataTableXML::GetRawData( const char *pszRow, const char *pszEntry, void *pBuffer, int nBufferSize )
{
	if ( !pBuffer || nBufferSize < 0 )
		return 0;
	std::string value;
	if ( !GetValue( MakeName( pszRow, pszEntry ), &value ) || (value.size() & 1) != 0 )
		return 0;
	const int decodedSize = static_cast<int>( value.size() / 2 );
	if ( decodedSize > nBufferSize )
		return 0;
	int actual = 0;
	NStr::StringToBin( value.c_str(), pBuffer, &actual );
	return actual;
}

void CDataTableXML::SetInt( const char *pszRow, const char *pszEntry, int val )
{
	SetValue( pszRow, pszEntry, NStr::Format( "%d", val ) );
}

void CDataTableXML::SetDouble( const char *pszRow, const char *pszEntry, double val )
{
	SetValue( pszRow, pszEntry, NStr::Format( "%.17g", val ) );
}

void CDataTableXML::SetString( const char *pszRow, const char *pszEntry, const char *val )
{
	SetValue( pszRow, pszEntry, val ? val : "" );
}

void CDataTableXML::SetRawData( const char *pszRow, const char *pszEntry, const void *pBuffer, int nBufferSize )
{
	if ( !pBuffer || nBufferSize < 0 )
		return;
	std::string value( static_cast<size_t>( nBufferSize ) * 2, '\0' );
	NStr::BinToString( pBuffer, nBufferSize, value.data() );
	SetValue( pszRow, pszEntry, value.c_str() );
}
