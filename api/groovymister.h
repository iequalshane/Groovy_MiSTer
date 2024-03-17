#ifndef __GROOVYMISTER_H__
#define __GROOVYMISTER_H__

#include <inttypes.h>

#ifdef WIN32 
 #include <winsock2.h> 
 #include <ws2tcpip.h> 
 #include <mswsock.h> 
 #include "rio.h" 
#endif
 
#define BUFFER_SIZE 1245312 // 720x576x3
#define BUFFER_SLICES 846
#define BUFFER_MTU 1472

/*! fpgaStatus :
 *  Data received after CmdInit and CmdBlit calls
 */
typedef struct fpgaStatus{
	uint32_t frame;		//frame on gpu
 	uint32_t frameEcho;	//frame received
 	uint16_t vCount;	//vertical count on gpu
 	uint16_t vCountEcho; 	//vertical received
 	
 	uint8_t vramEndFrame; 	//1-fpga has all pixels on vram for last CmdBlit
 	uint8_t vramReady;	//1-fpga has free space on vram
 	uint8_t vramSynced;	//1-fpga has synced (not red screen)
 	uint8_t vgaFrameskip;	//1-fpga used framebuffer (volatile framebuffer off)
 	uint8_t vgaVblank;	//1-fpga is on vblank
 	uint8_t vgaF1;		//1-field for interlaced
 	uint8_t audio;		//1-fgpa has audio activated
 	uint8_t vramQueue; 	//1-fpga has pixels prepared on vram
} fpgaStatus;

typedef unsigned long DWORD;

class GroovyMister
{
 public:
	
 	char *pBufferBlit; 	// This buffer are registered and aligned for sending rgb. Populate it before CmdBlit  	 	
 	char *pBufferAudio; 	// This buffer are registered and aligned for sending audio. Populate it before CmdAudio  	 	
 	fpgaStatus fpga;	// Data with last received ACK
 	
	GroovyMister();
	~GroovyMister();
	
	// Close connection
	void CmdClose(void);	
	// Init streaming with ip, port, (lz4frames = 0-raw, 1-lz4, 2-lz4hc), soundRate(1-22k, 2-44.1, 3-48 khz), soundChan(1 or 2), rgbMode(0-RGB888, 1-RGBA888)
	uint8_t CmdInit(const char* misterHost, uint16_t misterPort, uint8_t lz4Frames, uint32_t soundRate, uint8_t soundChan, uint8_t rgbMode);
	// Change resolution (check https://github.com/antonioginer/switchres) with modeline
	void CmdSwitchres(double pClock, uint16_t hActive, uint16_t hBegin, uint16_t hEnd, uint16_t hTotal, uint16_t vActive, uint16_t vBegin, uint16_t vEnd, uint16_t vTotal, uint8_t interlace);
	// Stream frame, vCountSync = 0 for auto frame delay or number of vertical line to sync with, margin with nanoseconds for auto frame delay)
	void CmdBlit(uint32_t frame, uint16_t vCountSync, uint32_t margin);
	// Stream audio
	void CmdAudio(uint16_t soundSize);
	// getACK is used internal on WaitSync, dwMilliseconds = 0 will time out immediately if no new data
	uint32_t getACK(DWORD dwMilliseconds); 
	// sleep to sync with crt raster 
	void WaitSync();
	
	void setVerbose(uint8_t sev);
 
 private:

        uint8_t m_verbose;

#ifdef WIN32 
        SOCKET m_sockFD;         
        RIO_EXTENSION_FUNCTION_TABLE m_rio;
	RIO_CQ m_sendQueue;
	RIO_CQ m_receiveQueue;
	RIO_RQ m_requestQueue;
	HANDLE m_hIOCP;
	RIO_BUFFERID m_sendRioBufferId;
	RIO_BUF m_sendRioBuffer; 
	RIO_BUFFERID m_receiveRioBufferId;
	RIO_BUF m_receiveRioBuffer; 
	RIO_BUFFERID m_sendRioBufferBlitId;		
	RIO_BUF m_sendRioBufferBlit; 
	RIO_BUF *m_pBufsBlit;
	RIO_BUFFERID m_sendRioBufferAudioId;		
	RIO_BUF m_sendRioBufferAudio; 
	RIO_BUF *m_pBufsAudio;
	
	LARGE_INTEGER m_tickStart;
        LARGE_INTEGER m_tickEnd;
        LARGE_INTEGER m_tickSync;
#else
        int m_sockFD;
        
        struct timespec m_tickStart;                     
        struct timespec m_tickEnd;     
        struct timespec m_tickSync;     
#endif
        struct sockaddr_in m_serverAddr; 
        char m_bufferSend[26]; 
        char m_bufferReceive[13]; 
        char *m_pBufferLZ4;
        char *m_pBufferAudio;
        uint8_t m_lz4Frames;
        uint8_t m_soundChan;
        uint8_t m_rgbMode;
        uint32_t m_RGBSize;
        uint8_t  m_interlace;
        uint16_t m_vTotal;
        uint32_t m_frame;        
        uint32_t m_frameTime;        
        uint32_t m_widthTime;        
        uint32_t m_streamTime;
        uint32_t m_emulationTime;             
                                  
	char *AllocateBufferSpace(const DWORD bufSize, const DWORD bufCount, DWORD& totalBufferSize, DWORD& totalBufferCount);
	void Send(void *cmd, int cmdSize);
	void SendStream(uint8_t whichBuffer, uint32_t bytesToSend, uint32_t cSize);
	void setTimeStart(void);
	void setTimeEnd(void);
	uint32_t DiffTime(void);
	void setFpgaStatus(void);
	int DiffTimeRaster(void);
};

#endif