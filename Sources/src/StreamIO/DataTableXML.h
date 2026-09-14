#ifndef __DATATABLEXML_H__
#define __DATATABLEXML_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <pugixml.hpp>
#include <string>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDataTableXML : public IDataTable
{
	OBJECT_MINIMAL_METHODS( CDataTableXML );
	//
	CPtr<IDataStream> pStream;						// stream, this table was open with
	pugi::xml_document xmlDocument;			// открытый документ
	pugi::xml_node xmlRootNode;				// root node
	bool bModified;
	//
	inline void SetModified() { bModified = true; }
	bool SaveDocument();
	bool GetValue( const std::string &szName, std::string *pValue ) const;
	pugi::xml_node FindNode( const std::string &szPath ) const;
	pugi::xml_node EnsureNode( const std::string &szPath );
	void SetValue( const char *pszRow, const char *pszEntry, const char *pszValue );
	//
	std::string MakeName( const char *pszRow, const char *pszEntry ) const;
public:
	CDataTableXML();
	virtual ~CDataTableXML();
	//
	bool Open( IDataStream *pStream, const char *pszBaseNode );
	// получить имена строк таблицы. каждое имя заканчивается на '\0', с строка в целом на '\0\0'
	virtual int STDCALL GetRowNames( char *pszBuffer, int nBufferSize );
	// получить имена колонок таблицы в данной строке. каждое имя заканчивается на '\0', с строка в целом на '\0\0'
	virtual int STDCALL GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize );
	// очистка секции
	virtual void STDCALL ClearRow( const char *pszRowName );
	// complete element access
	// get
	virtual int STDCALL GetInt( const char *pszRow, const char *pszEntry, int defval );
	virtual double STDCALL GetDouble( const char *pszRow, const char *pszEntry, double defval );
	virtual const char* STDCALL GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize );
	virtual int STDCALL GetRawData( const char *pszRow, const char *pszEntry, void *pBuffer, int nBufferSize );
	// set
	virtual void STDCALL SetInt( const char *pszRow, const char *pszEntry, int val );
	virtual void STDCALL SetDouble( const char *pszRow, const char *pszEntry, double val );
	virtual void STDCALL SetString( const char *pszRow, const char *pszEntry, const char *val );
	virtual void STDCALL SetRawData( const char *pszRow, const char *pszEntry, const void *pBuffer, int nBufferSize );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATATABLEXML_H__
