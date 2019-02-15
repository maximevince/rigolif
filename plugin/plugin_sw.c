#include "ds1000zlib.h"

#define c  261
#define d  294
#define e  329
#define f  349
#define g  391
#define gS  415
#define a  440
#define aS  455
#define b  466
#define cH  523
#define cSH  554
#define dH  587
#define dSH 622
#define eH  659
#define fH  698
#define fSH  740
#define gH  784
#define gSH  830
#define aH  881

int i = 0;

void ThreadBody(void)
{
    for(i = 0; i < 2; i++)
    {
        Beeper_Generate(a, 500);
        Beeper_Generate(a, 500);
        Beeper_Generate(a, 500);
        Beeper_Generate(f, 350);
        Beeper_Generate(cH, 150);  
        Beeper_Generate(a, 500);
        Beeper_Generate(f, 350);
        Beeper_Generate(cH, 150);
        Beeper_Generate(a, 650);

        Sleep(250);

        Beeper_Generate(eH, 500);
        Beeper_Generate(eH, 500);
        Beeper_Generate(eH, 500);   
        Beeper_Generate(fH, 350);
        Beeper_Generate(cH, 150);
        Beeper_Generate(gS, 500);
        Beeper_Generate(f, 350);
        Beeper_Generate(cH, 150);
        Beeper_Generate(a, 650);

        Sleep(250);

        Beeper_Generate(aH, 500);
        Beeper_Generate(a, 300);
        Beeper_Generate(a, 150);
        Beeper_Generate(aH, 400);
        Beeper_Generate(gSH, 200);
        Beeper_Generate(gH, 200); 
        Beeper_Generate(fSH, 125);
        Beeper_Generate(fH, 125);    
        Beeper_Generate(fSH, 250);

        Sleep(250);

        Beeper_Generate(aS, 250); 
        Beeper_Generate(dSH, 400); 
        Beeper_Generate(dH, 200);  
        Beeper_Generate(cSH, 200);  
        Beeper_Generate(cH, 125);  
        Beeper_Generate(b, 125);  
        Beeper_Generate(cH, 250);  

        Sleep(250);

        Beeper_Generate(f, 125);  
        Beeper_Generate(gS, 500);  
        Beeper_Generate(f, 375);  
        Beeper_Generate(a, 125);
        Beeper_Generate(cH, 500);
        Beeper_Generate(a, 375);  
        Beeper_Generate(cH, 125);
        Beeper_Generate(eH, 650);

        Beeper_Generate(aH, 500);
        Beeper_Generate(a, 300);
        Beeper_Generate(a, 150);
        Beeper_Generate(aH, 400);
        Beeper_Generate(gSH, 200);
        Beeper_Generate(gH, 200);
        Beeper_Generate(fSH, 125);
        Beeper_Generate(fH, 125);    
        Beeper_Generate(fSH, 250);

        Sleep(250);

        Beeper_Generate(aS, 250);  
        Beeper_Generate(dSH, 400);  
        Beeper_Generate(dH, 200);  
        Beeper_Generate(cSH, 200);  
        Beeper_Generate(cH, 125);  
        Beeper_Generate(b, 125);  
        Beeper_Generate(cH, 250);      

        Sleep(250);

        Beeper_Generate(f, 250);  
        Beeper_Generate(gS, 500);  
        Beeper_Generate(f, 375);  
        Beeper_Generate(cH, 125);
        Beeper_Generate(a, 500);   
        Beeper_Generate(f, 375);   
        Beeper_Generate(cH, 125); 
        Beeper_Generate(a, 650);   

        Sleep(2500);
    }
}

int PluginMain(void)
{
    task_template_struct Descr;
    Descr.TASK_TEMPLATE_INDEX = 0;
    Descr.TASK_ADDRESS = (void *)ThreadBody;
    Descr.TASK_STACKSIZE = 0x400;
    Descr.TASK_PRIORITY = 8;
    Descr.TASK_NAME = "sw";
    Descr.TASK_ATTRIBUTES = 4;
    Descr.CREATION_PARAMETER = 0;
    Descr.DEFAULT_TIME_SLICE = 0;
    task_create(0, 0, &Descr);
    // Simple return, no free memory!
    return(0);
}
