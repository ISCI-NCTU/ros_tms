//-----------------------------------------------------------
// @file   : pot_ctrl.cpp
// @author : Watanabe Yuuta
// @version: Ver0.1.4 (since 2014.05.02)
// @date   : 2016.06.09
//-----------------------------------------------------------

#include <ros/ros.h>
#include <pthread.h>
#include <fstream>
#include <std_msgs/String.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/MultiEchoLaserScan.h>
#include <urg_c/urg_sensor.h>
#include <urg_c/urg_utils.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <tms_msg_ss/tracking_points.h>
#include <tms_msg_ss/tracking_grid.h>

pthread_mutex_t mutex_laser = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_target = PTHREAD_MUTEX_INITIALIZER;

#include "opencv/cv.h"

#include "define.h"
#include "target.h"
#include "laser.h"

#include "particle_filter.h"
#include "multiple_particle_filter.h"

// PORTABLE_PARAMETOR-------------------------
#define ORIGIN_X 0.0
#define ORIGIN_Y 0.0
#define MAX_FIELD_X 15.0
#define MAX_FIELD_Y 7.0
#define PORTABLE_POSITION_X 10.0
#define PORTABLE_POSITION_Y 0.0
//-------------------------------------------

// LASER_PARAMETOR----------------------------
std::vector< float > scanData1;
std::vector< float > scanData2;
std::vector< float > scanData3;
CLaser laser;
bool CallbackCalled1 = false;
bool CallbackCalled2 = false;
bool CallbackCalled3 = false;
int nStep1 = 700;
int nStep2 = 700;
int nStep3 = 700;
double StartAngle1 = -90.0;
double StartAngle2 = -90.0;
double StartAngle3 = -90.0;
double DivAngle1 = 0.35;
double DivAngle2 = 0.35;
double DivAngle3 = 0.35;
//--------------------------------------------

void *Visualization(void *ptr)
{
  int ID;
  float X;
  float Y;
  ros::Rate r(10);
  ros::Publisher *pub = (ros::Publisher *)ptr;
  int latest_id = 0;

  while (ros::ok())
  {
    tms_msg_ss::tracking_grid grid;
    tms_msg_ss::tracking_points points;

    pthread_mutex_lock(&mutex_target);

    for (int i = 0; i < MAX_TRACKING_OBJECT; i++)
    {
      if (laser.m_pTarget[i] != NULL)
      {
        if (laser.m_pTarget[i]->cnt < 80)
        {
          continue;
        }

        ID = (laser.m_pTarget[i]->id);
        // X  = -(laser.m_pTarget[i]->py) + PORTABLE_POSITION_X * 1000.0;
        // Y  =  (laser.m_pTarget[i]->px) + PORTABLE_POSITION_Y * 1000.0;
        X = (laser.m_pTarget[i]->px);
        Y = (laser.m_pTarget[i]->py);
        // std::cout << " x " << X << " y "  << Y << std::endl;

        if (ORIGIN_X < X && X < MAX_FIELD_X * 1000.0 && ORIGIN_Y < Y && Y < MAX_FIELD_Y * 1000.0)
        {
          grid.id = ID;
          grid.x = X / 1000;
          grid.y = Y / 1000;

          points.tracking_grid.push_back(grid);
        }
      }
    }
    pthread_mutex_unlock(&mutex_target);

    pub->publish(points);
    r.sleep();
  }

  return 0;
}

void *Processing(void *ptr)
{
  CMultipleParticleFilter m_PF;
  ros::Rate r(120);

  laser.Init();
  /**********************************/
  laser.m_bNodeActive[0] = true;
  laser.m_bNodeActive[1] = true;
  laser.m_bNodeActive[2] = true;
  laser.m_nConnectNum = 3;
  laser.GetLRFParam();

  if (laser.m_bNodeActive[0])
    while (!CallbackCalled1)
      r.sleep();
  if (laser.m_bNodeActive[1])
    while (!CallbackCalled2)
      r.sleep();
  if (laser.m_bNodeActive[2])
    while (!CallbackCalled3)
      r.sleep();

  laser.m_nStep[0] = nStep1;
  laser.m_nStep[1] = nStep2;
  laser.m_nStep[2] = nStep3;
  laser.m_StartAngle[0] = StartAngle1;
  laser.m_StartAngle[1] = StartAngle2;
  laser.m_StartAngle[2] = StartAngle3;
  laser.m_DivAngle[0] = DivAngle1;
  laser.m_DivAngle[1] = DivAngle2;
  laser.m_DivAngle[2] = DivAngle3;
  /**********************************/

  CvMat *m_Rotate = cvCreateMat(2, 2, CV_64F);
  CvMat *m_Translate = cvCreateMat(2, 1, CV_64F);
  CvMat *Temp = cvCreateMat(2, 1, CV_64F);
  int count;
  double theta, range;
  int UPDATE = Config::is()->update;

  if (laser.m_bResetBackRangeData == true)
  {
    for (int it = 0; it < laser.m_ring; it++)
    {
      for (int n = 0; n < laser.m_cnMaxConnect; n++)
      {
        if (laser.m_bNodeActive[n])
        {
          pthread_mutex_lock(&mutex_laser);

          if (n == 0)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData1.size());
            for (int i = 0; i < scanData1.size(); i++)
              laser.m_LRFData[n][i] = scanData1[i];
            // std::cout << "msg to scanData1" << std::endl;
          }
          else if (n == 1)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData2.size());
            for (int i = 0; i < scanData2.size(); i++)
              laser.m_LRFData[n][i] = scanData2[i];
            // std::cout << "msg to scanData2" << std::endl;
          }
          else if (n == 2)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData3.size());
            for (int i = 0; i < scanData3.size(); i++)
              laser.m_LRFData[n][i] = scanData3[i];
            // std::cout << "msg to scanData3" << std::endl;
          }
          pthread_mutex_unlock(&mutex_laser);
        }
      }
      laser.GetBackLRFDataGaussian();
      r.sleep();
    }
    laser.m_bResetBackRangeData = false;
    std::cout << "Back range data is stored" << std::endl;
  }

  int iteration = 0;
  while (ros::ok())
  {
    if (laser.m_bResetBackRangeData == false)
    {
      for (int n = 0; n < laser.m_cnMaxConnect; n++)
      {
        if (laser.m_bNodeActive[n])
        {
          pthread_mutex_lock(&mutex_laser);

          if (n == 0)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData1.size());
            for (int i = 0; i < scanData1.size(); i++)
            {
              laser.m_LRFData[n][i] = scanData1[i];
            }
          }
          if (n == 1)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData2.size());
            for (int i = 0; i < scanData2.size(); i++)
            {
              laser.m_LRFData[n][i] = scanData2[i];
            }
          }
          if (n == 2)
          {
            laser.m_LRFData[n].clear();
            laser.m_LRFData[n].resize(scanData3.size());
            for (int i = 0; i < scanData3.size(); i++)
            {
              laser.m_LRFData[n][i] = scanData3[i];
            }
          }

          laser.GetDiffLRFCluster(n);

          cvmSet(m_Rotate, 0, 0, cos(deg2rad(laser.m_LRFParam[n].rz)));
          cvmSet(m_Rotate, 0, 1, -sin(deg2rad(laser.m_LRFParam[n].rz)));
          cvmSet(m_Rotate, 1, 0, sin(deg2rad(laser.m_LRFParam[n].rz)));
          cvmSet(m_Rotate, 1, 1, cos(deg2rad(laser.m_LRFParam[n].rz)));
          cvmSet(m_Translate, 0, 0, laser.m_LRFParam[n].tx);
          cvmSet(m_Translate, 1, 0, laser.m_LRFParam[n].ty);

          laser.m_LRFPoints[n].clear();
          laser.m_LRFPoints[n].resize(laser.m_DiffLRFData[n].size());

          for (int i = 0; i < laser.m_DiffLRFData[n].size(); i++)
          {
            count = laser.m_DiffLRFData[n][i].n;
            range = laser.m_DiffLRFData[n][i].range;
            theta = laser.m_DivAngle[n] * count + laser.m_StartAngle[n];

            cvmSet(laser.m_LRFPos[n][i], 0, 0, range * cos(deg2rad(theta)));
            cvmSet(laser.m_LRFPos[n][i], 1, 0, range * sin(deg2rad(theta)));
            cvmMul(m_Rotate, laser.m_LRFPos[n][i], Temp);     //
            cvmAdd(m_Translate, Temp, laser.m_LRFPos[n][i]);  //
            laser.m_LRFPoints[n][i].x = cvmGet(laser.m_LRFPos[n][i], 0, 0) * 1000.0;
            laser.m_LRFPoints[n][i].y = cvmGet(laser.m_LRFPos[n][i], 1, 0) * 1000.0;
          }

          // Number of clusters
          std::cout << "Number of clusters " << laser.m_LRFClsData[n].size() << std::endl;

          laser.m_LRFClsPoints[n].clear();
          laser.m_LRFClsPoints[n].resize(laser.m_LRFClsData[n].size());
          for (int i = 0; i < laser.m_LRFClsData[n].size(); i++)
          {
            count = laser.m_LRFClsData[n][i].n;
            range = laser.m_LRFClsData[n][i].range;
            theta = laser.m_DivAngle[n] * count + laser.m_StartAngle[n];

            cvmSet(laser.m_LRFClsPos[n][i], 0, 0, range * cos(deg2rad(theta)));
            cvmSet(laser.m_LRFClsPos[n][i], 1, 0, range * sin(deg2rad(theta)));
            cvmMul(m_Rotate, laser.m_LRFClsPos[n][i], Temp);     //
            cvmAdd(m_Translate, Temp, laser.m_LRFClsPos[n][i]);  //
            laser.m_LRFClsPoints[n][i].x = cvmGet(laser.m_LRFClsPos[n][i], 0, 0) * 1000.0;
            laser.m_LRFClsPoints[n][i].y = cvmGet(laser.m_LRFClsPos[n][i], 1, 0) * 1000.0;

            // std::cout << "n " << n << " " << laser.m_LRFClsPoints[n][i].x << " " << laser.m_LRFClsPoints[n][i].y <<
            // std::endl;
          }

          pthread_mutex_unlock(&mutex_laser);
        }
      }
    }
    std::vector< double > class_point_x;
    std::vector< double > class_point_y;

    for (int n = 0; n < laser.m_cnMaxConnect; n++)
    {
      for (int i = 0; i < laser.m_LRFClsData[n].size(); i++)
      {
        class_point_x.push_back(laser.m_LRFClsPoints[n][i].x);
        class_point_y.push_back(laser.m_LRFClsPoints[n][i].y);
      }
    }

    laser.m_LRFClsPoints[0].clear();
    laser.m_LRFClsPoints[0].resize(class_point_x.size());

    for (int i = 0; i < class_point_x.size(); i++)
    {
      laser.m_LRFClsPoints[0][i].x = class_point_x[i];
      laser.m_LRFClsPoints[0][i].y = class_point_y[i];
    }

    class_point_x.clear();
    class_point_y.clear();

    system("clear");

    pthread_mutex_lock(&mutex_target);
    m_PF.update(&laser);
    pthread_mutex_unlock(&mutex_target);

    // if (!(iteration % UPDATE)) laser.GetBackLRFDataGaussian();

    r.sleep();
    iteration++;
  }

  cvReleaseMat(&Temp);
  cvReleaseMat(&m_Rotate);
  cvReleaseMat(&m_Translate);
}

void LaserSensingCallback1(const sensor_msgs::LaserScan::ConstPtr &scan)
{
  pthread_mutex_lock(&mutex_laser);
  int num = floor((scan->angle_max - scan->angle_min) / scan->angle_increment) + 1;
  if (CallbackCalled1 == false)
  {
    nStep1 = num;
    StartAngle1 = scan->angle_min * 180.0 / M_PI;
    DivAngle1 = scan->angle_increment * 180.0 / M_PI;
    std::cout << "nStep1 " << nStep1 << " StartAngle1 " << StartAngle1 << " DivAngle1 " << DivAngle1 << std::endl;
  }
  if (scanData1.size() == 0)
    scanData1.resize(num);
  scanData1 = scan->ranges;
  pthread_mutex_unlock(&mutex_laser);
  CallbackCalled1 = true;
}

void LaserSensingCallback2(const sensor_msgs::LaserScan::ConstPtr &scan)
{
  pthread_mutex_lock(&mutex_laser);
  int num = floor((scan->angle_max - scan->angle_min) / scan->angle_increment) + 1;
  if (CallbackCalled2 == false)
  {
    nStep2 = num;
    StartAngle2 = scan->angle_min * 180.0 / M_PI;
    DivAngle2 = scan->angle_increment * 180.0 / M_PI;
    std::cout << "nStep2 " << nStep2 << " StartAngle2 " << StartAngle2 << " DivAngle2 " << DivAngle2 << std::endl;
  }
  if (scanData1.size() == 0)
    scanData2.resize(num);
  scanData2 = scan->ranges;
  pthread_mutex_unlock(&mutex_laser);
  CallbackCalled2 = true;
}

void LaserSensingCallback3(const sensor_msgs::LaserScan::ConstPtr &scan)
{
  pthread_mutex_lock(&mutex_laser);
  int num = floor((scan->angle_max - scan->angle_min) / scan->angle_increment) + 1;
  if (CallbackCalled3 == false)
  {
    nStep3 = num;
    StartAngle3 = scan->angle_min * 180.0 / M_PI;
    DivAngle3 = scan->angle_increment * 180.0 / M_PI;
    std::cout << "nStep3 " << nStep3 << " StartAngle3 " << StartAngle3 << " DivAngle3 " << DivAngle3 << std::endl;
  }
  if (scanData1.size() == 0)
    scanData3.resize(num);
  scanData3 = scan->ranges;
  pthread_mutex_unlock(&mutex_laser);
  CallbackCalled3 = true;
}

int main(int argc, char **argv)
{
  Config::is();

  pthread_t thread_p;
  pthread_t thread_v;
  ros::MultiThreadedSpinner spinner(4);

  ros::init(argc, argv, "urg_tracker");
  ros::NodeHandle n;
  ros::Publisher pub = n.advertise< tms_msg_ss::tracking_points >("tracking_points", 10);
  ros::Subscriber sub1 = n.subscribe("pot_urg1/LaserTracker", 1000, LaserSensingCallback1);
  ros::Subscriber sub2 = n.subscribe("pot_urg2/LaserTracker", 1000, LaserSensingCallback2);
  ros::Subscriber sub3 = n.subscribe("pot_urg3/LaserTracker", 1000, LaserSensingCallback3);

  if (pthread_create(&thread_v, NULL, Visualization, (void *)&pub))
  {
    printf("error creating thread.");
    abort();
  }

  if (pthread_create(&thread_p, NULL, Processing, NULL))
  {
    printf("error creating thread.");
    abort();
  }

  spinner.spin();  // spin() will not return until the node has been shutdown

  ros::waitForShutdown();

  if (pthread_join(thread_p, NULL))
  {
    printf("error joining thread.");
    abort();
  }
  if (pthread_join(thread_v, NULL))
  {
    printf("error joining thread.");
    abort();
  }

  scanData1.clear();
  scanData2.clear();
  scanData3.clear();

  return 0;
}
