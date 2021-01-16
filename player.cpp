#include "player.h"

bool Player::isRefresh = false;
SDL_mutex* Player::refreshMtx = nullptr;
#define REFRESH_EVENT (SDL_USEREVENT + 1)
int Player::refresh(void* data) {
    isRefresh = true;
    refreshMtx = SDL_CreateMutex();
    int delay = 1000 / *static_cast<int*>(data);
    while (true) {
        SDL_LockMutex(refreshMtx);
        if (!isRefresh) {
            SDL_UnlockMutex(refreshMtx);
            SDL_DestroyMutex(refreshMtx);
            break;
        }
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(delay);
    }

    return 0;
}

Player::Player(std::string& filename) :
	filename(filename) {
	avformat_network_init();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return;
    }

    if (!initialize()) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to initialize decode.\n");
        exit(1);
    }
}

Player::~Player() {

}

bool Player::initIFmtCtx() {
    //Allocate an AVFormatContext
    iFmtCtx = avformat_alloc_context();
    if (!iFmtCtx) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to allocate an AVFormatContext.\n");
        return false;
    }

    //Open an input stream and read the header. The codecs are not opened
    if ((avformat_open_input(&iFmtCtx, filename.c_str(), nullptr, nullptr)) < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to open camera.\n");
        return false;
    }

    //Read packets of a media file to get stream information
    if (avformat_find_stream_info(iFmtCtx, nullptr) < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to find stream information.\n");
        return false;
    }

    return true;
}

bool Player::findVideoIndex() {
    //Find the "best" stream in the file
    videoIndex = av_find_best_stream(iFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIndex < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Could not found video stream.\n");
        return false;
    }

    return true;
}

bool Player::initICodecCtx() {
    //Allocate an AVCodecContext and set its fields to default values
    iCodecCtx = avcodec_alloc_context3(nullptr);
    if (!iCodecCtx) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to allocate an AVCodecContext.\n");
        return false;
    }

    //Fill the codec context based on the values from the supplied codec parameters
    if (avcodec_parameters_to_context(iCodecCtx, iFmtCtx->streams[videoIndex]->codecpar) < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to fill the AVCodecContext.\n");
        return false;
    }

    return true;
}

bool Player::openICodec() {
    //Find a registered decoder with a matching codec ID
    iCodec = avcodec_find_decoder(iCodecCtx->codec_id);
    if (!iCodec) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to find decoder.\n");
        return false;
    }

    //Initialize the AVCodecContext to use the given AVCodec
    if (avcodec_open2(iCodecCtx, iCodec, nullptr) < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to open decoder.\n");
        return false;
    }

    return true;
}

bool Player::initIPacket() {
    //Allocate an AVPacketand set its fields to default values
    iPacket = av_packet_alloc();
    if (!iPacket) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to allocate AVpacket.\n");
        return false;
    }

    return true;
}

bool Player::initIFrame() {
    //Allocate an AVFrame and set its fields to default values
    tmpframe = av_frame_alloc();
    if (!tmpframe) {
        return false;
    }

    //Allocate an AVFrame and set its fields to default values
    iframe = av_frame_alloc();
    if (!iframe) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to allocate an AVFrame.\n");
        return false;
    }
    //Allocate new buffer(s) for video data
    iframe->width = iCodecCtx->width;
    iframe->height = iCodecCtx->height;
    iframe->format = iCodecCtx->pix_fmt;
    //Allocate new buffer(s) for audio or video data
    if (av_frame_get_buffer(iframe, 0) < 0) {
        av_log(nullptr, AV_LOG_FATAL, "Failed to allocate the video frame data.\n");
        return false;
    }

    return true;
}

void Player::initSwsCtx() {
    swsCtx = nullptr;
    swsCtx = sws_getCachedContext(swsCtx, iCodecCtx->width, iCodecCtx->height, iCodecCtx->pix_fmt,
        iCodecCtx->width, iCodecCtx->height, iCodecCtx->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
}

void Player::initSdl() {
    srcrect.x = 0;
    srcrect.y = 0;
    srcrect.w = iCodecCtx->width;
    srcrect.h = iCodecCtx->height;

    disrect.x = 0;
    disrect.y = 0;
    disrect.w = srcrect.w;
    disrect.h = srcrect.h;
}

bool Player::initWindow() {
    screen = SDL_CreateWindow("Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        disrect.w, disrect.h, SDL_WINDOW_OPENGL);
    if (!screen) {
        SDL_Log("Could not create window: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool Player::initRenderer() {
    renderer = SDL_CreateRenderer(screen, -1, 0);
    if (!renderer) {
        SDL_Log("Could not create renderer: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool Player::initTexture() {
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 
        srcrect.w, srcrect.h);
    if (!texture) {
        SDL_Log("Could not create texture: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool Player::initialize() {
    if (!initIFmtCtx())
        return false;
    
    if (!findVideoIndex())
        return false;

    if (!initICodecCtx())
        return false;

    if (!openICodec())
        return false;

    if (!initIPacket())
        return false;

    if (!initIFrame())
        return false;
}

void Player::decode(AVPacket* pkt) {
    int res = avcodec_send_packet(iCodecCtx, pkt);
    while (res >= 0) {
        res = avcodec_receive_frame(iCodecCtx, tmpframe);
        if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
            break;
        }
        if (res >= 0) {
            sws_scale(swsCtx, (const uint8_t* const*)tmpframe->data, tmpframe->linesize, 0,
                iCodecCtx->height, iframe->data, iframe->linesize);

            SDL_UpdateYUVTexture(texture, &srcrect,
                iframe->data[0], iframe->linesize[0],
                iframe->data[1], iframe->linesize[1],
                iframe->data[2], iframe->linesize[2]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, &srcrect, &disrect);
            SDL_RenderPresent(renderer);

            static int num = 1;
            av_log(nullptr, AV_LOG_INFO, "frame num: %d\n", num++);

            while (true) {
                SDL_Event event;
                SDL_WaitEvent(&event);
                if (event.type == REFRESH_EVENT)
                    break;
            }
        }
    }
}

void Player::doPlay() {
    while (av_read_frame(iFmtCtx, iPacket) == 0) {
        if (iPacket->stream_index != videoIndex) {
            av_packet_unref(iPacket);
            continue;
        }
        decode(iPacket);
        av_packet_unref(iPacket);
    }
    decode(nullptr);
}
