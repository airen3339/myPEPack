// ConsoleApplication5.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include"PEpack.h"
#include "../mystub/mystub.h"
#include<iostream>
#include<string>
using namespace std;
#define PATH _T("E:\\allenboy.exe")
bool Pack(
	BOOL bIsCompression,
	BOOL bIsNormalEncryption,
	BOOL bIsRegisteredProtection,
	BOOL bIsDynamicEncryption,
	BOOL bIsVerificationProtection,
	BOOL bIsAntiDebugging,
	BOOL bIsApiRedirect,
	BOOL bIsAntiDump,
	PCHAR pPath)
{
	BOOL ret = FALSE;
	//1 ��stub.dll���뵽�ڴ�
	HMODULE hStub = LoadLibrary(_T("..//release//mystub.dll"));
	//3 ���ڴ����ҵ���stub.dllͨѶ�� g_PackInfo
	PPACKINFO pPackInfo = (PPACKINFO)GetProcAddress(hStub, "g_PackInfo");
	PEpack obj;
	obj.ReadTargetFile(pPath, pPackInfo);

	// ��ȡtls��Ϣ
	BOOL bTlsUseful = obj.DealwithTLS(pPackInfo);


	// �Դ���ν��м���
	if (bIsNormalEncryption)
	{
		obj.Encode();
	}

	// �Ը����ν���ѹ��
	if (bIsCompression)
	{
		obj.CompressPE(pPackInfo);
	}



	//2 ��ȡstub.dll���ڴ��С�ͽ���ͷ(Ҳ����Ҫ������ͷ��)
	PIMAGE_DOS_HEADER pStubDos = (PIMAGE_DOS_HEADER)hStub;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pStubDos->e_lfanew + (PCHAR)hStub);
	DWORD dwImageSize = pNt->OptionalHeader.SizeOfImage;
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);


	//4 �ҵ���֮�����ú���ת��OEP
	pPackInfo->TargetOepRva = obj.GetOepRva();
	// ����Iamgebase
	pPackInfo->ImageBase = obj.GetImageBase();
	// ���ú�ѡ��

	pPackInfo->bIsCompression = bIsCompression;                      //ѹ��
	pPackInfo->bIsNormalEncryption = bIsNormalEncryption;            //����
	pPackInfo->bIsRegisteredProtection = bIsRegisteredProtection;    //ע�ᱣ��
	pPackInfo->bIsDynamicEncryption = bIsDynamicEncryption;          //��̬�ӽ���
	pPackInfo->bIsVerificationProtection = bIsVerificationProtection;//У��ϱ���
	pPackInfo->bIsAntiDebugging = bIsAntiDebugging;                  //����ʽ
	pPackInfo->bIsApiRedirect = bIsApiRedirect;                      //api�ض���
	pPackInfo->bIsAntiDump = bIsAntiDump;                            //��תdump
	// ���ú��ض�λ��rva�͵�����rva
	pPackInfo->ImportTableRva = obj.GetImportTableRva();
	pPackInfo->RelocRva = obj.GetRelocRva();

	//5 ���Start������Rva
	DWORD dwStartRva = (DWORD)pPackInfo->StartAddress - (DWORD)hStub;
	// ---���޸�������ͨѶ�ṹ�������֮���ٶ�dll�����ڴ濽��---
	//6 ����ֱ���ڱ��������޸Ļ�Ӱ�����,���Խ�dll����һ�ݵ�pStubBuf
	PCHAR pStubBuf = new CHAR[dwImageSize];
	memcpy_s(pStubBuf, dwImageSize, (PCHAR)hStub, dwImageSize);

	//7 �޸�dll�ļ��ض�λ,����ڶ�������Ӧ�ô���hStub,��Ϊ����dll����ʱ�ض�λ������
	obj.FixDllRloc(pStubBuf, (PCHAR)hStub);

	//8 ��stub���ֵĴ�������ΪĿ������������

	DWORD NewSectionRva = obj.AddSection(
		".stub",
		pSection->VirtualAddress + pStubBuf,
		pSection->SizeOfRawData,
		pSection->Characteristics
	);
	obj.SetTls(NewSectionRva, (PCHAR)hStub, pPackInfo);

	//=================�ض�λ���====================
	// ����ѡ��ȥ���ض�λ
	//obj.CancleRandomBase();
	// ���߽�stub���ض�λ����ճ�������,���ض�λ��ָ��֮,������֮ǰҲ����FixDllRloc,ʹ����Ӧ�µ�PE�ļ�
	obj.ChangeReloc(pStubBuf);

	//9 ��Ŀ������OEP����Ϊstub�е�start����

	DWORD dwChazhi = (dwStartRva - pSection->VirtualAddress);
	DWORD dwNewOep = (dwChazhi + NewSectionRva);
	obj.SetNewOep(dwNewOep);

	// ����ÿ�����ο�д
	obj.SetMemWritable();
	// ��IAT���м���
	obj.ChangeImportTable();

	FreeLibrary(hStub);
	//10 ������ļ�
	string savePath = pPath;
	savePath = savePath + "_pack.exe";
	obj.SaveNewFile((char*)savePath.c_str());


	return ret;
}
int main()
{
	PEpack obj;
                    
	//BOOL bIsCompression,            //ѹ��
	//BOOL bIsNormalEncryption,       //����  
	//BOOL bIsRegisteredProtection,  //ע�ᱣ��
	//BOOL bIsDynamicEncryption,     //��̬�ӽ���
	//BOOL bIsVerificationProtection,//У��ϱ���
	//BOOL bIsAntiDebugging,  //����ʽ
	//BOOL bIsApiRedirect,//api�ض���
	//BOOL bIsAntiDump,//��תdump
	Pack(1, 1, 1, 1, 1, 1, 1, 1,"E:\\allenboy.exe");
    return 0;
}

