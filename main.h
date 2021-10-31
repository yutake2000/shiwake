#pragma once

#include <ctime>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <GL/glut.h>

#define WINDOW_X (500)
#define WINDOW_Y (500)
#define WINDOW_NAME "main"
#define ARRAY_MAX 100000
#define TEXTURE_COUNT 4

using namespace cv;
using namespace std;

struct Color {
	GLfloat r, g, b, a;
};

struct Model {
	int vCount, fCount;
	float vertex[ARRAY_MAX];
	int faces[ARRAY_MAX];
	float normals[ARRAY_MAX];
	vector<pair<int, string>> materials;
	map<string, pair<Color, Color>> colors;
};

struct FlyingObject {
	int id;
	Model &model;
	time_t timeSpawn, timeCaught;
	float x, y, z;
	float vx, vy, vz;
	bool hit;
};

time_t g_time();

void getNv(int v0, int v1, int v2, float vertex[], float Nv[]);
void spherical_coordinates(float d, float th, float phi, GLfloat coord[]);

void set_texture();
void initOnce(); // 起動時に一回だけ初期化する
void init(); // 音楽を再生するたびに初期化する
void initCV(VideoCapture &cap, Mat &avg_img, Mat &sgm_img, Mat &tmp_img); // 背景差分法関連の初期化を行う
void init_GL(int argc, char *argv[]);

void set_callback_functions();
void glut_display();
void glut_keyboard(unsigned char key, int x, int y);
void glut_mouse(int button, int state, int x, int y);
void glut_motion(int x, int y);
void glut_idle();

void loadObj(Model &model, string filename);
void loadMtl(Model &model, string mtllib);

void draw_plane();
void draw_hands();
void draw_model(Model &model);

void load_score();
void startMusic();
void updateFlyingObjects();

void observeHands();
void doClap();
void doSlap();

void waitCommand(); // 標準入力でコマンドを受け付ける

void errorStatistics(time_t error); //ノーツのタイミングがどれだけずれているかを表示する

void quit();

/*!
 * 文字列描画
 * @param[in] str 文字列
 * @param[in] w,h ウィンドウサイズ
 * @param[in] x0,y0 文字列の位置(左上原点のスクリーン座標系,文字列の左下がこの位置になる)
 */
void drawString(string str, int w, int h, int x0, int y0);
void drawImage(int textureIndex, int width, int height, int x0, int y0, int w, int h);