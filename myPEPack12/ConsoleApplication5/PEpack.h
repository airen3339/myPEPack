#pragma once
#include <Windows.h>
#include "../mystub/mystub.h"
#include "../aplib/aplib.h"
#pragma comment(lib, "..\\aplib\\aplib.lib")
class PEpack
{
public:
	PEpack();
	~PEpack();
public:
	DWORD GetOepRva();
	void ReadTargetFile(char* pPath, PPACKINFO&  pPackInfo);
	DWORD AddSection(
		PCHAR szName,        //�����ε�����
		PCHAR pSectionBuf,   //�����ε�����
		DWORD dwSectionSize, //�����εĴ�С
		DWORD dwAttribute    //�����ε�����
	);
	DWORD GetFirstNewSectionRva();
	void SetNewOep(DWORD dwNewOep);
	void SaveNewFile(char* pPath);

	void FixDllRloc(PCHAR pBuf, PCHAR pOri);
	void Encode();
	void CancleRandomBase();
	DWORD GetImportTableRva();
	DWORD GetRelocRva();
	void ChangeImportTable();
	DWORD GetImageBase();
	void SetMemWritable();
	void ChangeReloc(PCHAR pBuf);
	DWORD GetNewSectionRva();
	DWORD GetLastSectionRva();
	void CompressPE(PPACKINFO & pPackInfo);
	//pSourceѹ��Դ��lInLength���ݵĴ�С��lOutLenght�жϺ�
	PCHAR Compress(PVOID pSource, long lInLength, OUT long &lOutLenght);
	BOOL DealwithTLS(PPACKINFO & pPackInfo);
	DWORD RvaToOffset(DWORD Rva);
	void SetTls(DWORD NewSectionRva, PCHAR pStubBuf, PPACKINFO pPackInfo);
private:
	DWORD  CalcAlignment(DWORD dwSize, DWORD dwAlignment);
private:// ԭʼ������
	DWORD m_OriSectionNumber;
	// �������������
	DWORD m_codeIndex;
	DWORD m_pResRva;
	DWORD m_pResSectionRva;
	DWORD m_ResSectionIndex;
	DWORD m_ResPointerToRawData;
	DWORD m_ResSizeOfRawData;


	DWORD m_pTlsDataRva;// �洢tls���ݵ�����,Ҳ����.tls����
	DWORD m_pTlsSectionRva;
	DWORD m_TlsSectionIndex;
	DWORD m_TlsPointerToRawData;
	DWORD m_TlsSizeOfRawData;

private://tls���е���Ϣ
	DWORD m_StartOfDataAddress;
	DWORD m_EndOfDataAddress;
	DWORD m_CallBackFuncAddress;

private://�ϵ�buf�е�
	PCHAR m_pBuf;
	DWORD m_FileSize;
private://�µ�buf�е�
	PCHAR m_pNewBuf;
	DWORD m_dwNewFileSize;

	PIMAGE_DOS_HEADER m_pDos;
	PIMAGE_NT_HEADERS m_pNt;
	PIMAGE_SECTION_HEADER m_pSection;

};
