#ifndef __ds1000zstr__
#define __ds1000zstr__

#include <stdint.h>

#define __cdecl

typedef void *FILE;
typedef unsigned long __int32;
#define __int8 char
#define __int16 short
#define __int64 long long
typedef unsigned int size_t;
//#define int64_t unsigned long long
#define NULL ((void*)(0))

typedef struct{
long long quot;
long long rem;
}RemDivI64;

typedef struct{
unsigned long long quot;
unsigned long long rem;
}RemDivU64;

typedef struct{
int quot;
int rem;
}RemDivI32;

typedef struct{
unsigned int quot;
unsigned int rem;
}RemDivU32;

typedef struct{
  int st_dev;
  int st_ino;
  int st_mode;
  int st_nlink;
  int st_uid;
  int st_gid;
  int st_rdev;
  __int32 st_size;
  int st_blksize;
  int st_blocks;
  unsigned int st_atime;
  unsigned int st_mtime;
  unsigned int st_ctime;
} uffs_stat_;



typedef enum
{
  FS_SELECT_USB = 0x1000,
  FS_SELECT_UFFS = 0x2000,
}FS_SELECT;

typedef enum{
  USB_DevType_0 = 0x0,
  USB_DevType_TMC = 0x1,
  USB_DevType_PictBridge = 0x2,
} USB_DevType;



#pragma pack(push, 1)
typedef struct {
  int FileSize;
  char Atribs;
  char Filename[259];
}FindFirstBuffer;


typedef struct {
  __int16 Width;
  __int16 Height;
}BitmapHeader;

typedef struct queue_element_struct_{
struct queue_element_struct_ *NEXT;
struct queue_element_struct_ *PREV;
}queue_element_struct;

typedef struct{
queue_element_struct Next;
__int16 Size;
__int16 Max;
} QUEUE_STRUCT;

typedef struct {
  queue_element_struct Next;
  QUEUE_STRUCT TD_Que;
  int Sign;
  int Value;
}LWSEM_STRUCT;

typedef struct {
  int TASK_TEMPLATE_INDEX;
  void *TASK_ADDRESS;
  int TASK_STACKSIZE;
  int TASK_PRIORITY;
  char *TASK_NAME;
  int TASK_ATTRIBUTES;
  int CREATION_PARAMETER;
  int DEFAULT_TIME_SLICE;
}task_template_struct;

typedef struct{
void *SPSave;
void *StartBlock;
int Port6000Socket;
unsigned long Dummy[4];
}PLG_Def;

#pragma pack(pop)

// Terminate plugin and free (NOT FOR Thread!!!)
void PLG_FreeAndRet(void);
PLG_Def *PLG_GetTable(void);


#endif
