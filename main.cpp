#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <lv2/process.h>
#include <psl1ght/lv2/thread.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/events.h>

#include <sys/stat.h>
#include <sysutil/events.h>
#include <io/msg.h>
#include <io/pad.h> 
#include <io/mouse.h> 
#include <sysmodule/sysmodule.h>

#include <tiny3d.h>
#include <pngdec\loadpng.h>
#include <io/pad.h> 
#include <sys\time.h>

#include "types.h"
#include "oskdialog.h"
#include "graphics.h"
#include "font.h"
#include "HTTP.h"
#include "packet.h"


// ----- DEFINES ----- //
#define N_IMAGES                9       
#define COOLDOWN_TIME			200

// ----- STRUCTS ----- //
typedef struct PAD_BLOCK{
        u8 UP_BLOCKED;
        u8 DOWN_BLOCKED;
        u8 LEFT_BLOCKED;
        u8 RIGHT_BLOCKED;
        
        struct timeval UP_COOLDOWN;
        struct timeval DOWN_COOLDOWN;
        struct timeval LEFT_COOLDOWN;
        struct timeval RIGHT_COOLDOWN;
}PAD_BLOCK;

enum userAction{
        ACTION_MENU = 0,
        ACTION_TROPHY_LOAD,
        ACTION_TROHPY,
}uAction = ACTION_MENU;



// ----- VARIABLES ----- //
static char hdd_folder_installed[64] = "RNA000001"; // folder for program

//Tiny3D Resources
u32 *texture_mem;
u32 * texture_pointer;
PngDatas IMAGES[N_IMAGES]; // PNG container of texture3
u32 IMAGES_offset[N_IMAGES] = {0}; // offset for texture3 (used to pass the texture)

//SDK modules
u32 module_flag;

//Store Status
volatile u8 running = 1;
u32 menu_sel = 0;
u8 fade = 0xFF;
u32 fade_anim = 0;
PAD_BLOCK pad_block = {0};

struct timeval menu_fade1 = {0};
struct timeval menu_fade2 = {0};


//Inner vars
OSK *cOSK = NULL;
char *username = NULL;
char *md5_pass = NULL;
char *temp_md5_pass = NULL;
s8 login_process = -1;

//Packet referent
CSocket *csocket = NULL;
int sock = NULL;


// ----- PROTOTYPES ----- //
void Debug( char *format, ... );
void drawScene( void );
void releaseAll( void );
int loadModules( void );
void customLoadPNG( void );
void loadTexture( void );
void drawScene( void );
static void sys_callback(uint64_t status, uint64_t param, void* userdata);
void login_result(unsigned char result);

// ----- CODE ----- //

void Debug(char *format, ...){
    char str[1024];
    char filename[1024];

    va_list argp;        
    va_start(argp, format);
    vsprintf(str, format, argp);
    va_end(argp);

    FILE *fDebug;
    sprintf(filename, "/dev_hdd0/game/%s/USRDIR/log.txt",hdd_folder_installed);
    fDebug = fopen(filename, "a+");

    fputs(str, fDebug);

    fclose(fDebug);
}


void releaseAll() {
	
	sysUnregisterCallback(EVENT_SLOT0);
	//sys_ppu_thread_join(thread1_id, &retval);
	
	delete csocket;

	if(module_flag & 2)
		SysUnloadModule(SYSMODULE_PNGDEC);

	if(module_flag & 1)
		SysUnloadModule(SYSMODULE_FS);

	running = 0;
}

int loadModules( void ){
	if(SysLoadModule(SYSMODULE_FS)!=0) return 0; else module_flag |=1;

	if(SysLoadModule(SYSMODULE_PNGDEC)!=0) return 0; else module_flag |=2;

	return 1;
}

static void sys_callback(uint64_t status, uint64_t param, void* userdata) {

     switch (status) {
		case EVENT_REQUEST_EXITAPP:
			releaseAll();
			sysProcessExit(1);
			break;
      
       default:
		   break;
         
	}
}

extern void login_result(unsigned char result){
	login_process = result;
	if(result == 2){
		//CCCC
		login_process = 0;
	}
}

void drawScene(){
        
    tiny3d_Project2D();
        
    DrawTexture2d(0, 0, 741.5583, 511.0518, IMAGES_offset[0], IMAGES[0]); //MASK 1
    DrawTexture2d(0, 0, 848, 512, IMAGES_offset[1], IMAGES[1]); //MASK 2

    //MENU FADE (FLECHAS HORIZONTALES)
    if(fade_anim > 0){
        gettimeofday(&menu_fade2, NULL);

        u32 d1 = (menu_fade1.tv_sec * 1000) + (menu_fade1.tv_usec / 1000);
        u32 d2 = (menu_fade2.tv_sec * 1000) + (menu_fade2.tv_usec / 1000);

        u32 at = ((d2 > d1)?d2-d1:d1-d2);
                
        if(at > 0){             
            if(fade_anim == 1){
                fade = 0xFF - (at * 0xFF / 1000); 
                if(fade == 0 || (255 - (at * 255 / 1000)) <= 0 ){
                    gettimeofday(&menu_fade1, NULL);
                    fade_anim = 2;
                }
            }else if(fade_anim == 2){
                fade = (at * 0xFF / 1000);
                if(fade == 0xFF || (at * 255 / 1000) >= 255)
                    fade_anim = 0;
            }
        }
    }
        
    //MENU DRAW
    SetCurrentFont(1);
    SetFontColor(0xFFFFFF00 + fade, 0x0);
    SetFontSize(22, 28);

    float x = 625.4, y = 294.4;
    for(int i = 0; i < 4; i++){
        if(menu_sel == i)
            DrawTexture2d(x, y, 34.45, 41.2444, IMAGES_offset[3], IMAGES[3]);
        else
            DrawTexture2d(x, y, 34.45, 41.2444, IMAGES_offset[2], IMAGES[2]);

        if(i == 0)
            DrawString(x + 40, y + 41.2444/2 - 28/2 - 5, "AMIGOS");
        else if(i == 1)
            DrawString(x + 40, y + 41.2444/2 - 28/2 - 5, "TROFEOS");
        else if(i == 2)
            DrawString(x + 40, y + 41.2444/2 - 28/2 - 5, "SAVES");
        else if(i == 3)
            DrawString(x + 40, y + 41.2444/2 - 28/2 - 5, "NETWORK");

        y += 38.8740;
    }

    x = 632.9083;
    y = 215.9703;
        
	SetFontSize(32, 28);
    SetFontColor(0xFFFFFFFF, 0x0);  
    DrawString(x, y, "Bienvenido,\n");

    SetFontColor(0xFFFD00FF, 0x0);
    DrawString(632.9083 + 30, GetFontY(), username);


    switch(uAction){
        case ACTION_TROPHY_LOAD:{
			/*
            sys_ppu_thread_t id;
            u64 priority = 1500;
            size_t stack_size = 0x1000;
            
			sys_ppu_thread_create(  &id, trophie_sync, (u64)NULL, priority, stack_size, THREAD_JOINABLE, "TROPHY_SYNC");

            msgType mdialogprogress = MSGDIALOG_SINGLE_PROGRESSBAR;
    
            msgDialogOpen2(mdialogprogress, "Sincronizando trofeos", my_dialog, (void *) 0x33330001, NULL);
            msgDialogProgressBarMessage(PROGRESSBAR_INDEX0, "Espere");
            msgDialogResetProgressBar(PROGRESSBAR_INDEX0);
   
            dialog_action = 0;
            while(!dialog_action){
                    sysCheckCallback();
                    tiny3d_Flip();
            }

            msgDialogClose();
			*/
        }break;
        default:
            break;
    }
}

void customLoadPNG(){

    for(int i = 0; i < N_IMAGES; i++){
        char filename[1024] = {0};

        if(i==0) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/MASK1.PNG",hdd_folder_installed);
        if(i==1) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/MASK2.PNG",hdd_folder_installed);
        if(i==2) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/MENU.PNG",hdd_folder_installed);
        if(i==3) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/MENUSEL.PNG",hdd_folder_installed);
        if(i==4) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/LOGO.PNG",hdd_folder_installed);
        if(i==5) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/CRUCETA.PNG",hdd_folder_installed);
        if(i==6) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/X.PNG",hdd_folder_installed);
        if(i==7) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/O.PNG",hdd_folder_installed);
        if(i==8) sprintf(filename, "/dev_hdd0/game/%s/USRDIR/SQ.PNG",hdd_folder_installed);
                        
        memset(&IMAGES[i], 0, sizeof(PngDatas));

        LoadPNG(&(IMAGES[i]), filename);
    }
}

void loadTexture()
{
    texture_mem = (u32*)tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)    

    if(!texture_mem) return; // fail!

    texture_pointer = texture_mem;
   
    customLoadPNG();
        
    for(int i = 0; i < N_IMAGES; i++){
        IMAGES_offset[i] = 0;
        if(IMAGES[i].bmp_out){
            memcpy(texture_pointer, IMAGES[i].bmp_out, IMAGES[i].wpitch * IMAGES[i].height);
            
            free(IMAGES[i].bmp_out);
            IMAGES[i].bmp_out = texture_pointer;
            texture_pointer += (IMAGES[i].wpitch/4 * IMAGES[i].height + 3) & ~3; 
            IMAGES_offset[i] = tiny3d_TextureOffset(IMAGES[i].bmp_out);      // get the offset (RSX use offset instead address)
        }
    }
	        
	cOSK = new OSK(texture_pointer, "/dev_hdd0/game/RNA000001/USRDIR");
	texture_pointer = cOSK->getTexturePointer();

	ResetFont();
    char filename[1024] = {0};

    sprintf(filename, "/dev_hdd0/game/%s/USRDIR/Base02.ttf",hdd_folder_installed);
    TTFLoadFont(filename, (void*)NULL, 0);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 32, 32, TTF_toBitmap);
    TTFUnloadFont();    
                
    sprintf(filename, "/dev_hdd0/game/%s/USRDIR/Decibel_2.ttf",hdd_folder_installed);
    TTFLoadFont(filename, (void*)NULL, 0);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 32, 32, TTF_toBitmap);
    TTFUnloadFont();    
	
	texture_pointer = cOSK->loadFont(2, "/dev_hdd0/game/RNA000001/USRDIR", texture_pointer);	
}

void login_thread(u64 arg){
	temp_md5_pass = send_login(username, md5_pass);

	sys_ppu_thread_exit(0);
}

int main(int argc, const char* argv[], const char* envp[])
{
	tiny3d_Init(1024*1024);
	tiny3d_Project2D();

	AdjustViewport( -70.0f, -50.0f );

	loadModules();
		
	ioPadInit(7);
	
    loadTexture();
	
	sysRegisterCallback(EVENT_SLOT0, sys_callback, NULL);

	csocket = new CSocket();
	sock = csocket->remoteConnect("85.50.221.189", 80);
	
	osk_point point = {250, 100};
	cOSK->setPos(point);

login:

	for(int i = 0; i < 2; i++){
		cOSK->open();

		while(cOSK->getStatus() != OSK_RETURN){
			tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

			// Enable alpha Test
			tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

			// Enable alpha blending.
			tiny3d_BlendFunc(1, (blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
				(blend_dst_func)(NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO),
				(blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));

			tiny3d_Project2D();

			cOSK->draw();
			cOSK->handlePad();

			SetCurrentFont(2);
			SetFontSize(18, 21);
			SetFontColor(0xFFFFFFFF, 0x0);
			
			if(i == 0)
				DrawString(250, 70, "Input username...");
			else if(i == 1)
				DrawString(250, 70, "Input password...");

			tiny3d_Flip();
			sysCheckCallback();
		}

		if(i == 0){
			username = cOSK->getBuffer();
			usleep(500000);
		}else if(i == 1)
			md5_pass = cOSK->getBuffer();
	}

	login_process = -1;

	sys_ppu_thread_t login_thread_id;
	sys_ppu_thread_create(&login_thread_id, login_thread, (u64)NULL, 1500, 0x3000, THREAD_JOINABLE, "LOGIN");

	while(login_process == -1){
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		// Enable alpha Test
		tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
		// Enable alpha blending.
		tiny3d_BlendFunc(1, (blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
			(blend_dst_func)(NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO),
			(blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));

		tiny3d_Project2D();
		
		SetFontSize(32, 38);
		DrawString(250, 80, "Wait...");

		tiny3d_Flip();
		sysCheckCallback();
	}
	
	free(md5_pass);

	if(login_process == 0 || login_process == -1){
		free(username);
		goto login;
	}else
		md5_pass = temp_md5_pass;
	

	/*** - MAIN LOOP - ***/
	while( running ){
		//CLEAR = BG
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

        // Enable alpha Test
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

        // Enable alpha blending.
        tiny3d_BlendFunc(1, (blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
            (blend_dst_func)(NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO),
            (blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));

		
		struct timeval tnow;
        gettimeofday(&tnow, NULL);
        long cur_time = (tnow.tv_sec * 1000 + tnow.tv_usec/1000);

        //EVITAR USO CONTINUO DEL PAD, BLOQUEAR DURANTE 500 MILISEC
        //RUTINA DE DESBLOQUEO:
        if(pad_block.UP_BLOCKED){
            if( (cur_time - (pad_block.UP_COOLDOWN.tv_sec * 1000 + pad_block.UP_COOLDOWN.tv_usec/1000) >= COOLDOWN_TIME) ||
                    ((pad_block.UP_COOLDOWN.tv_sec * 1000 + pad_block.UP_COOLDOWN.tv_usec/1000) - cur_time >= COOLDOWN_TIME))
				pad_block.UP_BLOCKED = false;
        }
        if(pad_block.DOWN_BLOCKED){
            if( (cur_time - (pad_block.DOWN_COOLDOWN.tv_sec * 1000 + pad_block.DOWN_COOLDOWN.tv_usec/1000) >= COOLDOWN_TIME) ||
                    ((pad_block.DOWN_COOLDOWN.tv_sec * 1000 + pad_block.DOWN_COOLDOWN.tv_usec/1000) - cur_time >= COOLDOWN_TIME))
                pad_block.DOWN_BLOCKED = false;
        }
        if(pad_block.LEFT_BLOCKED){
            if( (cur_time - (pad_block.LEFT_COOLDOWN.tv_sec * 1000 + pad_block.LEFT_COOLDOWN.tv_usec/1000) >= COOLDOWN_TIME) ||
                    ((pad_block.LEFT_COOLDOWN.tv_sec * 1000 + pad_block.LEFT_COOLDOWN.tv_usec/1000) - cur_time >= COOLDOWN_TIME))
                 pad_block.LEFT_BLOCKED = false;
        }
        if(pad_block.RIGHT_BLOCKED){
            if( (cur_time - (pad_block.RIGHT_COOLDOWN.tv_sec * 1000 + pad_block.RIGHT_COOLDOWN.tv_usec/1000) >= COOLDOWN_TIME) ||
                    ((pad_block.RIGHT_COOLDOWN.tv_sec * 1000 + pad_block.RIGHT_COOLDOWN.tv_usec/1000) - cur_time >= COOLDOWN_TIME))
                pad_block.RIGHT_BLOCKED = false;
        }

		drawScene();
                                
        PadInfo padinfo;
        PadData paddata;
        ioPadGetInfo(&padinfo);

        for(int i = 0; i < MAX_PADS; i++){
            if(padinfo.status[i]){
                ioPadGetData(i, &paddata);
                                
                if(paddata.BTN_DOWN && !pad_block.DOWN_BLOCKED){
                    if(menu_sel < 3)
                        menu_sel++;

                    pad_block.DOWN_BLOCKED = 1;
                    gettimeofday(&pad_block.DOWN_COOLDOWN, NULL);
                }
                if(paddata.BTN_UP && !pad_block.UP_BLOCKED){
                    if(menu_sel > 0)
                        menu_sel--;

                    pad_block.UP_BLOCKED = 1;
                    gettimeofday(&pad_block.UP_COOLDOWN, NULL);
                }

                if(paddata.BTN_LEFT && !pad_block.LEFT_BLOCKED){
                    fade_anim = 1;
                    gettimeofday(&menu_fade1, NULL);

                    pad_block.LEFT_BLOCKED = 1;
                    gettimeofday(&pad_block.LEFT_COOLDOWN, NULL);
                }
                if(paddata.BTN_RIGHT && !pad_block.RIGHT_BLOCKED){
                    fade_anim = 1;
                    gettimeofday(&menu_fade1, NULL);

                    pad_block.RIGHT_BLOCKED = 1;
                    gettimeofday(&pad_block.RIGHT_COOLDOWN, NULL);
                }
            }
        }
				
        tiny3d_Flip();
		sysCheckCallback();

	}
	
	return 0;

}
