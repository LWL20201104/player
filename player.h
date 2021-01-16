#ifndef __DECODE__
#define __DECODE__
#include <string>
extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#include "SDL.h"
}

//if 0, use CPU decode and encode, and if 1 use GPU decode and encode
#define USE_GPU 0
//if 0, no play, and if 1, use sdl play
#define SDL_PLAY 1
//if 0, no encode, and if 1, encode
#define EN_CODE 1

class Player {
public:
	Player(std::string& filename);
	~Player();

	/* decoder */
	bool initIFmtCtx();
	bool findVideoIndex();
	bool initICodecCtx();
	bool openICodec();
	bool initIPacket();
	bool initIFrame();
	void initSwsCtx();
	bool initialize();
	void decode(AVPacket* pkt);
	void doPlay();

	/* refresh thread */
	static SDL_mutex* refreshMtx;
	static int refresh(void* data);

	/* SDL play */
	void initSdl();
	bool initWindow();
	bool initRenderer();
	bool initTexture();

private:
	std::string filename;

	/* decoder */
	AVFormatContext* iFmtCtx;
	AVCodecContext* iCodecCtx;
	AVPacket* iPacket;
	AVCodec* iCodec;
	AVFrame* tmpframe;
	AVFrame* iframe;
	SwsContext* swsCtx;
	int videoIndex;

	/* refresh thread */
	static bool isRefresh;

	/* SDL play */
	SDL_Rect srcrect;
	SDL_Rect disrect;
	SDL_Window* screen;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
};

#endif //__DECODE__
