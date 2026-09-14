#ifndef __DATATREEXML_H__
#define __DATATREEXML_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <pugixml.hpp>
#include <list>
#include <string>
#include <vector>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNodesList
{
	std::vector<pugi::xml_node> nodes;
	int nCurrElement;
	//
	SNodesList() : nCurrElement( -1 ) {}
};
class CDataTreeXML : public IDataTree
{
	OBJECT_MINIMAL_METHODS( CDataTreeXML );
	//
	CPtr<IDataStream> pStream;						// stream, this table was open with
	pugi::xml_document xmlDocument;			// открытый документ
	//
	std::list<pugi::xml_node> nodes;		// стек нодов по иерархии углублени
	std::list<SNodesList> nodelists;			// стек списков нодов по иерархии углублени
	pugi::xml_node xmlCurrNode;				// текущий node
	//
	std::list<pugi::xml_node> elements;	// стек элементов по иерархии углублени
	std::list<pugi::xml_node> arrbases;	// стек элементов сонований массивов по иерархии углублени
	pugi::xml_node xmlCurrElement;			// текущий элемент в блочной структуре при записи
	//
	IDataTree::EAccessMode eMode;
	//
	// получить текстовый node по имени. Это либо атрибут текущего node, либо single node из текущего.
	bool GetTextValue( DTChunkID idChunk, std::string *pValue ) const;
	bool SaveDocument();
public:
	CDataTreeXML( IDataTree::EAccessMode eMode );
	virtual ~CDataTreeXML();
	//
	bool Open( IDataStream *pStream, DTChunkID idBaseNode );
	// is opened in the READ mode?
	virtual bool STDCALL IsReading() const { return eMode == IDataTree::READ; }
	// start new complex chunk
	virtual int STDCALL StartChunk( DTChunkID idChunk );
	// finish complex chunk
	virtual void STDCALL FinishChunk();
	// simply data chunk: text, integer, fp
	virtual int STDCALL GetChunkSize();
	virtual bool STDCALL RawData( void *pData, int nSize );
	virtual bool STDCALL StringData( char *pData );
	virtual bool STDCALL StringData( WORD *pData );
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData );
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData );
	// array data serialization (special case)
	virtual int STDCALL CountChunks( DTChunkID idChunk );
	virtual bool STDCALL SetChunkCounter( int nCount );
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk );
	virtual void STDCALL FinishContainerChunk();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Kept as a source-compatible no-op for callers from the old MSXML implementation.
void InitCOM();
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATATREEXML_H__
