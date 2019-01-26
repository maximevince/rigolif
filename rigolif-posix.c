//#define _CRT_SECURE_NO_WARNINGS
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netdb.h>
//#include <winsock2.h>

//#pragma comment(lib,"Ws2_32.lib")

enum workmode {
    WORK_VIEW,
    WORK_READ,
    WORK_CALL,
    WORK_LOAD,
    WORK_WRITE
};

static enum workmode Wm;
static bool silent_flag = false;
static uint32_t address = 1;
static uint32_t length = 0;
static uint32_t value = 0;
static uint32_t param1 = 0;
static uint32_t param2 = 0;
static char *filename = NULL;
static FILE *fIO = NULL;

static const int timeout_io = 200;
static const int timeout_run = 2000;

#define READ_MAX_QUANT (50)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)

#pragma pack(push,1)
// 12 bytes
typedef struct {
    /*
       ? - ask id (for compatible rigol)
       C - call function (*address)(param1, param2)
       R - Read Memory at address (max 3 uint32_t, size in param1)
       W - Write Memory at address (source in param1)
       Warning! uint32_t align !!!
     */
    uint8_t Cmd;
    uint8_t Terminator;
    uint16_t param2;
    uint32_t address;
    uint32_t param1;
} __attribute__((packed)) RigolReqBlock;
#pragma pack(pop)

//#if sizeof(RigolReqBlock) != 0xC
//#error Bad packet Size
//#endif

#define NUM_WAIT_REP    (3)

static int closesocket(int s)
{
    return close(s);
}

static int NetSelAndRecv(int s, char *buf, int len, unsigned int Timeout)
{
    fd_set readfds;
    struct timeval timeout;
    int RetVal;

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    //readfds.fd_array[0] = s;
    //readfds.fd_count = 1;

    timeout.tv_sec = Timeout / 1000;
    timeout.tv_usec = 1000 * (Timeout % 1000);
    if (1 != (RetVal = select(s + 1, &readfds, NULL, NULL, &timeout))) {
        return(0);
    }
    return(recv(s, buf, len, 0));
}

static int NetSelAndRecvfrom(int s, uint8_t *buf, int len, unsigned int Timeout, struct sockaddr *from, int *fromlen)
{
    fd_set readfds;
    struct timeval timeout;
    int RetVal;

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    //readfds.fd_array[0] = s;
    //readfds.fd_count = 1;

    timeout.tv_sec = Timeout / 1000;
    timeout.tv_usec = 1000 * (Timeout % 1000);
    if (1 != (RetVal = select(s + 1, &readfds, NULL, NULL, &timeout))) return(RetVal);
    return(recvfrom(s, buf, len, 0, from, (socklen_t *)fromlen));
}


// Получить список всех
static int FindRigol(struct sockaddr *RigolAddr)
{
    int SCK;
    uint32_t optval;
    struct sockaddr SA;
    int len;
    uint8_t Answ[1024];
    int Fromlen;
    int no_answer;
    static const char AskCmd[] = "?";

    SCK = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (INVALID_SOCKET == SCK) {
        return(-1);
    }
    optval = 1;
    setsockopt(SCK, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof(optval));
    // Посылка запроса на отклик
    memset(&SA, 0, sizeof(SA));
    // Шириковещательно - 255.255.255.255
    *(uint32_t *)&SA.sa_data[2] = -1;
    // Целевой порт
    *(uint16_t *)&SA.sa_data[0] = htons(6000u);
    SA.sa_family = AF_INET;
    // пошлем ?
    len = sendto(SCK, AskCmd, sizeof(AskCmd), 0, &SA, sizeof(SA));
    if (len != sizeof(AskCmd)) {
        closesocket(SCK);
        return(-1);
    }
    no_answer = 5;
    do {
        // Принимаем ответы откликнувшихся плат
        Fromlen = sizeof(SA);
        len = NetSelAndRecvfrom(SCK, Answ, sizeof(Answ), 100u, RigolAddr, &Fromlen);
        if (len > 0) {
            Answ[len] = 0;
            if (!silent_flag) printf("Autodetect answer: [%s] from %u.%u.%u.%u\n",
                                     Answ,
                                     (unsigned char)RigolAddr->sa_data[2],
                                     (unsigned char)RigolAddr->sa_data[3],
                                     (unsigned char)RigolAddr->sa_data[4],
                                     (unsigned char)RigolAddr->sa_data[5]
                                     );
            closesocket(SCK);
            return(0);
        }
        no_answer--;
    } while (no_answer > 0);
    closesocket(SCK);
    if (!silent_flag) printf("No Rigol detected\n");
    return(-2);
}


// <=0 - error
int RigolExchange(int Sock, RigolReqBlock *SrcBuf, void *Dst, size_t Maxlen, int Timeout)
{
    int RealSize;
    // Зашлем управляющий блок
    RealSize = send(Sock, (void *) SrcBuf, sizeof(RigolReqBlock), 0);
    if ( RealSize != sizeof(RigolReqBlock) ) {
        if (!silent_flag) printf("TX Error (%d/%d), code =%d\n", RealSize, (int)sizeof(RigolReqBlock), errno);
        return(-1);
    }
    // Проверим ответ с таймаутом
    return(NetSelAndRecv(Sock, Dst, Maxlen, Timeout));
}


int RigolGetIdent(int Sock, uint8_t *TextBuffer, int Maxlen)
{
    int len;
    RigolReqBlock Req;
    Req.Cmd = '?';
    Req.Terminator = 0;
    len = RigolExchange(Sock, &Req, TextBuffer, Maxlen - 1, timeout_io);
    if (len <= 0) {
        if (!silent_flag) printf("Get Ident Io Error, %d\n", len);
        return(-1);
    }
    if (len > Maxlen - 1) len = Maxlen - 1;
    TextBuffer[len] = 0;
    return(len);
}

int RigolWriteDword(int Sock, uint32_t address, uint32_t value)
{
    int len;
    RigolReqBlock Req;
    Req.Cmd = 'W';
    Req.Terminator = 0;
    Req.address = address;
    Req.param1  = value;
    len = RigolExchange(Sock, &Req, &Req, sizeof(Req), timeout_io);
    if (len != sizeof(Req)) {
        if (!silent_flag) printf("Error on write %08X to %08X, %d\n", value, address, len);
        return(-1);
    }
    if ((Req.address != address) ||
        ((Req.param1 ^ ~value) != 0) ||
        (Req.Cmd != 'W')) {
        if (!silent_flag) printf("Bad answer packet on write %08X to %08X\n", value, address);
        return(-2);
    }
    return(1);
}

static uint32_t Repaddress = 0;
static int RepPktCntr = 0;

// Читать блок памяти
int RigolReadDword(int Sock, uint32_t address, unsigned int Num, void *answerBuffer)
{
    int len;
    int RepeatCounter = 0;
    RigolReqBlock Req;
    uint32_t answerBuf[READ_MAX_QUANT + 2];
    while (Num > 0) {
        int cur_len;
        cur_len = Num;
        if (cur_len > READ_MAX_QUANT) cur_len = READ_MAX_QUANT;

        for (;;) {
            Req.Cmd = 'R';
            Req.Terminator = 0;
            Req.address = address;
            Req.param2  = cur_len - 1;
            len = RigolExchange(Sock, &Req, answerBuf, sizeof(answerBuf), timeout_io);
Repp:
            if (len == (cur_len + 2) * 4) break;
            if (RepeatCounter++ > 5) {
                if (!silent_flag) printf("Fatal error on read %d dwords from %08X, %d\n",
                                         Req.param2 + 1, address, len);
                return(-1);
            }
            RepPktCntr++;
            Repaddress = address;
        }
        // Check For Duplicate
        if ((RepPktCntr > 0) && (Repaddress != address)) {
            if ( (answerBuf[0] == Repaddress) ||
                 (answerBuf[1] == cur_len - 1)) {
                // Старый повторный пакет - выкинем и запустим новое чтение
                RepPktCntr--;
                len = NetSelAndRecv(Sock, (void *)answerBuf, sizeof(answerBuf), 100);
                goto Repp;
            }
        }
        if ( (answerBuf[0] != address) ||
             (answerBuf[1] != cur_len - 1)) {
            if (RepeatCounter++ > 5) {
                if (!silent_flag) printf("Fatal, bad answer packet on read %d dwords from %08X (%d@%X)\n",
                                         cur_len, address,
                                         answerBuf[1] + 1,
                                         answerBuf[0]
                                         );
                return(-1);
            }
            // repeat request
            continue;
        }
        // Ушли за повторный пакет - сбросим счетчик
        if (address > Repaddress) RepPktCntr = 0;

        Num -= cur_len;
        address += cur_len * 4;
        memcpy(answerBuffer, answerBuf + 2, cur_len * 4);
        answerBuffer = ((unsigned char *)answerBuffer) + (cur_len * 4);
        RepeatCounter = 0;
    }
    return(0);
}

int RigolCallFunc(int Sock, uint32_t address, uint32_t param1, uint16_t param2, uint32_t *Result)
{
    int len;
    RigolReqBlock Req;
    Req.Cmd = 'C';
    Req.Terminator = 0;
    Req.address = address;
    Req.param1  = param1;
    Req.param2  = param2;
    len = RigolExchange(Sock, &Req, &Req, sizeof(Req), timeout_run);
    if (len != sizeof(Req)) {
        if (!silent_flag) printf("Error on call %08X(%08X,%08X) =  %d\n", address, param1, param2, len);
        return(-1);
    }
    if ((Req.address != address) ||
        (Req.Cmd != 'C')) {
        if (!silent_flag) printf("Bad answer packet on call %08X\n", address);
        return(-2);
    }
    if (NULL != Result) *Result = Req.param1;
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
        " -i<address> - set scope IP address\n"
        " -s          - silent mode\n"
        " -a<address> - setup target address\n"
        " -v<value>   - setup value to write\n"
        " -p1<param1> - first function parameter\n"
        " -p2<param2> - second function parameter\n"
        " -f<File>    - File for read/write/run\n"
        " -l<len>	  - length in dwords\n"
        );
}



static uint32_t Getvalue(char *Text)
{
    char *EndTxt = NULL;
    uint32_t Val;
    Val = strtoul(Text, &EndTxt, 0);
    if ((EndTxt == NULL) || (EndTxt[0] != 0)) {
        if (!silent_flag) printf("Bad value: [%s]\n", Text);
        HelpOut();
        exit(23);
    }
    return(Val);
}

static int LoadFile2Scope(int Sock, FILE *Src, uint32_t address, uint32_t length)
{
    while (length > 0) {
        int len;
        uint32_t Val = 0;
        fread(&Val, 1, 4, Src);
        if (0 >= (len = RigolWriteDword(Sock, address, Val))) {
            if (!silent_flag) printf("Write error at address %08X\n", address);
            return(-1);
        }
        address += 4;
        length -= 4;
    }
    return(1);
}

static uint32_t GetFSize(FILE *f)
{
    long CurPos;
    long TailPos;
    CurPos = ftell(f);
    fseek(f, 0, SEEK_END);
    TailPos = ftell(f);
    fseek(f, CurPos, SEEK_SET);
    if (TailPos < 0) return(0);
    return(TailPos);
}


int main(int argc, char * *argv)
{
    int retcode = 0;
    int Sock;
    struct sockaddr_in SAddr;
    struct hostent *he = NULL;
    uint8_t answer[READ_MAX_QUANT * 4];
    int len;
    bool long_operation = false;
    signed long items_cnt;

    /* Sanity check */
    if (sizeof(RigolReqBlock) != 0xC) {
        printf("RigolReqBlock should be exactly 12 bytes -- something went wrong compiling the program!\n");
        return -1;
    }

    if (argc < 2) {
        HelpOut();
        return(1);
    }
    memset(&SAddr, 0, sizeof(SAddr));

    // parse keys
    {
        int CurKey;
        switch (argv[1][0])
        {
            case 'v':
            case 'V':
                Wm = WORK_VIEW;
                break;

            case 'r':
            case 'R':
                Wm = WORK_READ;
                break;

            case 'w':
            case 'W':
                Wm = WORK_WRITE;
                break;

            case 'c':
            case 'C':
                Wm = WORK_CALL;
                break;

            case 'l':
            case 'L':
                Wm = WORK_LOAD;
                break;

            default:
                if (!silent_flag) printf("Unknown work mode [%s]\n", argv[1]);
                HelpOut();
                return(1);
        }

        for (CurKey = 2; CurKey < argc; CurKey++) {
            char *Key;
            Key = argv[CurKey];
            switch (Key[0])
            {
                case '-':
                case '/': break;

                default:
                    if (!silent_flag) printf("Bad key [%s]\n", Key);
                    HelpOut();
                    return(1);
            }
            switch (Key[1])
            {
                //	" -i<address> - set scope IP address\n"
                case 'i':
                case 'I':
                    he = gethostbyname(Key + 2);
                    if (NULL == he) {
                        printf("Unable to get IP address for [%s]\n", Key + 2);
                        HelpOut();
                        return(1);
                    }
                    break;

                //		" -s          - silent mode\n"
                case 's':
                case 'S':
                    silent_flag = true;
                    break;

                //" -a<address> - setup target address\n"
                case 'a':
                case 'A':
                    address = Getvalue(Key + 2);
                    break;

                //		" -v<value>   - setup value to write\n"
                case 'v':
                case 'V':
                    if (NULL != filename) {
                        printf("File-mode already selected\n");
                        HelpOut();
                        return(1);
                    }
                    value = Getvalue(Key + 2);
                    break;

                //		" -p1<param1> - first function parameter\n"
                //		" -p2<param2> - second function parameter\n"
                case 'p':
                case 'P':
                    switch (Key[2])
                    {
                        case '1': param1 = Getvalue(Key + 3); break;

                        case '2': param2 = Getvalue(Key + 3); break;

                        default:
                            printf("Bad parameter key [%s]\n", Key);
                            HelpOut();
                            return(1);
                    }
                    break;

                //	" -f<File>    - File for read/write/run\n"
                case 'F':
                case 'f':
                    filename = Key + 2;
                    break;

                //	" -l<len>	  - length in dwords\n"
                case 'L':
                case 'l':
                    length = Getvalue(Key + 2);
                    break;

                default:
                    printf("Unknown key [%s]\n", Key);
                    HelpOut();
                    return(1);
            }
        }
    }
    // check params
    switch (Wm)
    {
        case WORK_VIEW:
            break;

        case WORK_READ:
            if ((address & 3) || (length < 1)) {
                printf("Bad read parametrs\n");
                HelpOut();
                return(1);
            }
            if (NULL != filename) {
                if (NULL == (fIO = fopen(filename, "wb"))) {
                    printf("Unable to open file [%s]\n", filename);
                    HelpOut();
                    return(1);
                }
            }
            break;

        case WORK_CALL:
            if ((address & 3)) {
                printf("Bad call parametrs\n");
                HelpOut();
                return(1);
            }
            if (NULL != filename) {
                if (NULL == (fIO = fopen(filename, "wb"))) {
                    printf("Unable to open file [%s]\n", filename);
                    HelpOut();
                    return(1);
                }
            }
            break;

        case WORK_LOAD:
            if (NULL == filename) {
                printf("No plugin name\n");
                HelpOut();
                return(1);
            }
            if (NULL == (fIO = fopen(filename, "rb"))) {
                printf("Unable to open plugin file [%s]\n", filename);
                HelpOut();
                return(1);
            }
            break;

        case WORK_WRITE:
            if ((address & 3)) {
                printf("Bad write address\n");
                HelpOut();
                return(1);
            }
            if (NULL != filename) {
                if (NULL == (fIO = fopen(filename, "rb"))) {
                    printf("Unable to open binary file [%s]\n", filename);
                    HelpOut();
                    return(1);
                }
            }
            break;
    }

    if (NULL == he) {
        if (FindRigol((struct sockaddr *)&SAddr)) {
            printf("Unable to autodetect IP-address\r\n");
            return(1);
        }
    }

    Sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (Sock == INVALID_SOCKET ) {
        printf("Unable to create socket, errror %d\r\n", errno);
        return(1);
    }
    SAddr.sin_port = htons(6000u);
    SAddr.sin_family = AF_INET;

    if (NULL != he) {
        memcpy(&SAddr.sin_addr, he->h_addr_list[0], sizeof(SAddr.sin_addr));
    }

    if (SOCKET_ERROR == connect(Sock, (struct sockaddr *)&SAddr, 16)) {
        printf("connect error %d\r\n", errno);
        return(3);
    }

    switch (Wm)
    {
        case WORK_VIEW:
            if (0 < (len = RigolGetIdent(Sock, answer, sizeof(answer)))) {
                printf("Device ID: [%s]\n", answer);
            }
            else {
                if (!silent_flag)
                    printf("Io Error, %d\n", len);
                retcode = 10;
            }
            break;

        case WORK_READ:
            if (length > 100000) {
                long_operation = true;
                //if(!silent_flag) printf("\n", address);
            }
            items_cnt = 0;
            while (length > 0) {
                unsigned int cur_len;
                cur_len = sizeof(answer) / 4;
                if (cur_len > length) cur_len = length;
                if ((items_cnt <= 0) && long_operation) {
                    items_cnt = 100000;
                    if (!silent_flag) printf("\r%08X ", address);
                }
                if (0 > (len = RigolReadDword(Sock, address, cur_len, answer))) {
                    if (!silent_flag) printf("Io Error, %d\n", len);
                    retcode = 10;
                    break;
                }
                if (NULL != fIO) {
                    fwrite(answer, 1, cur_len * 4, fIO);
                }
                else{
                    //out to stdout - to doooooooooo.....
                }
                items_cnt -= cur_len;
                address += cur_len * 4;
                length -= cur_len;
            }
            break;

        case WORK_CALL:
            if (0 < (len = RigolCallFunc(Sock, address, param1, (uint16_t)param2, (void *)&answer))) {
                if (NULL != fIO) fwrite(answer, 1, 4, fIO);
                else printf("Result=0x%08X\n", *(uint32_t *)answer);
            }
            else {
                if (!silent_flag) printf("Io Error, %d\n", len);
                retcode = 10;
            }
            break;

        case WORK_LOAD:
            if (0 >= (length = GetFSize(fIO))) {
                if (!silent_flag) printf("Bad plugin size\n");
                retcode = 10;
                break;
            }
            //
            length = (length + 3) & (~3);
            // Alloc Memory
            address = 0;
            if (0 >= (len = RigolCallFunc(Sock, 0x40026F84, length, 0, &address))) {
                if (!silent_flag) printf("unable to call malloc, %d\n", len);
                retcode = 10;
                break;
            }
            if (0 == address) {
                if (!silent_flag) printf("unable to alloc %d bytes\n", length);
                retcode = 20;
                break;
            }
            else if (!silent_flag) printf("Allocate %u bytes at %08X\n", length, address);
            // Load To address
            if (0 >= (len = LoadFile2Scope(Sock, fIO, address, length))) {
                if (!silent_flag)
                    printf("Load File Error, %d\n", len);
                retcode = 20;
                break;
            }
            // Call Plugin
            if (0 >= (len = RigolCallFunc(Sock, address, 0, 0, &address))) {
                if (!silent_flag)
                    printf("answer Error, %d\n", len);
                retcode = 10;
                break;
            }
            break;

        case WORK_WRITE:
            if (NULL != fIO) {
                if (0 >= (length = GetFSize(fIO))) {
                    if (!silent_flag)
                        printf("Bad image size\n");
                    retcode = 10;
                    break;
                }
                len = LoadFile2Scope(Sock, fIO, address, length);
            }
            else len = RigolWriteDword(Sock, address, value);
            if (0 >= len) {
                if (!silent_flag)
                    printf("Write data Error, %d\n", len);
                retcode = 20;
                break;
            }
            break;
    }

    if (NULL != fIO)
        fclose(fIO);
    closesocket(Sock);

    return retcode;
}

