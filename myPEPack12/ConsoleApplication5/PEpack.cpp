#include "stdafx.h"
#include "PEpack.h"


PEpack::PEpack()
{
}


PEpack::~PEpack()
{
}



//��ȡ��ǰ�������ڵ�Rva
DWORD PEpack::GetOepRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pBuf);
	return pNt->OptionalHeader.AddressOfEntryPoint;
}
//��ȡҪ�����ļ����ڴ�  ͬʱ��ȡ��ص�������Ϣ
void PEpack::ReadTargetFile(char* pPath, PPACKINFO& pPackInfo)
{
	DWORD dwRealSize = 0;
	//1 ���ļ�
	HANDLE hFile = CreateFileA(
		pPath, 0x0001, FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL
	);
	//2 ��ȡ�ļ���С
	m_FileSize = GetFileSize(hFile, NULL);
	m_dwNewFileSize = m_FileSize;
	//3 ������ô��Ŀռ�
	m_pBuf = new CHAR[m_FileSize];
	m_pNewBuf = m_pBuf;
	memset(m_pBuf, 0, m_FileSize);

	//4 ���ļ����ݶ�ȡ��������Ŀռ���
	ReadFile(hFile, m_pBuf, m_FileSize, &dwRealSize, NULL);


	m_pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	m_pNt = (PIMAGE_NT_HEADERS)(m_pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(m_pNt);
	// ����ԭʼ������
	m_OriSectionNumber = m_pNt->FileHeader.NumberOfSections;

	// ��ȡOEP
	DWORD dwOEP = m_pNt->OptionalHeader.AddressOfEntryPoint;
	// �����Դ�ε���Ϣ
	m_pResRva = m_pNt->OptionalHeader.DataDirectory[2].VirtualAddress;
	m_pResSectionRva = 0;
	m_ResSectionIndex = -1;
	m_ResPointerToRawData = 0;
	m_ResSizeOfRawData = 0;

	// ��ȡtls����Ϣ
	m_pTlsSectionRva = 0;
	m_TlsSectionIndex = -1;
	m_TlsPointerToRawData = 0;
	m_TlsSizeOfRawData = 0;
	if (m_pNt->OptionalHeader.DataDirectory[9].VirtualAddress)
	{
		// ���tls��ָ��
		PIMAGE_TLS_DIRECTORY32 g_lpTlsDir =
			(PIMAGE_TLS_DIRECTORY32)(RvaToOffset(m_pNt->OptionalHeader.DataDirectory[9].VirtualAddress) + m_pNewBuf);
		// ���tls������ʼrva
		m_pTlsDataRva = g_lpTlsDir->StartAddressOfRawData - m_pNt->OptionalHeader.ImageBase;
	}

	for (int i = 0; i < m_pNt->FileHeader.NumberOfSections; i++)
	{
		// ���oep���������,���ж���������Ǵ����
		if (dwOEP >= pSection->VirtualAddress &&
			dwOEP <= pSection->VirtualAddress + pSection->Misc.VirtualSize)
		{
			// ��ȡ����������������[ͨ��oep�ж�]
			m_codeIndex = i;
		}
		if (m_pResRva >= pSection->VirtualAddress &&
			m_pResRva <= pSection->VirtualAddress + pSection->Misc.VirtualSize)
		{
			// ��ȡrsrc�ε���Ϣ
			m_pResSectionRva = pSection->VirtualAddress;
			m_ResPointerToRawData = pSection->PointerToRawData;
			m_ResSizeOfRawData = pSection->SizeOfRawData;
			m_ResSectionIndex = i;
		}
		// ��ȡtls��Ϣ
		if (m_pNt->OptionalHeader.DataDirectory[9].VirtualAddress)
		{
			if (m_pTlsDataRva >= pSection->VirtualAddress &&
				m_pTlsDataRva <= pSection->VirtualAddress + pSection->Misc.VirtualSize)
			{
				m_pTlsSectionRva = pSection->VirtualAddress;
				m_TlsSectionIndex = i;
				m_TlsPointerToRawData = pSection->PointerToRawData;
				m_TlsSizeOfRawData = pSection->SizeOfRawData;
			}
		}

		pSection = pSection + 1;
	}

	//5 �ر��ļ�
	CloseHandle(hFile);
}
//����ѹ������
PCHAR PEpack::Compress(PVOID pSource, long lInLength, OUT long &lOutLenght)
{

	//packed����ѹ�����ݵĿռ䣬workmemΪ���ѹ����Ҫʹ�õĿռ�
	PCHAR packed, workmem;
	if ((packed = (PCHAR)malloc(aP_max_packed_size(lInLength))) == NULL ||
		(workmem = (PCHAR)malloc(aP_workmem_size(lInLength))) == NULL)
	{
		return NULL;
	}
	//����aP_packѹ������
	lOutLenght = aPsafe_pack(pSource, packed, lInLength, workmem, NULL, NULL);

	if (lOutLenght == APLIB_ERROR)
	{
		return NULL;
	}
	if (NULL != workmem)
	{
		free(workmem);
		workmem = NULL;
	}

	return packed;//���ر����ַ
}


// 0. tls�����rdata��,pe�ļ��е�tls��ᱻѹ��,����ʹ��stub���е�tls��(stub���ں�֮��tls�δ���.text����)
// 1. �������̴߳���֮ʱ���ȶ�ȡtls�ε����ݵ�һ��ռ�,�ռ��ַ������FS:[0x2C],֮��Ҳ����ʹ�����ռ�
//    ���Բ�Ҫ��ͼ�ڿǴ������޸�tls��,������ʹ�����ṩ���ڴ�ռ�,Ӧ���ڼӿ�ʱ��Ӧ�ô�����ⷽ�������
// 2. index������FS:[0x2C]�´��ָ���ҵ�tls��ʹ�õ��ڴ�ռ�ָ��
// 3. ����ֻ���Լ�ѭ�����ü���
// ����:
// 0. ��peĿ¼��9ָ��stub��tls��
// 1. ��ѹ��tls���ݶ�[tls���ݶε�Ѱ�ҷ�ʽ:ͨ��tls���е�StartAddressOfRawData��������Ѱ��]
// 2. ��index���빲����Ϣ�ṹ��,�������������rva(��FixRloc֮������Ϊrva-0x1000+allensection_rva+pe_imagebase)
// 3. stub��tls��ǰ����ͬpe��tls��,��ֵ����Ҫת��(��FixRloc֮������Ϊ��pe��tls������ͬ����)
// 4. stub��addressOfFuncͬpe��tls��,��ֵ����Ҫת��(��FixRloc֮������Ϊ��pe��tls������ͬ����)

BOOL PEpack::DealwithTLS(PPACKINFO & pPackInfo)
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	DWORD dwImageBase = pNt->OptionalHeader.ImageBase;
	//���ж�
	if (pNt->OptionalHeader.DataDirectory[9].VirtualAddress == 0)
	{
		pPackInfo->bIsTlsUseful = FALSE;
		return FALSE;
	}
	else
	{
		//����Ϊ������tls
		pPackInfo->bIsTlsUseful = TRUE;

		PIMAGE_TLS_DIRECTORY32 g_lpTlsDir =
			(PIMAGE_TLS_DIRECTORY32)(RvaToOffset(pNt->OptionalHeader.DataDirectory[9].VirtualAddress) + m_pNewBuf);
		// ��ȡtlsIndex(������ַ����Offset 
		DWORD indexOffset = RvaToOffset(g_lpTlsDir->AddressOfIndex - dwImageBase);
		// ��ȡ����tlsIndex��ֵ
		pPackInfo->TlsIndex = 0;//indexһ��Ĭ��ֵΪ0
		//�������ļ��е�
		if (indexOffset != -1)
		{
			//ȡ����
			pPackInfo->TlsIndex = *(DWORD*)(indexOffset + m_pNewBuf);
		}
		// ����tls���е���Ϣ 
		m_StartOfDataAddress = g_lpTlsDir->StartAddressOfRawData;
		m_EndOfDataAddress = g_lpTlsDir->EndAddressOfRawData;
		m_CallBackFuncAddress = g_lpTlsDir->AddressOfCallBacks;

		// ��tls�ص�����rva���õ�������Ϣ�ṹ��
		pPackInfo->TlsCallbackFuncRva = m_CallBackFuncAddress;
		return TRUE;
	}


}
// ���ڽ�PE�ļ���rvaתΪ�ļ�ƫ��
DWORD PEpack::RvaToOffset(DWORD Rva)
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	for (int i = 0; i<pNt->FileHeader.NumberOfSections; i++)
	{
		if (Rva >= pSection->VirtualAddress&&
			Rva <= pSection->VirtualAddress + pSection->Misc.VirtualSize)
		{
			// ����ļ���ַΪ0,���޷����ļ����ҵ���Ӧ������
			if (pSection->PointerToRawData == 0)
			{
				return -1;
			}
			return Rva - pSection->VirtualAddress + pSection->PointerToRawData;
		}
		pSection = pSection + 1;
	}
}


//����tls��Ϣ,�������allen��������
// 0. tls�����rdata��,pe�ļ��е�tls��ᱻѹ��,����ʹ��stub���е�tls��(stub���ں�֮��tls�δ���.text����)
// 1. �������̴߳���֮ʱ���ȶ�ȡtls�ε����ݵ�һ��ռ�,�ռ��ַ������FS:[0x2C],֮��Ҳ����ʹ�����ռ�
//    ���Բ�Ҫ��ͼ�ڿǴ������޸�tls��,������ʹ�����ṩ���ڴ�ռ�,Ӧ���ڼӿ�ʱ��Ӧ�ô�����ⷽ�������
// 2. index������FS:[0x2C]�´��ָ���ҵ�tls��ʹ�õ��ڴ�ռ�ָ��
// 3. ����ֻ���Լ�ѭ�����ü���
// ����:
// 0. ��peĿ¼��9ָ��stub��tls��
// 1. ��ѹ��tls���ݶ�[tls���ݶε�Ѱ�ҷ�ʽ:ͨ��tls���е�StartAddressOfRawData��������Ѱ��]
// 2. ��index���빲����Ϣ�ṹ��,�������������va(��FixRloc֮������Ϊrva-0x1000+allensection_rva+pe_imagebase)
// 3. stub��tls��ǰ����ͬpe��tls��,��ֵ����Ҫת��(��FixRloc֮������Ϊ��pe��tls������ͬ����)
// 4. stub��addressOfFuncͬpe��tls��,��ֵ����Ҫת��(��FixRloc֮������Ϊ��pe��tls������ͬ����)
// ����ֵ:	void

// SetTls
// 	
// void
// 	DWORD NewSectionRva  ����ӵ�allen���ε�rva
//	PCHAR pStubBuf       stubdll���ڴ��ָ��
// 	DWORD pPackInfo		 ������Ϣ�ṹ����׵�ַ,PackInfo�б�����tlsRva

void PEpack::SetTls(DWORD NewSectionRva, PCHAR pStubBuf, PPACKINFO pPackInfo)
{
	PIMAGE_DOS_HEADER pStubDos = (PIMAGE_DOS_HEADER)pStubBuf;
	PIMAGE_NT_HEADERS pStubNt = (PIMAGE_NT_HEADERS)(pStubDos->e_lfanew + pStubBuf);

	PIMAGE_DOS_HEADER pPeDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pPeNt = (PIMAGE_NT_HEADERS)(pPeDos->e_lfanew + m_pNewBuf);

	//0 ��peĿ¼��9ָ��stub��tls��
	pPeNt->OptionalHeader.DataDirectory[9].VirtualAddress =
		(pStubNt->OptionalHeader.DataDirectory[9].VirtualAddress - 0x1000) + NewSectionRva;
	pPeNt->OptionalHeader.DataDirectory[9].Size =
		pStubNt->OptionalHeader.DataDirectory[9].Size;

	PIMAGE_TLS_DIRECTORY32  pITD =
		(PIMAGE_TLS_DIRECTORY32)(RvaToOffset(pPeNt->OptionalHeader.DataDirectory[9].VirtualAddress) + m_pNewBuf);
	// ��ȡ�����ṹ����tlsIndex��va
	//������Ϣ�ṹ����׵�ַ-stubdll���ڴ��ָ��+4(�ڽṹ�����ƫ�ƣ�-stubͷ��+NewSectionRva+���ػ�ַ   
	DWORD indexva = ((DWORD)pPackInfo - (DWORD)pStubBuf + 4) - 0x1000 + NewSectionRva + pPeNt->OptionalHeader.ImageBase;
	pITD->AddressOfIndex = indexva;
	pITD->StartAddressOfRawData = m_StartOfDataAddress;
	pITD->EndAddressOfRawData = m_EndOfDataAddress;

	// ������ȡ��tls�Ļص�����,������Ϣ�ṹ���д���tls�ص�����ָ��,��stub��ǵĹ������ֶ�����tls,����tls�ص�����ָ�����û�ȥ
	pITD->AddressOfCallBacks = 0;

	m_pBuf = m_pNewBuf;
}

//ѹ��PE�ļ�

void PEpack::CompressPE(PPACKINFO &pPackInfo)
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);

	// ���ڼ�¼ѹ�����εĸ���
	pPackInfo->PackSectionNumber = 0;

	// 1.1 ��ȡ�ļ�ͷ�Ĵ�С,����ȡ����Դ��,tls֮��ε��ļ��е��ܴ�С
	DWORD SecSizeWithOutResAndTls = 0;
	PIMAGE_SECTION_HEADER pSectionTmp1 = pSection;
	BOOL isFirstNoEmptySec = TRUE;
	DWORD dwHeaderSize = 0;
	//�ȼ���Ҫ�������ܴ�С 
	for (SIZE_T i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		// ���ڻ�õ�һ���ǿ����ε��ļ�ƫ��,Ҳ�����ļ�ͷ��С
		if (isFirstNoEmptySec && pSectionTmp1->SizeOfRawData != 0)
		{
			dwHeaderSize = pSectionTmp1->PointerToRawData;
			isFirstNoEmptySec = FALSE;
		}
		// ���ڻ�ȡ �� rsrc/tls�ε��ܴ�С
		if (pSectionTmp1->VirtualAddress != m_pResSectionRva &&
			pSectionTmp1->VirtualAddress != m_pTlsSectionRva)
		{
			SecSizeWithOutResAndTls += pSectionTmp1->SizeOfRawData;
		}
		pSectionTmp1 = pSectionTmp1 + 1;
	}
	// 1.2 ��ȡҪѹ���Ķε��ڴ�
	// �����ڴ�
	PCHAR memWorked = new CHAR[SecSizeWithOutResAndTls];
	// �Ѿ��������ڴ��С
	DWORD dwCopySize = 0;
	// ������Щ���ε��ڴ�
	PIMAGE_SECTION_HEADER pSectionTmp2 = pSection;
	//�ٴ�ѭ��
	for (SIZE_T i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		//�ж��Ƿ�Ϊtls����Դ��
		if (pSectionTmp2->VirtualAddress != m_pResSectionRva &&
			pSectionTmp2->VirtualAddress != m_pTlsSectionRva)
		{
		 //��ʼ����
			memcpy_s(memWorked + dwCopySize, pSectionTmp2->SizeOfRawData,
				m_pNewBuf + pSectionTmp2->PointerToRawData, pSectionTmp2->SizeOfRawData);
			dwCopySize += pSectionTmp2->SizeOfRawData;
		}
		pSectionTmp2 = pSectionTmp2 + 1;
	}
	// 1.3 ѹ��,����ȡѹ����Ĵ�С(�ļ���������������н���)
	LONG blen;
	PCHAR packBuf = Compress(memWorked, SecSizeWithOutResAndTls, blen);

	// 1.4 ������Դ�� ���ڴ�ռ�
	PCHAR resBuffer = new CHAR[m_ResSizeOfRawData];
	PCHAR tlsBuffer = new CHAR[m_TlsSizeOfRawData];
	//ԭ��
	memcpy_s(resBuffer, m_ResSizeOfRawData, m_ResPointerToRawData + m_pNewBuf, m_ResSizeOfRawData);
	memcpy_s(tlsBuffer, m_TlsSizeOfRawData, m_TlsPointerToRawData + m_pNewBuf, m_TlsSizeOfRawData);

	// 1.6 ����ѹ����Ϣ����Ϣ�ṹ����
	//     ���ҽ�m_pBuf�еķ���Դ�κͷ�tls�ε������ļ�ƫ�ƺʹ�С��Ϊ0
	PIMAGE_DOS_HEADER pOriDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pOriNt = (PIMAGE_NT_HEADERS)(pOriDos->e_lfanew + m_pBuf);
	PIMAGE_SECTION_HEADER pOriSection = IMAGE_FIRST_SECTION(pOriNt);

	for (int i = 0; i < pOriNt->FileHeader.NumberOfSections; i++)
	{
		if (pOriSection->VirtualAddress != m_pResSectionRva&&
			pOriSection->VirtualAddress != m_pTlsSectionRva)
		{
			// ���ڻ�ȡѹ������������
			pPackInfo->PackSectionNumber++;
			// ���õ�i���ڵ�ѹ������index
			pPackInfo->PackInfomation[pPackInfo->PackSectionNumber][0] = i;
			// ����ѹ���������ļ���С
			pPackInfo->PackInfomation[pPackInfo->PackSectionNumber][1] = pOriSection->SizeOfRawData;

			// ����ԭ���Ľ������ļ���ƫ�ƺʹ�СΪ0
			pOriSection->SizeOfRawData = 0;
			pOriSection->PointerToRawData = 0;
		}

		pOriSection = pOriSection + 1;
	}


	// 1.6 �����¿ռ�,ʹm_pNewBufָ��֮,��m_pBuf�ļ�ͷ����
	m_FileSize = dwHeaderSize + m_ResSizeOfRawData + m_TlsSizeOfRawData;
	//��ʼ����
	m_pNewBuf = nullptr;
	m_pNewBuf = new CHAR[m_FileSize];

	// 1.6 �޸�res�ε�����ͷ,������
	pOriSection = IMAGE_FIRST_SECTION(pOriNt);
	//��ʼ�޸���Դ��tls�ε�ͷ  ���ж�������λ��
	if (m_ResSectionIndex < m_TlsSectionIndex)
	{
		pOriSection[m_ResSectionIndex].PointerToRawData = dwHeaderSize;
		pOriSection[m_TlsSectionIndex].PointerToRawData = dwHeaderSize + m_ResSizeOfRawData;
		memcpy_s(m_pNewBuf, dwHeaderSize, m_pBuf, dwHeaderSize);
		memcpy_s(m_pNewBuf + dwHeaderSize, m_ResSizeOfRawData, resBuffer, m_ResSizeOfRawData);
		memcpy_s(m_pNewBuf + dwHeaderSize + m_ResSizeOfRawData
			, m_TlsSizeOfRawData, tlsBuffer, m_TlsSizeOfRawData);
	}
	else if (m_ResSectionIndex > m_TlsSectionIndex)
	{
		pOriSection[m_TlsSectionIndex].PointerToRawData = dwHeaderSize;
		pOriSection[m_ResSectionIndex].PointerToRawData = dwHeaderSize + m_TlsSizeOfRawData;
		memcpy_s(m_pNewBuf, dwHeaderSize, m_pBuf, dwHeaderSize);
		memcpy_s(m_pNewBuf + dwHeaderSize, m_TlsSizeOfRawData, tlsBuffer, m_TlsSizeOfRawData);
		memcpy_s(m_pNewBuf + dwHeaderSize + m_TlsSizeOfRawData
			, m_ResSizeOfRawData, resBuffer, m_ResSizeOfRawData);
	}
	else
	{
		memcpy_s(m_pNewBuf, dwHeaderSize, m_pBuf, dwHeaderSize);
	}
	/*else if(m_ResSectionIndex == -1 && m_TlsSectionIndex == -1)
	{
	}*/

	delete[] m_pBuf;
	m_pBuf = m_pNewBuf;

	// ������Σ�ѹ������������ݣ�
	pPackInfo->packSectionRva = AddSection(".pack", packBuf, blen, 0xC0000040);
	pPackInfo->packSectionSize = CalcAlignment(blen, 0x200);
	// 1.7 ���.pack��
	delete[] memWorked;
	free(packBuf);
	delete[] resBuffer;
}



//�������
DWORD PEpack::AddSection(
	PCHAR szName,        //�����ε�����
	PCHAR pSectionBuf,   //�����ε�����
	DWORD dwSectionSize, //�����εĴ�С
	DWORD dwAttribute    //�����ε�����
)
{
	//1 ���ݸղŶ�ȡ��exe�ļ������ݣ��õ���������κ��µ�exe�ļ��Ĵ�С
	m_dwNewFileSize = m_FileSize + CalcAlignment(dwSectionSize, 0x200);
	//2 ����ռ�
	m_pNewBuf = nullptr;  //���ÿ�
	m_pNewBuf = new CHAR[m_dwNewFileSize];
	memset(m_pNewBuf, 0, m_dwNewFileSize);
	//3 ��ԭ����PE���ݿ�����������Ŀռ���
	memcpy(m_pNewBuf, m_pBuf, m_FileSize);
	//4 �������ο�����PE�ļ��ĺ���
	memcpy(m_pNewBuf + m_FileSize, pSectionBuf, dwSectionSize);
	//5 �޸����α�
	m_pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	m_pNt = (PIMAGE_NT_HEADERS)(m_pDos->e_lfanew + m_pNewBuf);
	m_pSection = IMAGE_FIRST_SECTION(m_pNt);
	//�õ����α�����һ��
	PIMAGE_SECTION_HEADER pLastSection =
		m_pSection + m_pNt->FileHeader.NumberOfSections - 1;
	//�õ����α�����һ��ĺ���
	PIMAGE_SECTION_HEADER pNewSection = pLastSection + 1;
	pNewSection->Characteristics = dwAttribute;    //����
	strcpy_s((char *)pNewSection->Name, 8, szName);//������--->�˴�������,����㲻����Ϊ֮����ռ�,������ӽ���ͷʱ���ܻ�Խ��.

												   // �����ڴ�ƫ�ƺ��ڴ��С
	pNewSection->Misc.VirtualSize = dwSectionSize; //�ڴ��еĴ�С������Ҫ���룩
	pNewSection->VirtualAddress = pLastSection->VirtualAddress +
		CalcAlignment(pLastSection->Misc.VirtualSize, 0x1000);
	pNewSection->SizeOfRawData = CalcAlignment(dwSectionSize, 0x200);

	// �����ļ�ƫ�ƺ��ļ���С
	while (TRUE)
	{
		if (pLastSection->PointerToRawData)
		{
			// �ҵ�ǰһ����0������
			pNewSection->PointerToRawData = pLastSection->PointerToRawData +
				pLastSection->SizeOfRawData;
			break;
		}
		pLastSection = pLastSection - 1;
	}

	//6 �޸����������;����С
	m_pNt->FileHeader.NumberOfSections++;
	m_pNt->OptionalHeader.SizeOfImage = pNewSection->VirtualAddress + dwSectionSize;



	// ����һ�ݵ�ǰ�Ĵ�С
	m_FileSize = m_dwNewFileSize;
	// �ͷ�֮ǰ���ڴ�,�����浱ǰ�ĵ�����
	delete[] m_pBuf;
	m_pBuf = m_pNewBuf;

	// ������������ε�rva
	return pNewSection->VirtualAddress;
}
//��ȡ��һ�������ε�rva
DWORD PEpack::GetFirstNewSectionRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	PIMAGE_SECTION_HEADER pLastSection = pSection + m_OriSectionNumber - 1;

	return pLastSection->VirtualAddress +
		CalcAlignment(pLastSection->Misc.VirtualSize, 0x1000);
}
//�����µĳ�����ڵ�
void PEpack::SetNewOep(DWORD dwNewOep)
{
	m_pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	m_pNt = (PIMAGE_NT_HEADERS)(m_pDos->e_lfanew + m_pNewBuf);
	m_pNt->OptionalHeader.AddressOfEntryPoint = dwNewOep;
}
//�����ļ�
void PEpack::SaveNewFile(char* pPath)
{
	//1 ���ļ�
	DWORD dwRealSize = 0;
	HANDLE hFile = CreateFileA(
		pPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL
	);
	//2 ���ڴ��е�����д�뵽�ļ���
	WriteFile(hFile,
		m_pNewBuf, m_dwNewFileSize, &dwRealSize, NULL);
	//3 �ر��ļ������
	CloseHandle(hFile);
}

//��ȡ�����Ĵ�С
DWORD  PEpack::CalcAlignment(DWORD dwSize, DWORD dwAlignment)
{
	if (dwSize%dwAlignment == 0)
	{
		return dwSize;
	}
	else
	{
		return (dwSize / dwAlignment + 1)*dwAlignment;
	}
}


//���������εĵ�ַ�޸�dll���ض�λ[dll�Ǽ��ص��ڴ��,�������Ĭ�ϼ��ػ�ַ,����ӵĽ�����rva�Լ���ԭ������ʼ�Ĳ�ֵ����������.text���ض�λ]
void PEpack::FixDllRloc(PCHAR pBuf, PCHAR pOri)
{
	// �����ض�λ��Ϣ�ṹ��
	typedef struct _TYPE
	{
		unsigned short offset : 12;
		unsigned short type : 4;
	}TYPE, *PTYPE;

	//��λ����һ���ض�λ��
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + pBuf);
	PIMAGE_DATA_DIRECTORY pRelocDir = (pNt->OptionalHeader.DataDirectory + 5);
	PIMAGE_BASE_RELOCATION pReloc =
		(PIMAGE_BASE_RELOCATION)(pRelocDir->VirtualAddress + pBuf);

	// ��ʼ�޸��ض�λ
	while (pReloc->SizeOfBlock != 0)
	{
		// �ض�λ�ʼ����
		DWORD BeginLoc = (DWORD)(pReloc->VirtualAddress + pBuf);
		// �ض�λ��ĸ���
		DWORD dwCount = (pReloc->SizeOfBlock - 8) / 2;
		// �ض�λ����
		PTYPE pType = (PTYPE)(pReloc + 1);
		// �޸�ÿһ���ض�λ��
		for (size_t i = 0; i < dwCount; i++)
		{
			// ���������3
			if (pType->type == 3)
			{
				// ��ȡ�ض�λ��ַ
				PDWORD pReloction = (PDWORD)(pReloc->VirtualAddress + pType->offset + pBuf);
				// ��ȡ���ض�λ��ַ���ض�λ�������ͷ��ƫ��
				DWORD Chazhi = *pReloction - (DWORD)pOri - 0x1000;
				// ��ƫ�Ƽ����½�����rva��ø��ض�λ���rva,�ڼ��ϵ�ǰĬ�ϼ��ػ�ַ�����޸��ض�λ
				*pReloction = Chazhi + GetNewSectionRva() + GetImageBase();
			}
			//��λ����һ���ض�λ��
			pType++;
		}
		// ��λ����һ���ض�λ��
		pReloc = (PIMAGE_BASE_RELOCATION)((PCHAR)pReloc + pReloc->SizeOfBlock);
	}
}


//�Դ���ν��м���
void PEpack::Encode()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	// ��λ�������,����ÿ���μ���
	pSection = pSection + m_codeIndex;
	PCHAR pStart = pSection->PointerToRawData + m_pNewBuf;
	for (int i = 0; i < (pSection->Misc.VirtualSize); i++)
	{
		pStart[i] ^= 0x20;
	}

}
//ȡ���ض�λ

void PEpack::CancleRandomBase()
{
	m_pNt->OptionalHeader.DllCharacteristics &=
		~IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
}

//	��ȡ������rva
DWORD PEpack::GetImportTableRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pBuf);
	return pNt->OptionalHeader.DataDirectory[1].VirtualAddress;
}

//��ȡ�ض�λ���rva
DWORD PEpack::GetRelocRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pBuf);
	return pNt->OptionalHeader.DataDirectory[0].VirtualAddress;
}

//�Ե������и��� ȫ����Ϊ0
void PEpack::ChangeImportTable()
{
	// 3.��Ŀ¼��ĵ��������0
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	pNt->OptionalHeader.DataDirectory[1].VirtualAddress = 0;
	pNt->OptionalHeader.DataDirectory[1].Size = 0;

	pNt->OptionalHeader.DataDirectory[12].VirtualAddress = 0;
	pNt->OptionalHeader.DataDirectory[12].Size = 0;

}

//��ȡ���ػ�ַ
DWORD PEpack::GetImageBase()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pBuf);
	return pNt->OptionalHeader.ImageBase;
}

//����ÿ������Ϊ��д״̬
void PEpack::SetMemWritable()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	DWORD SectionNumber = pNt->FileHeader.NumberOfSections;

	for (int i = 0; i < SectionNumber; i++)
	{
		pSection[i].Characteristics |= 0x80000000;
	}
}

//���ڶ�̬���ػ�ַ,��Ҫ��stub���ض�λ����(.reloc)�޸ĺ󱣴�,��PE�ض�λ��Ϣָ��ָ��õ�ַ

void PEpack::ChangeReloc(PCHAR pBuf)
{
	// ��λ����һ���ض�λ��
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + pBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	PIMAGE_DATA_DIRECTORY pRelocDir = (pNt->OptionalHeader.DataDirectory + 5);
	PIMAGE_BASE_RELOCATION pReloc =
		(PIMAGE_BASE_RELOCATION)(pRelocDir->VirtualAddress + pBuf);

	// ��ʼ�����ض�λ
	while (pReloc->SizeOfBlock != 0)
	{
		
		// �ض�λ�ʼ����,���䶨λ���ڴ�֮ǰ���allen��(Ҫ��ԭ����RVA ��Ϊ�Ǵ����е�RVA)
		//ԭ��:��λ���ÿһ���� pReloc->VirtualAddress Ϊÿһҳ����ʼ��ַ(Ҳ����0x1000 0x2000...)������ҳ�ֿ�
		//ԭ���Ĵ����������ڼ��ػ�ַ������0x400000)�ĵ�2ҳҲ����0x1000(�ڶ�ҳ����ʼ��ַ����ʾPE�е�ͷ������Ŀ¼��ȵ�
		// ������ض�λ��dll ��Ҳ������û��ͷ�� Ҫ��ȥ0x1000�����еģ�  ͬʱ���˽ڵ�RVA,Ҳ�������һ�ڿǴ���Ŀ�ʼ��RVA)  �൱�ڴ���δ����￪ʼ
		//����Ҫ�ҵ���Ҫ�ض�λ���Ǹ���ַ��va  va=���ػ�ַ +ҳ��ַ(pReloc->VirtualAddress)+ҳ�ڵ�ַ���ض�λ���е�һ�����鱣����ǣ�
		pReloc->VirtualAddress = (DWORD)(pReloc->VirtualAddress - 0x1000 + GetLastSectionRva());
		// ��λ����һ���ض�λ��
		pReloc = (PIMAGE_BASE_RELOCATION)((PCHAR)pReloc + pReloc->SizeOfBlock);
	}
	DWORD dwRelocRva = 0;
	DWORD dwRelocSize = 0;
	DWORD dwSectionAttribute = 0;
	while (TRUE)
	{
		if (!strcmp((char*)pSection->Name, ".reloc"))
		{
			dwRelocRva = pSection->VirtualAddress;
			dwRelocSize = pSection->SizeOfRawData;
			dwSectionAttribute = pSection->Characteristics;
			break;
		}
		pSection = pSection + 1;
	}

	// ��stubdll��.reloc��ӵ�PE�ļ������,����Ϊ.nreloc,���ظ����ε�Rva
	DWORD RelocRva = AddSection(".nreloc", dwRelocRva + pBuf, dwRelocSize, dwSectionAttribute);

	// ���ض�λ��Ϣָ������ӵ�����
	PIMAGE_DOS_HEADER pExeDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pExeNt = (PIMAGE_NT_HEADERS)(pExeDos->e_lfanew + m_pNewBuf);
	pExeNt->OptionalHeader.DataDirectory[5].VirtualAddress = RelocRva;
	pExeNt->OptionalHeader.DataDirectory[5].Size = dwRelocSize;



}


//���Ҫ���һ��������,�����������ε�rva

DWORD PEpack::GetNewSectionRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	PIMAGE_SECTION_HEADER pLastSection = pSection + pNt->FileHeader.NumberOfSections - 1;

	return pLastSection->VirtualAddress +
		CalcAlignment(pLastSection->Misc.VirtualSize, 0x1000);
}


//��ȡ���һ���ε�rva
DWORD PEpack::GetLastSectionRva()
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pNewBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pNewBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	PIMAGE_SECTION_HEADER pLastSection = pSection + pNt->FileHeader.NumberOfSections - 1;

	return (DWORD)pLastSection;
}

