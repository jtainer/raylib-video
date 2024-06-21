// 
// Functions for extracting video frames into raylib textures
//
// Copyright (c) 2023-2024, Jonathan Tainer. Subject to the BSD 2-Clause License.
//

#include "rvideo.h"
#include <stdio.h>
#include <rlgl.h>

VideoStream* OpenVideoStream(const char* filename) {
	VideoStream* stream = malloc(sizeof(VideoStream));

	// Open the video file
	stream->formatContext = NULL;
	if (avformat_open_input(&stream->formatContext, filename, NULL, NULL) != 0) {
		printf("Failed to open video file.\n");
		return NULL;
	}

	// Get the stream information
	if (avformat_find_stream_info(stream->formatContext, NULL) < 0) {
		printf("Failed to get stream information.\n");
		return NULL;
	}

	// Find the video stream
	stream->videoStreamIndex = -1;
	for (int i = 0; i < stream->formatContext->nb_streams; i++) {
		if (stream->formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream->videoStreamIndex = i;
			break;
		}
	}
	if (stream->videoStreamIndex == -1) {
		printf("Failed to find video stream.\n");
		return NULL;
	}

	// Get the video codec parameters
	stream->codecParameters = stream->formatContext->streams[stream->videoStreamIndex]->codecpar;
	stream->codec = avcodec_find_decoder(stream->codecParameters->codec_id);
	if (stream->codec == NULL) {
		printf("Failed to find codec.\n");
		return NULL;
	}

	// Open the codec
	stream->codecContext = avcodec_alloc_context3(stream->codec);
	if (stream->codecContext == NULL) {
		printf("Failed to allocate codec context.\n");
		return NULL;
	}
	if (avcodec_parameters_to_context(stream->codecContext, stream->codecParameters) < 0) {
		printf("Failed to copy codec parameters to codec context.\n");
		return NULL;
	}
	if (avcodec_open2(stream->codecContext, stream->codec, NULL) < 0) {
		printf("Failed to open codec.\n");
		return NULL;
	}

	// Create the scaler context
	stream->frame = av_frame_alloc();
	stream->scaledFrame = av_frame_alloc();
	int numBytes = av_image_get_buffer_size(
			AV_PIX_FMT_RGBA,
			stream->codecContext->width,
			stream->codecContext->height,
			1);
	stream->bufferRGB = av_malloc(numBytes * sizeof(uint8_t));
	av_image_fill_arrays(
			stream->scaledFrame->data,
			stream->scaledFrame->linesize,
			stream->bufferRGB,
			AV_PIX_FMT_RGBA,
			stream->codecContext->width,
			stream->codecContext->height,
			1);

	stream->scalerContext = sws_getContext(
			stream->codecParameters->width,
			stream->codecParameters->height,
			stream->codecContext->pix_fmt,
			stream->codecParameters->width,
			stream->codecParameters->height,
			AV_PIX_FMT_RGBA,
			SWS_BICUBIC,
			NULL,
			NULL,
			NULL);

	if (stream->scalerContext == NULL) {
		printf("Failed to create scaler context.\n");
		return NULL;
	}

	stream->width = stream->codecParameters->width;
	stream->height = stream->codecParameters->height;

	return stream;
}

Texture LoadTextureFromVideoStream(VideoStream* stream) {
	Texture texture = { 0 };
	texture.id = rlLoadTexture(
			NULL,
			stream->width,
			stream->height,
			PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
			1);
	texture.width = stream->width;
	texture.height = stream->height;
	texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	texture.mipmaps = 1;
	return texture;
}

int UpdateTextureFromVideoStream(Texture* texture, VideoStream* stream) {
	while(av_read_frame(stream->formatContext, &stream->packet) == 0) {
		if (stream->packet.stream_index == stream->videoStreamIndex) {
			// Decode the frame
			int response = avcodec_send_packet(stream->codecContext, &stream->packet);
			while (response >= 0) {
				response = avcodec_receive_frame(stream->codecContext, stream->frame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
					break;
				} else if (response < 0) {
					printf("Error decoding frame.\n");
					return -1;
				}

				// Scale the frame
				sws_scale(
						stream->scalerContext,
						(const uint8_t* const*) stream->frame->data,
						stream->frame->linesize,
						0,
						stream->codecParameters->height,
						stream->scaledFrame->data,
						stream->scaledFrame->linesize);

				// Copy pixel data to VRAM
				UpdateTexture(*texture, stream->scaledFrame->data[0]);
				return 1;
			}
		}
		av_packet_unref(&stream->packet);
	}
	return 0;
}

void UnloadVideoStream(VideoStream* stream) {
	sws_freeContext(stream->scalerContext);
	av_frame_free(&stream->frame);
	av_frame_free(&stream->scaledFrame);
	avcodec_close(stream->codecContext);
	avcodec_free_context(&stream->codecContext);
	avformat_close_input(&stream->formatContext);
	free(stream->bufferRGB);
	free(stream);
}

