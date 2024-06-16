
/* UDP server */
#include <stdlib.h>
#include <cerrno>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#include <stdio.h>
#include <memory.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>

#include <linux/filter.h>
#include <sys/mman.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <poll.h>

#include <sys/time.h> 
#include <sys/stat.h>
#include <time.h>

#include <unistd.h>
#include <sched.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "../../hardware.h"
#include "../../shmem.h"
#include "../../fpga_io.h"
#include "../../spi.h"

#include "logo.h"
#include "pll.h"

// FPGA SPI commands 
#define UIO_GET_GROOVY_STATUS     0xf0 
#define UIO_GET_GROOVY_HPS        0xf1 
#define UIO_SET_GROOVY_INIT       0xf2
#define UIO_SET_GROOVY_SWITCHRES  0xf3
#define UIO_SET_GROOVY_BLIT       0xf4
#define UIO_SET_GROOVY_LOGO       0xf5
#define UIO_SET_GROOVY_AUDIO      0xf6
#define UIO_SET_GROOVY_BLIT_LZ4   0xf7


// FPGA DDR shared
#define BASEADDR 0x1C000000
#define HEADER_LEN 0xff       
#define CHUNK 7 
#define HEADER_OFFSET HEADER_LEN - CHUNK                        
#define FRAMEBUFFER_SIZE  (720 * 576 * 4 * 2) // RGBA 720x576 with 2 fields
#define AUDIO_SIZE (8192 * 2 * 2)             // 8192 samples with 2 16bit-channels
#define LZ4_SIZE (720 * 576 * 4)              // Estimated LZ4 MAX   
#define FIELD_OFFSET 0x195000                 // 0x12fcff position for fpga (after first field)    
#define AUDIO_OFFSET 0x32a000                 // 0x25f8ff position for fpga (after framebuffer)    
#define LZ4_OFFSET_A 0x332000                 // 0x2678ff position for fpga (after audio)          
#define LZ4_OFFSET_B 0x4c7000                 // 0x3974ff position for fpga (after lz4_offset_A)   
#define BUFFERSIZE FRAMEBUFFER_SIZE + AUDIO_SIZE + LZ4_SIZE + LZ4_SIZE + HEADER_LEN


// UDP server 
#define UDP_PORT 32100
#define UDP_PORT_INPUTS 32101
#define CMD_CLOSE 1
#define CMD_INIT 2
#define CMD_SWITCHRES 3
#define CMD_AUDIO 4
#define CMD_GET_STATUS 5
#define CMD_BLIT_VSYNC 6

// PF_PACKETv3 RX RING
/*
#define BLOCKSIZ (1 << 12)  // 4096 bytes
#define FRAMESIZ (1 << 11)  // 2048 bytes
#define BLOCKNUM 200 
*/

#define LOGO_TIMER 16
#define KEEP_ALIVE_FRAMES 45 * 60

//https://stackoverflow.com/questions/64318331/how-to-print-logs-on-both-console-and-file-in-c-language
#define LOG_TIMER 25
static struct timeval logTS, logTS_ant, blitStart, blitStop; 
static int doVerbose = 0; 
static int difUs = 0;
static unsigned long logTime = 0;
static FILE * fp = NULL;

#define LOG(sev,fmt, ...) do {	\
			        if (sev == 0) printf(fmt, __VA_ARGS__);	\
			        if (sev <= doVerbose) { \
			        	gettimeofday(&logTS, NULL); 	\
			        	difUs = (difUs != 0) ? ((logTS.tv_sec - logTS_ant.tv_sec) * 1000000 + logTS.tv_usec - logTS_ant.tv_usec) : -1;	\
					fprintf(fp, "[%06.3f]",(double) difUs/1000); \
					fprintf(fp, fmt, __VA_ARGS__);	\
                                	gettimeofday(&logTS_ant, NULL); \
                                } \
                           } while (0)

#define NUM_SCANCODES 255
#define ARRAY_BIT_SIZE(x) (x/8+(!!(x%8)))
static const int key2sdl[] =
{
	0,   //0   KEY_RESERVED
	41,  //1   KEY_ESC
	30,  //2   KEY_1
	31,  //3   KEY_2
	32,  //4   KEY_3
	33,  //5   KEY_4
	34,  //6   KEY_5
	35,  //7   KEY_6
	36,  //8   KEY_7
	37,  //9   KEY_8
	38,  //10  KEY_9
	39,  //11  KEY_0
	45,  //12  KEY_MINUS
	46,  //13  KEY_EQUAL
	42,  //14  KEY_BACKSPACE
	43,  //15  KEY_TAB
	20,  //16  KEY_Q
	26,  //17  KEY_W
	8,   //18  KEY_E
	21,  //19  KEY_R
	23,  //20  KEY_T
	28,  //21  KEY_Y
	24,  //22  KEY_U
	12,  //23  KEY_I
	18,  //24  KEY_O
	19,  //25  KEY_P
	184, //26  KEY_LEFTBRACE
	184, //27  KEY_RIGHTBRACE
	40,  //28  KEY_ENTER
	224, //29  KEY_LEFTCTRL
	4,   //30  KEY_A
	22,  //31  KEY_S
	7,   //32  KEY_D
	9,   //33  KEY_F
	10,  //34  KEY_G
	11,  //35  KEY_H
	13,  //36  KEY_J
	14,  //37  KEY_K
	15,  //38  KEY_L
	51,  //39  KEY_SEMICOLON
	52,  //40  KEY_APOSTROPHE
	53,  //41  KEY_GRAVE
	225, //42  KEY_LEFTSHIFT
	49,  //43  KEY_BACKSLASH
	28,  //44  KEY_Z
	27,  //45  KEY_X
	6,   //46  KEY_C
	25,  //47  KEY_V
	5,   //48  KEY_B
	17,  //49  KEY_N
	16,  //50  KEY_M
	54,  //51  KEY_COMMA
	55,  //52  KEY_DOT
	56,  //53  KEY_SLASH
	229, //54  KEY_RIGHTSHIFT
	206, //55  KEY_KPASTERISK
	226, //56  KEY_LEFTALT
	44,  //57  KEY_SPACE
	57,  //58  KEY_CAPSLOCK
	58,  //59  KEY_F1
	59,  //60  KEY_F2
	60,  //61  KEY_F3
	61,  //62  KEY_F4
	62,  //63  KEY_F5
	63,  //64  KEY_F6
	64,  //65  KEY_F7
	65,  //66  KEY_F8
	66,  //67  KEY_F9
	67,  //68  KEY_F10
	83,  //69  KEY_NUMLOCK
	71,  //70  KEY_SCROLLLOCK
	95,  //71  KEY_KP7
	96,  //72  KEY_KP8
	97,  //73  KEY_KP9
	86,  //74  KEY_KPMINUS
	92,  //75  KEY_KP4
	93,  //76  KEY_KP5
	94,  //77  KEY_KP6
	87,  //78  KEY_KPPLUS
	89,  //79  KEY_KP1
	90,  //80  KEY_KP2
	91,  //81  KEY_KP3
	98,  //82  KEY_KP0
	99,  //83  KEY_KPDOT
	0,   //84  ???
	0,   //85  KEY_ZENKAKU
	0,   //86  KEY_102ND
	68,  //87  KEY_F11
	69,  //88  KEY_F12
	0,   //89  KEY_RO
	0,   //90  KEY_KATAKANA
	0,   //91  KEY_HIRAGANA
	0,   //92  KEY_HENKAN
	0,   //93  KEY_KATAKANA
	0,   //94  KEY_MUHENKAN
	0,   //95  KEY_KPJPCOMMA
	88,  //96  KEY_KPENTER
	228, //97  KEY_RIGHTCTRL
	0,   //98  KEY_KPSLASH
	0,   //99  KEY_SYSRQ
	230, //100 KEY_RIGHTALT
	0,   //101 KEY_LINEFEED
	74,  //102 KEY_HOME
	82,  //103 KEY_UP
	75,  //104 KEY_PAGEUP
	80,  //105 KEY_LEFT
	79,  //106 KEY_RIGHT
	77,  //107 KEY_END
	81,  //108 KEY_DOWN
	78,  //109 KEY_PAGEDOWN
	73,  //110 KEY_INSERT
	76,  //111 KEY_DELETE
	0,   //112 KEY_MACRO
	0,   //113 KEY_MUTE
	0,   //114 KEY_VOLUMEDOWN
	0,   //115 KEY_VOLUMEUP
	0,   //116 KEY_POWER
	0,   //117 KEY_KPEQUAL
	0,   //118 KEY_KPPLUSMINUS
	72,  //119 KEY_PAUSE
	0,   //120 KEY_SCALE
	0,   //121 KEY_KPCOMMA
	0,   //122 KEY_HANGEUL
	0,   //123 KEY_HANJA
	0,   //124 KEY_YEN
	0,   //125 KEY_LEFTMETA
	0,   //126 KEY_RIGHTMETA
	0,   //127 KEY_COMPOSE
	0,   //128 KEY_STOP
	0,   //129 KEY_AGAIN
	0,   //130 KEY_PROPS
	0,   //131 KEY_UNDO
	0,   //132 KEY_FRONT
	0,   //133 KEY_COPY
	0,   //134 KEY_OPEN
	0,   //135 KEY_PASTE
	0,   //136 KEY_FIND
	0,   //137 KEY_CUT
	0,   //138 KEY_HELP
	0,   //139 KEY_MENU
	0,   //140 KEY_CALC
	0,   //141 KEY_SETUP
	0,   //142 KEY_SLEEP
	0,   //143 KEY_WAKEUP
	0,   //144 KEY_FILE
	0,   //145 KEY_SENDFILE
	0,   //146 KEY_DELETEFILE
	0,   //147 KEY_XFER
	0,   //148 KEY_PROG1
	0,   //149 KEY_PROG2
	0,   //150 KEY_WWW
	0,   //151 KEY_MSDOS
	0,   //152 KEY_SCREENLOCK
	0,   //153 KEY_DIRECTION
	0,   //154 KEY_CYCLEWINDOWS
	0,   //155 KEY_MAIL
	0,   //156 KEY_BOOKMARKS
	0,   //157 KEY_COMPUTER
	0,   //158 KEY_BACK
	0,   //159 KEY_FORWARD
	0,   //160 KEY_CLOSECD
	0,   //161 KEY_EJECTCD
	0,   //162 KEY_EJECTCLOSECD
	0,   //163 KEY_NEXTSONG
	0,   //164 KEY_PLAYPAUSE
	0,   //165 KEY_PREVIOUSSONG
	0,   //166 KEY_STOPCD
	0,   //167 KEY_RECORD
	0,   //168 KEY_REWIND
	0,   //169 KEY_PHONE
	0,   //170 KEY_ISO
	0,   //171 KEY_CONFIG
	0,   //172 KEY_HOMEPAGE
	0,   //173 KEY_REFRESH
	0,   //174 KEY_EXIT
	0,   //175 KEY_MOVE
	0,   //176 KEY_EDIT
	0,   //177 KEY_SCROLLUP
	0,   //178 KEY_SCROLLDOWN
	0,   //179 KEY_KPLEFTPAREN
	0,   //180 KEY_KPRIGHTPAREN
	0,   //181 KEY_NEW
	0,   //182 KEY_REDO
	0,   //183 KEY_F13
	0,   //184 KEY_F14
	0,   //185 KEY_F15
	0,   //186 KEY_F16
	0,   //187 KEY_F17
	0,   //188 KEY_F18
	0,   //189 KEY_F19
	0,   //190 KEY_F20
	0,   //191 KEY_F21
	0,   //192 KEY_F22
	0,   //193 KEY_F23
	49,  //194 U-mlaut on DE mapped to backslash
	0,   //195 ???
	0,   //196 ???
	0,   //197 ???
	0,   //198 ???
	0,   //199 ???
	0,   //200 KEY_PLAYCD
	0,   //201 KEY_PAUSECD
	0,   //202 KEY_PROG3
	0,   //203 KEY_PROG4
	0,   //204 KEY_DASHBOARD
	0,   //205 KEY_SUSPEND
	0,   //206 KEY_CLOSE
	0,   //207 KEY_PLAY
	0,   //208 KEY_FASTFORWARD
	0,   //209 KEY_BASSBOOST
	0,   //210 KEY_PRINT
	0,   //211 KEY_HP
	0,   //212 KEY_CAMERA
	0,   //213 KEY_SOUND
	0,   //214 KEY_QUESTION
	0,   //215 KEY_EMAIL
	0,   //216 KEY_CHAT
	0,   //217 KEY_SEARCH
	0,   //218 KEY_CONNECT
	0,   //219 KEY_FINANCE
	0,   //220 KEY_SPORT
	0,   //221 KEY_SHOP
	0,   //222 KEY_ALTERASE
	0,   //223 KEY_CANCEL
	0,   //224 KEY_BRIGHT_DOWN
	0,   //225 KEY_BRIGHT_UP
	0,   //226 KEY_MEDIA
	0,   //227 KEY_SWITCHVIDEO
	0,   //228 KEY_DILLUMTOGGLE
	0,   //229 KEY_DILLUMDOWN
	0,   //230 KEY_DILLUMUP
	0,   //231 KEY_SEND
	0,   //232 KEY_REPLY
	0,   //233 KEY_FORWARDMAIL
	0,   //234 KEY_SAVE
	0,   //235 KEY_DOCUMENTS
	0,   //236 KEY_BATTERY
	0,   //237 KEY_BLUETOOTH
	0,   //238 KEY_WLAN
	0,   //239 KEY_UWB
	0,   //240 KEY_UNKNOWN
	0,   //241 KEY_VIDEO_NEXT
	0,   //242 KEY_VIDEO_PREV
	0,   //243 KEY_BRIGHT_CYCLE
	0,   //244 KEY_BRIGHT_AUTO
	0,   //245 KEY_DISPLAY_OFF
	0,   //246 KEY_WWAN
	0,   //247 KEY_RFKILL
	0,   //248 KEY_MICMUTE
	0,   //249 ???
	0,   //250 ???
	0,   //251 ???
	0,   //252 ???
	0,   //253 ???
	0,   //254 ???
	000  //255 ???
};

typedef union
{
  struct
  {
    unsigned char bit0 : 1;
    unsigned char bit1 : 1;
    unsigned char bit2 : 1;
    unsigned char bit3 : 1;
    unsigned char bit4 : 1;
    unsigned char bit5 : 1;
    unsigned char bit6 : 1;
    unsigned char bit7 : 1;   
  }u;
   uint8_t byte;
} bitByte;


typedef struct {   		
   //frame sync           
   uint32_t PoC_frame_recv; 	     
   uint32_t PoC_frame_ddr; 	         
      
   //modeline + pll -> burst 3
   uint16_t PoC_H; 	// 08
   uint8_t  PoC_HFP; 	// 10
   uint8_t  PoC_HS; 	// 11
   uint8_t  PoC_HBP; 	// 12
   uint16_t PoC_V; 	// 13
   uint8_t  PoC_VFP; 	// 15   
   uint8_t  PoC_VS;     // 16
   uint8_t  PoC_VBP;    // 17
   
   //pll   
   uint8_t  PoC_pll_M0;  // 18 High
   uint8_t  PoC_pll_M1;  // 19 Low
   uint8_t  PoC_pll_C0;  // 20 High
   uint8_t  PoC_pll_C1;  // 21 Low
   uint32_t PoC_pll_K;   // 22 
   uint8_t  PoC_ce_pix;  // 26
   
   uint8_t  PoC_interlaced;  
   uint8_t  PoC_FB_progressive;  
   
   double   PoC_pclock;   
   
   uint32_t PoC_buffer_offset; // FIELD/AUDIO/LZ4 position on DDR
        
   //framebuffer          
   uint32_t PoC_bytes_len;               
   uint32_t PoC_pixels_len;               
   uint32_t PoC_bytes_recv;                             
   uint32_t PoC_pixels_ddr;   
   uint32_t PoC_field_frame;  
   uint8_t  PoC_field;  
   
   double PoC_width_time; 
   uint16_t PoC_V_Total;        
   
   //audio   
   uint32_t PoC_bytes_audio_len;
   
   //lz4
   uint32_t PoC_bytes_lz4_len;
   uint32_t PoC_bytes_lz4_ddr;     
   uint8_t  PoC_field_lz4; 
   
   //joystick
   uint32_t  PoC_joystick_keep_alive; 
   uint8_t   PoC_joystick_order;  
   uint32_t  PoC_joystick_map1;  
   uint32_t  PoC_joystick_map2;        
   
   char      PoC_joystick_l_analog_X1;  
   char      PoC_joystick_l_analog_Y1;  
   char      PoC_joystick_r_analog_X1;  
   char      PoC_joystick_r_analog_Y1;  

   char      PoC_joystick_l_analog_X2;  
   char      PoC_joystick_l_analog_Y2;  
   char      PoC_joystick_r_analog_X2;  
   char      PoC_joystick_r_analog_Y2;  
   
   //ps2
   uint32_t  PoC_ps2_keep_alive;
   uint8_t   PoC_ps2_order;  
   uint8_t   PoC_ps2_keyboard_keys[ARRAY_BIT_SIZE(NUM_SCANCODES)]; //32 bytes   
   uint8_t   PoC_ps2_mouse;
   uint8_t   PoC_ps2_mouse_x;
   uint8_t   PoC_ps2_mouse_y;
   uint8_t   PoC_ps2_mouse_z;
               
} PoC_type;

union {
    double d;
    uint64_t i;
} u;


/* PF_PACKETv3 variables */
/*
static int sockfd3;
static struct iovec* rd = { 0 };
static uint16_t current_block_num = 0;
static struct pollfd pfd = { 0 };

struct block_desc {
    uint32_t version;
    uint32_t offset_to_priv;
    struct tpacket_hdr_v1 h1;
};
*/
/* AF_XDP */
//static int sockfd_xdp;

/* General Server variables */
static int groovyServer = 0;
static int sockfd;
static struct sockaddr_in servaddr;	
static struct sockaddr_in clientaddr;
static socklen_t clilen = sizeof(struct sockaddr); 
static char recvbuf[65536] = { 0 };
static char sendbuf[41];

static int sockfdInputs;
static struct sockaddr_in servaddrInputs;
static struct sockaddr_in clientaddrInputs;

/* Logo */
static int groovyLogo = 0;
static int logoX = 0;
static int logoY = 0;
static int logoSignX = 0;
static int logoSignY = 0;
static unsigned long logoTime = 0;

static PoC_type *poc;
static uint8_t *map = 0;    
static uint8_t* buffer;

static int blitCompression = 0;
static uint8_t audioRate = 0;
static uint8_t audioChannels = 0;
static uint8_t rgbMode = 0;

static int isBlitting = 0;
static int isCorePriority = 0;

static uint8_t hpsBlit = 0; 
static uint16_t numBlit = 0;
static uint8_t doScreensaver = 0;
static uint8_t doPs2Inputs = 0;
static uint8_t doJoyInputs = 0;
static uint8_t doJumboFrames = 0;
static uint8_t isConnected = 0; 
static uint8_t isConnectedInputs = 0; 

/* FPGA HPS EXT STATUS */
static uint16_t fpga_vga_vcount = 0;
static uint32_t fpga_vga_frame = 0;
static uint32_t fpga_vram_pixels = 0;
static uint32_t fpga_vram_queue = 0;
static uint8_t  fpga_vram_end_frame = 0;
static uint8_t  fpga_vram_ready = 0;
static uint8_t  fpga_vram_synced = 0;
static uint8_t  fpga_vga_frameskip = 0;
static uint8_t  fpga_vga_vblank = 0;
static uint8_t  fpga_vga_f1 = 0;
static uint8_t  fpga_audio = 0;
static uint8_t  fpga_init = 0;
static uint32_t fpga_lz4_uncompressed = 0;

/* DEBUG */
/*
static uint32_t fpga_lz4_writed = 0;
static uint8_t  fpga_lz4_state = 0;
static uint8_t  fpga_lz4_run = 0;
static uint8_t  fpga_lz4_resume = 0;
static uint8_t  fpga_lz4_test1 = 0;
static uint8_t  fpga_lz4_test2 = 0;
static uint8_t  fpga_lz4_stop= 0;
static uint8_t  fpga_lz4_AB = 0;
static uint8_t  fpga_lz4_cmd_fskip = 0;
static uint32_t fpga_lz4_compressed = 0;
static uint32_t fpga_lz4_gravats = 0;
static uint32_t fpga_lz4_llegits = 0;
static uint32_t fpga_lz4_subframe_bytes = 0;
static uint16_t fpga_lz4_subframe_blit = 0;
*/

static void initDDR()
{
	memset(&buffer[0],0x00,0xff);	  
}

static void initVerboseFile()
{
	fp = fopen ("/tmp/groovy.log", "wt"); 
	if (fp == NULL)
	{
		printf("error groovy.log\n");       			
	}  	
	struct stat stats;
    	if (fstat(fileno(fp), &stats) == -1) 
    	{
        	printf("error groovy.log stats\n");        		
    	}		   		    		
    	if (setvbuf(fp, NULL, _IOFBF, stats.st_blksize) != 0)    		    	
    	{
        	printf("setvbuf failed \n");         		
    	}    		
    	logTime = GetTimer(1000);						
	
}

static void groovy_FPGA_hps()
{
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_GET_GROOVY_HPS);
    } while (req == 0);           
    uint16_t hps = spi_w(0); 						    
    DisableIO(); 	
    bitByte bits;  
    bits.byte = (uint8_t) hps;
  	    		    			
    if (bits.u.bit0 == 1 && bits.u.bit1 == 0) doVerbose = 1;
    else if (bits.u.bit0 == 0 && bits.u.bit1 == 1) doVerbose = 2;
    else if (bits.u.bit0 == 1 && bits.u.bit1 == 1) doVerbose = 3;
    else doVerbose = 0;
	
    initVerboseFile();				 	
    hpsBlit = bits.u.bit2;			
    doScreensaver = !bits.u.bit3;
    
    if (bits.u.bit4 == 1 && bits.u.bit5 == 0) doPs2Inputs = 1;
    else if (bits.u.bit4 == 0 && bits.u.bit5 == 1) doPs2Inputs = 2;	
    else doPs2Inputs = 0;
    
    if (bits.u.bit6 == 1 && bits.u.bit7 == 0) doJoyInputs = 1;
    else if (bits.u.bit6 == 0 && bits.u.bit7 == 1) doJoyInputs = 2;	
    else doJoyInputs = 0;
    
    bits.byte = (uint8_t) ((hps & 0xFF00) >> 8);
    doJumboFrames = bits.u.bit0;
    
    LOG(0, "[HPS][doVerbose=%d hpsBlit=%d doScreenSaver=%d doPs2Inputs=%d doJoyInputs=%d doJumboFrames=%d]\n", doVerbose, hpsBlit, doScreensaver, doPs2Inputs, doJoyInputs, doJumboFrames);	
}

static void groovy_FPGA_status(uint8_t isACK)
{
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_GET_GROOVY_STATUS);
    } while (req == 0);
    
    fpga_vga_frame   = spi_w(0) | spi_w(0) << 16;  	  			
    fpga_vga_vcount  = spi_w(0);  						  			
    uint16_t word16  = spi_w(0);  			 
    uint8_t word8_l  = (uint8_t)(word16 & 0x00FF);  
  			
    bitByte bits;  
    bits.byte = word8_l;
    fpga_vram_ready     = bits.u.bit0;
    fpga_vram_end_frame = bits.u.bit1;
    fpga_vram_synced    = bits.u.bit2;  
    fpga_vga_frameskip  = bits.u.bit3;   
    fpga_vga_vblank     = bits.u.bit4;  
    fpga_vga_f1         = bits.u.bit5; 
    fpga_audio          = bits.u.bit6;  
    fpga_init           = bits.u.bit7; 
			
    uint8_t word8_h = (uint8_t)((word16 & 0xFF00) >> 8);			  			    
    fpga_vram_queue = word8_h; // 8b
  			 										
    if (fpga_vga_vcount <= poc->PoC_interlaced) //end line
    {
	if (poc->PoC_interlaced)
	{
		if (!fpga_vga_vcount) //based on field
		{
			fpga_vga_vcount = poc->PoC_V_Total;
		}
		else
		{
			fpga_vga_vcount = poc->PoC_V_Total - 1;
		}
	}
	else
	{
		fpga_vga_vcount = poc->PoC_V_Total;
	}			
    }	
        
    if (!isACK)
    {
    	fpga_vram_queue |= spi_w(0) << 8; //24b
    	fpga_vram_pixels = spi_w(0) | spi_w(0) << 16;	
		
	if (blitCompression)
	{	
		fpga_lz4_uncompressed  = spi_w(0) | spi_w(0) << 16; 
	}	 
	
		/* DEBUG	
			fpga_lz4_state = spi_w(0);
			fpga_lz4_writed = spi_w(0) | spi_w(0) << 16;
			
			uint16_t wordlz4 = spi_w(0);
			
			bits.byte = (uint8_t) wordlz4;
			fpga_lz4_cmd_fskip = bits.u.bit6;
			fpga_lz4_AB = bits.u.bit5;
			fpga_lz4_stop = bits.u.bit4;
			fpga_lz4_test2 = bits.u.bit3;
			fpga_lz4_test1 = bits.u.bit2;
			fpga_lz4_resume = bits.u.bit1;
                	fpga_lz4_run = bits.u.bit0;
		
			fpga_lz4_compressed =  spi_w(0) | spi_w(0) << 16;
			fpga_lz4_gravats = spi_w(0) | spi_w(0) << 16;
			fpga_lz4_llegits = spi_w(0) | spi_w(0) << 16;
			fpga_lz4_subframe_bytes = spi_w(0) | spi_w(0) << 16;
			fpga_lz4_subframe_blit = spi_w(0);
	        */		   	
    }									    	
    DisableIO(); 	
}

static void groovy_FPGA_switchres()
{
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_SET_GROOVY_SWITCHRES);
    } while (req == 0);
    spi_w(1); 	
    DisableIO();        
}

static void groovy_FPGA_blit()
{	
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_SET_GROOVY_BLIT);
    } while (req == 0);    				
    spi_w(1);    	
    DisableIO();        
}

static void groovy_FPGA_blit_lz4(uint32_t compressed_bytes)
{	
    uint16_t req = 0;   
    EnableIO();	
    do
    {		
 	req = fpga_spi_fast(UIO_SET_GROOVY_BLIT_LZ4);							
    } while (req == 0);	
    uint16_t lz4_zone = (poc->PoC_field_lz4) ? 0 : 1;
    spi_w(lz4_zone);
    spi_w((uint16_t) compressed_bytes);
    spi_w((uint16_t) (compressed_bytes >> 16));
    DisableIO();        
}

static void groovy_FPGA_init(uint8_t cmd, uint8_t audio_rate, uint8_t audio_chan, uint8_t rgb_mode)
{
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_SET_GROOVY_INIT);
    } while (req == 0);
    spi_w(cmd);	
    bitByte bits; 
    bits.byte = audio_rate;
    bits.u.bit2 = (audio_chan == 1) ? 1 : 0;
    bits.u.bit3 = (audio_chan == 2) ? 1 : 0;
    bits.u.bit4 = (rgb_mode == 1) ? 1 : 0;			
    bits.u.bit5 = (rgb_mode == 2) ? 1 : 0;			
    spi_w((uint16_t) bits.byte);			
    DisableIO();
}

static void groovy_FPGA_logo(uint8_t cmd)
{
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_SET_GROOVY_LOGO);
    } while (req == 0);
    spi_w(cmd);		
    DisableIO();        
}

static void groovy_FPGA_audio(uint16_t samples)
{	
    uint16_t req = 0;
    EnableIO();	
    do
    {
    	req = fpga_spi_fast(UIO_SET_GROOVY_AUDIO);
    } while (req == 0);
    spi_w(samples);		
    DisableIO();   
}

static void loadLogo(int logoStart)
{	
	if (!doScreensaver)
	{
		return;
	}
	
	if (logoStart)
	{
		do
		{
			groovy_FPGA_status(0);			
		}  while (fpga_init != 0);	
		
		buffer[0] = (1) & 0xff;  
	 	buffer[1] = (1 >> 8) & 0xff;	       	  
	     	buffer[2] = (1 >> 16) & 0xff;    
	  	buffer[3] = (61440) & 0xff;  
	   	buffer[4] = (61440 >> 8) & 0xff;	       	  
	 	buffer[5] = (61440 >> 16) & 0xff;      	 			         	      	                     
		buffer[6] = (1) & 0xff; 	
		buffer[7] = (1 >> 8) & 0xff;
		
		logoTime = GetTimer(LOGO_TIMER); 					
	}

	if (CheckTimer(logoTime))    	
	{		
		groovy_FPGA_status(0);
		if (fpga_vga_vcount == 241)
		{								 		
			memset(&buffer[HEADER_OFFSET], 0x00, 184320);   	         		   	         								         		   	         				
		       	int z=0;		       	
		       	int offset = (256 * logoY * 3) + (logoX * 3); 		     
		       	for (int i=0; i<64; i++)
		       	{       				       		
		       		memcpy(&buffer[HEADER_OFFSET+offset], (char*)&logoImage[z], 192);
		       		offset += 256 * 3;		       		
		       		z += 64 * 3;		       	
		       	}              		       			       		                          	                       	 			         	      	                                         		       		       
		       	logoTime = GetTimer(LOGO_TIMER);   
		       	
		       	logoX = (logoSignX) ? logoX - 1 : logoX + 1;
		       	logoY = (logoSignY) ? logoY - 2 : logoY + 2;	       		       	
		       	
		       	if (logoX >= 192 && !logoSignX)
		       	{
		       		logoSignX = !logoSignX;		       		       			       		
		       	}    	                   	        
		       	
		       	if (logoY >= 176 && !logoSignY)
		       	{	       		
		       		logoSignY = !logoSignY;
		       	}    
		       	
		       	if (logoX <= 0 && logoSignX)
		       	{
		       		logoSignX = !logoSignX;		       		       			       		
		       	}    	                   	        
		       	
		       	if (logoY <= 0 && logoSignY)
		       	{	       		
		       		logoSignY = !logoSignY;
		       	}    		       			       			     
		}                   	        	       	
	}
}

static void groovy_FPGA_blit(uint32_t bytes, uint16_t numBlit)
{            		
    poc->PoC_pixels_ddr = (rgbMode == 1) ? bytes >> 2 : (rgbMode == 2) ? bytes >> 1 : bytes / 3;     
    	                       	 			         	      	                                         
    buffer[3] = (poc->PoC_pixels_ddr) & 0xff;  
    buffer[4] = (poc->PoC_pixels_ddr >> 8) & 0xff;	       	  
    buffer[5] = (poc->PoC_pixels_ddr >> 16) & 0xff;      	 			         	      	                 
    
    buffer[6] = (numBlit) & 0xff; 	
    buffer[7] = (numBlit >> 8) & 0xff;	       	  
    
    if (poc->PoC_frame_ddr != poc->PoC_frame_recv)
    {
    	poc->PoC_frame_ddr  = poc->PoC_frame_recv;
    	
    	buffer[0] = (poc->PoC_frame_ddr) & 0xff;  
    	buffer[1] = (poc->PoC_frame_ddr >> 8) & 0xff;	       	  
    	buffer[2] = (poc->PoC_frame_ddr >> 16) & 0xff;        	  
    	
    	groovy_FPGA_blit();         	    	
    }	                     		         	      	             
       
}

static void groovy_FPGA_blit_lz4(uint32_t bytes, uint16_t numBlit)
{          	    
    poc->PoC_bytes_lz4_ddr = bytes;	                       	 			         	      	                                         
    buffer[35] = (poc->PoC_bytes_lz4_ddr) & 0xff;  
    buffer[36] = (poc->PoC_bytes_lz4_ddr >> 8) & 0xff;	       	  
    buffer[37] = (poc->PoC_bytes_lz4_ddr >> 16) & 0xff;      	 			         	      	                 
    
    buffer[38] = (numBlit) & 0xff; 	
    buffer[39] = (numBlit >> 8) & 0xff;	       	  
    
    if (poc->PoC_frame_ddr != poc->PoC_frame_recv)
    {
    	poc->PoC_frame_ddr  = poc->PoC_frame_recv;
    	
    	buffer[32] = (poc->PoC_frame_ddr) & 0xff;  
    	buffer[33] = (poc->PoC_frame_ddr >> 8) & 0xff;	       	  
    	buffer[34] = (poc->PoC_frame_ddr >> 16) & 0xff;        	  
    	    	
    	groovy_FPGA_blit_lz4(poc->PoC_bytes_lz4_len);    	
    }	                     		         	      	             
       
}

static void setSwitchres()
{               	   	 	    
    //modeline			       
    uint64_t udp_pclock_bits; 
    uint16_t udp_hactive;
    uint16_t udp_hbegin;
    uint16_t udp_hend;
    uint16_t udp_htotal;
    uint16_t udp_vactive;
    uint16_t udp_vbegin;
    uint16_t udp_vend;
    uint16_t udp_vtotal;
    uint8_t  udp_interlace;
     
    memcpy(&udp_pclock_bits,&recvbuf[1],8);			     
    memcpy(&udp_hactive,&recvbuf[9],2);			     
    memcpy(&udp_hbegin,&recvbuf[11],2);			     
    memcpy(&udp_hend,&recvbuf[13],2);			     
    memcpy(&udp_htotal,&recvbuf[15],2);			     
    memcpy(&udp_vactive,&recvbuf[17],2);			     
    memcpy(&udp_vbegin,&recvbuf[19],2);			     
    memcpy(&udp_vend,&recvbuf[21],2);			     
    memcpy(&udp_vtotal,&recvbuf[23],2);			     
    memcpy(&udp_interlace,&recvbuf[25],1);			     
           
    u.i = udp_pclock_bits;
    double udp_pclock = u.d;     
                                          
    poc->PoC_width_time = (double) udp_htotal * (1 / (udp_pclock * 1000)); //in ms, time to raster 1 line
    poc->PoC_V_Total = udp_vtotal;
                                                                                            			      			      			       
    LOG(1,"[Modeline] %f %d %d %d %d %d %d %d %d %s(%d)\n",udp_pclock,udp_hactive,udp_hbegin,udp_hend,udp_htotal,udp_vactive,udp_vbegin,udp_vend,udp_vtotal,udp_interlace?"interlace":"progressive",udp_interlace);
       	                     	                       	                                    	                      			       
    poc->PoC_pixels_ddr = 0;
    poc->PoC_H = udp_hactive;
    poc->PoC_HFP = udp_hbegin - udp_hactive;
    poc->PoC_HS = udp_hend - udp_hbegin;
    poc->PoC_HBP = udp_htotal - udp_hend;
    poc->PoC_V = udp_vactive;
    poc->PoC_VFP = udp_vbegin - udp_vactive;
    poc->PoC_VS = udp_vend - udp_vbegin;
    poc->PoC_VBP = udp_vtotal - udp_vend;         
           
    poc->PoC_ce_pix = (udp_pclock * 16 < 90) ? 16 : (udp_pclock * 12 < 90) ? 12 : (udp_pclock * 8 < 90) ? 8 : (udp_pclock * 6 < 90) ? 6 : 4;	// we want at least 40Mhz clksys for vga scaler	       
    
    poc->PoC_interlaced = (udp_interlace >= 1) ? 1 : 0; 
    poc->PoC_FB_progressive = (udp_interlace == 0 || udp_interlace == 2) ? 1 : 0;  
    
    poc->PoC_field_frame = poc->PoC_frame_ddr + 1;
    poc->PoC_field = 0;    
           
    int M=0;    
    int C=0;
    int K=0;	
    
    getMCK_PLL_Fractional(udp_pclock*poc->PoC_ce_pix,M,C,K);
    poc->PoC_pll_M0 = (M % 2 == 0) ? M >> 1 : (M >> 1) + 1;
    poc->PoC_pll_M1 = M >> 1;    
    poc->PoC_pll_C0 = (C % 2 == 0) ? C >> 1 : (C >> 1) + 1;
    poc->PoC_pll_C1 = C >> 1;	        
    poc->PoC_pll_K = K;	        
    
    poc->PoC_pixels_len = poc->PoC_H * poc->PoC_V;    
    
    if (poc->PoC_interlaced && !poc->PoC_FB_progressive)
    {
    	poc->PoC_pixels_len = poc->PoC_pixels_len >> 1;
    } 
     
    poc->PoC_bytes_len = (rgbMode == 1) ? poc->PoC_pixels_len << 2 : (rgbMode == 2) ? poc->PoC_pixels_len << 1 : poc->PoC_pixels_len * 3;            
    poc->PoC_bytes_recv = 0;          
    poc->PoC_buffer_offset = 0;
    
    LOG(1,"[FPGA header] %d %d %d %d %d %d %d %d ce_pix=%d PLL(M0=%d,M1=%d,C0=%d,C1=%d,K=%d) \n",poc->PoC_H,poc->PoC_HFP, poc->PoC_HS,poc->PoC_HBP,poc->PoC_V,poc->PoC_VFP, poc->PoC_VS,poc->PoC_VBP,poc->PoC_ce_pix,poc->PoC_pll_M0,poc->PoC_pll_M1,poc->PoC_pll_C0,poc->PoC_pll_C1,poc->PoC_pll_K);				       			       	
    
    //clean pixels on ddr (auto_blit)
    buffer[4] = 0x00;  
    buffer[5] = 0x00;	       	  
    buffer[6] = 0x00;  
    buffer[7] = 0x00; 	           		         
     
    //modeline + pll -> burst 3
    buffer[8]  =  poc->PoC_H & 0xff;        
    buffer[9]  = (poc->PoC_H >> 8);
    buffer[10] =  poc->PoC_HFP;
    buffer[11] =  poc->PoC_HS;
    buffer[12] =  poc->PoC_HBP;
    buffer[13] =  poc->PoC_V & 0xff;        
    buffer[14] = (poc->PoC_V >> 8);			
    buffer[15] =  poc->PoC_VFP;
    buffer[16] =  poc->PoC_VS;
    buffer[17] =  poc->PoC_VBP;
    
    //pll   
    buffer[18] =  poc->PoC_pll_M0;  
    buffer[19] =  poc->PoC_pll_M1;      
    buffer[20] =  poc->PoC_pll_C0;  
    buffer[21] =  poc->PoC_pll_C1; 
    buffer[22] = (poc->PoC_pll_K) & 0xff;  
    buffer[23] = (poc->PoC_pll_K >> 8) & 0xff;	       	  
    buffer[24] = (poc->PoC_pll_K >> 16) & 0xff;    
    buffer[25] = (poc->PoC_pll_K >> 24) & 0xff;   
    buffer[26] =  poc->PoC_ce_pix;  
    buffer[27] =  udp_interlace;     
    
    groovy_FPGA_switchres();                 
}


static void setClose()
{          				
	groovy_FPGA_init(0, 0, 0, 0);	
	isBlitting = 0;		
	numBlit = 0;
	blitCompression = 0;		
	free(poc);	
	initDDR();	
	isConnected = 0;	
	isConnectedInputs = 0; 	
	
	// load LOGO
	if (doScreensaver)
	{			
		loadLogo(1);		
		groovy_FPGA_init(1, 0, 0, 0);    	
		groovy_FPGA_blit(); 
		groovy_FPGA_logo(1);
		groovyLogo = 1;	
	}
	
}

static void groovy_send_joysticks()
{	
	int len = 9;		
	sendbuf[0] = poc->PoC_frame_ddr & 0xff;
	sendbuf[1] = poc->PoC_frame_ddr >> 8;	
	sendbuf[2] = poc->PoC_frame_ddr >> 16;	
	sendbuf[3] = poc->PoC_frame_ddr >> 24;	
	sendbuf[4] = poc->PoC_joystick_order;
	sendbuf[5] = poc->PoC_joystick_map1 & 0xff;
	sendbuf[6] = poc->PoC_joystick_map1 >> 8;
	sendbuf[7] = poc->PoC_joystick_map2 & 0xff;
	sendbuf[8] = poc->PoC_joystick_map2 >> 8;	
	if (doJoyInputs == 2)
	{
		sendbuf[9]  = poc->PoC_joystick_l_analog_X1;
		sendbuf[10] = poc->PoC_joystick_l_analog_Y1;				
		sendbuf[11] = poc->PoC_joystick_r_analog_X1;				
		sendbuf[12] = poc->PoC_joystick_r_analog_Y1;
		sendbuf[13] = poc->PoC_joystick_l_analog_X2;				
		sendbuf[14] = poc->PoC_joystick_l_analog_Y2;				
		sendbuf[15] = poc->PoC_joystick_r_analog_X2;				
		sendbuf[16] = poc->PoC_joystick_r_analog_Y2;
		len += 8;
	}
	sendto(sockfdInputs, sendbuf, len, MSG_CONFIRM, (struct sockaddr *)&clientaddrInputs, clilen);
	poc->PoC_joystick_keep_alive = 0;
}

static void groovy_send_ps2()
{	
	int len = 37;		
	sendbuf[0] = poc->PoC_frame_ddr & 0xff;
	sendbuf[1] = poc->PoC_frame_ddr >> 8;	
	sendbuf[2] = poc->PoC_frame_ddr >> 16;	
	sendbuf[3] = poc->PoC_frame_ddr >> 24;	
	sendbuf[4] = poc->PoC_ps2_order;
	memcpy(&sendbuf[5], &poc->PoC_ps2_keyboard_keys, 32);	
	if (doPs2Inputs == 2)
	{		
		sendbuf[37] = poc->PoC_ps2_mouse;				
		sendbuf[38] = poc->PoC_ps2_mouse_x;				
		sendbuf[39] = poc->PoC_ps2_mouse_y;
		sendbuf[40] = poc->PoC_ps2_mouse_z;						
		len += 4;
	}
	sendto(sockfdInputs, sendbuf, len, MSG_CONFIRM, (struct sockaddr *)&clientaddrInputs, clilen);
	poc->PoC_ps2_keep_alive = 0;
}

static void sendACK(uint32_t udp_frame, uint16_t udp_vsync)
{          	
	LOG(2, "[ACK_%s]\n", "STATUS");	
	
	int flags = 0;	
	flags |= MSG_CONFIRM;		
	//echo
	sendbuf[0] = udp_frame & 0xff;
	sendbuf[1] = udp_frame >> 8;	
	sendbuf[2] = udp_frame >> 16;	
	sendbuf[3] = udp_frame >> 24;	
	sendbuf[4] = udp_vsync & 0xff;
	sendbuf[5] = udp_vsync >> 8;
	//gpu
	sendbuf[6] = fpga_vga_frame  & 0xff;
	sendbuf[7] = fpga_vga_frame  >> 8;	
	sendbuf[8] = fpga_vga_frame  >> 16;	
	sendbuf[9] = fpga_vga_frame  >> 24;	
	sendbuf[10] = fpga_vga_vcount & 0xff;
	sendbuf[11] = fpga_vga_vcount >> 8;
	//debug bits
	bitByte bits;
	bits.byte = 0;
	bits.u.bit0 = fpga_vram_ready;
	bits.u.bit1 = fpga_vram_end_frame;
	bits.u.bit2 = fpga_vram_synced;
	bits.u.bit3 = fpga_vga_frameskip;
	bits.u.bit4 = fpga_vga_vblank;
	bits.u.bit5 = fpga_vga_f1;
	bits.u.bit6 = fpga_audio;
	bits.u.bit7 = (fpga_vram_queue > 0) ? 1 : 0;
	sendbuf[12] = bits.byte;
			
	sendto(sockfd, sendbuf, 13, flags, (struct sockaddr *)&clientaddr, clilen);	
	
	if (poc->PoC_joystick_keep_alive >= KEEP_ALIVE_FRAMES)
	{
		LOG(2, "[JOY_ACK][%s]\n", "KEEP_ALIVE");	
		groovy_send_joysticks();
	}															
	
	if (poc->PoC_ps2_keep_alive >= KEEP_ALIVE_FRAMES)
	{
		LOG(2, "[KBD_ACK][%s]\n", "KEEP_ALIVE");
		groovy_send_ps2();		
	}															
}

static void setInit(uint8_t compression, uint8_t audio_rate, uint8_t audio_chan, uint8_t rgb_mode)
{          				
	blitCompression = (compression <= 1) ? compression : 0;
	audioRate = (audio_rate <= 3) ? audio_rate : 0;
	audioChannels = (audio_chan <= 2) ? audio_chan : 0;	
	rgbMode = (rgb_mode <= 2) ? rgb_mode : 0;	
	poc = (PoC_type *) calloc(1, sizeof(PoC_type));
	initDDR();
	isBlitting = 0;	
	numBlit = 0;	
	
	
	char hoststr[NI_MAXHOST];
	char portstr[NI_MAXSERV];
	// load LOGO
	if (doScreensaver)
	{
		groovy_FPGA_init(0, 0, 0, 0);
		groovy_FPGA_logo(0);																				
		groovyLogo = 0;					
	}
	
	if (!isConnected)
	{								
		getnameinfo((struct sockaddr *)&clientaddr, clilen, hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
		LOG(1,"[Connected][%s:%s]\n", hoststr, portstr);		
		isConnected = 1;		
	}
	
	if (doPs2Inputs || doJoyInputs)
  	{
  		int len = recvfrom(sockfdInputs, recvbuf, 1, 0, (struct sockaddr *)&clientaddrInputs, &clilen);  			
  		if (len > 0)
  		{  						
			getnameinfo((struct sockaddr *)&clientaddrInputs, clilen, hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
			LOG(1,"[Inputs][%s:%s]\n", hoststr, portstr);					
  			isConnectedInputs = 1;  			
  		}	
  	}
	
	do
	{
		groovy_FPGA_status(0);
	} while (fpga_init != 0);	
	
	groovy_FPGA_init(1, audioRate, audioChannels, rgbMode);		
}

static void setBlit(uint32_t udp_frame, uint32_t udp_lz4_size)
{          			
	poc->PoC_frame_recv = udp_frame;		
	poc->PoC_bytes_recv = 0;			
	poc->PoC_bytes_lz4_ddr = 0;		
	poc->PoC_bytes_lz4_len = (blitCompression) ? udp_lz4_size : 0;	
	poc->PoC_field = (!poc->PoC_FB_progressive) ? (poc->PoC_frame_recv + poc->PoC_field_frame) % 2 : 0; 
	poc->PoC_buffer_offset = (blitCompression) ? (poc->PoC_field_lz4) ? LZ4_OFFSET_B : LZ4_OFFSET_A : (!poc->PoC_FB_progressive && poc->PoC_field) ? FIELD_OFFSET : 0;					
	poc->PoC_field_lz4 = (blitCompression) ? !poc->PoC_field_lz4 : 0;
	poc->PoC_joystick_order = 0;		
	poc->PoC_ps2_order = 0;	
	
	if (isConnectedInputs && doJoyInputs)
	{
		poc->PoC_joystick_keep_alive++;	
	}

	if (isConnectedInputs && doPs2Inputs)
	{
		poc->PoC_ps2_keep_alive++;	
	}
	
	isBlitting = 1;	
	isCorePriority = 1;
	numBlit = 0;	
	
	if (!hpsBlit) //ASAP fpga starts to poll ddr
	{
		if (blitCompression)
		{
			groovy_FPGA_blit_lz4(0, 0); 	
		}	
		else
		{
			groovy_FPGA_blit(0, 0);
		}
	} 
	else
	{
		poc->PoC_pixels_ddr = 0;
		poc->PoC_bytes_lz4_ddr = 0;
	}
				
	if (doVerbose > 0 && doVerbose < 3)
	{
		groovy_FPGA_status(0);
		LOG(1, "[GET_STATUS][DDR fr=%d bl=%d][GPU fr=%d vc=%d fskip=%d vb=%d fd=%d][VRAM px=%d queue=%d sync=%d free=%d eof=%d][AUDIO=%d][LZ4 inf=%d]\n", poc->PoC_frame_ddr, numBlit, fpga_vga_frame, fpga_vga_vcount, fpga_vga_frameskip, fpga_vga_vblank, fpga_vga_f1, fpga_vram_pixels, fpga_vram_queue, fpga_vram_synced, fpga_vram_ready, fpga_vram_end_frame, fpga_audio, fpga_lz4_uncompressed);								
	}	
	
	if (!doVerbose && !fpga_vram_synced)
 	{
 		groovy_FPGA_status(0);
 		LOG(0, "[GET_STATUS][DDR fr=%d bl=%d][GPU fr=%d vc=%d fskip=%d vb=%d fd=%d][VRAM px=%d queue=%d sync=%d free=%d eof=%d][AUDIO=%d][LZ4 inf=%d]\n", poc->PoC_frame_ddr, numBlit, fpga_vga_frame, fpga_vga_vcount, fpga_vga_frameskip, fpga_vga_vblank, fpga_vga_f1, fpga_vram_pixels, fpga_vram_queue, fpga_vram_synced, fpga_vram_ready, fpga_vram_end_frame, fpga_audio, fpga_lz4_uncompressed);		
 	}		 		
	gettimeofday(&blitStart, NULL); 	
}

static void setBlitAudio(uint16_t udp_bytes_samples)
{	
	poc->PoC_bytes_audio_len = udp_bytes_samples;	
	poc->PoC_buffer_offset = AUDIO_OFFSET;	
	poc->PoC_bytes_recv = 0;	
		
	isBlitting = 2;																	
	isCorePriority = 1;
}

static void setBlitRawAudio(uint16_t len)
{
	poc->PoC_bytes_recv += len;	
	isBlitting = (poc->PoC_bytes_recv >= poc->PoC_bytes_audio_len) ? 0 : 2;	
	
	LOG(2, "[DDR_AUDIO][%d/%d]\n", poc->PoC_bytes_recv, poc->PoC_bytes_audio_len);
			
	if (isBlitting == 0)
	{
		uint16_t sound_samples = (audioChannels == 0) ? 0 : (audioChannels == 1) ? poc->PoC_bytes_audio_len >> 1 : poc->PoC_bytes_audio_len >> 2;		
		groovy_FPGA_audio(sound_samples);				
		poc->PoC_buffer_offset = 0;
		isCorePriority = 0;		
	}	
}
												
static void setBlitRaw(uint16_t len)
{       	
	poc->PoC_bytes_recv += len;									
	isBlitting = (poc->PoC_bytes_recv >= poc->PoC_bytes_len) ? 0 : 1;
	       	
       	if (!hpsBlit) //ASAP
       	{
       		numBlit++;     		
		groovy_FPGA_blit(poc->PoC_bytes_recv, numBlit);		
		LOG(2, "[ACK_BLIT][(%d) px=%d/%d %d/%d]\n", numBlit, poc->PoC_pixels_ddr, poc->PoC_pixels_len, poc->PoC_bytes_recv, poc->PoC_bytes_len);      
       	}
       	else
       	{
       		LOG(2, "[DDR_BLIT][%d/%d]\n", poc->PoC_bytes_recv, poc->PoC_bytes_len);	
       	}
		
        if (isBlitting == 0)
        {	
        	isCorePriority = 0;	        	
        	if (poc->PoC_pixels_ddr < poc->PoC_pixels_len)
        	{        		
        		numBlit++;        		
			groovy_FPGA_blit(poc->PoC_bytes_recv, numBlit);				
			LOG(2, "[ACK_BLIT][(%d) px=%d/%d %d/%d]\n", numBlit, poc->PoC_pixels_ddr, poc->PoC_pixels_len, poc->PoC_bytes_recv, poc->PoC_bytes_len);      
        	}
        	poc->PoC_buffer_offset = 0;        	
		gettimeofday(&blitStop, NULL); 
        	int difBlit = ((blitStop.tv_sec - blitStart.tv_sec) * 1000000 + blitStop.tv_usec - blitStart.tv_usec);
		LOG(1, "[DDR_BLIT][TOTAL %06.3f][(%d) Bytes=%d]\n",(double) difBlit/1000, numBlit, poc->PoC_bytes_len); 			                	 
        }			     
}

static void setBlitLZ4(uint16_t len)
{
	poc->PoC_bytes_recv += len;
	isBlitting = (poc->PoC_bytes_recv >= poc->PoC_bytes_lz4_len) ? 0 : 1;
		
	if (!hpsBlit) //ASAP
       	{
       		numBlit++;     		
		groovy_FPGA_blit_lz4(poc->PoC_bytes_recv, numBlit); 		
		LOG(2, "[ACK_BLIT][(%d) %d/%d]\n", numBlit, poc->PoC_bytes_recv, poc->PoC_bytes_lz4_len);
       	} 
       	else
       	{
       		LOG(2, "[LZ4_BLIT][%d/%d]\n", poc->PoC_bytes_recv, poc->PoC_bytes_lz4_len);
       	}			
			
	if (isBlitting == 0)
        {	
        	isCorePriority = 0;	
        	if (poc->PoC_bytes_lz4_ddr < poc->PoC_bytes_lz4_len)
        	{        		
        		numBlit++;     		
			groovy_FPGA_blit_lz4(poc->PoC_bytes_recv, numBlit); 		
			LOG(2, "[ACK_BLIT][(%d) %d/%d]\n", numBlit, poc->PoC_bytes_recv, poc->PoC_bytes_lz4_len);   
        	}        	        	
        	poc->PoC_buffer_offset = 0;            		    	
		gettimeofday(&blitStop, NULL); 
        	int difBlit = ((blitStop.tv_sec - blitStart.tv_sec) * 1000000 + blitStop.tv_usec - blitStart.tv_usec);
		LOG(1, "[LZ4_BLIT][TOTAL %06.3f][(%d) Bytes=%d]\n",(double) difBlit/1000, numBlit, poc->PoC_bytes_lz4_len); 			                	 
        }	
}

static void groovy_map_ddr()
{	
    	int pagesize = sysconf(_SC_PAGE_SIZE);
    	if (pagesize==0) pagesize=4096;       	
    	int offset = BASEADDR;
    	int map_start = offset & ~(pagesize - 1);
    	int map_off = offset - map_start;
    	int num_bytes=BUFFERSIZE;
    	
    	map = (uint8_t*)shmem_map(map_start, num_bytes+map_off);      	    	
    	buffer = map + map_off;         	
    	    
    	initDDR();
    	poc = (PoC_type *) calloc(1, sizeof(PoC_type));	
    	
    	isCorePriority = 0;	    	
    	isBlitting = 0;	    	
}

   

/*    pf_packetv   */     

/*

static void flush_block(struct block_desc *pbd) {
    pbd->h1.block_status = TP_STATUS_KERNEL;
}

    uint32_t received_packets = 0;
    uint32_t received_bytes = 0;
    
static int walk_block(struct block_desc *pbd, const int block_num) {
	
    int num_pkts = pbd->h1.num_pkts, i;
    unsigned long bytes = 0;
    struct tpacket3_hdr *ppd;
    struct ethhdr *eth;
    struct iphdr *ip;
    struct udphdr *udp;
    
    ppd = (struct tpacket3_hdr *) ((uint8_t *) pbd + pbd->h1.offset_to_first_pkt);
    eth = (struct ethhdr *) ((uint8_t *)ppd + ppd->tp_mac);
    ip = (struct iphdr *) ((uint8_t *) eth + ETH_HLEN);
    udp = (struct udphdr *) (((char *)ip ) + sizeof(struct iphdr));
    char sbuff[NI_MAXHOST], dbuff[NI_MAXHOST];
    
   // int iphdrlen = ip->ihl * 4;
   
    if (eth->h_proto == htons(ETH_P_IP) && ntohs(udp->dest) == UDP_PORT)   
    {
	struct sockaddr_in ss, sd ;

 	memset(&ss, 0, sizeof (ss));
 	ss.sin_family = PF_INET;
	ss.sin_addr.s_addr = ip->saddr;
 	getnameinfo((struct sockaddr *)&ss, sizeof(ss), sbuff, sizeof(sbuff), NULL, 0, NI_NUMERICHOST);

	memset(&sd, 0, sizeof(sd));
 	sd.sin_family = PF_INET;
 	sd.sin_addr.s_addr = ip->daddr;
 	getnameinfo((struct sockaddr *)&sd, sizeof(sd), dbuff, sizeof(dbuff), NULL, 0, NI_NUMERICHOST);
 	
 	//printf("block %d, packets %d --> HOST %s ADDR %s port (%d->%d), payload len %d\n",block_num, num_pkts, sbuff, dbuff, ntohs(udp->source), ntohs(udp->dest), ntohs(udp->len) - sizeof(struct udphdr));	
 	
    }   
    else
    {
    	return 0;
    }
  //    printf("sizes %d %d %d %d\n", sizeof(struct ethhdr), iphdrlen, sizeof(struct udphdr));
  //  int header_size =  sizeof(struct ethhdr) + iphdrlen + sizeof(udp);
  
 
    for (i = 0; i < num_pkts; ++i) {
        bytes += ppd->tp_snaplen;               
        eth = (struct ethhdr *) ((uint8_t *) ppd + ppd->tp_mac);
        char* data_pointer = (char*)((uint8_t *) ppd + ppd->tp_mac);
        ip = (struct iphdr *) ((uint8_t *) eth + ETH_HLEN);
        udp = (struct udphdr *) (((char *)ip ) + sizeof(struct iphdr));
      //  printf("packet %d, bytes %d payload udplen %d, byte 42=%d\n",i, ppd->tp_snaplen, ntohs(udp->len) - sizeof(struct udphdr), data_pointer[42]);      
      
        if (isBlitting == 1 && (ntohs(udp->len) - sizeof(struct udphdr)) <= 1472) {
        	memcpy(&recvbuf[0],&data_pointer[42],(ntohs(udp->len) - sizeof(struct udphdr))); 
        	if (received_packets > 1) {
        		gettimeofday(&blitStop, NULL); 
    			int difBlit = ((blitStop.tv_sec - blitStart.tv_sec) * 1000000 + blitStop.tv_usec - blitStart.tv_usec);
    			LOG(0, "[TOTAL %06.3f][Bytes=%d, %d]\n",(double) difBlit/1000, received_bytes, received_packets); 
    		}
    		received_bytes += (ntohs(udp->len) - sizeof(struct udphdr));    		
    		received_packets ++;
	    	 
	    	if ((ntohs(udp->len) - sizeof(struct udphdr)) < 1472) {
	    		isBlitting = 0;
	    	}
	    	
        }
        
        if (isBlitting == 0 && ((ntohs(udp->len) - sizeof(struct udphdr)) == 7 || (ntohs(udp->len) - sizeof(struct udphdr)) == 11)) {
        	gettimeofday(&blitStart, NULL);     
        	isBlitting = 1;
        	received_packets = 0 ;
        	received_bytes = 0;
        } 
      
        
        ppd = (struct tpacket3_hdr *) ((uint8_t *) ppd + ppd->tp_next_offset);                           
    

   
}
   
    //printf("packets %d, bytes %d \n", num_pkts, bytes);
   // gettimeofday(&blitStop, NULL); 
   // int difBlit = ((blitStop.tv_sec - blitStart.tv_sec) * 1000000 + blitStop.tv_usec - blitStart.tv_usec);
   // LOG(0, "[TOTAL %06.3f][Bytes=%d (%d)]\n",(double) difBlit/1000, bytes, num_pkts); 
   return 1;
}

static void groovy_udp_server_init3()
{
	LOG(1, "[UDP][SERVER][START][%s]\n", "V3");

	//setup PF_PACKETv3				
	sockfd3 = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	if (sockfd3 < 0)
    	{
    		printf("socket error\n");       		
    	}    
    	
    	int v = TPACKET_V3;	
    	if (setsockopt(sockfd3, SOL_PACKET, PACKET_VERSION, &v, sizeof(v)) < 0)
        {
                printf("packet_version error\n");                
        }    
        
        // Non blocking socket                                                                                                             
    	int flags;
    	flags = fcntl(sockfd3, F_GETFD, 0);    	
    	if (flags < 0) 
    	{
      		printf("get falg error\n");       		
    	}
    	flags |= O_NONBLOCK;
    	if (fcntl(sockfd3, F_SETFL, flags) < 0) 
    	{
       		printf("set nonblock fail\n");       		
    	}   	
    	  	
        
    	//index of eth0
        struct sockaddr_ll sll;
        struct ifreq ifr;
        memset(&sll, 0, sizeof(sll));
        memset(&ifr, 0, sizeof(ifr));        
        strncpy((char *)ifr.ifr_name, "eth0", IFNAMSIZ);
        if((ioctl(sockfd3, SIOCGIFINDEX, &ifr)) < 0)
        {     
        	printf("Error getting Interface hw address!\n");     
        }
       
	// promiscue mode       
        struct packet_mreq mreq;    
        memset(&mreq, 0, sizeof(mreq));  
        mreq.mr_ifindex = ifr.ifr_ifindex;
        mreq.mr_type = PACKET_MR_PROMISC;
    	if (setsockopt(sockfd3, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        {
        	printf("error promiscue mode \n");
        }    	    	     	 	        	    	     	    	
        
        // filter udp traffic
        struct sock_fprog prog;
        struct sock_filter filter[] = {
            { BPF_LD + BPF_H + BPF_ABS,  0, 0,     12 },
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1,  0x800 },
            { BPF_LD + BPF_B + BPF_ABS,  0, 0,     23 },
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1,   0x11 },
            { BPF_RET + BPF_K,           0, 0, 0xffff },
            { BPF_RET + BPF_K,           0, 0, 0x0000 },
        };

        prog.len = sizeof(filter)/sizeof(filter[0]);
        prog.filter = filter;

        if (setsockopt(sockfd3, SOL_SOCKET, SO_ATTACH_FILTER, &prog, sizeof(prog)) != 0)
        {
        	printf("Error filter udp\n");   
        }
       
        int busyPoll = 50;
	if (setsockopt(sockfd3, SOL_SOCKET, SO_BUSY_POLL, (void*)&busyPoll,sizeof(busyPoll)) < 0)
	{
        	printf("Error so_busy_poll\n");        	
        }   

#define SO_PREFER_BUSY_POLL     69
#define SO_BUSY_POLL_BUDGET     70
        
        int preferBusyPoll = 1;
	if (setsockopt(sockfd3, SOL_SOCKET, SO_PREFER_BUSY_POLL, (void*)&preferBusyPoll,sizeof(preferBusyPoll)) < 0)
	{
        	printf("Error so_prefer_busy_poll\n");        	
        }   

   	int busyPollBudget = 600;
	if (setsockopt(sockfd3, SOL_SOCKET, SO_BUSY_POLL_BUDGET, (void*)&busyPollBudget,sizeof(busyPollBudget)) < 0)
	{
        	printf("Error so_busy_poll_budget\n");        	
        }  
                
        // kernel to export data through mmap() rings
        struct tpacket_req3 tp;  
        memset(&tp, 0, sizeof(tp));	
  	tp.tp_block_size = BLOCKSIZ;
    	tp.tp_frame_size = FRAMESIZ;
    	tp.tp_block_nr = BLOCKNUM;
    	tp.tp_frame_nr = (BLOCKSIZ * BLOCKNUM) / FRAMESIZ;
    	tp.tp_retire_blk_tov = 1; // Timeout in msec (0 is block full)
    	tp.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
	
	if (setsockopt(sockfd3, SOL_PACKET, PACKET_RX_RING, (void *) &tp, sizeof(tp)) < 0)    	
    	{
    		printf("packet_rx_ring error\n");
    	}
    	
    //	if (setsockopt(sockfd3, SOL_PACKET, PACKET_TX_RING, (void *) &tp, sizeof(tp)) < 0)
   // 	{
    //		printf("packet_tx_ring error\n");
    	//}
    	    	
        static uint8_t* rx_ring_buffer = NULL;               
    	rx_ring_buffer = (uint8_t *) mmap(NULL, tp.tp_block_size * tp.tp_block_nr, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, sockfd3, 0);
        if (rx_ring_buffer == MAP_FAILED) {
                printf("error rx_ring_buffer mmap\n");                
        };
        
        // allocate iov structure for each block
    	rd = (struct iovec*)malloc(tp.tp_block_nr * sizeof(struct iovec));    	
    	// initilize iov structures
    	for (uint8_t i = 0; i < tp.tp_block_nr; ++i) {
        	rd[i].iov_base = rx_ring_buffer + (i * tp.tp_block_size);
        	rd[i].iov_len = tp.tp_block_size;
    	}
                       
        // fill sockaddr_ll struct to prepare binding 
   	sll.sll_family = AF_PACKET;
   	sll.sll_protocol = htons(ETH_P_IP);
   	sll.sll_ifindex =  ifr.ifr_ifindex;        	
   	if (bind(sockfd3, (struct sockaddr *)&sll, sizeof(struct sockaddr_ll)) != 0)
   	{
   		printf("Error binding\n");    
   	}
   	       	
    	memset(&pfd, 0, sizeof(pfd));
    	pfd.fd = sockfd3;
    	pfd.events = POLLIN | POLLRDNORM | POLLERR;
    	pfd.revents = 0;   	   	                              	
        
        current_block_num = 0;
        
    	while (1)
    	{
    		 struct block_desc *pbd = (struct block_desc *) rd[current_block_num].iov_base;
		   		
    		 if ((pbd->h1.block_status & TP_STATUS_USER) == 0) {       		    		    
	            poll(&pfd, 1, -1);	        
            	    continue;
        	 }  
        	
        	 walk_block(pbd, current_block_num);
        	 flush_block(pbd);
        	 current_block_num = (current_block_num + 1) % BLOCKNUM;
    	
        }  	
      	    	    	 			
} 
*/
/*
static void groovy_udp_server_init_xdp()
{
	LOG(1, "[UDP][SERVER_XDP][%s]\n", "START");	
	
	sockfd_xdp = socket(PF_XDP, SOCK_RAW, 0);	
				
    	if (sockfd_xdp < 0)
    	{
    		int err=errno;    		
    		printf("socket error %s\n", strerror(err));       		
    	}  else
    	{
    		printf("funcking shit %d\n", sockfd_xdp);
    	}
    	
    	while (1) {};      	
}
*/

static char* getNet(int spec)
{
	int netType = 0;
	struct ifaddrs *ifaddr, *ifa, *ifae = 0, *ifaw = 0;
	static char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1)
	{
		printf("getifaddrs: error\n");
		return NULL;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL) continue;
		if (!memcmp(ifa->ifa_addr->sa_data, "\x00\x00\xa9\xfe", 4)) continue; // 169.254.x.x

		if ((strcmp(ifa->ifa_name, "eth0") == 0)     && (ifa->ifa_addr->sa_family == AF_INET)) ifae = ifa;
		if ((strncmp(ifa->ifa_name, "wlan", 4) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) ifaw = ifa;
	}

	ifa = 0;
	netType = 0;
	if (ifae && (!spec || spec == 1))
	{
		ifa = ifae;
		netType = 1;
	}

	if (ifaw && (!spec || spec == 2))
	{
		ifa = ifaw;
		netType = 2;
	}

	if (spec && ifa)
	{
		strcpy(host, "IP: ");
		getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host + strlen(host), NI_MAXHOST - strlen(host), NULL, 0, NI_NUMERICHOST);
	}

	freeifaddrs(ifaddr);
	return spec ? (ifa ? host : 0) : (char*)netType;
}

static void groovy_udp_server_init()
{	
	int flags, size, beTrueAddr;	
	char *net;	    	
	net = getNet(1);
	if (net)
	{
		LOG(1, "[UDP][SERVER][START %s]\n", net);		
	}
	else
	{		
		net = getNet(2);
		if (net)
		{
			LOG(1, "[UDP][SERVER][START %s]\n", net);		
		}
		else
		{
			goto init_error;   
		}
	}
	
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);				
    	if (sockfd < 0)
    	{
    		printf("socket error\n");    
    		goto init_error;   		
    	}     
    	  	
	//jumbo 		    				    	        	
	struct ifreq ifr;	       
        memset(&ifr, 0, sizeof(ifr));                
        strncpy((char *)ifr.ifr_name, "eth0", IFNAMSIZ);
	if (ioctl (sockfd, SIOCGIFMTU, &ifr) < 0)
    	{
    		printf("error get mtu\n");
    		goto init_error;
	}		
	if ((doJumboFrames && ifr.ifr_mtu == 1500) || (!doJumboFrames && ifr.ifr_mtu != 1500)) // kernel 5.13 stmmac needs stop eth for change mtu (fixed on new kernels)
	{		
		ifr.ifr_flags = 1 & ~IFF_UP;    
		if (ioctl (sockfd, SIOCSIFFLAGS, &ifr) < 0)
    		{
    			printf("error eth0 down\n");
    			goto init_error;
		}
		LOG(1, "[ETH0][%s]\n", "DOWN");				
		ifr.ifr_mtu = doJumboFrames ? 4042 : 1500; 
		if (ioctl(sockfd, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		{
			ifr.ifr_mtu = doJumboFrames ? 3800 : 1500;
			if (ioctl(sockfd, SIOCSIFMTU, (caddr_t)&ifr) < 0)
			{				
				ifr.ifr_mtu = 1500; 							
				doJumboFrames = 0;
			} 			
		}		
		LOG(1, "[ETH][MTU 1500 -> %d]\n", ifr.ifr_mtu);
		ifr.ifr_flags = 1 | IFF_UP;    
		if (ioctl (sockfd, SIOCSIFFLAGS, &ifr) < 0)
    		{
    			printf("error eth0 up\n");
    			goto init_error;
		}	
		LOG(1, "[ETH0][%s]\n", "UP");	
		close(sockfd);
		goto init_error;						    			
	}	
		        		    
    	memset(&servaddr, 0, sizeof(servaddr));
    	servaddr.sin_family = AF_INET;
    	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    	servaddr.sin_port = htons(UDP_PORT);    	    	
    	  	
        // Non blocking socket                                                                                                                         	
    	flags = fcntl(sockfd, F_GETFD, 0);    	
    	if (flags < 0) 
    	{
      		printf("get falg error\n");    
      		goto init_error;   		
    	}
    	flags |= O_NONBLOCK;
    	if (fcntl(sockfd, F_SETFL, flags) < 0) 
    	{
       		printf("set nonblock fail\n");   
       		goto init_error;    		
    	}   	
    	   		     	    	        	 	    		    	    		
	// Settings	
	size = 2 * 1024 * 1024;	
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUFFORCE, (void*)&size, sizeof(size)) < 0)     
        {
        	printf("Error so_rcvbufforce\n");  
        	goto init_error;      	
        }  
                                    	
	beTrueAddr = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&beTrueAddr,sizeof(beTrueAddr)) < 0)
	{
        	printf("Error so_reuseaddr\n");    
        	goto init_error;    	
        }                
        
        if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, (char*)&beTrueAddr,sizeof(beTrueAddr)) < 0)
	{
        	printf("Error so_ip_tos\n");      
        	goto init_error;  	
        }     
       /*
        int busyPoll = 100;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, (void*)&busyPoll,sizeof(busyPoll)) < 0)
	{
        	printf("Error so_busy_poll\n");        	
        }   
        
                            	                         	         
#define SO_PREFER_BUSY_POLL     69
#define SO_BUSY_POLL_BUDGET     70
        
        int preferBusyPoll = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_PREFER_BUSY_POLL, (void*)&preferBusyPoll,sizeof(preferBusyPoll)) < 0)
	{
        	printf("Error so_prefer_busy_poll\n");        	
        }   

   	int busyPollBudget = 600;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL_BUDGET, (void*)&busyPollBudget,sizeof(busyPollBudget)) < 0)
	{
        	printf("Error so_busy_poll_budget\n");        	
        }                          	                         	         
*/
    	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    	{
    		printf("bind error\n");      
    		goto init_error;  	
    	}         	    	    	 	    	    	    	    	
	
	groovyServer = 2;  	
	
init_error:   
	{}	
    	   	    	    	 			
}

static void groovy_udp_server_init_inputs()
{		
	int flags, beTrueAddr;
	sockfdInputs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);				
    	if (sockfdInputs < 0)
    	{
    		printf("socket error\n");     
    		goto inputs_error;  		
    	}    	    				    	        	
	        		    
    	memset(&servaddrInputs, 0, sizeof(servaddrInputs));
    	servaddrInputs.sin_family = AF_INET;
    	servaddrInputs.sin_addr.s_addr = htonl(INADDR_ANY);
    	servaddrInputs.sin_port = htons(UDP_PORT_INPUTS);
    	  	
        // Non blocking socket                                                                                                                 	
    	flags = fcntl(sockfdInputs, F_GETFD, 0);    	
    	if (flags < 0) 
    	{
      		printf("get falg error\n");    
      		goto inputs_error;    		
    	}
    	flags |= O_NONBLOCK;
    	if (fcntl(sockfdInputs, F_SETFL, flags) < 0) 
    	{
       		printf("set nonblock fail\n");   
       		goto inputs_error;     		
    	}   	    	    		     	    	        	 	    		    	    		
        
        beTrueAddr = 1;                            		
	if (setsockopt(sockfdInputs, SOL_SOCKET, SO_REUSEADDR, (void*)&beTrueAddr,sizeof(beTrueAddr)) < 0)
	{
        	printf("Error so_reuseaddr\n");    
        	goto inputs_error;     	
        }              	
        
        if (setsockopt(sockfdInputs, IPPROTO_IP, IP_TOS, (char*)&beTrueAddr,sizeof(beTrueAddr)) < 0)
	{
        	printf("Error so_ip_tos\n");     
        	goto inputs_error;    	
        }     
                            	                         	         
    	if (bind(sockfdInputs, (struct sockaddr *)&servaddrInputs, sizeof(servaddrInputs)) < 0)
    	{
    		printf("bind error\n");       
    		goto inputs_error;  	
    	}         	    	

inputs_error:   
    	isConnectedInputs = 0;        		 	    	    	    	    	    	  	    	    	 			
}

static void groovy_start()
{	
	if (!groovyServer)
	{		
		printf("Groovy-Server 0.3 starting\n");
	
		// get HPS Server Settings
		groovy_FPGA_hps();   
	
		// reset fpga    
    		groovy_FPGA_init(0, 0, 0, 0);    	    	
	
		// map DDR 		
		groovy_map_ddr();		
	
		groovyServer = 1;
	}	
	
    	// UDP Server     	
	groovy_udp_server_init();  
	//groovy_udp_server_init3();  
	//groovy_udp_server_init_xdp();  
	
	if (groovyServer != 2)
	{
		goto start_error;
	}
	
	if (doPs2Inputs || doJoyInputs)
	{
		groovy_udp_server_init_inputs();  
	} 	  	  		    	    	    	           	    	    	    	    	    	        	    	

	// load LOGO	
	if (doScreensaver)
	{
		loadLogo(1);
		groovy_FPGA_init(1, 0, 0, 0);    			      		
		groovy_FPGA_blit(); 
		groovy_FPGA_logo(1); 			
		groovyLogo = 1;			
	}	
		    	
    	printf("Groovy-Server 0.3 started\n");    			    	                          		

start_error:
    	{}
}

void groovy_poll()
{	
	if (groovyServer != 2)
	{
		groovy_start();
		return;
	}		    	                                                                          	                                               					   											  	  		  	  								
  	
  	int len = 0; 					
	char* recvbufPtr;
	
	do
	{												
		if (doVerbose == 3 && isConnected)		
		{		
			groovy_FPGA_status(0);																		 
			//LOG(3, "[GET_STATUS][DDR fr=%d bl=%d][GPU vc=%d fr=%d fskip=%d vb=%d fd=%d][VRAM px=%d queue=%d sync=%d free=%d eof=%d][LZ4 state_1=%d inf=%d wr=%d, run=%d resume=%d t1=%d t2=%d cmd_fskip=%d stop=%d AB=%d com=%d grav=%d lleg=%d, sub=%d blit=%d]\n", poc->PoC_frame_ddr, numBlit, fpga_vga_vcount, fpga_vga_frame, fpga_vga_frameskip, fpga_vga_vblank, fpga_vga_f1, fpga_vram_pixels, fpga_vram_queue, fpga_vram_synced, fpga_vram_ready, fpga_vram_end_frame, fpga_lz4_state, fpga_lz4_uncompressed, fpga_lz4_writed, fpga_lz4_run, fpga_lz4_resume, fpga_lz4_test1, fpga_lz4_test2, fpga_lz4_cmd_fskip, fpga_lz4_stop, fpga_lz4_AB, fpga_lz4_compressed, fpga_lz4_gravats, fpga_lz4_llegits, fpga_lz4_subframe_bytes, fpga_lz4_subframe_blit);     			     						
			LOG(3, "[GET_STATUS][DDR fr=%d bl=%d][GPU fr=%d vc=%d fskip=%d vb=%d fd=%d][VRAM px=%d queue=%d sync=%d free=%d eof=%d][LZ4 un=%d]\n", poc->PoC_frame_ddr, numBlit, fpga_vga_frame, fpga_vga_vcount, fpga_vga_frameskip, fpga_vga_vblank, fpga_vga_f1, fpga_vram_pixels, fpga_vram_queue, fpga_vram_synced, fpga_vram_ready, fpga_vram_end_frame, fpga_lz4_uncompressed);     			     									
		}					     		    
						
		recvbufPtr = (isBlitting) ? (char *) (buffer + HEADER_OFFSET + poc->PoC_buffer_offset + poc->PoC_bytes_recv) : (char *) &recvbuf[0];											
		len = recvfrom(sockfd, recvbufPtr, 65536, 0, (struct sockaddr *)&clientaddr, &clilen);							
									
		if (len > 0) 
		{    				
			if (isBlitting)
			{								
				//udp error lost detection 				
				if (len > 0 && len < 1472)
				{	
					int prev_len = len;
					int tota_len = 0;							
					if (isBlitting == 1 && !blitCompression && poc->PoC_bytes_recv + len != poc->PoC_bytes_len) // raw rgb
					{
						if (!hpsBlit)
						{
							groovy_FPGA_blit(poc->PoC_bytes_len, 65535);
						}	
						isBlitting = 0;
						prev_len = poc->PoC_bytes_len % 1472;
						tota_len = poc->PoC_bytes_len;
					}					
					if (isBlitting == 1 && blitCompression && poc->PoC_bytes_recv + len != poc->PoC_bytes_lz4_len) // lz4 rgb
					{
						if (!hpsBlit)
						{
							groovy_FPGA_blit_lz4(poc->PoC_bytes_lz4_len, 65535);
						}	
						isBlitting = 0;						
						prev_len = poc->PoC_bytes_lz4_len % 1472;
						tota_len = poc->PoC_bytes_lz4_len;
					}																										
					if (isBlitting == 2 && poc->PoC_bytes_recv + len != poc->PoC_bytes_audio_len) // audio
					{
						isBlitting = 0;
						prev_len = poc->PoC_bytes_audio_len % 1472;
						tota_len = poc->PoC_bytes_audio_len;
					}					
					if (!isBlitting)
					{
						isCorePriority = 0;							
						if (len != prev_len && len <= 26)
						{							 
							memcpy((char *) &recvbuf[0], recvbufPtr, len);
							recvbufPtr = (char *) &recvbuf[0]; 
							LOG(0,"[UDP_ERROR][RECONFIG fr=%d recv=%d/%d prev_len=%d len=%d]\n", poc->PoC_frame_ddr, poc->PoC_bytes_recv, tota_len, prev_len, len);								
						} 
						else
						{
							LOG(0,"[UDP_ERROR][fr=%d recv=%d/%d len=%d]\n", poc->PoC_frame_ddr, poc->PoC_bytes_recv, tota_len, len);	
							len = -1;
						}
					}															
				}
			}	
											
			if (!isBlitting)
			{				
	    			switch (recvbufPtr[0]) 
	    			{		
		    			case CMD_CLOSE:
					{	
						if (len == 1)
						{	
							LOG(1, "[CMD_CLOSE][%d]\n", recvbufPtr[0]);				   											        							
							setClose();																					
						}	
					}; break;
					
					case CMD_INIT:
					{	
						if (len == 4 || len == 5)
						{
							if (doVerbose)
							{
								initVerboseFile();
							}
							uint8_t compression = recvbufPtr[1];														
							uint8_t audio_rate = recvbufPtr[2];
							uint8_t audio_channels = recvbufPtr[3];	
							uint8_t rgb_mode = (len == 5) ? recvbufPtr[4] : 0;														
							setInit(compression, audio_rate, audio_channels, rgb_mode);							
							sendACK(0, 0);				
							LOG(1, "[CMD_INIT][%d][LZ4=%d][Audio rate=%d chan=%d][%s]\n", recvbufPtr[0], compression, audio_rate, audio_channels, (rgb_mode == 1) ? "RGBA888" : (rgb_mode == 2) ? "RGB565" : "RGB888");											       										
						}	
					}; break;
					
					case CMD_SWITCHRES:
					{	
						if (len == 26)
						{			       	       				       			
				       			setSwitchres();			
				       			LOG(1, "[CMD_SWITCHRES][%d]\n", recvbufPtr[0]);		       						       				       						       						       		
				       		}	
					}; break;
					
					case CMD_AUDIO:
					{	
						if (len == 3)
						{							
							uint16_t udp_bytes_samples = ((uint16_t) recvbufPtr[2]  << 8) | recvbufPtr[1];							
							setBlitAudio(udp_bytes_samples);					
							LOG(1, "[CMD_AUDIO][%d][Bytes=%d]\n", recvbufPtr[0], udp_bytes_samples);	
						}
					}; break;											
					
					case CMD_GET_STATUS:
					{	
						if (len == 1)
						{
							groovy_FPGA_status(1);					
							sendACK(0, 0);							        											
				       			LOG(1, "[CMD_GET_STATUS][%d][GPU fr=%d vc=%d fskip=%d vb=%d fd=%d][VRAM px=%d queue=%d sync=%d free=%d eof=%d][AUDIO=%d][LZ4 inf=%d]\n", recvbufPtr[0], fpga_vga_frame, fpga_vga_vcount, fpga_vga_frameskip, fpga_vga_vblank, fpga_vga_f1, fpga_vram_pixels, fpga_vram_queue, fpga_vram_synced, fpga_vram_ready, fpga_vram_end_frame, fpga_audio, fpga_lz4_uncompressed);				       		
							
						}	
					}; break;
					
					case CMD_BLIT_VSYNC:
					{	
						if (len == 7 || len == 11 || len == 9)
						{	
							uint32_t udp_lz4_size = 0;				
							uint32_t udp_frame = ((uint32_t) recvbufPtr[4]  << 24) | ((uint32_t)recvbufPtr[3]  << 16) | ((uint32_t)recvbufPtr[2]  << 8) | recvbufPtr[1];
							uint16_t udp_vsync = ((uint16_t) recvbufPtr[6]  << 8) | recvbufPtr[5];
							if (!blitCompression)
							{
								LOG(1, "[CMD_BLIT][%d][Frame=%d][Vsync=%d]\n", recvbufPtr[0], udp_frame, udp_vsync);
							}							
					       		else if (len == 11 && blitCompression)
							{
								udp_lz4_size = ((uint32_t) recvbufPtr[10]  << 24) | ((uint32_t)recvbufPtr[9]  << 16) | ((uint32_t)recvbufPtr[8]  << 8) | recvbufPtr[7];
								LOG(1, "[CMD_BLIT][%d][Frame=%d][Vsync=%d][CSize=%d]\n", recvbufPtr[0], udp_frame, udp_vsync, udp_lz4_size);							
							}																													       		
					       		setBlit(udp_frame, udp_lz4_size);					       			
					       		groovy_FPGA_status(1);
					       		sendACK(udp_frame, udp_vsync);	
					       	}					       					       				       					       		
					}; break;										
					
					default: 
					{
						LOG(1,"command: %i (len=%d)\n", recvbufPtr[0], len);						
					}										
				}				
			}			
			else
			{						        																																		
				if (poc->PoC_bytes_len > 0) // modeline?
				{       
					if (isBlitting == 1) 
					{
						if (blitCompression)
						{
							setBlitLZ4(len);																					
						}
						else
						{
							setBlitRaw(len); 
						}
					}  										
					else 
					{
						setBlitRawAudio(len);
					} 					
				}
				else
				{					
					LOG(1, "[UDP_BLIT][%d bytes][Skipped no modeline]\n", len);
					isBlitting = 0;
					isCorePriority = 0;					
				}																				
			}							
		} 							       						
								
	} while (isCorePriority);
	
	if (doScreensaver && groovyLogo)
	{
		loadLogo(0);
	}		
				        
	if (doVerbose > 0 && CheckTimer(logTime))				
   	{   	      		
		fflush(fp);	
		logTime = GetTimer(LOG_TIMER);		
   	} 	   	    	
   	              
} 

void groovy_send_joystick(unsigned char joystick, uint32_t map)
{				
	poc->PoC_joystick_order++;
	if (joystick == 0)
	{
		poc->PoC_joystick_map1 = map;
	}
	if (joystick == 1)
	{
		poc->PoC_joystick_map2 = map;
	}
		
	if (isConnectedInputs && doJoyInputs)
	{		
		groovy_send_joysticks();
		LOG(2, "[JOY_ACK][%d][map=%d]\n", joystick, map);	
	} 
	else
	{
		LOG(2, "[JOY][%d][map=%d]\n", joystick, map);	
	}
}

void groovy_send_analog(unsigned char joystick, unsigned char analog, char valueX, char valueY)
{	
	poc->PoC_joystick_order++;
	if (joystick == 0)
	{
		if (analog == 0)
		{
			poc->PoC_joystick_l_analog_X1 = valueX;
			poc->PoC_joystick_l_analog_Y1 = valueY;
		}
		else
		{
			poc->PoC_joystick_r_analog_X1 = valueX;
			poc->PoC_joystick_r_analog_Y1 = valueY;
		}	
	}
	if (joystick == 1)
	{
		if (analog == 0)
		{
			poc->PoC_joystick_l_analog_X2 = valueX;
			poc->PoC_joystick_l_analog_Y2 = valueY;
		}
		else
		{
			poc->PoC_joystick_r_analog_X2 = valueX;
			poc->PoC_joystick_r_analog_Y2 = valueY;
		}	
	}
	
	if (isConnectedInputs && doJoyInputs == 2)
	{		
		groovy_send_joysticks();
		LOG(2, "[JOY_%s_ACK][%d][x=%d,y=%d]\n", (analog) ? "R" : "L", joystick, valueX, valueY);	
	} 
	else
	{
		LOG(2, "[JOY_%s][%d][x=%d,y=%d]\n", (analog) ? "R" : "L", joystick, valueX, valueY);	
	}			
}

void groovy_send_keyboard(uint16_t key, int press)
{
	poc->PoC_ps2_order++;
	int index = key2sdl[key];
	int bit = 1 & (poc->PoC_ps2_keyboard_keys[index / 8] >> (index % 8));
	if (bit)
	{
		if (!press)
		{
			poc->PoC_ps2_keyboard_keys[index / 8] ^= 1 << (index % 8);
		}
	}	
	else
	{
		if (press)
		{
			poc->PoC_ps2_keyboard_keys[index / 8] ^= 1 << (index % 8);
		}
	}
		
	if (isConnectedInputs && doPs2Inputs)
	{
		groovy_send_ps2();
		LOG(2, "[KBD_ACK][key=%d sdl=%d (%d->%d)]\n", key, index, bit, press);
	}
	else
	{
		LOG(2, "[KBD][key=%d sdl=%d (%d->%d)]\n", key, index, bit, press);
	}
}

void groovy_send_mouse(unsigned char ps2, unsigned char x, unsigned char y, unsigned char z)
{
	bitByte bits;
	bits.byte = ps2;
	poc->PoC_ps2_order++;
	poc->PoC_ps2_mouse = ps2;
	poc->PoC_ps2_mouse_x = x;
	poc->PoC_ps2_mouse_y = y;
	poc->PoC_ps2_mouse_z = z;
	if (isConnectedInputs && doPs2Inputs == 2)
	{
		groovy_send_ps2();
		LOG(2, "[MIC_ACK][yo=%d,xo=%d,ys=%d,xs=%d,1=%d,bm=%d,br=%d,bl=%d][x=%d,y=%d,z=%d]\n", bits.u.bit7, bits.u.bit6, bits.u.bit5, bits.u.bit4, bits.u.bit3, bits.u.bit2, bits.u.bit1, bits.u.bit0, x , y , z);	
	}
	else
	{
		LOG(2, "[MIC][yo=%d,xo=%d,ys=%d,xs=%d,1=%d,bm=%d,br=%d,bl=%d][x=%d,y=%d,z=%d]\n", bits.u.bit7, bits.u.bit6, bits.u.bit5, bits.u.bit4, bits.u.bit3, bits.u.bit2, bits.u.bit1, bits.u.bit0, x , y , z);	
	}	
}	






     