#define _CRT_SECURE_NO_WARNINGS
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#pragma comment(lib,"Ws2_32.lib")

typedef enum{
Work_view,
Work_read,
Work_call,
Work_load,
Work_write
}WorkMode;



static WorkMode Wm;
static BOOL SilentFlg = FALSE;
static unsigned long Address = 1;
static unsigned long Length = 0;
static unsigned long Value = 0;
static unsigned long Param1 = 0;
static unsigned long Param2 = 0;
static char *Filename = NULL;
static FILE *fIO = NULL;

static const int timeoutIO = 200;
static const int timeoutRun = 2000;
#define READ_MAX_QUANT 50

#pragma pack(push,1)
// 12 bytes
typedef struct{
/*
? - ask id (for compatible rigol)
C - call function (*Address)(Param1, Param2)
R - Read Memory at Address (max 3 dword, size in Param1)
W - Write Memory at Address (source in Param1)
Warning! DWORD align !!!
*/
char Cmd;
char Terminator;
unsigned short Param2;
unsigned long Address;
unsigned long Param1;
}RigolReqBlock;
#pragma pack(pop)

//#if sizeof(RigolReqBlock) != 0xC
//#error Bad packet Size
//#endif

#define NUM_WAIT_REP	(3)

static int pascal NetSelAndRecv(
		SOCKET s, 
		char *buf, int len, 
		unsigned int Timeout)
{
fd_set readfds;
struct timeval timeout;
int RetVal;
int WaitRepeat = 0;  

 readfds.fd_array[0] = s;
 readfds.fd_count = 1;

 timeout.tv_sec = Timeout / 1000;
 timeout.tv_usec = 1000 * (Timeout % 1000);
 if(1 != (RetVal = select(0, &readfds, NULL, NULL, &timeout))){
	 return(0);
 }
 return(recv(s, buf, len, 0));
}

static int pascal NetSelAndRecvfrom(
		SOCKET s, 
		char *buf, int len, 
		unsigned int Timeout, 
		struct sockaddr *from, 
		int *fromlen)
{
fd_set readfds;
struct timeval timeout;
int RetVal;
  
 readfds.fd_array[0] = s;
 readfds.fd_count = 1;

 timeout.tv_sec = Timeout / 1000;
 timeout.tv_usec = 1000 * (Timeout % 1000);
 if(1 != (RetVal = select(0, &readfds, NULL, NULL, &timeout))) return(RetVal);
 return(recvfrom(s, buf, len, 0, from, fromlen));
}


// Получить список всех
static int FindRigol(SOCKADDR *RigolAddr)
{
SOCKET SCK;
DWORD optval;
SOCKADDR SA;
int Len;
unsigned char Answ[1024];
int FromLen;
int NoAnswer;
static const char AskCmd[]="?";

SCK = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
if(INVALID_SOCKET == SCK){
	return(-1);
}
optval = 1;
setsockopt(SCK, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof(optval));
	// Посылка запроса на отклик
	memset(&SA,0,sizeof(SA));
	// Шириковещательно - 255.255.255.255
	*(DWORD *)&SA.sa_data[2] = -1;
	// Целевой порт 
	*(WORD *)&SA.sa_data[0] = htons(6000u);
	SA.sa_family = AF_INET;
	// пошлем ?
	Len = sendto(SCK, AskCmd, sizeof(AskCmd),0, &SA, sizeof(SA));
	if(Len != sizeof(AskCmd)){
		closesocket(SCK);
		return(-1);
	}
	NoAnswer = 5;
	do{
		// Принимаем ответы откликнувшихся плат
		FromLen = sizeof(SA);
		Len = NetSelAndRecvfrom(SCK, Answ, sizeof(Answ), 100u, RigolAddr, &FromLen);
		if(Len > 0){
			Answ[Len]=0;
			if(!SilentFlg) printf("Autodetect answer: [%s] from %u.%u.%u.%u\n",
				Answ,
				(unsigned char)RigolAddr->sa_data[2],
				(unsigned char)RigolAddr->sa_data[3],
				(unsigned char)RigolAddr->sa_data[4],
				(unsigned char)RigolAddr->sa_data[5]
				);
			closesocket(SCK);
			return(0);
		}
		NoAnswer--;
	}while(NoAnswer > 0);
	closesocket(SCK);
	if(!SilentFlg) printf("No Rigol detected\n");
	return(-2);
}


// <=0 - error
int RigolExchange(SOCKET Sock, RigolReqBlock *SrcBuf, void *Dst, size_t MaxLen, int Timeout)
{
int RealSize;
 // Зашлем управляющий блок
 RealSize = send(Sock, (void *) SrcBuf, sizeof(RigolReqBlock), 0);
 if( RealSize != sizeof(RigolReqBlock) ){
	 if(!SilentFlg) printf("TX Error (%d/%d), code =%d\n", RealSize, sizeof(RigolReqBlock), WSAGetLastError());
	return(-1);
 }
 // Проверим ответ с таймаутом
 return(NetSelAndRecv(Sock, Dst, MaxLen, Timeout));
}


int RigolGetIdent(SOCKET Sock, char *TextBuffer, int MaxLen)
{
int Len;
RigolReqBlock Req;
Req.Cmd = '?';
Req.Terminator = 0;
Len = RigolExchange(Sock, &Req, TextBuffer, MaxLen - 1, timeoutIO);
if(Len <=0){
	if(!SilentFlg) printf("Get Ident Io Error, %d\n", Len);
	return(-1);
}
if(Len > MaxLen - 1)Len = MaxLen - 1;
TextBuffer[Len]=0;
return(Len);
}

int RigolWriteDword(SOCKET Sock, unsigned long Address, unsigned long Value)
{
int Len;
RigolReqBlock Req;
Req.Cmd = 'W';
Req.Terminator = 0;
Req.Address	= Address;
Req.Param1	= Value;
Len = RigolExchange(Sock, &Req, &Req, sizeof(Req), timeoutIO);
if(Len != sizeof(Req)){
	if(!SilentFlg) printf("Error on write %08.8X to %08.8X, %d\n", Value, Address, Len);
	return(-1);
}
if((Req.Address != Address) ||
   ((Req.Param1 ^ ~Value) !=0) ||
   (Req.Cmd != 'W')){
	if(!SilentFlg) printf("Bad answer packet on write %08.8X to %08.8X\n", Value, Address);
	return(-2);
}
return(1);
}

static unsigned long RepAddress = 0;
static int RepPktCntr = 0;

// Читать блок памяти
int RigolReadDword(SOCKET Sock, unsigned long Address, unsigned int Num, void *AnswerBuffer)
{
int Len;
int RepeatCounter = 0;
RigolReqBlock Req;
unsigned long AnswerBuf[READ_MAX_QUANT + 2];
while(Num > 0){
	int CurLen;
	CurLen = Num;
	if(CurLen > READ_MAX_QUANT) CurLen = READ_MAX_QUANT;
	
	for(;;){
		Req.Cmd = 'R';
		Req.Terminator = 0;
		Req.Address	= Address;
		Req.Param2	= CurLen - 1;
		Len = RigolExchange(Sock, &Req, AnswerBuf, sizeof(AnswerBuf), timeoutIO);
Repp:
		if(Len == (CurLen + 2) * 4) break;
		if(RepeatCounter ++ > 5){
			if(!SilentFlg) printf("Fatal error on read %ld dwords from %08.8X, %d\n", 
					Req.Param2 + 1 , Address, Len);
			return(-1);
		}
		RepPktCntr++;
		RepAddress = Address;
	}
	// Check For Duplicate
	if((RepPktCntr > 0) && (RepAddress != Address)){
		if(	(AnswerBuf[0] == RepAddress) ||
			(AnswerBuf[1] == CurLen - 1)){
			// Старый повторный пакет - выкинем и запустим новое чтение
			RepPktCntr --;
			Len = NetSelAndRecv(Sock, (void*)AnswerBuf, sizeof(AnswerBuf), 100);
			goto Repp;
		}
	}
	if(	(AnswerBuf[0] != Address) ||
		(AnswerBuf[1] != CurLen - 1)){
		if(RepeatCounter ++ > 5){
			if(!SilentFlg) printf("Fatal, bad answer packet on read %d dwords from %08.8X (%d@%X)\n", 
			CurLen, Address,
			AnswerBuf[1] + 1,
			AnswerBuf[0]
			);
			return(-1);
		}
		// repeat request
		continue;
	}
	// Ушли за повторный пакет - сбросим счетчик
	if(Address > RepAddress) RepPktCntr = 0;

	Num -= CurLen;
	Address += CurLen * 4;
	memcpy(AnswerBuffer, AnswerBuf + 2, CurLen * 4);
	AnswerBuffer = ((unsigned char *)AnswerBuffer) + (CurLen * 4);
	RepeatCounter = 0;
}
return(0);
}

int RigolCallFunc(SOCKET Sock, unsigned long Address, unsigned long Param1, unsigned short Param2, unsigned long *Result)
{
int Len;
RigolReqBlock Req;
Req.Cmd = 'C';
Req.Terminator = 0;
Req.Address	= Address;
Req.Param1	= Param1;
Req.Param2	= Param2;
Len = RigolExchange(Sock, &Req, &Req, sizeof(Req), timeoutRun);
if(Len != sizeof(Req)){
	if(!SilentFlg) printf("Error on call %08.8X(%08.8X,%08.8X) =  %d\n", Address, Param1, Param2, Len);
	return(-1);
}
if((Req.Address != Address) ||
   (Req.Cmd != 'C')){
	if(!SilentFlg) printf("Bad answer packet on call %08.8X\n", Address);
	return(-2);
}
if(NULL != Result) *Result = Req.Param1;
return(1);
}


static void HelpOut(void)
{
	printf(
		"Usage: rigolif mode [keys]\n"
		"mode: \n"
		" v[iew]  - show scope ID\n"
		" r[read] - read memory from scope\n"
		" w[rite] - write data to scope memory\n"
		" c[all]  - call scope function and dump result\n"
		" l[oad]  - load plugin and run in scope\n"
		"keys:\n"
		" -i<Address> - set scope IP address\n"
		" -s          - silent mode\n"
		" -a<Address> - setup target address\n"
		" -v<Value>   - setup value to write\n"
		" -p1<Param1> - first function parameter\n"
		" -p2<Param2> - second function parameter\n"
		" -f<File>    - File for read/write/run\n"
		" -l<Len>	  - Length in dwords\n"
		);
}



static unsigned long GetValue(char *Text)
{
char *EndTxt=NULL;
unsigned long Val;
Val = strtoul(Text, &EndTxt, 0);
if((EndTxt == NULL) || (EndTxt[0] !=0)){
	if(!SilentFlg) printf("Bad value: [%s]\n", Text);
	HelpOut();
	exit(23);
}
return(Val);
}

static int LoadFile2Scope(SOCKET Sock, FILE *Src, unsigned long Address, unsigned long Length)
{
while(Length > 0){
	int Len;
	unsigned long Val = 0;
	fread(&Val, 1, 4, Src);
	if(0 >= (Len = RigolWriteDword(Sock, Address, Val))){
		if(!SilentFlg) printf("Write error at address %08.8lX\n", Address);
		return(-1);
	}
	Address +=4;
	Length -=4;
}
return(1);
}

static unsigned long GetFSize(FILE *f)
{
long CurPos;
long TailPos;
CurPos = ftell(f);
fseek(f, 0, SEEK_END);
TailPos = ftell(f);
fseek(f, CurPos, SEEK_SET);
if(TailPos < 0) return(0);
return(TailPos);
}


int main(char argc, char **argv)
{
int RetCode = 0;
SOCKET Sock;
struct sockaddr_in SAddr;
struct hostent * he = NULL;
unsigned char Answer[READ_MAX_QUANT * 4];
int Len;
BOOL LongOperation = FALSE;
signed long ItemsCounter;
if(argc < 2){
	HelpOut();
	return(1);
}
{
WORD wVersionRequested;
WSADATA wsaData;
int err;
wVersionRequested = MAKEWORD( 2, 2 );
 
err = WSAStartup( wVersionRequested, &wsaData );
if ( err != 0 ) {
	if(!SilentFlg) printf("Winsock init err %d\n", err);
    return 1;
}
}
	memset(&SAddr, 0, sizeof(SAddr));

// parse keys
{
int CurKey;
switch(argv[1][0]){
case 'v':
case 'V':	Wm = Work_view;
			break;
case 'r':
case 'R':	Wm = Work_read;
			break;
case 'w':
case 'W':	Wm = Work_write;
			break;
case 'c':
case 'C':	Wm = Work_call;
			break;
case 'l':
case 'L':	Wm = Work_load;
			break;
default:
		if(!SilentFlg) printf("Unknown work mode [%s]\n", argv[1]);
		HelpOut();
		return(1);
}
for(CurKey=2;CurKey < argc;CurKey++){
	char *Key;
	Key = argv[CurKey];
	switch(Key[0]){
	case '-':
	case '/': break;
	default:
		if(!SilentFlg) printf("Bad key [%s]\n", Key);
		HelpOut();
		return(1);
	}
	switch(Key[1]){
	//	" -i<Address> - set scope IP address\n"
	case 'i':
	case 'I':	he = gethostbyname(Key + 2);
				if(NULL == he){
					printf("Unable to get IP address for [%s]\n", Key + 2);
					HelpOut();
					return(1);
				}
				break;
	//		" -s          - silent mode\n"
	case 's':
	case 'S':	SilentFlg = TRUE;
				break;
	//" -a<Address> - setup target address\n"
	case 'a':
	case 'A':	Address = GetValue(Key + 2);
				break;

//		" -v<Value>   - setup value to write\n"
	case 'v':
	case 'V':	if(NULL != Filename){
					printf("File-mode already selected\n");
					HelpOut();
					return(1);
				}
				Value = GetValue(Key + 2);
				break;

//		" -p1<Param1> - first function parameter\n"
//		" -p2<Param2> - second function parameter\n"
	case 'p':
	case 'P':
			switch(Key[2]){
			case '1': Param1 = GetValue(Key + 3); break;
			case '2': Param2 = GetValue(Key + 3); break;
			default: 
				printf("Bad parameter key [%s]\n", Key);
				HelpOut();
				return(1);
			}
			break;

//	" -f<File>    - File for read/write/run\n"
	case 'F':
	case 'f':	Filename = Key + 2;	
				break;
//	" -l<Len>	  - Length in dwords\n"
	case 'L':
	case 'l':	Length = GetValue(Key + 2);
				break;

	default: 
				printf("Unknown key [%s]\n", Key);
				HelpOut();
				return(1);
	}
}
}
	// check params
	switch(Wm){
	case Work_view:		break;

	case Work_read:		if((Address & 3) || (Length < 1)){
								printf("Bad read parametrs\n");
								HelpOut();
								return(1);
						}
						if(NULL != Filename){
							if(NULL == (fIO = fopen(Filename,"wb"))){
								printf("Unable to open file [%s]\n", Filename);
								HelpOut();
								return(1);
							}
						}
						break;

	case Work_call:		if((Address & 3)){
								printf("Bad call parametrs\n");
								HelpOut();
								return(1);
						}
						if(NULL != Filename){
							if(NULL == (fIO = fopen(Filename,"wb"))){
								printf("Unable to open file [%s]\n", Filename);
								HelpOut();
								return(1);
							}
						}
						break;

	case Work_load:		if(NULL == Filename){
								printf("No plugin name\n");
								HelpOut();
								return(1);
						}
						if(NULL == (fIO = fopen(Filename,"rb"))){
							printf("Unable to open plugin file [%s]\n", Filename);
							HelpOut();
							return(1);
						}
						break;

	case Work_write:	if((Address & 3)){
								printf("Bad write address\n");
								HelpOut();
								return(1);
						}
						if(NULL != Filename){
							if(NULL == (fIO = fopen(Filename,"rb"))){
								printf("Unable to open binary file [%s]\n", Filename);
								HelpOut();
								return(1);
							}
						}
						break;
	}
	if(NULL == he){
		if(FindRigol((struct sockaddr *)&SAddr)){
			printf("Unable to autodetect IP-address\r\n");
			return(1);
		}

	}
	Sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(Sock == INVALID_SOCKET ){
		printf("Unable to create socket, errror %d\r\n", WSAGetLastError());
		return(1);
	}
	SAddr.sin_port = htons(6000u);
	SAddr.sin_family = AF_INET;
	if(NULL != he){
		memcpy(&SAddr.sin_addr, he->h_addr_list[0], sizeof(SAddr.sin_addr));
	}
    if(SOCKET_ERROR == connect(Sock, (struct sockaddr *)&SAddr, 16)){
		printf("connect error %d\r\n", WSAGetLastError());
		return(3);
    }
	switch(Wm){
	case Work_view:
			if(0 < (Len = RigolGetIdent(Sock, Answer, sizeof(Answer)))){
				printf("Device ID: [%s]\n", Answer);
			}
			else {
				if(!SilentFlg) printf("Io Error, %d\n", Len);
				RetCode = 10;
			}
			break;

	case Work_read:
					if(Length > 100000){
						LongOperation = TRUE;
//						if(!SilentFlg) printf("\n", Address);
					}
					ItemsCounter = 0;
					while(Length > 0){
						unsigned int CurLen;
						CurLen = sizeof(Answer) / 4;
						if(CurLen > Length)CurLen = Length;
						if((ItemsCounter <=0) && LongOperation){
							ItemsCounter = 100000;
							if(!SilentFlg) printf("\r%08.8X ", Address);
						}
						if(0 > (Len = RigolReadDword(Sock, Address, CurLen, Answer))){
							if(!SilentFlg) printf("Io Error, %d\n", Len);
							RetCode = 10;
							break;
						}
						if(NULL != fIO){
							fwrite(Answer, 1, CurLen * 4, fIO);
						}
						else{
							// out to stdout - to doooooooooo.....


						}
						ItemsCounter -=CurLen;
						Address += CurLen*4;
						Length -= CurLen;
					}
					break;

	case Work_call:		
					if(0 < (Len = RigolCallFunc(Sock, Address, Param1, (unsigned short)Param2, (void*)&Answer))){
						if(NULL != fIO) fwrite(Answer, 1, 4, fIO);
						else printf("Result=0x%08.8X\n", *(unsigned long *)Answer);
					}
					else {
						if(!SilentFlg) printf("Io Error, %d\n", Len);
						RetCode = 10;
					}
					break;
	//
	case Work_load:	if(0>= (Length = GetFSize(fIO))){
						if(!SilentFlg) printf("Bad plugin size\n");
						RetCode = 10;
						break;
					}
					// 
					Length = (Length + 3) & (~3);
					// Alloc Memory 
					Address = 0;
					if(0 >= (Len = RigolCallFunc(Sock, 0x40026F84, Length, 0, &Address))){
						if(!SilentFlg) printf("unable to call malloc, %d\n", Len);
						RetCode = 10;
						break;
					}
					if(0 == Address){
						if(!SilentFlg) printf("unable to alloc %d bytes\n", Length);
						RetCode = 20;
						break;
					}
					else if(!SilentFlg)	printf("Allocate %u bytes at %08.8X\n", Length, Address);
					// Load To Address
					if(0 >= (Len= LoadFile2Scope(Sock, fIO, Address, Length))){
						if(!SilentFlg) printf("Load File Error, %d\n", Len);
						RetCode = 20;
						break;
					}
					// Call Plugin
					if(0 >= (Len = RigolCallFunc(Sock, Address, 0,0, &Address))){
						if(!SilentFlg) printf("Answer Error, %d\n", Len);
						RetCode = 10;
						break;
					}
					break;

	case Work_write:if(NULL != fIO){
						if(0>= (Length = GetFSize(fIO))){
							if(!SilentFlg) printf("Bad image size\n");
							RetCode = 10;
							break;
						}
						Len = LoadFile2Scope(Sock, fIO, Address, Length);
					}
					else Len = RigolWriteDword(Sock, Address, Value);
					if(0 >= Len){
						if(!SilentFlg) printf("Write data Error, %d\n", Len);
						RetCode = 20;
						break;
					}
					break;
	}
	if(NULL != fIO) fclose(fIO);
	closesocket(Sock);
	return(RetCode);
}
