// Copyright 2011 Boston Biomedical Research Institute
// Authors: Peng Wei and Oliver King (king@bbri.org)
// Version 0.1.7, July 7, 2011
//
// Examples of how to compile, depeding on platform and where OpenCV headers and libraries are installed 
//
// Mac or Ubuntu, OpenCV 2.2 or 2.3
// g++ -o rff-bin rffmain.cpp -I/usr/local/include/opencv -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_ml -lopencv_objdetect -lopencv_imgproc
//
// Mac, OpenCV 2.1 --- note, this version appears to have memory leak on snow leopard!
// g++ -o rff-bin rffmain.cpp -I/usr/local/include/opencv -L/usr/local/lib -lcxcore -lcv -lml -lhighgui -lcvaux
//
// Windows 7, OpenCV 2.0
// g++ -o rff-bin.exe rffmain.cpp -I C:\OpenCV2.0\include\opencv -L C:\OpenCV2.0\lib -lcxcore200 -lcv200 -lml200 -lhighgui200 

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

// switch to single header for OpenCV2.2?
#include "cv.h"
#include "ml.h"
#include "highgui.h"
#include "cxcore.h"
#include "cvaux.h"

// try to avoid this --- not needed?
// using namespace std; 

// output options
#define BEST_HIT_PER_BOUT 0
#define FIRST_HIT_PER_BOUT 1
#define ALL_HITS_PER_BOUT 2
#define ALL_FRAMES_PER_BOUT 3

#define FORCE_PRINT 1
#define PRINT_IF_HIT 0
#define DO_NOT_PRINT -1

#define JUST_TUNE_ROI 1
#define DONT_TUNE_ROI 2
#define NO_DISPLAY 3
#define TUNE_AND_RUN 0

#define MAX_N 10


// 1118 x 635: 1.76, close to 1.77 = 16/9 = 4/3 * 4/3 (PAR = 4/3, DAR = 16/9)
// 1440 x 1080: 4:3 ratio

char adjust_roi(IplImage* frame, const char* adjust_text);
int detect_and_draw(IplImage* img, IplImage* img2, IplImage* clean, IplImage* raw, IplImage* gray1, IplImage* gray2, 
                    IplImage* gray_diff, double frame_num, double* diff_score, int print_me);
double image_diff(IplImage* img1, IplImage* img2);
void my_mouse_callback(int event, int x, int y, int flags, void* param);
IplImage* crop_image( IplImage* src,  CvRect roi);
IplImage* deinterlace_image( IplImage* src, int mode, int scale_factor, double par); // make in-place version?
int frame2mmssff(double frame_number, double fps, char* mmssff);
int frame2mmssss(double frame_number, double fps, char* mmssss);

double frame_rate;
char frame_name[12];
char frame_name1[12];
char frame_name2[12];

ofstream logfile;

CvRect roi_box;
CvRect roi_box_bak;
CvRect roi_box_big;

CvHaarClassifierCascade* eye;
CvHaarClassifierCascade* ear;

CvCapture* capture;

string stem_string; // shouldn't need both!
char* image_stem;
char* video_name;
char gui_name[] = "rodent face finder";

char version_string[] = "Rodent Face Finder version 0.1.7, July 7, 2011";


// just for hd video
bool hd_force_169 = 0;
bool hd_half_size = 0;
bool hd_skip_second = 0;

// for any video
bool force_169 = 0;
bool half_size = 0;
bool skip_second = 0;

// adjust zones for both cages.
// this is quicker than two separate calls to main, particularly when first 
// second is skipped, because video only needs to be initialized once
bool queue_both = false; 

// skip ears if no eyes are found
bool short_circuit = false;

int skip_count = 0; // how many initial frames to skip --- should be enough to get to valid key frame
double par = 1; // 4.0/3.0; // pixel aspect ratio
double ar_num = 0; // screen aspect ratio numerator
double ar_denom = 0; // screen aspect ratio denominator
int scale_factor = 1; // reduce size by this factor during haar stage. export fullsize images though?

int mode = TUNE_AND_RUN; // 1 for just roi, 2 for just analysis (no roi), 3 for no display,  0 for both

int image_opt = 0;  // 0 for best hit in burst, 1 for 1st hit in burst, 2 for all hits in burst, 3 for all frames in burst

int with_box = 0;    // if 1, show boxes in output images
int without_box = 0; // if 1, print clean images (without boxes)
int motion_diff = 0; // save images differences between consec frames

// cropping options:
int whole_frame = 0;
int just_zone = 0;
int just_face = 0;
int full_size = 0;

int vert_res = -1;
int hori_res = -1;

int vert_res_raw = -1;
int hori_res_raw = -1;

int cage_num = 1;  // 0 for pre-set zone, 1 for left, 2 for right

int verbose = 0;    // print various diagnostics
int slow_mode = 2;  // if 1 decompress all frames, not just those analysed (useful for mpgs without indices), 0 for fast mode, 2 for autodetect
int use_trackbar = 1;
int deinterlace_mode = 2; // 0 for no deinterlacing, 1 for bob, 2 for linear

double pos_0 = -1.0;
double pos_1 = -1.0;
double pos_2 = -1.0;

bool drawing_box = false;

int redraw_me = 1;

int allow_draw = 1; // draw roi, then disable ability to change zone.
// could also set null callback?

int is_sleeping = 0;
double sleep_start = 0.0;
double sleep_end = 0.0;

CvFont img_font;

// declare all these in main instead? They all get passed by reference to subroutines.
IplImage* frame1;
IplImage* frame2;
IplImage* frame1_clean;
IplImage* frame1_raw;
IplImage* frame2_raw;

int n = 1; // frames per burst to save

// should probably use dynamic memory allocation instead
IplImage* best1[MAX_N];
IplImage* best2[MAX_N];
IplImage* best1_raw[MAX_N];

IplImage* best1_clean;
IplImage* gray1;
IplImage* gray2;
IplImage* gray_diff;

double best_dexes[MAX_N];
double best_scores[MAX_N];
int worst_of_best = 0; // worst of best indices --- next to fill in

// add option for formating time in output? e.g. hh:mm:ss instead of frame?

int main(int argc, char **argv){
  printf("%s\n", version_string);

  // char mfs[12]; // frame number formated as mmssdd
  // frame2mmssff(14.00,30.0, mfs);
  // printf("%s\n", mfs);
  // frame2mmssff(31.00,30.0, mfs);
  // printf("%s\n", mfs);
  // frame2mmssff(6000.00,30.0, mfs);
  // printf("%s\n", mfs);

  char c = -1;
  //  if (argc < 3) { // need to fix all of this!!!!!
  //  printf("Not enough arguments.\n");
  //  printf("Example usage: mouseface -i video.avi\n");
  //  printf("  where video.avi is name of videofile.\n");
  //  return 0;
  // }

  int ca;
  int errflg = 0;
  extern char *optarg;
  extern int optopt; // optind
  // int optopt = 1;
  int gap_length = -1;
  int burst_length = -1;
  roi_box.x = -1;
  roi_box.y = -1;
  roi_box.width = 1;
  roi_box.height = -1;
  roi_box_bak.x = -1;
  roi_box_bak.y = -1;
  roi_box_bak.width = 1;
  roi_box_bak.height = -1;

  while ((ca = getopt(argc, argv, "VxanwjfvhceHCEi:o:g:b:z:p:d:s:k:m:r:")) != -1) {
   switch (ca) {
     case 'V':
      return 0;    
     case 'x':
      motion_diff = 1;
      break;
    case 'a': // a for annotate
      with_box = 1;
      break;
    case 'n':
      without_box = 1;
      break;
    case 'w':
      whole_frame = 1;
      break;
    case 'j':
      just_zone = 1;
      break;
    case 'f':
      just_face = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'c': // compensate for pixel-aspect ratio --- go from 1440 x 1080 to 1920 x 1080. Or, shrink to 1440 x 810? 
      force_169 = true;
      break;
    case 'h': // half-size during playback
      half_size = true;
      break;
    case 'e': // skip first second
      skip_second = true;
      break;
    case 'C': // compensate for pixel-aspect ratio --- go from 1440 x 1080 to 1920 x 1080. Or, shrink to 1440 x 810? 
      hd_force_169 = true;
      break;
    case 'H': // half-size during playback
      hd_half_size = true;
      break;
    case 'E': // skip first second
      hd_skip_second = true;
      break;
    case 'i':
      video_name = optarg;
      printf("video name %s\n", video_name);
      break;
    case 'o':
      image_stem = optarg;
      break;
    case 'g':
      gap_length= atoi(optarg);         // check for nice int --- change to strtol?
      break;
    case 'b':
      burst_length = atoi(optarg);              // check for nice int
      break;
    case 'p': // p for print options
      image_opt = atoi(optarg);                 // check for valid int, 0,1,2,3
      break;
    case 'd':
      deinterlace_mode = atoi(optarg);  // check for valid int, 0,1,2
      break;
    case 's':
      slow_mode = atoi(optarg);                 // check for valid int, 0,1,2,3
      break;
    case 'k':
      n = atoi(optarg);
      break;
    case 'm':
      mode = atoi(optarg); // check for 0, 1,2 ,3
      break;
    case 'r': // force aspect ratio
      char* pEnd;
      ar_num = strtod(optarg, &pEnd);
      ar_denom = strtod(pEnd+1, NULL);
      printf("The parsed aspect ratio values: %.2f x %.2f\n", ar_num, ar_denom);
      if ((ar_num < 1) || (ar_denom < 1)) {
	ar_num = 0; 
	ar_denom = 0;
	printf("not parsed correctly --- using default aspect ratio\n");
      }
      break;
    case 'z':
      printf("zone to monitor %s\n", optarg); // parse this as %d,%d,%d,%d --- use scanf????
      if (optarg[0]=='L') cage_num = 1;
      else if (optarg[0]=='R') cage_num = 2;
      else if (optarg[0]=='B') {
        cage_num = 1; 
        queue_both = true;
      }
      else {
        cage_num = 0;
        char* pEnd;
        long int li1, li2, li3, li4;
        li1 = strtol(optarg,&pEnd,10);
        li2 = strtol(pEnd+1,&pEnd,10);
        li3 = strtol(pEnd+1,&pEnd,10);
        li4 = strtol(pEnd+1,NULL,10);
        printf("The parsed values: %ld,%ld,%ld,%ld.\n", li1, li2, li3, li4);
        roi_box.x = (int)li1;
        roi_box.y = (int)li2;
        roi_box.width = (int)li3;
        roi_box.height = (int)li4;
        printf("  upperleft-x, upperleft-y, width, height: (%d,%d,%d,%d)\n",roi_box.x,roi_box.y,roi_box.width,roi_box.height);
      }
      break;
    case ':':       // missing expected operand
      fprintf(stderr,
              "Option -%c requires an operand\n", optopt);
      errflg++;
      break;
    case '?':
      fprintf(stderr,
              "Unrecognized option: -%c\n", optopt);
      errflg++;
    }
  }
  if (errflg) {
    fprintf(stderr, "usage: rff-bin -i video.avi [plus many other optional flags] \n");
    exit(2);
  }

  if (mode == NO_DISPLAY) short_circuit = true;

  queue_both = queue_both & (mode == JUST_TUNE_ROI);
       
   if (!video_name) {
    printf("No video specified.\n");
    printf("Example usage: rff-bin -i video.avi [plus many optional flags; use GUI to configure]\n");
    printf("  where video.avi is name of videofile.\n");
    return 0;
  }
 
  if (!image_stem) {
    printf("No image stem entered, using default.\n");
    stem_string = video_name;
    size_t dot;
    dot = stem_string.rfind("/",1000);
    if (dot!=string::npos) stem_string = stem_string.substr(dot+1,1000);
    dot = stem_string.rfind("\\",1000);
    if (dot!=string::npos) stem_string = stem_string.substr(dot+1,1000);
    dot = stem_string.rfind(".",1000);
    if (dot!=string::npos) stem_string = stem_string.substr(0,dot);
    stem_string = "img_" + stem_string;
    image_stem = (char*) stem_string.c_str();
    printf("using image stem: %s \n", image_stem);
    // get rid of spaces etc?
    // embed other commandline flags in output name?
  }

  if (!(whole_frame || just_face || just_zone) || !(with_box || without_box || motion_diff)) {
    printf("warning: you should select at least one cropping option and at least one image option,\n");
    printf("otherwise no images will be saved\n");
  }

  if (n > MAX_N) {
    printf("reducing best-k from %d to %d\n",n , MAX_N);
    n = MAX_N;
  }

  cvInitFont(&img_font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);

  if (mode != NO_DISPLAY) cvNamedWindow(gui_name, CV_WINDOW_AUTOSIZE);
  // check that file exists first?
  capture= cvCreateFileCapture(video_name);
  if (!capture) {
    printf("Unable to open video file %s\n", video_name);
    printf("Double-check the path and filename.\n");
    printf("It could also be an incompatible format.\n");
    return 0;
  }

  if (mode == NO_DISPLAY) use_trackbar = 0;

  // trackbar doesnt work for some mpegs with broken index!!!
  int trackbar_pos = 0;
  if (use_trackbar) {
    cvCreateTrackbar("progress", gui_name, &trackbar_pos, 1002, NULL);
    //cvSetTrackbarPos("progress", gui_name, 50);
    //cvSetTrackbarPos("progress", gui_name, 500);
    //cvSetTrackbarPos("progress", gui_name, 0); // opencv 2.1 crashes at 0!!!! 
  }

  cvQueryFrame(capture); // added 3/29/2011, to see if capture properties would then be better initialized
  double frame_count = cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_COUNT);
  frame_rate = cvGetCaptureProperty(capture,CV_CAP_PROP_FPS);
        
  time_t rawtime;
  struct tm * timeinfo;
  char sdate[9];
  char stime[9];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(sdate,9, "%x", timeinfo);
  strftime(stime,9, "%X", timeinfo);

  sdate[2] = '-';
  sdate[5] = '-';
  stime[2] = '-';
  stime[5] = '-';

  string logfile_string(image_stem);
  size_t loc1;
  size_t loc2;
  loc1 = logfile_string.find("<");
  if (loc1 != string::npos) {
    logfile_string.erase(loc1);
  }
  loc1 = logfile_string.rfind("/");
  if (loc1 != string::npos) {
    logfile_string.erase(loc1+1);
  } else {
    loc2 = logfile_string.rfind(".");
    if (loc2 != string::npos) {
      logfile_string.erase(loc2);
    }
  }

  logfile_string.append("logfile_date");
  logfile_string.append(sdate);
  logfile_string.append("_time");
  logfile_string.append(stime);
  logfile_string.append(".txt");

  // stringstream logfile_name;
  // logfile_name.str("");
  // logfile_name << logfile_string << "_logfile_d" << sdate << "_t" << stime << ".txt"; // include date, to avoid overwrites?
  // logfile.open(logfile_name.str().c_str(), ios::out);

  logfile.open(logfile_string.c_str(), ios::out);
  //logfile << setiosflags(ios::left);
  logfile << version_string << "\n";
  logfile << "Invoked at " << stime << " on " << sdate << " with command: \n";
  for (int k=0; k<argc; k++) logfile << " " << argv[k];
  logfile << "\n";
  
  // printf("%s\n", argv[0]);   // exectutible
  printf("video file name: %s\n", video_name);          // video file name
  printf("cage number (1=L,2=R,0=coords_given): %d\n", cage_num);       
  printf("burst length (in seconds): %d\n", burst_length);      // number of consecutive frames per burst
  printf("gap between bursts (in seconds): %d\n", gap_length);   // gaps between bursts of consecutive frames
  printf("stem for saved images: %s\n", image_stem); // subfolder for images
  printf("deinterlace mode (0=none,1=bob,2=linear): %d\n", deinterlace_mode); 
  printf("----------------------\n");
  printf("video frame height: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT));
  printf("video frame width:  %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH));
  printf("video frame rate:   %8.2f\n", frame_rate);
  printf("video frame count:  %12.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_COUNT));
  // printf("video codec id:     %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_FOURCC)); // convert to 4 chars!
  printf("length in minutes:  %8.2f\n", frame_count/(60.0*cvGetCaptureProperty(capture,CV_CAP_PROP_FPS)));
  printf("----------------------\n");

  ///////////////////////////////////////////////////////////
  logfile << "video file name: " << video_name << "\n";         
  logfile << "cage number (1=L,2=R,0=coords_given): " << cage_num << "\n";      
  logfile << "burst length (in seconds): " << burst_length << "\n";     
  logfile << "gap between bursts (in seconds): " << gap_length << "\n";  
  logfile << "stem for saved images: " << image_stem << "\n"; 
  logfile << "deinterlace mode (0=none,1=bob,2=linear): " <<  deinterlace_mode << "\n"; 
  logfile << "----------------------\n";
  logfile << "video frame height: " << cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT) << "\n";
  logfile << "video frame width:  " << cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH) << "\n";
  logfile << "video frame rate:   " << frame_rate << "\n";
  logfile << "video frame count:  " <<  cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_COUNT) << "\n";
  // printf("video codec id:     %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_FOURCC)); // convert to 4 chars! sometimes crashes.
  logfile << "length in minutes:  " << frame_count/(60.0*cvGetCaptureProperty(capture,CV_CAP_PROP_FPS)) << "\n";
  logfile << "----------------------" << endl;
  ////////////////////////////////////////////////////////////////

  if (cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT) < 1 || cvGetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH) < 1) {
      printf("cannot determine frame size, could be upsupported codec.\n");
      logfile << "cannot determine frame size, could be upsupported codec\n";
      return 0; //-1 instead? send message to GUI, so it can display error
  }

  // round off frame_rate for convenience?
  frame_rate = floor(frame_rate + 0.5);
  if ((frame_rate < 2) || (frame_rate > 120))  {
    printf("frame rate seems weird --- using 30fps for time stamps in image names.\n");
    logfile << "frame rate seems weird (" << frame_rate << ") --- using 30fps for time stamps\n";
    frame_rate = 30;
  }
        
  gap_length = (int) (gap_length*frame_rate);
  burst_length = (int) (burst_length*frame_rate);
        
  if (burst_length < 1) {
    printf("Note: burst num should be at least 1, otherwise nothing will be done.\n");
    burst_length = 30;
    printf("Setting burst num to 30 frames.\n");
  }
  if (gap_length < 0 ) {
    printf("Note: gap num should be at least 0, otherwise bursts will overlap.\n");
    gap_length = 1200;
    printf("Setting gap_length = %d frames instead\n",gap_length);
  }
  gap_length = gap_length + burst_length; // gaps between starts, not end and start.
        
  double current_score; // passed by ref to draw_and_detect

  int interval_count = 1;
  int interval_saved = 0;
  int frames_saved = 0;

  ear=(CvHaarClassifierCascade*)cvLoad("xml/mouse-ear-f.xml",0,0,0); // green
  eye=(CvHaarClassifierCascade*)cvLoad("xml/mouse-eye.xml",0,0,0);   // red
        
  if (!eye || !ear) {
    printf("Couldn't load classifiers from xml folder.\nCheck that xml folder is in same directory as program.\n");
    return 0;
  }

  double offset = (1.0*gap_length - 1.0*burst_length + 1.0); // exact gap_lengths may not be respected. divisible by 12?

  // printf("my offset %8.2f: \n ", offset);
  int end_loop = 0;
  double frame_num = 0.0;
  int num_found = 0;
  int penalty_count=0;

  
  pos_0 = cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES);
  frame1_raw = deinterlace_image(cvQueryFrame(capture), 0, 1, 1); //native size and aspect ratio
  hori_res_raw = frame1_raw->width;
  vert_res_raw = frame1_raw->height;
 
  bool is_hd = (vert_res_raw == 1080); //720, also?

  // get rid of this block?
  if (force_169 || (is_hd && hd_force_169)) {
    if (9*hori_res_raw != 16*vert_res_raw) {
      par = (16.0/9.0)/((hori_res_raw + 0.0)/(vert_res_raw + 0.0));
      printf("Forcing 16:9 screen aspect ratio by using pixel aspect ratio %1.4f\n", par);
      logfile << "Forcing 16:9 screen aspect ratio by using pixel aspect ratio " <<  par << "\n";
    }
    else {
      printf("Screen aspect ratio is already 16:9\n");
    }
  }
  
  if ((ar_num >0) && (ar_denom > 0) && (((int) (ar_denom*hori_res_raw)) != ((int) (ar_num*vert_res_raw)))) {
    par = ((ar_num + 0.0)/(ar_denom + 0.0))/((hori_res_raw + 0.0)/(vert_res_raw + 0.0));
    printf("Forcing %.2f:%.2f screen aspect ratio by using pixel aspect ratio %1.4f\n", ar_num, ar_denom, par);
    logfile << "Forcing "<< ar_num << ":" << ar_denom << " screen aspect ratio by using pixel aspect ratio " <<  par << "\n";
  }
  

  if (half_size || (is_hd && hd_half_size)) {
    scale_factor = 2;
    printf("Displaying and analyzing video at half-size.\n");
    logfile << "Displaying and analyzing video at half-size.\n";

  }
  if (skip_second || (is_hd && hd_skip_second)) {
    skip_count = 30; // subtract 1 sec from output times?
    printf("Skipping first 30 frames.\n");
    logfile << "Skipping first 30 frames.\n";
  }

  full_size = without_box*(scale_factor > 1);

  frame1 = deinterlace_image(frame1_raw, deinterlace_mode, scale_factor, par);
  pos_1 = cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES);
  frame2_raw = deinterlace_image(cvQueryFrame(capture), 0, 1, 1); //native size
  pos_2 = cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES);

  for (int i=0; i<skip_count-3; i++) frame2_raw = deinterlace_image(cvQueryFrame(capture), 0, 1, 1); 
  
  frame2 = deinterlace_image(frame2_raw, deinterlace_mode, scale_factor, par);

  for (int i=0; i<n; i++){
    best1_raw[i] = cvCreateImage(cvSize(frame2_raw->width,frame2_raw->height),frame2_raw->depth,frame2_raw->nChannels);
    best1[i] = cvCreateImage(cvSize(frame2->width,frame2->height),frame2->depth,frame2->nChannels);
    best2[i] = cvCreateImage(cvSize(frame2->width,frame2->height),frame2->depth,frame2->nChannels);
  }
   
  frame1_raw = cvCreateImage(cvSize(frame2_raw->width,frame2_raw->height),frame2_raw->depth,frame2_raw->nChannels);
  frame1_clean = cvCreateImage(cvSize(frame2->width,frame2->height),frame2->depth,frame2->nChannels);
  best1_clean = cvCreateImage(cvSize(frame2->width,frame2->height),frame2->depth,frame2->nChannels);

  gray1 = cvCreateImage(cvSize(frame2->width,frame2->height),8,1);
  gray2 = cvCreateImage(cvSize(frame2->width,frame2->height),8,1);
  gray_diff = cvCreateImage(cvSize(frame2->width,frame2->height),8,1);

  printf("First three frame positions: %12.1f,%12.1f,%12.1f\n", pos_0, pos_1, pos_2);
  logfile << "First three frame positions: " << pos_0 << ", " << pos_1 << ", " << pos_2 << "\n";
  if ((pos_2 - pos_1 > 2.0) && (slow_mode==0)) {
    printf("Indexing inaccurate --- recommend using slow mode.\n");
    logfile << "Indexing inaccurate --- recommend using slow mode.\n";
  }
  if ((pos_2 - pos_1 > 2.0) && (slow_mode!=0)) { 
    printf("Indexing inaccurate --- using slow mode.\n");
    logfile << "Indexing inaccurate --- using slow mode.\n";
  }
  if (slow_mode == 2) { // auto speed
    if ((pos_2 - pos_1 > 2.0) || (gap_length < 1.25*burst_length)) {
      slow_mode = 1; 
      logfile << "Auto-speed: using slow mode (decompressing each frame in gaps).\n";
    }
    else {
      slow_mode = 0;
      logfile << "Auto-speed: using fast mode (skipping over gaps, may cause frame counts to be slightly off).\n";
    }
  }

  if (frame2)  {
    stringstream adjust_text;
    if (cage_num == 1) adjust_text << "Adjust left zone then press c";
    else if (cage_num == 2) adjust_text << "Adjust right zone then press c";
    else adjust_text << "Adjust zone then press c";
    char ccc = adjust_roi(frame2,adjust_text.str().c_str());
    if (ccc == 113) end_loop = 1;
    // printf("return char %d\n",ccc);
    if (!end_loop && queue_both) { 
      cage_num = 2;
      adjust_text.str("");
      adjust_text << "Now adjust right zone then press c";
      // save left roi, then reset
      roi_box_bak= roi_box; // deep copy?
      roi_box.x = -1;
      roi_box.y = -1;
      roi_box.width = -1;
      roi_box.height = -1;
      redraw_me = 1;
      ccc  = adjust_roi(frame2, adjust_text.str().c_str());
      if (ccc == 113) end_loop = 1;
      // printf("zone2 -z%d,%d,%d,%d\n",roi_box.x,roi_box.y,roi_box.width,roi_box.height);      
      // printf("zone1 -z%d,%d,%d,%d\n",roi_box_bak.x,roi_box_bak.y,roi_box_bak.width,roi_box_bak.height);        
    } 
    if (end_loop) {
      logfile << "Quit during zone adjustment. Exiting" << endl;
   }
  } else {
    end_loop = 1;
  }

  // fix for L and R?
  // logfile << "Final region to monitor:\n";
  // logfile << "  upperleft-x, upperleft-y, width, height: (";
  // logfile << roi_box.x << "," << roi_box.y << "," << roi_box.width<< "," << roi_box.height << ")\n";

  if (pos_2 > frame_count) {
    printf("Don't know video length, so trackbar may not be accurate.\n");
    // frame_count = (pos_2 - pos_1)*30*60*5;
  }
        
  if (!end_loop) { 
    printf("Command for non-interactive analysis:\n");
    logfile << "Command for non-interactive analysis:\n";
    for (int k=0; k<argc; k++) { //filter out -z,-m tokens!
      if ((strncmp(argv[k],"-m",2)!=0) && (strncmp(argv[k],"-z",2)!=0)) {
        bool quote_me = false;
        if (strchr(argv[k],' ')!=NULL || strchr(argv[k],'&')!=NULL || 
            strchr(argv[k],'(')!=NULL || strchr(argv[k],')')!=NULL || 
            strchr(argv[k],'+')!=NULL || strchr(argv[k],'/')!=NULL) quote_me = true;
        if (quote_me) {
          printf("\"%s\" ", argv[k]);
          logfile << "\"" << argv[k] << "\" ";
        }
        else {
          printf("%s ", argv[k]);
          logfile << argv[k] << " ";
        } 
      }
    }
    printf("-m2 -z%d,%d,%d,%d",roi_box.x,roi_box.y,roi_box.width,roi_box.height);        
    logfile << "-m2 -z" << roi_box.x << "," << roi_box.y << "," << roi_box.width<< "," << roi_box.height;
    if (queue_both) {
      printf(":%d,%d,%d,%d\n",roi_box_bak.x,roi_box_bak.y,roi_box_bak.width,roi_box_bak.height);        
      logfile << ":" << roi_box_bak.x << "," << roi_box_bak.y << "," << roi_box_bak.width<< "," << roi_box_bak.height;
    }
    printf("\n");
    logfile << endl;  
  }
        
  if (mode == JUST_TUNE_ROI) {
     printf("Exiting without analyzing video.\n" );
     logfile << "Exiting without analyzing video." << endl;
     end_loop = 1;
  }

  allow_draw = 0;

  int print_flag = PRINT_IF_HIT;
  if (image_opt == BEST_HIT_PER_BOUT) print_flag = DO_NOT_PRINT;
  if (image_opt == ALL_FRAMES_PER_BOUT) print_flag = FORCE_PRINT;

  // running max of diffs, flag potential sleep bouts?
  int still_count = 0;
  int sleep_threshold = 300;    // frames of stillness
  double still_threshold = 2.0; // ugly! --- should be adaptive
        
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(sdate,9, "%x", timeinfo);
  strftime(stime,9, "%X", timeinfo);
  logfile << "----------------------\n";
  logfile << "Start of analysis: " << stime << " on " << sdate << "\n";

  while((c != 113) && !end_loop){
    is_sleeping = 0;
    still_count = 0;
    sleep_start = -1.0;
    sleep_end = -1.0;
                
    penalty_count = 0;
    for (int j=0; j<n; j++){
      best_dexes[j] = -1.0;
      best_scores[j] = 1000.0;
    }
    current_score = 1000.0;
    worst_of_best = 0;
    printf("Analyzing interval %d ...\n", interval_count);
    logfile << "Analyzing interval " << interval_count << "..." << endl; // flush buffer once per interval
    interval_count++;
    for(int k=0; k<burst_length+1; k++){
      // cvCopy(frame2,frame1,0); // should be able to do this with pointers!!!!!
      frame1 = frame2; // frame2 may be leftover from previous burst --- only analyze for k>0!
      cvCopy(frame1,frame1_clean,0); // only needed for (image_opt == BEST_HIT_PER_BOUT)
      if (verbose) printf("pre current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
      // frame2_clean = deinterlace_image(cvQueryFrame(capture), 0, 1);
      if (scale_factor > 1) {
        frame1_raw = frame2_raw;
        frame2_raw = deinterlace_image(cvQueryFrame(capture),0,1,1);                    
        frame2 = deinterlace_image(frame2_raw, deinterlace_mode, scale_factor, par);
      } else {
        frame2 = deinterlace_image(cvQueryFrame(capture), deinterlace_mode, scale_factor, par);
      }
      if (frame1 == frame2) printf("problem: same pointer to both frames!\n"); // assert
      frame_num++;
      if(!frame1 || !frame2) {end_loop=1; break;}
      // double frame_num2 = cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES); // use this in fast mode?
      if (k>0) {
        num_found = detect_and_draw(frame1,frame2,frame1_clean,frame1_raw,gray1,gray2,gray_diff,frame_num, &current_score, print_flag); 
      } else num_found=0;
                        

      // warn if mouse may be sleeping. For best-k mode, can look backwards too.
      // should use softer threshold, e.g. with HMM. 
      if (current_score < still_threshold) {
        if (sleep_start < 0) sleep_start = frame_num;
        still_count++;
        if (still_count >= sleep_threshold) {
          is_sleeping = 1;
          if (still_count % sleep_threshold == 0) { 
            printf("warning --- mouse has been still for %d frames\n", still_count);
            logfile << "warning --- mouse has been still for " << still_count << " frames\n";
          }
          // print warning on output frame, too, or on live stream?
        }
      } else {
        if (is_sleeping) {
          sleep_end = frame_num;
          frame2mmssff(sleep_start, frame_rate, frame_name1);
          frame2mmssff(sleep_end, frame_rate, frame_name2);
          printf("possible sleep from %s to %s\n", frame_name1, frame_name2);
          logfile << "possible sleep from " << frame_name1 << " to " << frame_name2 <<"\n";
        }
        sleep_start = -1;
        sleep_end = -1;
        still_count = 0;
        is_sleeping = 0;
      }
      if (mode != NO_DISPLAY) cvShowImage(gui_name,frame1); // also only for k>0?
      if ((image_opt == ALL_HITS_PER_BOUT) && (num_found > 0)) frames_saved++;
      if (verbose) {
        printf("details: %d, %4.2f, %4.0f\n", num_found, current_score, frame_num);
        printf("post current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
      }
      if (use_trackbar) { // position 0 crashed opencv2.1 on os x
        // printf("at position %10.2f of %10.2f \n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES),frame_count);
        // printf("setting trackbar to %d \n", (int)(1+(1000*cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES))/frame_count));
        cvSetTrackbarPos("progress", gui_name, (int)(1+(1000*(cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES)-pos_1))/frame_count));
      }
      if ((image_opt == FIRST_HIT_PER_BOUT) && (num_found > 0)) { //jump to next burst, and fix frame_num count!
        frames_saved++;
        if (worst_of_best == 0) interval_saved++;
        best_dexes[worst_of_best] = frame_num;
        best_scores[worst_of_best] = current_score;
        worst_of_best++;
        if (worst_of_best == n) {
          penalty_count = burst_length-k;
          break;
        }
      }
      // Can there be spurious 0 scores for empty frames? Should be okay, since num_found would be zero.
      if ((image_opt != FIRST_HIT_PER_BOUT) && (num_found > 0) && (current_score < best_scores[worst_of_best])) {
        if (verbose) printf("best so far! %d, %4.2f,%4.2f\n", num_found, current_score, frame_num);
        best_scores[worst_of_best] = current_score;
        best_dexes[worst_of_best] = frame_num;
        if (image_opt == BEST_HIT_PER_BOUT) {
          cvCopy(frame1_clean, best1[worst_of_best], 0);
          cvCopy(frame2, best2[worst_of_best], 0);
          cvCopy(frame1_raw, best1_raw[worst_of_best],0);
        }
        // update worst of best
        worst_of_best = 0;
        for (int j=0; j<n; j++) {
          if (best_scores[j] > best_scores[worst_of_best]) worst_of_best = j;
          // if ((best_dexes[j] < 0) | (best_scores[j] > best_scores[worst_of_best])) worst_of_best = j;
        }
      }

      c=cvWaitKey(10);
      if(c==113) {end_loop=1; break;} // lower-case q to quit: break inner loop then while test fails

      // hang on to frame in gui?
      cvReleaseImage(&frame1);
      if (scale_factor > 1) cvReleaseImage(&frame1_raw);
      // release anything else?
    }

    // for jumps-ahead, or intervals ending in sleep 
    if (is_sleeping) {
      sleep_end = frame_num;
      frame2mmssff(sleep_start, frame_rate, frame_name1);
      frame2mmssff(sleep_end, frame_rate, frame_name2);
      printf("possible sleep from %s to %s\n", frame_name1, frame_name2);
      logfile << "possible sleep from " << frame_name1 << " to " << frame_name2 <<"\n";
    }

    if (image_opt == BEST_HIT_PER_BOUT) { //fill from left
      int num_found = 0;
      if (verbose) printf("printing best so far\n");
      for (int j=0; j<n; j++) { //could use while loop
        if (best_dexes[j] > 0 ) {
          frames_saved++;
          num_found++;
          cvCopy(best1[j],best1_clean,0);
          detect_and_draw(best1[j], best2[j], best1_clean, best1_raw[j], 
                          gray1, gray2, gray_diff, best_dexes[j], &current_score, FORCE_PRINT);
          // detect_and_draw(best1[j], best2[j], best1_raw[j], gray1, gray2, gray_diff, best_dexes[j], &current_score, FORCE_PRINT);
        }
      }
      printf("Saved %d out of requested %d frames.\n", num_found,n);
      logfile << "Saved " << num_found <<" out of requested " << n << " frames.\n";
      if (num_found > 0) interval_saved++;
    }

    if ((image_opt == ALL_HITS_PER_BOUT) && (best_dexes[0] > 0)) {
      interval_saved++;
    }

    if (image_opt == FIRST_HIT_PER_BOUT) {
      printf("Saved %d out of requested %d frames.\n", worst_of_best,n);
      logfile << "Saved " << worst_of_best <<" out of requested " << n << " frames.\n";
    }

    if (c==113) {end_loop=1; break;}

    if (slow_mode) {
      int skipped_frame = 1;
      for(int k=0; k<gap_length-burst_length + penalty_count - 1; k++){
        if (verbose) printf("skipping pre current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
        // frame=cvQueryFrame(capture); // can we just grab but not decompress?
        skipped_frame = cvGrabFrame(capture);
        if(!skipped_frame) {end_loop=1; break;}
        frame_num++;
        if (verbose) printf("skipping post current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
        if (use_trackbar) 
          cvSetTrackbarPos("progress", gui_name, (int)((1000*(cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES)-pos_1))/frame_count));
        c=cvWaitKey(2);
        if(c==113) {end_loop=1; break;}
      }
    } else { // slow_mode == 0
      int set_okay = 0;
      if (verbose) printf("incrementing by %d - %d +1\n",gap_length,burst_length); // and penalty count
      // could remove this pre-test and just check set_okay return value
      if (cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES) + offset + penalty_count -1 < frame_count) {
        if (verbose) printf("inc pre current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
        set_okay = cvSetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES,
                                        cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES)+offset + penalty_count -1);
        if (verbose) printf("inc post current frame: %8.2f\n", cvGetCaptureProperty(capture,CV_CAP_PROP_POS_FRAMES));
        frame_num = frame_num + gap_length-burst_length + penalty_count -1;
      } else {end_loop=1; break;}
      if (set_okay == 0) {end_loop=1; break;}
    }
    if (c==113) {end_loop=1; break;}
  }

  cvReleaseImage(&frame1_clean);
  cvReleaseImage(&frame1_raw);
  cvReleaseImage(&frame2_raw);
  cvReleaseImage(&gray1);
  cvReleaseImage(&gray2);
  cvReleaseImage(&gray_diff);
  cvReleaseImage(&frame1);
  cvReleaseImage(&frame2);
  for (int i=0; i<n; i++) {
    cvReleaseImage(&best1[i]);
    cvReleaseImage(&best2[i]);
    cvReleaseImage(&best1_raw[i]);
  }
  cvReleaseImage(&best1_clean);
  cvReleaseCapture(&capture);
  if (mode != NO_DISPLAY) cvDestroyWindow(gui_name);

  if (c == 113) {
    printf("Video analysis interrupted.\n"); 
    logfile << "Video analysis interrupted.\n";
  } else if (mode != JUST_TUNE_ROI) {
    printf("Video analysis completed.\n");
    printf("Images saved for %d of the %d intervals analyzed.\n", interval_saved, interval_count);
    printf("Total of %d frames saved.\n", frames_saved);
    logfile << "Video analysis completed.\n";
    logfile << "Images saved for " << interval_saved << " of the " << interval_count << " intervals analyzed.\n"; 
    logfile << "Total of " << frames_saved << " frames saved.\n";
  }

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(sdate,9, "%x", timeinfo);
  strftime(stime,9, "%X", timeinfo);
  
  logfile << "End of analysis: " << stime << " on " << sdate << endl; 
  logfile.close(); 
  return 1;
}



void my_mouse_callback( int event, int x, int y, int flags, void* param ){
  //IplImage* image = (IplImage*) param;
  if (allow_draw) { // only use during adjust_roi --- unregister listener afterward instead?
    switch( event ){
    case CV_EVENT_MOUSEMOVE:
      if( drawing_box ){
        roi_box.width = x - roi_box.x;
        roi_box.height = y - roi_box.y;
      }
      redraw_me = 1;
      break;

    case CV_EVENT_LBUTTONDOWN:
      drawing_box = true;
      roi_box = cvRect( x, y, 1, 1 );
      redraw_me = 1;
      break;

    case CV_EVENT_LBUTTONUP:
      drawing_box = false;
      if( roi_box.width < 0 ){
        roi_box.x += roi_box.width;
        roi_box.width *= -1;
      }
      if( roi_box.height < 0 ){
        roi_box.y += roi_box.height;
        roi_box.height *= -1;
      }
      redraw_me = 2; // event not registering in windows????
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////

char adjust_roi(IplImage* frame, const char* adjust_text){
  vert_res = frame->height; // also use for bounding feature sizes and distances?
  hori_res = frame->width;
  if (verbose) {
    printf("Loaded first video frame.\n");
    printf("Vertical Resolution: %d\n", vert_res);
    printf("Horizontal Resolution: %d\n", hori_res);
  }
  if ((roi_box.x < 0) || (roi_box.y < 0) || (roi_box.width < 0) || (roi_box.height < 0)) {
    roi_box.width = (40*hori_res)/100;
    roi_box.height = (80*vert_res)/100;
    if (cage_num == 2) {roi_box.x = (55*hori_res)/100;} else {roi_box.x = (5*hori_res)/100;}
    roi_box.y = (5*vert_res)/100;
  }
        
  if ((mode == DONT_TUNE_ROI) || (mode == NO_DISPLAY)) return 0;
        
  IplImage* frame3;
  frame3 = cvCloneImage(frame);
  printf("----------------------\n");
  printf("\nNow, adjust the blue box to your zone of interest.\n");
  printf("You may meed to click the video frame to make it active, then press:\n");
  printf("  u for up\n");
  printf("  d for down\n");
  printf("  l for left\n");
  printf("  r for right\n");
  printf("  (the arrow keys may also work, but maybe not) \n");
  printf("  t for taller\n");
  printf("  s for shorter\n");
  printf("  w for wider\n");
  printf("  n for narrower\n");
  printf("Or, draw a new blue box using the mouse.\n");
  printf("When you are done with this, press c to continue.\n");
  printf("To quit the playback at any time, click the video then press q\n");
  printf("Or press control-c in the command terminal window\n");

  cvSetMouseCallback(gui_name, my_mouse_callback, (void*) frame3);      // pointer needed?

  char cc = -1;
  while ((cc!=99) && (cc!=113)) { // c,q
    if (redraw_me > 0) {
      if (redraw_me == 2) {
        if( roi_box.width < 0 ){
          roi_box.x += roi_box.width;
          roi_box.width *= -1;
        }
        if( roi_box.height < 0 ){
          roi_box.y += roi_box.height;
          roi_box.height *= -1;
        }
        if (roi_box.height == 0) roi_box.height = 1;
        if (roi_box.width == 0) roi_box.width = 1;
        if (roi_box.x < 0) roi_box.x = 0;
        if (roi_box.x > hori_res) roi_box.x = hori_res;
        if (roi_box.y < 0) roi_box.y = 0;
        if (roi_box.y > vert_res) roi_box.y = vert_res;
        printf("  upperleft-x, upperleft-y, width, height: (%d,%d,%d,%d)\n",roi_box.x,roi_box.y,roi_box.width,roi_box.height);
        if ((roi_box.width < 20) || (roi_box.height < 20)) {
          printf("Warning: the size of the region to monitor (indicated by the blue box) is very small!\n");
        }
      }

      cvCopy(frame,frame3,0);
      CvScalar color_box={{255,0,0}}; // blue
      cvRectangle(
                  frame3,
                  cvPoint(roi_box.x,roi_box.y),
                  cvPoint(roi_box.x + roi_box.width, roi_box.y + roi_box.height),
                  color_box
                  );

      if (queue_both) {
        CvScalar color_box_bak={{255,150,150}}; // lightblue
        cvRectangle(
                    frame3,
                    cvPoint(roi_box_bak.x,roi_box_bak.y),
                    cvPoint(roi_box_bak.x + roi_box_bak.width, roi_box_bak.y + roi_box_bak.height),
                    color_box_bak
                    );
      }

      cvPutText(frame3, adjust_text, cvPoint(5, 15), &img_font, cvScalar(255, 0, 0, 0));


      cvShowImage(gui_name,frame3);
      redraw_me = 0;
    }
    cc=cvWaitKey(10);

    // printf("%d\n", c);

    if(cc == 'u' || cc == 30){ roi_box.y--; redraw_me=2;}
    if(cc == 'd' || cc == 31){ roi_box.y++; redraw_me=2;}
    if(cc == 'l' || cc == 29){ roi_box.x++; redraw_me=2;}
    if(cc == 'r' || cc == 28){ roi_box.x--; redraw_me=2;}
    if(cc == 'n' || cc == 30){ roi_box.width--; redraw_me=2; }
    if(cc == 'w' || cc == 31){ roi_box.width++;  redraw_me=2;}
    if(cc == 't' || cc == 29){ roi_box.height++; redraw_me=2;}
    if(cc == 's' || cc == 28){ roi_box.height--;  redraw_me=2;}

  }
  if( roi_box.width < 0 ){
    roi_box.x += roi_box.width;
    roi_box.width *= -1;
  }
  if( roi_box.height < 0 ){
    roi_box.y += roi_box.height;
    roi_box.height *= -1;
  }
  if (roi_box.height == 0) roi_box.height=1;
  if (roi_box.width == 0) roi_box.width=1;
  if (roi_box.y + roi_box.height > vert_res) {
    roi_box.height = vert_res - roi_box.y;
    printf("Reducing zone height so that zone is contained in frame.\n");
  }
  if (roi_box.x + roi_box.width > hori_res) {
    roi_box.width = hori_res - roi_box.x;
    printf("Reducing zone width so that zone is contained in frame.\n");
  }

  printf("Final region to monitor:\n");
  printf("  upperleft-x, upperleft-y, width, height: (%d,%d,%d,%d)\n",roi_box.x,roi_box.y,roi_box.width,roi_box.height);
  printf("----------------------\n");
  if ((roi_box.width < 20) || (roi_box.height < 20)) {
    printf("Warning: the size of the region to monitor (indicated by the blue box) is very small!\n");
    // give another chance to enlarge box here?
  }

  cvReleaseImage(&frame3);

  return cc;
}


// converts frame number to mm:ss:ff, where this is approximate for 29.97 fps?
// could also use mm:ss.ss also. Need hh?
int frame2mmssff(double frame_number, double fps, char* mmssff) {
  fps = 30.0; // round if 29.97?
  int mm = (int) (frame_number/(fps*60));
  frame_number = frame_number - (fps*60)*mm;
  int ss = (int) (frame_number/fps);
  int ff = (int) (frame_number - (fps*ss) + 1);
  // char mmssff[11];
  sprintf(mmssff,"%02dm%02ds%02df",mm,ss,ff);
  // mmssff << mm << "m" << ss << ":" << ff;
  return 1;
}

int frame2mmssss(double frame_number, double fps, char* mmssss) {
  fps = 30.0; // round if 29.97?
  int mm = (int) (frame_number/(fps*60));
  frame_number = frame_number - (fps*60)*mm;
  double ss = (frame_number/fps);
  // int ff = (int) (frame_number - (fps*ss));
  // char mmssff[11];
  sprintf(mmssss,"%02dm%02.2fs",mm,ss);
  // mmssff << mm << ":" << ss << ":" << ff;
  return 1;
}
/////////////////////////////////////////////

// make this more generic? pass pointer?
string constructOutstring(string name_template, string frame_string, string option_string){
    size_t loc;
    loc = name_template.find("<frame_number>");
    while(loc!=string::npos){
        name_template.replace((int)loc,14,frame_string);
        loc = name_template.find("<frame_num>");
    }
    loc = name_template.find("<img_option>");
    while(loc!=string::npos){
        name_template.replace((int)loc,12,option_string);
        loc = name_template.find("<img_option>");
    }
    // make sure no remaing < or >!!!!!
    return name_template;
}

// memory leak here? not currently used, just set ROI before saving image.
IplImage* crop_image(IplImage* src,  CvRect roi){
  cvResetImageROI( src ); // want this? Do ROIs nest?
  IplImage* cropped = cvCreateImage( cvSize(roi.width,roi.height), src->depth, src->nChannels );
  cvSetImageROI( src, roi );
  cvCopy( src, cropped );
  cvResetImageROI( src ); // okay, but do we want the previous ROI from roi_box restored?
  return cropped;
}

///////////////////////////////
// odd vs even option for bob?
IplImage* deinterlace_image(IplImage* src, int mode, int scale_factor, double par) {
  if (!src) return 0;
  IplImage* res;
 
  if ((mode == 0) && (scale_factor == 1) && (par == 1)) {
    res = cvCloneImage(src);
    return res;
  } else if ((mode == 0) && ((scale_factor != 1) || (par != 1))) {
    res = cvCreateImage(cvSize((int)((src->width)*par/scale_factor), (int)((src->height)/scale_factor)), src->depth, src->nChannels);
    cvResize(src,res, CV_INTER_LINEAR);
    return res;
  } else if ((mode != 0) && ((scale_factor % 2) == 0)) {
    res = cvCreateImage(cvSize((int)((src->width)/scale_factor), (int)((src->height)/scale_factor)), src->depth, src->nChannels);
    cvResize(src,res, CV_INTER_NN);
    if (par != 1) {
      // printf("stretching horizontal res\n");
      IplImage* res2 = cvCreateImage(cvSize((int)((res->width)*par), (int)((res->height))), res->depth, res->nChannels);
      cvResize(res,res2, CV_INTER_LINEAR);
      cvReleaseImage(&res);
      return res2;
    } else return res;
  } else { // mode != 0 and odd scale factor, so need to deinterlace before downsizing
    res = cvCloneImage(src);
    
    uchar* linea;
    uchar* lineb;
    uchar* linec;
    
    if (mode == 2) { // linear
      for (int i=1; i < res->height-1; i+=2) {
        linea = (uchar*)res->imageData + ((i-1)*res->widthStep);
        lineb = (uchar*)res->imageData + ((i)*res->widthStep);
        linec = (uchar*)res->imageData + ((i+1)*res->widthStep);
        for (int j = 0; j < res->width * res->nChannels; j++){
          lineb[j] = (uchar)((linea[j] + linec[j])/2);
        }
      }
    }
    
    if (mode == 1) { // bob --- make odd and even version?
      for (int i=1; i < res->height-1; i+=2) {
        linea = (uchar*)res->imageData + ((i-1)*res->widthStep);
        lineb = (uchar*)res->imageData + ((i)*res->widthStep);
        for (int j = 0; j < res->width * res->nChannels; j++){
          lineb[j] = (uchar)(linea[j]);         
        }
      }
    }
  }
  
  if ((scale_factor != 1) || (par != 1)) {
    IplImage* res2 = cvCreateImage(cvSize((int)((res->width)*par/scale_factor), (int)((res->height)/scale_factor)), res->depth, res->nChannels);
    cvResize(res,res2, CV_INTER_AREA); // also LINEAR, CUBIC, AREA
    cvReleaseImage(&res);
    return res2;
  } else return res;

}

////////////////////////// not used ////////////////////////////////

double image_diff(IplImage* img1, IplImage* img2){

  CvScalar diff_sum;
  double diff_norm;

  IplImage* gray1 = cvCreateImage(cvSize(img1->width,img1->height),8,1);
  IplImage* gray2 = cvCreateImage(cvSize(img2->width,img2->height),8,1);
  IplImage* gray_diff  = cvCreateImage(cvSize(img2->width,img2->height),8,1); //  = cvCreateImage(cvSize(img2->width,img2->height),8,1);

  cvCvtColor(img1,gray1,CV_BGR2GRAY);
  cvCvtColor(img2,gray2,CV_BGR2GRAY);
  cvAbsDiff( gray1, gray2, gray_diff );
  diff_sum = cvSum( gray_diff);
  diff_norm = cvNorm( img1, img2, CV_L1, 0 );

  return diff_norm;
}

/////////////////////////////////////////////////////////////////
//
//  How do we pick a sensible threshold for movement?
//  Collect statistics from many frames?
//  Monitor background region as well, to estimate background noise?
//  Just pick frame with smallest movement in burst, regardless of threshold?
//
///////////////////////////////////////////////////////////////

// Also return movement in ROI? And pointers to images to potentially save, rather than re-analyzing raw frames?
// img2 is prev image. Or null for no diff estimation??
int detect_and_draw(IplImage* img1, IplImage* img2, IplImage* clean, IplImage* raw, IplImage* gray1, IplImage* gray2, 
                    IplImage* gray_diff, double frame_num, double* diff_score, int print_me){ 

  CvScalar color_eye={{0,0,255}}; // red
  CvScalar color_ear={{0,255,0}}; // green
  CvScalar color_box={{255,0,0}}; // blue
  CvScalar color_face={{255,0,255}}; // blue

  // ROIs need to agree in size, not images!!!!!!
  // printf("img1 size %d x %d x %d \n",img1->width, img1->height,img1->nChannels);
  // printf("gray1 size %d x %d x %d \n",gray1->width, gray1->height,gray1->nChannels);
  cvCvtColor(img1,gray1,CV_BGR2GRAY);
  cvCvtColor(img2,gray2,CV_BGR2GRAY);

  CvScalar diff_sum;
  // double diff_norm;
  double mean_diff;
  CvScalar diff_sum_roi;
  // double diff_norm_roi;
  double mean_diff_roi;

  cvAbsDiff( gray1, gray2, gray_diff);
  diff_sum = cvSum( gray_diff );
  // diff_norm = cvNorm( gray1, gray2, CV_L1, 0 );

  // cvEqualizeHist(gray,gray); //? need reference image here, right?
  // CvRect* myROI=(CvRect*)cvGetSeqElem(eyes,i);

  cvSetImageROI(gray1,roi_box); // set ROI before converting to greyscale? Might need to change indices later.
  cvSetImageROI(gray2,roi_box);
  cvSetImageROI(gray_diff,roi_box);
  // cvAbsDiff( gray1, gray2, gray_diff);

  diff_sum_roi = cvSum( gray_diff );
  // diff_norm_roi = cvNorm( gray1, gray2, CV_L1, 0 );
  // use number of pixels exceeding threshold instead ?????

  mean_diff = diff_sum.val[0]/(1.0*(img1->width)*(img1->height));
  mean_diff_roi = diff_sum_roi.val[0]/(1.0*(roi_box.width)*(roi_box.height));
  // need true ROI, contained within image here!!!

  // cvScalar has four components, need to use val here!
  // printf("diffs1: %8.2f, %8.2f, %8.2f, %8.2f\n", diff_sum.val[0], diff_norm, diff_sum_roi.val[0], diff_norm_roi);

  if (verbose) printf("mean diffs: %8.2f, %8.2f\n", mean_diff, mean_diff_roi);

  // printf("diffs2: %8.2f, %8.2f, %8.2f, %8.2f\n", diff_sum.val[0], diff_sum.val[1], diff_sum.val[2], diff_sum.val[3]);
  // printf("diffs3: %8.2f, %8.2f, %8.2f, %8.2f\n", diff_sum_roi.val[0], diff_sum_roi.val[1], diff_sum_roi.val[2], diff_sum_roi.val[3]);

  // careful! rect and rectangle have different semantics!
  cvRectangle(
              img1,
              cvPoint(roi_box.x,roi_box.y),
              cvPoint(roi_box.x + roi_box.width, roi_box.y + roi_box.height),
              color_box
              );

  CvMemStorage* storage_eye = cvCreateMemStorage(0);
  cvClearMemStorage(storage_eye);
  CvMemStorage* storage_ear = cvCreateMemStorage(0);
  cvClearMemStorage(storage_ear);

  // symmetrize features by flipping image left-right?
  CvSeq* eyes = cvHaarDetectObjects(gray1, eye, storage_eye);
  
  CvSeq* ears;
  // short-circuit this when no display is shown
  if (!short_circuit || (eyes && (eyes->total > 0)))
    ears = cvHaarDetectObjects(gray1, ear, storage_ear);

  // if dispay is off, could do this just for images to be saved
  for(int i=0;i<(eyes ? eyes->total :0);i++){
    CvRect* r=(CvRect*)cvGetSeqElem(eyes,i);
    cvRectangle(
                img1,
                cvPoint(r->x + roi_box.x, r->y + roi_box.y),
                cvPoint(r->x + r->width + roi_box.x, r->y + r->height + roi_box.y),
                color_eye
                );
  }
  for(int i=0;i<(ears ? ears->total :0);i++){
    CvRect* r=(CvRect*)cvGetSeqElem(ears,i);
    cvRectangle(
                img1,
                cvPoint(r->x + roi_box.x, r->y + roi_box.y),
                cvPoint(r->x + r->width + roi_box.x, r->y + r->height + roi_box.y),
                color_ear
                );
  }

  int compatible_features = 0;
  CvRect face_box; //  set to ROI in case no face found
  face_box.x = roi_box.x;
  face_box.y = roi_box.y;
  face_box.width = roi_box.width;
  face_box.height = roi_box.height;

  for(int i=0;i<(eyes ? eyes->total :0);i++){
    CvRect* r1=(CvRect*) cvGetSeqElem(eyes,i);
    double cx1 = (r1->x) + (r1->width)/2.0;
    double cy1 = (r1->y) + (r1->height)/2.0;
    for(int j=0;j<(ears ? ears->total :0);j++){
      CvRect* r2=(CvRect*) cvGetSeqElem(ears,j);
      double cx2 = (r2->x) + (r2->width)/2.0;
      double cy2 = (r2->y) + (r2->height)/2.0;
      if (
          (cy1 + 2.0*(r1->height) < roi_box.height) && // eyes not too low, 
          (cy1 > cy2) &&  // eyes below ears // bound sizes of features, too?
          (pow(cx1-cx2,2.0) + pow(cy1-cy2,2.0) < pow(4.0*(r1->height),2.0)) && // eyes and ears close
          (pow(cx1-cx2,2.0) + pow(cy1-cy2,2.0) > pow(1.0*(r1->height),2.0)) // but not too close. shouldn't overlap, either?
          ) {
        compatible_features++;
        face_box.x = r1->x + (r1->width)/2 + roi_box.x - 3*(r1->width);   // center of eye, x-coord;
        face_box.y = r1->y + (r1->height)/2 + roi_box.y - 4*(r1->height); // center of eye, y-coord;
        face_box.width  = 6*(r1->width);  // roi_box.width;
        face_box.height = 7*(r1->height); // roi_box.height;
        if (face_box.x < 0) face_box.x = 0;
        if (face_box.y < 0) face_box.y = 0;
        if (face_box.x + face_box.width > hori_res) face_box.width = hori_res - face_box.x - 1; // need -1?
        if (face_box.y + face_box.height > vert_res) face_box.height = vert_res - face_box.y - 1; // need -1?
                                
        cvRectangle(
                    img1,
                    cvPoint(face_box.x, face_box.y),
                    cvPoint(face_box.x + face_box.width, face_box.y + face_box.height),
                    color_face
                    );
      }
    }
  }

  if ((print_me == FORCE_PRINT) || ((print_me == PRINT_IF_HIT) && (compatible_features > 0))){
        
    frame2mmssff(frame_num, frame_rate, frame_name); // frame rate and frame name global -- was the name a slow memory leak?

    IplImage* raw2; // need to delete if never used?

    // if ((scale_factor > 0) && without_box) {
    if (full_size) {
      raw2 = deinterlace_image(raw, deinterlace_mode, 1, par); // expand horizontal or compress vertical?
    }

    stringstream image_name;

    string diff_stub = "motion";
    string whole_stub = "whole";
    string zone_stub = "zone";
    string face_stub = "face";
    string box_stub = "box";
    string clean_stub = "_clean";
    

    if (whole_frame) {
      if (with_box) {
	string image_string(image_stem);
        // implicit cast of frame_name to string?
        image_string = constructOutstring(image_string, frame_name, whole_stub);
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),img1);
      }
      if (without_box && !full_size) {
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, whole_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),clean);
      }
      if (full_size) {
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, whole_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),raw2);
      }
    }

    if (just_zone) {
      if (with_box) {
        cvSetImageROI(img1,roi_box);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, zone_stub);
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),img1);
      }
      if (without_box && !full_size) {
        cvSetImageROI(clean,roi_box);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, zone_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),clean);
      }
      if (full_size) {
        CvRect roi_box_big; //  set to ROI in case no face found
        roi_box_big.x = scale_factor*roi_box.x;
        roi_box_big.y = scale_factor*roi_box.y;
        roi_box_big.width = scale_factor*roi_box.width;
        roi_box_big.height = scale_factor*roi_box.height;
        cvSetImageROI(raw2,roi_box_big);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, zone_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),raw2);
      }

    }

    if (just_face) {
      if (with_box) {
        cvSetImageROI(img1,face_box);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, face_stub);
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),img1);
      }
      if (without_box && !full_size) {
        cvSetImageROI(clean,face_box);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, face_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),clean);
      }
      if (full_size) {
        CvRect face_box_big; //  set to ROI in case no face found
        face_box_big.x = scale_factor*face_box.x;
        face_box_big.y = scale_factor*face_box.y;
        face_box_big.width = scale_factor*face_box.width;
        face_box_big.height = scale_factor*face_box.height;
        cvSetImageROI(raw2,face_box_big);
        string image_string(image_stem);
        image_string = constructOutstring(image_string, frame_name, face_stub.append(clean_stub));
        printf("saving image %s\n", image_string.c_str());
        logfile << " -- saving image " << image_string.c_str() << "\n";
        cvSaveImage(image_string.c_str(),raw2);
      }

    }

    if (motion_diff) {
      stringstream image_text;
      image_text << "average diff: " << ((double) ((long) (100*(mean_diff_roi)))/100.0);
      cvPutText(gray_diff, image_text.str().c_str(), cvPoint(10, 30), &img_font, cvScalar(255, 255, 255, 0));
      // reset roi before saving?
      string image_string(image_stem);
      image_string = constructOutstring(image_string, frame_name, diff_stub);
      printf("saving image %s\n", image_string.c_str());
      logfile << " -- saving image " << image_string.c_str() << "\n";
      cvSaveImage(image_string.c_str(),gray_diff);
    }

    if (full_size) cvReleaseImage(&raw2);
  }

  cvReleaseMemStorage(&storage_eye);
  cvReleaseMemStorage(&storage_ear);

  cvResetImageROI(img1);
  cvResetImageROI(img2);
  cvResetImageROI(clean);
  cvResetImageROI(raw);
  cvResetImageROI(gray1);
  cvResetImageROI(gray2);
  cvResetImageROI(gray_diff);

  *diff_score = mean_diff_roi;
  return compatible_features;

}
