#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/gpu/gpu.hpp"
#include "opencv2/ocl/ocl.hpp"
#include "opencv2/nonfree/ocl.hpp"

#include <pthread.h>

#include <iostream>
#include <ctype.h>
#include <ctime>
#include <sstream>
#include <cmath>
#include <unistd.h>
#include <sys/time.h>

using namespace cv;
using namespace std;
//#define DEBUG
#define PICTURE

//#define MAX_TREATED_LOOP 3
//#define MAX_UNTREATED_LOOP 20

#include "imageProcessing.h"
#include "postDetection.h"
#include "lib.h"

int HUE_CHANNEL = 0 ;
int SATURATION_CHANNEL = 2 ; 
int LIGHTNESS_CHANNEL = 1 ;

int H_min = 80 ;
int H_max = 95 ;
int S_min = 0 ;
int S_max = 255 ;
int V_min = 0 ;
int V_max = 255 ;

Mat hlsChannels[3];
stringstream stringOutput;

const char WINDOW_ORIGIN[] = "Original Image";
const char WINDOW_THRESHOLD[] = "Image with threshold applied";
const char WINDOW_THRESHOLD_NOISE[] = "Image with threshold applied and noise cancelation";
const char WINDOW_THRESHOLD_NOISE_BLUR[] = "Image with threshold applied and noise cancelation and blur";
const char WINDOW_CONFIG[] = "Configuration";
const char WINDOW_TEST[] = "Test";


const char WINDOW_HUE[] = "Channel Hue";
const char WINDOW_LIGHT[] = "Channel Lightness";
const char WINDOW_SATURATION[] = "Channel Saturation";

const char TRACKBAR_HUE_MIN[] = "Hue min";
const char TRACKBAR_HUE_MAX[] = "Hue max";

const char TRACKBAR_SATURATION_MIN[] = "Saturation min";
const char TRACKBAR_SATURATION_MAX[] = "Saturation max";

const char TRACKBAR_VALUE_MIN[] = "Value min";
const char TRACKBAR_VALUE_MAX[] = "Value max";

extern manchoul CI;
extern louf CF;
extern WallsAngles wallsAngles;


void onMouse( int event, int x, int y, int /*flags*/, void* channel )
{
    if( event == CV_EVENT_LBUTTONDOWN )
    {
        int chan = *((int*) channel) ;
        int value  = hlsChannels[chan].at<int>((int) x, (int) y) ;
        cout << "X : " << x << ", Y : " << y << " " << chan << " value clicked " << value << endl ;
    }
}


int main( int argc, char** argv )
{
    // Benchmark variables
    //double filteringTime=0 , noiseTime=0, contourTime=0, readTime=0, waitFinal=0, totalTime, finalLoop; 
    Benchmark bench;

    // Video vars
    VideoCapture cap;
    TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
    Size subPixWinSize(10,10), winSize(31,31);

    // Get the video (filename or device)
    cap.open("IMG_6214.JPG");
    if( !cap.isOpened() )
    {
        cout << "Could not initialize capturing...\n";
        return 0;
    }

    // Display vars
    int i = 0 ;
    int n = cap.get(CV_CAP_PROP_FRAME_COUNT);

    #ifdef DISPLAY
        // Create the windows
        namedWindow(WINDOW_ORIGIN) ;
        namedWindow(WINDOW_THRESHOLD);
        namedWindow(WINDOW_THRESHOLD_NOISE);
        namedWindow(WINDOW_THRESHOLD_NOISE_BLUR);  
        namedWindow(WINDOW_CONFIG);  

        createTrackbar(TRACKBAR_HUE_MIN, WINDOW_CONFIG, &H_min, 255) ;  
        createTrackbar(TRACKBAR_HUE_MAX, WINDOW_CONFIG, &H_max, 255) ;  
        createTrackbar(TRACKBAR_SATURATION_MIN, WINDOW_CONFIG, &S_min, 255) ;  
        createTrackbar(TRACKBAR_SATURATION_MAX, WINDOW_CONFIG, &S_max, 255) ;  
        createTrackbar(TRACKBAR_VALUE_MIN, WINDOW_CONFIG, &V_min, 255) ;  
        createTrackbar(TRACKBAR_VALUE_MAX, WINDOW_CONFIG, &V_max, 255) ;  

        moveWindow(WINDOW_ORIGIN, 0, 0) ;
        moveWindow(WINDOW_THRESHOLD, 0, 0);
        moveWindow(WINDOW_THRESHOLD_NOISE, 0, 0);
        moveWindow(WINDOW_THRESHOLD_NOISE_BLUR, 0, 0);
        moveWindow(WINDOW_CONFIG, 0, 0);

        namedWindow(WINDOW_TEST);
        moveWindow(WINDOW_TEST, 0, 0);
    #endif

    #ifdef OCL
        ocl::PlatformsInfo platforms;
        ocl::getOpenCLPlatforms(platforms);
        ocl::DevicesInfo devices;
        ocl::getOpenCLDevices(devices);
        std::cout << "platforms " << platforms.size() << "  devices " << devices.size() << " " << devices[0]->deviceName << std::endl;
        ocl::setDevice(devices[0]);

    #endif

    // déclaration des threads
    pthread_t threadMaths, threadWall;
    
    //pthread_create(&threadWall, NULL, mathsRoutine, arg)

    gettimeofday(&bench.beginTime, NULL);
    BallState ballState_t1, ballState_t2;
    int numberOfTreatedLoop = 0, numberOfNonTreatedLoop = 0, noTreatment = 0;
    for(i=0 ; i < n ; i++) // boucle for car vidéo, avec un stream, sûrement un while(1)
    {
        if(noTreatment <= 0)//la balle doit être dans une position intéressante pour la vue à approfondir
        {
            numberOfTreatedLoop++;
            numberOfNonTreatedLoop = 0;
            ballState_t1 = ballState_t2;
            std::vector<CircleFound> circles;
            CircleFound ballCircle;
            findBall(cap, bench, circles);
            ballCircle = getBestCircle(circles);
            if(ballCircle.radius == 0) // means no circle
                //break;
            getBallPosition(ballCircle.x, ballCircle.y, ballCircle.radius*2, ballState_t2);
            calculateBallSpeed(ballState_t2);
            if(ballState_t2.vy < 0) // si la balle revient en arrière, on arrete le traitement un instant
            {
                noTreatment = 3;
            }
            copyStateToMaths(ballState_t1, ballState_t2, CI);

            // Start maths part
            if(ballState_t2.v)
                pthread_create(&threadMaths, NULL, Pb_Inv, NULL);

            #ifdef DEBUG
                cout << "x : "<< ballState.x << "   y : " << ballState.y << "   z : " << ballState.z <<endl;
            #endif
            #ifdef DISPLAY
                #ifdef PICTURE
                    char c = (char)waitKey(100000);
                #else
                    char c = (char)waitKey(1);
                #endif
                if( c == 27 )
                {
                    //break;
                }
            #endif
        }
        else
        {
            Mat tmp;
            cap.read(tmp);
        }

        noTreatment--;      
    }
    
    bench.finalLoop = (double) (bench.tv_end.tv_sec - bench.tv_begin.tv_sec) + ((double) (bench.tv_end.tv_usec - bench.tv_begin.tv_usec)/1000000);
    gettimeofday(&bench.endTime, NULL);
    bench.totalTime = (double) (bench.endTime.tv_sec - bench.beginTime.tv_sec) + ((double) (bench.endTime.tv_usec - bench.beginTime.tv_usec)/1000000);
    

    #ifndef DISPLAY
        //cout << stringOutput.str() ;
    #endif
    cout << "FRAMES : " << n << "  THREADS : " << getNumThreads() << "  CPUs : " << getNumberOfCPUs() <<endl;
    cout << "Total Time : " << bench.totalTime << "s" << endl;
    cout << " Read time : " << bench.readTime << "s" << endl;
    cout << " Filtering time : " << bench.filteringTime << "s" << endl;
    cout << " Noise cancellation time : " << bench.noiseTime << "s" << endl;
    cout << " Contour determination time : " << bench.contourTime << "s" << endl;
    cout << " Final Loop : " << bench.finalLoop << "s" <<  "  Supposed total : " << bench.readTime+bench.filteringTime+bench.noiseTime+bench.contourTime+bench.finalLoop << endl;

    return 0;
}
