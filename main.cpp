#include "main.h"
#include "yswavfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string.h>
#include <math.h>
#include <future>
#include <sys/time.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

bool g_enableCV = true;
bool g_enableGuideSphere = false;
time_t g_videoTimelag = 130;
time_t g_tolerance = 200;
time_t g_tolerancePerfect = 100;
int g_handTh = 2000;
int g_slapTh = 10;
int INIT_TIME = 50;
double B_PARAM = 1.0 / 50.0;
int T_PARAM_EXP = 76;
int Zeta = 15;
double g_angle1 = 0.0;
double g_angle2 = 0.0;
double g_angle3 = 0.0;
double g_angle4 = 3.141592/4;
double g_distance = 8.6;
string g_objDir = "obj/";
string g_scoreDir = "score/";
string g_musicDir = "music/";
string g_musicFilename = g_musicDir + "bo-tto_hidamari.wav";
string g_scoreFilename = g_scoreDir + "bo-tto_hidamari.txt";
string g_video_device = "/dev/video1";
const char* textureFileNames[TEXTURE_COUNT] = {"img/floor.jpg", "img/wall.jpg", "img/kuukiyomi1.jpg", "img/kuukiyomi2.jpg"};

bool g_isLeftButtonOn = false;
bool g_isRightButtonOn = false;
float g_clap = 0, g_slap = 0;
Model g_leftHand;
Model g_rightHand;
Model g_resistor;
Model g_resistorZero;
list<FlyingObject> g_flyingObjects;
GLuint g_TextureHandles[TEXTURE_COUNT] = {0,0};
thread g_musicThread;
time_t g_beginTime = 1LL<<60;
bool g_musicPlaying = false;
bool g_waitQuit = false;
int g_BPM = 0;
double g_quarterlength = 0;
time_t g_offsetLength = 0;
time_t g_skipLength = 0;
double g_varLength = 0;
int g_countGood = 0, g_countPerfect = 0, g_countMiss = 0;

int main(int argc, char *argv[])
{

	int opt;
	while ((opt = getopt(argc, argv, "d:l:s:t:g")) != -1) {
		switch (opt) {
			case 'd':
				g_video_device = optarg;
				break;
			case 'l':
				g_musicFilename = g_musicDir + string(optarg) + ".wav";
				g_scoreFilename = g_scoreDir + string(optarg) + ".txt";
				break;
			case 's':
				g_skipLength = atoi(optarg);
				break;
			case 't':
				g_videoTimelag = atoi(optarg);
				break;
			case 'g':
				g_enableCV = false;
				break;
		}
	}

	/* OpenGLの初期化 */
	init_GL(argc,argv);

	/* このプログラム特有の初期化 */
	initOnce();
	init();

	/* コールバック関数の登録 */
	set_callback_functions();

	loadObj(g_rightHand, "rightHand.obj");
	loadObj(g_leftHand, "leftHand.obj");
	loadObj(g_resistor, "resistor.obj");
	loadObj(g_resistorZero, "resistorZero.obj");

	auto commandThread =thread(waitCommand);

  	if (g_enableCV) {
	  auto cvThread = thread(observeHands);
	  glutMainLoop();
	  cvThread.join();
	} else {
	  glutMainLoop();
	}

	g_musicThread.join();

  return 0;
}

void waitCommand() {
	string command;
	while (true) {
		cin >> command;
		if (command == "load") {
			string name;
			cin >> name;
			g_musicFilename = g_musicDir + name + ".wav";
			g_scoreFilename = g_scoreDir + name + ".txt";
		}
	}
}

void initOnce() {
	glGenTextures(TEXTURE_COUNT, g_TextureHandles);
	for(int i = 0; i < TEXTURE_COUNT; i++){
		glBindTexture(GL_TEXTURE_2D, g_TextureHandles[i]);
	}

	set_texture();
}

void set_texture(){
	for(int i = 0; i < TEXTURE_COUNT; i++){
		cv::Mat input = cv::imread(textureFileNames[i], 1);
		// BGR -> RGBAの変換
		cv::cvtColor(input, input, CV_BGR2RGBA);
		int w = input.cols;
		int h = input.rows;

		glBindTexture(GL_TEXTURE_2D, g_TextureHandles[i]);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
		GL_RGBA, GL_UNSIGNED_BYTE, input.data);
	}
}

void init() {
	glClearColor(0.0, 0.0, 0.0, 0.0);         // 背景の塗りつぶし色を指定
	
	g_musicPlaying = false;
	if (g_musicThread.joinable()) {
		g_musicThread.join();
	}
	g_countGood = 0;
	g_countPerfect = 0;
	g_countMiss = 0;
	g_flyingObjects.clear();
	g_beginTime = 1LL<<60;
}

void doClap() {
	g_clap = 1;
	time_t now = g_time();
	for (auto &o : g_flyingObjects) {
		time_t error = abs(now - o.timeCaught);
		if (error < g_tolerance && !o.hit) {
			o.vx = 0;
			o.vy = 0;
			o.vz = 0;
			o.hit = true;
			if (o.id == 0) {
				errorStatistics(now - o.timeCaught);
				if (error < g_tolerancePerfect) g_countPerfect++;
				else if (error < g_tolerance) g_countGood++;
			} else {
				g_countMiss++;
			}
			break;
		}
	}
}

void doSlap() {
	g_slap = 1;
	time_t now = g_time();
	for (auto &o : g_flyingObjects) {
		time_t error = abs(now - o.timeCaught);
		if (error < g_tolerance && !o.hit) {
			o.vx = -0.02;
			o.vy -= 0.04;
			o.vz += 0.02;
			o.hit = true;
			if (o.id == 1) {
				errorStatistics(now - o.timeCaught);
				if (error < g_tolerancePerfect) g_countPerfect++;
				else if (error < g_tolerance) g_countGood++;
			} else {
				g_countMiss++;
			}
			break;
		}
	}
}

void initCV(VideoCapture &cap, Mat &avg_img, Mat &sgm_img, Mat &tmp_img) {

  Mat frame;
  Size s = avg_img.size();
  cv::Rect cutRect(s.width, 0, s.width, s.height);
  
  // 2. prepare window for showing images
  cv::namedWindow("original", 1);
  cv::namedWindow("cut", 1);

  // 3. calculate initial value of background

  printf("Background statistics initialization start\n");

  avg_img = cv::Scalar(0, 0, 0);

  for( int i = 0; i < INIT_TIME; i++){
    cap >> frame;
	imshow("original", frame);
    if (frame.empty()) {
      printf("Not enough frames\n");
      exit(1);
    }
	frame = Mat(frame, cutRect);
	imshow("cut", frame);
    cv::Mat tmp;
    frame.convertTo(tmp, avg_img.type());
    cv::accumulate(tmp, avg_img);
	cv::waitKey(33); // この間に画像を描画してもらう
  }

  avg_img.convertTo(avg_img, -1, 1.0 / INIT_TIME);
  sgm_img = cv::Scalar(0, 0, 0);

  for( int i = 0; i < INIT_TIME; i++){
    cap >> frame;
	imshow("original", frame);
    if (frame.empty()) {
      printf("Not enough frames\n");
      exit(1);
    }
	frame = Mat(frame, cutRect);
	imshow("cut", frame);
    frame.convertTo(tmp_img, avg_img.type());
    cv::subtract(tmp_img, avg_img, tmp_img);
    cv::pow(tmp_img, 2.0, tmp_img);
    tmp_img.convertTo(tmp_img, -1, 2.0);
    cv::sqrt(tmp_img, tmp_img);
    cv::accumulate(tmp_img, sgm_img);
	char key =cv::waitKey(33);
  }

  sgm_img.convertTo(sgm_img, -1, 1.0 / INIT_TIME);

  cv::destroyWindow("original");
  cv::destroyWindow("cut");

  printf("Background statistics initialization finish\n");
}

void init_GL(int argc, char *argv[]){
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	glutInitWindowSize(WINDOW_X,WINDOW_Y);
	glutCreateWindow(WINDOW_NAME);
}

void set_callback_functions(){
	glutDisplayFunc(glut_display);
	glutKeyboardFunc(glut_keyboard);
	glutMouseFunc(glut_mouse);
	glutMotionFunc(glut_motion);
	glutPassiveMotionFunc(glut_motion);
	glutIdleFunc(glut_idle);
}

void loadScore() {

	ifstream fin(g_scoreFilename);
	if (!fin.is_open()) {
		cerr << "error" << endl;
		return;
	}

	string var;
	int v = 0;
	time_t timeCaught = 0;

	while (!fin.eof()) {
		fin >> var;
		if (var == "BPM") {
			fin >> g_BPM;
			g_quarterlength = 60.0 / g_BPM * 1000;
		} else if (var == "offset") {
			fin >> g_offsetLength;
		} else if (var == "rhythm") {
			int a, b; // a/b 拍子
			fin >> a >> b;
			g_varLength = g_quarterlength * a;
		} else if (var == "score") {
			break;
		}
	}

	while (!fin.eof()) {

		fin >> var;
		if (var == "" || var[0] == '#') continue;
		
		timeCaught = g_offsetLength + v * g_varLength;
		if (g_enableCV) timeCaught += g_videoTimelag;

		for (int i=0; i<var.length(); i++) {
			if (var[i] != '-') {
				int id = (var[i] == 'o' ? 0 : 1);
				FlyingObject o{id, (id == 0 ? g_resistor : g_resistorZero), timeCaught - 722, timeCaught, -3, 0.15, 0, 0.09, 0.06, 0, false};
				g_flyingObjects.push_back(o);
			}
			timeCaught += g_varLength / var.length();
		}

		v++;
	}

}

void loadMtl(Model &model, string mtllib) {

	ifstream fin(g_objDir + mtllib);
	if (!fin.is_open()) {
		cerr << "error" << endl;
		return;
	}

	string s = "", mtl = "";
	Color Ka, Kd;

	while (!fin.eof()) {
		fin >> s;

		if (s == "newmtl") {
			fin >> mtl;
		} else if (s == "Ka") {
			float r, g, b;
			fin >> r >> g >> b;
			Ka = {r, g, b, 1};
		} else if (s == "Kd") {
			float r, g, b;
			fin >> r >> g >> b;
			Kd = {r, g, b, 1};
			model.colors[mtl] = make_pair(Ka, Kd);
		} else {
			getline(fin, s);
		}

	}

}

void loadObj(Model &model, string filename) {

	ifstream fin(g_objDir + filename);
	if (!fin.is_open()) {
		cerr << "error" << endl;
		return;
	}

	string s = "", mtllib = "";
	int vCount = 0, fCount = 0;
	string faces[3];
	
	while (!fin.eof()) {
		fin >> s;
		if (s == "#") {
			getline(fin, s);
		} else if (s == "mtllib") {
			fin >> mtllib;
		} else if (s == "v") {
			fin >> model.vertex[vCount*3] >> model.vertex[vCount*3 + 1] >> model.vertex[vCount*3 + 2]; 
			vCount++;
		} else if (s == "f") {
			fin >> faces[0] >> faces[1] >> faces[2];
			for (int i=0; i<3; i++) {
				model.faces[fCount*3 + i] = stoi(faces[i].substr(0, faces[i].find('/'))) - 1;
			}
			fCount++;
		} else if (s == "usemtl") {
			string mtl;
			fin >> mtl;
			model.materials.push_back(make_pair(fCount, mtl));
		} else {
			getline(fin, s);
		}
	}

	model.vCount = vCount;
	model.fCount = fCount;

	for(int i=0;i<fCount;i++){
		getNv(model.faces[i*3], model.faces[i*3+1], model.faces[i*3+2], model.vertex, &model.normals[3*i]);
  	}

	if (mtllib != "") loadMtl(model, mtllib);

}

void startMusic() {
	init();
	g_musicPlaying = true;
	char *filename = (char*)calloc(sizeof(char), 128);
	strcpy(filename, g_musicFilename.c_str());
	g_musicThread = thread(playSound, filename, ref(g_musicPlaying), ref(g_beginTime), g_skipLength);
}

void quit() {
	g_musicPlaying = false;
	g_musicThread.join();
	exit(0);
}

void observeHands() {
  
  VideoCapture cap;
  Mat frame;
  Mat avg_img, sgm_img;
  Mat lower_img, upper_img, tmp_img;
  Mat dst_img, msk_img;
  Mat hands_img;
  Mat prevHands;

  bool loop_flag = true;
  double ux = 0, uy = 0, lx = 0, ly = 0;
  int clapFrames = 0, slapFrames = 0;
  bool moving;

  cap.open(g_video_device);
  if(!cap.isOpened()){
    printf("Cannot open the video.\n");
    exit(0);
  }

  cap >> frame;
  Size s = frame.size();
  s.width /= 3;
  Rect cutRect(s.width, 0, s.width, s.height);
  avg_img.create(s, CV_32FC3);
  sgm_img.create(s, CV_32FC3);
  lower_img.create(s, CV_32FC3);
  upper_img.create(s, CV_32FC3);
  tmp_img.create(s, CV_32FC3);

  dst_img.create(s, CV_8UC3);
  msk_img.create(s, CV_8UC1);
  hands_img.create(s, CV_8UC3);
  prevHands.create(s, CV_8UC3);

  initCV(cap, avg_img, sgm_img, tmp_img);

  cv::namedWindow("Input", 1);
  cv::namedWindow("FG", 1);
  cv::namedWindow("mask", 1);
  cv::namedWindow("Hands", 1);
  //cv::namedWindow("Average Background Image", 1);
  cv::createTrackbar("Zeta", "Hands", &Zeta, 20, 0);
  cv::createTrackbar("T", "Hands", &T_PARAM_EXP, 100, 0);
  cv::createTrackbar("Hand", "Hands", &g_handTh, 10000, 0);
  cv::createTrackbar("Slap", "Hands", &g_slapTh, 30, 0);

  while (loop_flag) {

    if (clapFrames > 0) clapFrames--;
    if (slapFrames > 0) slapFrames--;

    cap >> frame;
    if (frame.empty()) {
      break;
    }
	frame = Mat(frame, cutRect);

    frame.convertTo(tmp_img, tmp_img.type());

    // 4. check whether pixels are background or not
    cv::subtract(avg_img, sgm_img, lower_img);
    cv::subtract(lower_img, Zeta, lower_img);
    cv::add(avg_img, sgm_img, upper_img);
    cv::add(upper_img, Zeta, upper_img);
    cv::inRange(tmp_img, lower_img, upper_img, msk_img);

    // 5. recalculate
    cv::subtract(tmp_img, avg_img, tmp_img);
    cv::pow(tmp_img, 2.0, tmp_img);
    tmp_img.convertTo(tmp_img, -1, 2.0);
    cv::sqrt(tmp_img, tmp_img);

    // 6. renew avg_img and sgm_img
    cv::accumulateWeighted(frame, avg_img, B_PARAM, msk_img);
    cv::accumulateWeighted(tmp_img, sgm_img, B_PARAM, msk_img);

    cv::bitwise_not(msk_img, msk_img);
    cv::accumulateWeighted(tmp_img, sgm_img, exp(-T_PARAM_EXP/10.0), msk_img);

    dst_img = cv::Scalar(0);
    frame.copyTo(dst_img, msk_img);

	hands_img.copyTo(prevHands);
	dst_img.copyTo(hands_img);

    // 物体の境界線を探す
    vector<vector<Point>> contours;
    findContours(msk_img, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

    int countHands = 0;

	vector<pair<float, int>> ais(contours.size());
	for (int i=0; i<ais.size(); i++) {
		ais[i] = make_pair(contourArea(contours[i]), i);
	}
	sort(ais.begin(), ais.end(), greater<>());

	vector<vector<Point>> hands;
	for (int i=0; i<ais.size(); i++) {
		if (ais[i].first > g_handTh) hands.push_back(contours[ais[i].second]);
		else break;
	}

	    // 物体の境界線を描画する
	if (contours.size() >= 1) cv::polylines(hands_img, {contours[ais[0].second]}, true, Scalar(0, 0, 255), 2);
    if (contours.size() >= 2) cv::polylines(hands_img, {contours[ais[1].second]}, true, Scalar(0, 255, 0), 2);

    // 手の重心を描画する
	for (auto h : hands) {
		Moments mu = moments(h);
        Point2f mc = Point2f( mu.m10/mu.m00 , mu.m01/mu.m00 );
		circle(hands_img, mc, 6, Scalar(100), 2, 4);
	}

    // clap の場合は赤い円、slap の場合は黄色の円を左上に表示する
    if (clapFrames) {
      circle(hands_img, Point(30, 30), 30, Scalar(0, 0, 255), -1);
    } else if (slapFrames) {
      circle(hands_img, Point(30, 30), 30, Scalar(0, 255, 255), -1);
    }

    cv::imshow("Input", frame);
    cv::imshow("FG", dst_img);
    cv::imshow("mask", msk_img);
    cv::imshow("Hands", hands_img);

	//Mat3b avg_img_8UC3;
	//sgm_img.convertTo(avg_img_8UC3, CV_8UC3, 255);
	//cv::imshow("Average Background Image", avg_img_8UC3);
    
    if (hands.size() >= 2) {
      // 重心の動きを検知する
      Moments mu;
      mu = moments( hands[0]);
      Point2f mcu = Point2f( mu.m10/mu.m00 , mu.m01/mu.m00 );
      mu = moments( hands[1]);
      Point2f mcl = Point2f( mu.m10/mu.m00 , mu.m01/mu.m00 );
      
      if (mcu.y > mcl.y) swap(mcu, mcl);

      double vyu = mcu.y - uy;
      double vyl = mcl.y - ly;

      if (uy != -1 && ly != -1 && clapFrames == 0 && slapFrames == 0) {
        if (vyu >= g_slapTh) {
		  if (!moving) {
		  }
          moving = true;
        } else {
          // 前のフレームまで上の手が動いていて、両手が離れたまま止まったら slap と判定する
          if (moving) {
            slapFrames = 5;
			doSlap();
          }
          moving = false;
        }
      }

      uy = mcu.y;
      ly = mcl.y;
    } else if (hands.size() == 1) {

		if (uy != -1 && ly != -1) {
			Moments mu = moments( hands[0]);
			Point2f mc = Point2f( mu.m10/mu.m00 , mu.m01/mu.m00 );
			int th = 10;
			if (uy + th < mc.y && mc.y < ly - th) {
				clapFrames = 5;
				doClap();
			}
		}

      uy = -1;
      ly = -1;
      moving = false;
    }

    char key =cv::waitKey(33);
    if(key == 27){
      loop_flag = false;
    }
  
  }

}


void glut_keyboard(unsigned char key, int x, int y){
	switch(key){
		case 'c':
			doClap();
			break;
		case 's':
			doSlap();
			break;
		case ' ':
			startMusic();
			loadScore();
			break;
		case 'i':
			init();
			break;
	case 'q':
	case 'Q':
	case '\033':
		quit();
	}

	glutPostRedisplay();
}

void glut_mouse(int button, int state, int x, int y){
	if(button == GLUT_LEFT_BUTTON){
		if(state == GLUT_UP){
			g_isLeftButtonOn = false;
		}else if(state == GLUT_DOWN){
			g_isLeftButtonOn = true;
		}
	}else if(button == GLUT_RIGHT_BUTTON){
		if(state == GLUT_UP){
			g_isRightButtonOn = false;
		}else if(state == GLUT_DOWN){
			g_isRightButtonOn = true;
		}
	}
}

void glut_motion(int x, int y){
	static int px = -1, py = -1;

	if(g_isLeftButtonOn == true){
		if(px >= 0 && py >= 0){
			g_angle1 += (double)-(x - px)/20;
			g_angle2 += (double)(y - py)/20;
		}
		px = x;
		py = y;
	}else if(g_isRightButtonOn == true){
		if(px >= 0 && py >= 0){
			g_angle3 += (double)(x - px)/20;
		}
		px = x;
		py = y;
	}else{
		px = -1;
		py = -1;
	}
	glutPostRedisplay();
}

void updateFlyingObjects() {

	time_t t = g_time();
	int count = 0;
	for (auto &o : g_flyingObjects) {
		if (t < o.timeSpawn) continue;
		if (t > o.timeCaught + 600) {
			count++;
			break;
		}
		o.x += o.vx;
		o.y += o.vy;
		o.z += o.vz;
		
		o.vy -= 0.003;
	}

	while (count--) {
		if (!g_flyingObjects.front().hit) g_countMiss++;
		g_flyingObjects.pop_front();
	}

}

void glut_idle() {

	if (g_clap > 0) g_clap -= 0.05;
	if (g_clap < 0) g_clap = 0;
	if (g_slap > 0) g_slap -= 0.05;
	if (g_slap < 0) g_slap = 0;

	updateFlyingObjects();

	glutPostRedisplay();
}

void drawString(string str, int w, int h, int x0, int y0)
{
    glDisable(GL_LIGHTING);
    // 平行投影にする
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, w, h, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 画面上にテキスト描画
    glRasterPos2f(x0, y0);
    int size = (int)str.size();
    for(int i = 0; i < size; ++i){
        char ic = str[i];
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, ic);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawImage(int textureIndex, int width, int height, int x0, int y0, int w, int h) {

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

	glBindTexture(GL_TEXTURE_2D, g_TextureHandles[textureIndex]);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_POLYGON);
	glTexCoord2d(0, 0);
	glVertex2d(x0, y0);
	glTexCoord2d(0, 1);
	glVertex2d(x0, y0+h);
	glTexCoord2d(1, 1);
	glVertex2d(x0+w, y0+h);
	glTexCoord2d(1, 0);
	glVertex2d(x0+w, y0);
	glEnd();
	glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

}

void glut_display(){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(30.0, 1.0, 0.1, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if (cos(g_angle2)>0){
	gluLookAt(g_distance * cos(g_angle2) * sin(g_angle1),
		g_distance * sin(g_angle2),
		g_distance * cos(g_angle2) * cos(g_angle1),
		0.0, 0.0, 0.0, 0.0, 1.0, 0.0);}
	else{
 	gluLookAt(g_distance * cos(g_angle2) * sin(g_angle1),
                g_distance * sin(g_angle2),
                g_distance * cos(g_angle2) * cos(g_angle1),
                0.0, 0.0, 0.0, 0.0, -1.0, 0.0);}

	GLfloat lightpos[] = {0, 0, 0, 1.0f};
	spherical_coordinates(5, g_angle4, g_angle3, lightpos);
	GLfloat diffuse[] = {1, 1, 1, 1};

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	glColor3d(1, 1, 1);
	glPushMatrix();
	glTranslatef(lightpos[0], lightpos[1], lightpos[2]);
	glutSolidSphere(0.2, 50, 50);
	glPopMatrix();

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

	if (g_enableCV && g_enableGuideSphere) {
		glPushMatrix();
		GLfloat white[] = {0.8, 0.8, 0.8};
		glMaterialfv(GL_FRONT, GL_AMBIENT, white);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, white);
		glTranslatef(1.0 - 0.09 * 60 / 1000 * g_videoTimelag, 0.6, -2);
		glutSolidSphere(0.3, 50, 50);
		glPopMatrix();
	}

	glPushMatrix();
	glTranslatef(0.0, -2.0, 0.0);
	draw_plane();
	glPopMatrix();

	glPushMatrix();
	glScalef(3, 3, 3);
	glEnable( GL_RESCALE_NORMAL );
	draw_hands();
	glDisable( GL_RESCALE_NORMAL );
	glPopMatrix();

	time_t t = g_time();
	for (auto &o : g_flyingObjects) {
		if (t < o.timeSpawn) continue;
		glPushMatrix();
		glTranslatef(o.x, o.y, o.z);
	    glScalef(0.7, 0.7, 0.7);
	    //glScalef(2, 2, 2);
		if (o.id == 1)
			glRotatef(90, 0, 0, 1);
		glEnable( GL_RESCALE_NORMAL );
		draw_model(o.model);
		glDisable( GL_RESCALE_NORMAL );
		glPopMatrix();
	}

	glFlush();

	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glColor3d(0, 0.5, 0.5);
	drawString("Perfect:" + to_string(g_countPerfect) + " Good: " + to_string(g_countGood) + " Miss: " + to_string(g_countMiss), WINDOW_X, WINDOW_Y, 50, 50);

	if (!g_musicPlaying && g_countPerfect + g_countGood + g_countMiss > 0) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4d(1, 1, 1, 0.5);
		int imageIndex = (g_countPerfect % 2 == 0 ? 2 : 3);
		drawImage(imageIndex, WINDOW_X, WINDOW_Y, WINDOW_X-300, WINDOW_Y-70, 300, 70);
		glDisable(GL_BLEND);
	}

	glutSwapBuffers();
}

void draw_plane(){
	
	GLfloat white[] = {0.8, 0.8, 0.8};
	glMaterialfv(GL_FRONT, GL_AMBIENT, white);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, white);

	glBindTexture(GL_TEXTURE_2D, g_TextureHandles[0]);
	glNormal3d(0.0, 1.0, 0.0);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2d(1, 1);
	glVertex3d(4.0, 0.0, 4.0);
	glTexCoord2d(0, 1);
	glVertex3d(4.0, 0.0, -4.0);
	glTexCoord2d(0, 0);
	glVertex3d(-4.0, 0.0, -4.0);
	glTexCoord2d(1, 0);
	glVertex3d(-4.0, 0.0, 4.0);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	glNormal3d(0.0, 0.0, 1.0);
	glBindTexture(GL_TEXTURE_2D, g_TextureHandles[1]);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2d(1, 1);
	glVertex3d(4.0, 0.0, -4.0);
	glTexCoord2d(0, 1);
	glVertex3d(-4.0, 0.0, -4.0);
	glTexCoord2d(0, 0);
	glVertex3d(-4.0, 6.0, -4.0);
	glTexCoord2d(1, 0);
	glVertex3d(4.0, 6.0, -4.0);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void draw_model(Model &model) {

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3,GL_FLOAT,0,model.vertex);
	glBegin(GL_TRIANGLES);	
	int mi = 0;
    for(int i=0;i<model.fCount;i++) {
		if (mi < model.materials.size() && model.materials[mi].first == i) {
			auto c = model.colors[model.materials[mi].second];
			GLfloat aColor[] = {c.first.r, c.first.g, c.first.b, 1};
			GLfloat dColor[] = {c.second.r, c.second.g, c.second.b, 1};
			glMaterialfv(GL_FRONT, GL_AMBIENT, aColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, dColor);
			mi++;
		}
		glNormal3d(model.normals[i*3], model.normals[i*3+1], model.normals[i*3+2]);
    	glArrayElement(model.faces[i*3]);
    	glArrayElement(model.faces[i*3+1]);
    	glArrayElement(model.faces[i*3+2]);
    }
	glEnd();
	glDisableClientState(GL_VERTEX_ARRAY);

}

void draw_hands() {

	if (g_clap > 0) {

		float angle = max(1-3*sin(g_clap * M_PI), 0.0) * 40;

		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(angle, 0, 0, -1);
		draw_model(g_rightHand);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(-angle, 0, 0, -1);
		draw_model(g_leftHand);
		glPopMatrix();

	} else if (g_slap > 0) {

		float angle = max(1-2*sin(g_slap * M_PI), 0.0) * 70 - 30;

		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(-45, 1, 0, 0);
		glRotatef(angle, 0, 0, -1);
		draw_model(g_rightHand);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(-40, 0, 0, -1);
		draw_model(g_leftHand);
		glPopMatrix();

	} else {
		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(40, 0, 0, -1);
		draw_model(g_rightHand);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(1, 0, 0);
		glRotatef(-40, 0, 0, -1);
		draw_model(g_leftHand);
		glPopMatrix();
	}

}

void errorStatistics(time_t error) {
	static time_t errorSum;
	static int errorCount;
	static double errorAverage;
	errorSum += error;
	errorCount++;
	errorAverage = (double)errorSum / errorCount;
	printf("timelag[ms]: %4ld average[ms]: %.1f\n", error, errorAverage);
}


time_t g_time() {
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - g_beginTime;
}

void spherical_coordinates(float d, float th, float phi, GLfloat coord[]) {
	coord[0] = d * cosf(th) * sinf(phi);
	coord[1] = d * sinf(th);
	coord[2] = d * cosf(th) * cosf(phi);
}

void getNv(int v0, int v1, int v2, float vertex[], float Nv[]) {

	double x0 = vertex[v0*3];
	double y0 = vertex[v0*3+1];
	double z0 = vertex[v0*3+2];
	
	double x1 = vertex[v1*3];
	double y1 = vertex[v1*3+1];
	double z1 = vertex[v1*3+2];
	
	double x2 = vertex[v2*3];
	double y2 = vertex[v2*3+1];
	double z2 = vertex[v2*3+2];

	double N_x = (y0-y1)*(z2-z1)-(z0-z1)*(y2-y1);
	double N_y = (z0-z1)*(x2-x1)-(x0-x1)*(z2-z1);
	double N_z = (x0-x1)*(y2-y1)-(y0-y1)*(x2-x1);

	double length = sqrt(N_x * N_x + N_y * N_y + N_z * N_z);

	Nv[0] = -N_x / length;
	Nv[1] = -N_y / length;
	Nv[2] = -N_z / length;
}