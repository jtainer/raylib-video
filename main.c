// 
// Example of video playback using avcodec and raylib
//
// 2023, Jonathan Tainer
//

#include "rvideo.h"

int main(int argc, char** argv) {
	if (argc < 2) return 0;
	const char* filename = argv[1];

	VideoStream* stream = OpenVideoStream(filename);
	if (stream == NULL) return 1;

	InitWindow(stream->width, stream->height, filename);
	SetTargetFPS(30);

	Texture texture = LoadTextureFromVideoStream(stream);

	while (!WindowShouldClose() && UpdateTextureFromVideoStream(&texture, stream)) {
		BeginDrawing();
		ClearBackground(BLACK);
		DrawTexture(texture, 0, 0, WHITE);
		EndDrawing();
	}

	UnloadTexture(texture);
	CloseWindow();
	UnloadVideoStream(stream);

	return 0;
}
