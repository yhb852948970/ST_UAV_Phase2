/*
	main function to receive synced ros messages
*/
#include "libsgm.h"
#include "stereoGPU.h"
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

#include<iostream>
#include<algorithm>
#include<fstream>
//#include<chrono>
#include<string>
#include<thread>
//#include <unistd.h>

#include<ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/exact_time.h>
#include <message_filters/sync_policies/approximate_time.h>

#include<opencv2/core/core.hpp>

using namespace std;

class ImageGrabber{

public:
    ImageGrabber(stereoGPU* pstereoGPU):mpstereoGPU(pstereoGPU){}

    void GrabStereo(const sensor_msgs::ImageConstPtr& msgLeft,const sensor_msgs::ImageConstPtr& msgRight);

    stereoGPU* mpstereoGPU;
};

int main(int argc, char **argv)
{
  ros::init(argc, argv, "stereoGPU");		// change the name
  //ros::start();

  cout << endl << "Usage: rosrun stereo_gpu stereo_gpu_node [path_to_calibration_file]" << endl;
  cout << endl << "Start the stereo node." << endl;

  string calibration = (argc == 2) ? argv[1] : "/home/haibo/catkin_ws/src/ST_UAV_Phase2/stereo_gpu/stereoCalib.yaml";

  ros::NodeHandle nh;
  ros::NodeHandle n("~"); // meaning?

  stereoGPU STEREO(nh, calibration);
  ImageGrabber igb(&STEREO);
  // Input setting parameters from ROS
  // if(n.getParam("calibration", calibration))
  //     ROS_INFO("Get calibration parameters: %s", calibration.c_str());
  // else
  //     ROS_WARN("Use default calibration file position: %s", calibration.c_str());

  message_filters::Subscriber<sensor_msgs::Image> left_sub(nh, "/camera/left/image_raw", 1);
  message_filters::Subscriber<sensor_msgs::Image> right_sub(nh, "/camera/right/image_raw", 1);
  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image> SyncPolicy;
  message_filters::Synchronizer<SyncPolicy> sync(SyncPolicy(10), left_sub, right_sub);
  //sync.setMaxIntervalDuration(ros::Duration(time_diff));
  sync.registerCallback(boost::bind(&ImageGrabber::GrabStereo,&igb,_1,_2));	// first arg: callback function; // second arg: this pointer

  ros::spin();
  ros::shutdown();
  return 0;
}

// callback function
void ImageGrabber::GrabStereo(const sensor_msgs::ImageConstPtr& msgLeft,const sensor_msgs::ImageConstPtr& msgRight){
    mpstereoGPU->process(msgLeft, msgRight);

}
