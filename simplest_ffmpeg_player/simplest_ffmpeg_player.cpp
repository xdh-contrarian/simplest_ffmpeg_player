/**
 * 最简单的基于FFmpeg的视频播放器 2
 * Simplest FFmpeg Player 2
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 第2版使用SDL2.0取代了第一版中的SDL1.2
 * Version 2 use SDL 2.0 instead of SDL 1.2 in version 1.
 *
 * 本程序实现了视频文件的解码和显示(支持HEVC，H.264，MPEG2等)。
 * 是最简单的FFmpeg视频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 * This software is a simplest video player based on FFmpeg.
 * Suitable for beginner of FFmpeg.
 *
 */



#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV420P data as a file 
#define OUTPUT_YUV420P 0

int main(int argc, char* argv[])
{
	// AVFormatContext 统领全局的基本结构体。主要用于处理封装格式（FLV/MKV/RMVB等）
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	// 视音频流对应的结构体，用于视音频编解码。
	AVCodecContext	*pCodecCtx;
	// 编解码器
	AVCodec			*pCodec;
	// AVFrame 存储非压缩的数据（视频对应RGB/YUV像素数据，音频对应PCM采样数据）
	AVFrame	*pFrame,*pFrameYUV;
	unsigned char *out_buffer;
	// AVPacket 存储压缩数据（视频对应H.264等码流数据，音频对应AAC/MP3等码流数据）
	AVPacket *packet;
	int y_size;
	int ret, got_picture;
	// libswscale模块提供图像缩放、图像格式转换功能。其中贯穿整个模块的是SwsContext结构体
	struct SwsContext *img_convert_ctx;

	char filepath[]="bigbuckbunny_480x272.h265";
	//SDL---------------------------
	int screen_w=0,screen_h=0;
	SDL_Window *screen; 
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;

	FILE *fp_yuv;

	// 用于注册所有muxers、demuxers与protocols
	// FFmpeg4.0以后改为数组存储，并且av_register_all方法已被标记为过时
	av_register_all();

	// 执行网络库的全局初始化。这是可选的，不再推荐。此函数仅用于解决旧GnuTLS或OpenSSL库的线程安全问题
	avformat_network_init();
	// 用于为AVFormatContext变量开辟内存空间
	pFormatCtx = avformat_alloc_context();
	
	// 打开输入流，读取媒体文件的头信息，但不打开编码器，需要使用编码相关的函数打开编码器
	if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}

	// 读取多媒体文件的流信息，但不会改变文件的逻辑地址
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
	if(videoindex==-1){
		printf("Didn't find a video stream.\n");
		return -1;
	}

	pCodecCtx=pFormatCtx->streams[videoindex]->codec;

	// 用于查找已经注册的解码器
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL){
		printf("Codec not found.\n");
		return -1;
	}

	// 初始化一个视音频编解码器的AVCodecContext
	// 参数1初始化的AVCodecContext,参数2输入的AVCodec
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}
	
	// av_frame_alloc AVFrame的初始化函数
	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();
	// av_malloc 最常见的内存分配函数
	// av_image_get_buffer_size 通过指定像素格式、图像宽、图像高来计算所需的内存大小
	out_buffer=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,  pCodecCtx->width, pCodecCtx->height,1));
	// av_image_fill_arrays 根据指定的图像参数和提供的数组设置数据指针和行大小
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,out_buffer,
		AV_PIX_FMT_YUV420P,pCodecCtx->width, pCodecCtx->height,1);
	
	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	// av_dump_format 用于输出输入或者输出流的相信信息
	av_dump_format(pFormatCtx,0,filepath,0);
	printf("-------------------------------------------------\n");
	// libswscale是一个主要用于处理图片像素数据的类库。可以完成图片像素格式的转换，图片的拉伸等工作
	// sws_getContext 初始化一个SwsContext
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 

#if OUTPUT_YUV420P 
    fp_yuv=fopen("output.yuv","wb+");  
#endif  
	
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	} 

	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,
		SDL_WINDOW_OPENGL);

	if(!screen) {  
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);  
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);  

	sdlRect.x=0;
	sdlRect.y=0;
	sdlRect.w=screen_w;
	sdlRect.h=screen_h;

	//SDL End----------------------
	// av_read_frame 读取码流中的音频若干帧或者视频一帧
	while(av_read_frame(pFormatCtx, packet)>=0){
		if(packet->stream_index==videoindex){
			// 在版本3 avcodec_decode_video2函数处于deprecated废弃状态
			// 解码一帧视频数据。输入一个压缩编码的结构体AVPacket，输出一个解码后的结构体AVFrame
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if(ret < 0){
				printf("Decode Error.\n");
				return -1;
			}
			if(got_picture){
				// sws_scale 用于转换像素的函数
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, 
					pFrameYUV->data, pFrameYUV->linesize);
				
#if OUTPUT_YUV420P
				y_size=pCodecCtx->width*pCodecCtx->height;  
				fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y 
				fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
				fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V
#endif
				//SDL---------------------------
#if 0
				SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );  
#else
				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
				pFrameYUV->data[0], pFrameYUV->linesize[0],
				pFrameYUV->data[1], pFrameYUV->linesize[1],
				pFrameYUV->data[2], pFrameYUV->linesize[2]);
#endif	
				
				SDL_RenderClear( sdlRenderer );  
				SDL_RenderCopy( sdlRenderer, sdlTexture,  NULL, &sdlRect);  
				SDL_RenderPresent( sdlRenderer );  
				//SDL End-----------------------
				//Delay 40ms
				SDL_Delay(40);
			}
		}
		// av_free_packet来释放缓存
		av_free_packet(packet);
	}
	//flush decoder
	//FIX: Flush Frames remained in Codec
	while (1) {
		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
		if (ret < 0)
			break;
		if (!got_picture)
			break;
		sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, 
			pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
		int y_size=pCodecCtx->width*pCodecCtx->height;  
		fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
		fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V
#endif
		//SDL---------------------------
		SDL_UpdateTexture( sdlTexture, &sdlRect, pFrameYUV->data[0], pFrameYUV->linesize[0] );  
		SDL_RenderClear( sdlRenderer );  
		SDL_RenderCopy( sdlRenderer, sdlTexture,  NULL, &sdlRect);  
		SDL_RenderPresent( sdlRenderer );  
		//SDL End-----------------------
		//Delay 40ms
		SDL_Delay(40);
	}
	// 释放sws_scale
	sws_freeContext(img_convert_ctx);

#if OUTPUT_YUV420P 
    fclose(fp_yuv);
#endif 

	SDL_Quit();

	// 释放内存
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	// 用于关闭编码器
	avcodec_close(pCodecCtx);
	// 用于关闭使用avformat_open_input打开的媒体文件句柄
	avformat_close_input(&pFormatCtx);

	return 0;
}

