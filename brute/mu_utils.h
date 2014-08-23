#pragma  once

#include <Windows.h>
#include <stdint.h>

#pragma pack(push, 1)

struct PBMSG_HEAD	// Packet - Byte Type
{
public:
	void set ( LPBYTE lpBuf, BYTE head, BYTE size)	// line : 18
	{
		lpBuf[0] = 0xC1;
		lpBuf[1] = size;
		lpBuf[2] = head;
	};	// line : 22

	void setE ( LPBYTE lpBuf, BYTE head, BYTE size)	// line : 25
	{
		lpBuf[0] = 0xC3;
		lpBuf[1] = size;
		lpBuf[2] = head;
	};	// line : 29

	BYTE c;
	BYTE size;
	BYTE headcode;
};

typedef struct _PMSG_JOINRESULT
{
	PBMSG_HEAD h;	// C1:F1
	BYTE scode;	// 3
	BYTE result;	// 4
	BYTE NumberH;	// 5
	BYTE NumberL;	// 6
	BYTE CliVersion[5];	// 7
} PMSG_JOINRESULT, *pPMSG_JOINRESULT;

struct PMSG_IDPASS_V1
{
	PBMSG_HEAD h;
	BYTE subcode;	// 3
	char Id[10];	// 4
	char Pass[10];	// E
	DWORD TickCount;	// 18
	BYTE CliVersion[5];	// 1C
	BYTE CliSerial[16];	// 21  
};

struct PMSG_IDPASS_V2
{
	PBMSG_HEAD h;
	BYTE subcode;	// 3
	char Id[10];	// 4
	char Pass[12];	// E
	DWORD TickCount;	// 18
	BYTE CliVersion[5];	// 1C
	BYTE CliSerial[16];	// 21  
};

struct PMSG_IDPASS_V3
{
	PBMSG_HEAD h;
	BYTE subcode;	// 3
	char Id[10];	// 4
	char Pass[20];	// E
	DWORD TickCount;	// 18
	BYTE CliVersion[5];	// 1C
	BYTE CliSerial[16];	// 21
};

struct PMSG_RESULT
{
	PBMSG_HEAD h;
	unsigned char subcode;	// 3
	unsigned char result;	// 4
};

struct PMSG_CLIENTTIME
{
	PBMSG_HEAD h;
	BYTE subcode;	// 3
	DWORD Time;	// 4
	WORD AttackSpeed;	// 8
	WORD MagicSpeed;	// A
	BYTE CliVersion[5];
};

#pragma pack(pop)

/*
static unsigned char bux_keys[3] = {0xFC, 0xCF, 0xAB};

static uint8_t auth_key[32] = {
	0xE7, 0x6D, 0x3A, 0x89, 0xBC, 0xB2, 0x9F, 0x73, 
	0x23, 0xA8, 0xFE, 0xB6, 0x49, 0x5D, 0x39, 0x5D, 
	0x8A, 0xCB, 0x63, 0x8D, 0xEA, 0x7D, 0x2B, 0x5F, 
	0xC3, 0xB1, 0xE9, 0x83, 0x29, 0x51, 0xE8, 0x56
};*/

/*static uint8_t auth_key[32] = {
	0xAB, 0x11, 0xCD, 0xFE, 0x18, 0x23, 0xC5, 0xA3,
	0xCA, 0x33, 0xC1, 0xCC, 0x66, 0x67, 0x21, 0xF3, 
	0x32, 0x12, 0x15, 0x35, 0x29, 0xFF, 0xFE, 0x1D, 
	0x44, 0xEF, 0xCD, 0x41, 0x26, 0x3C, 0x4E, 0x4D
};*/

/*
void bux_convert(char* buf, int size);
void auth_encrypt(void* data, int start, int end, int inc);
void build_login_packet(PMSG_IDPASS* packet, const char* login,
	const char* passwd, const char* version, const char* serial, uint32_t& counter);
void build_login_packet(PMSG_IDPASS_EX* packet, const char* login,
	const char* passwd, const char* version, const char* serial, uint32_t& counter);*/
////////////////////////////////////

#pragma pack(1)

struct ENCDEC_FILEHEADER
{
	short sFileHeader;
	int dwSize;
};

typedef DWORD dwXORTable[4];

#pragma pack()


class CSimpleModulus
{

public:

	CSimpleModulus();
	virtual ~CSimpleModulus();

	void Init();

	DWORD m_dwModulus[4];	// 4
	DWORD m_dwEncryptionKey[4];	// 14
	DWORD m_dwDecryptionKey[4];	// 24
	DWORD m_dwXORKey[4];	// 34

protected:

	static DWORD s_dwSaveLoadXOR[4];

public:

	int Encrypt(void * lpDest, void * lpSource, int iSize);
	int Decrypt(void * lpDest, void * lpSource, int iSize);

protected:

	int EncryptBlock(void *, void *, int);
	int DecryptBlock(void *, void *);
	int CSimpleModulus::AddBits(void*lpDest,int iDestBitPos,void*lpSource,int iBitSourcePos,int iBitLen);
	void Shift(void*lpBuff,int iSize,int ShiftLen);
	int GetByteOfBit(int);

public:

	BOOL SaveAllKey(LPSTR lpszFileName);
	BOOL LoadAllKey(LPSTR lpszFileName);
	BOOL SaveEncryptionKey(LPSTR lpszFileName);
	BOOL LoadEncryptionKey(LPSTR lpszFileName);
	BOOL SaveDecryptionKey(LPSTR lpszFileName);
	BOOL LoadDecryptionKey(LPSTR lpszFileName);

protected:

	BOOL SaveKey(LPSTR lpszFileName, WORD wFileHeader, BOOL bSaveModulus, BOOL bSaveEncKey, BOOL bSaveDecKey, BOOL bSaveXORKey);
	BOOL LoadKey(LPSTR lpszFileName, WORD wFileHeader, BOOL bLoadModulus, BOOL bLoadEncKey, BOOL bLoadDecKey, BOOL bLoadXORKey);
};