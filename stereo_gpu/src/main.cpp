/*
	GPU SGM stereo matching

	Original from github libSGM
	Modified by Haibo
	Data: 20-May-2016
*/


// ROS headers
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <geometry_msgs/Quaternion.h>
#include <tf/transform_broadcaster.h>

//OpenCV headers
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/contrib/contrib.hpp>

// system headers
#include <math.h>
#include <iterator>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
//#include <typeinfo>

#include "libsgm.h"

#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

#define WIDTH 752
#define HEIGHT 480


double r_data[9] = {0.999851516004708, -0.00498457045275044, 0.0164954539340047, 0.00497160703876063, 0.999987299688415, 0.000826792189086883, -0.0164993656405163, -0.000744660508793523, 0.999863598904464};
double t_data[3] = {-299.268590358311,	-0.641667329520781,	1.09821941809761};
double M_1[9] = { 421.199009407044, 0.412760990698931, 366.699973333038, 0, 421.655468122576,  239.600056673828, 0, 0, 1 };
double M_2[9] = {423.526696160263, -0.123652334236988, 375.431005950529, 0, 423.751038507976, 237.088401774212, 0, 0, 1  };
double D_1[4] = {-0.307057933010874,	0.120679939434720, -0.000340441435229576,	-0.000347173827361101};
double D_2[4] = {-0.306923298818246,	0.121786424922333, -0.000810100722834597,	0.000975575619740761};


class stereo_disparity
{
  ros::NodeHandle nh;
  image_transport::ImageTransport it;
  image_transport::Subscriber img_combine;
  image_transport::Publisher img_disparity;
  image_transport::Publisher img_depth;

  cv::Size img_size;
  cv::Rect roi_left;
  cv::Rect roi_right;
  cv::Rect roi_half;

  cv::Mat img_left, img_right;
  cv::Mat img_left_rect, img_right_rect;
  cv::Mat img_left_half, img_right_half;

  cv::Mat M1, D1, M2, D2;
  cv::Mat R, T;

  cv::Mat map11, map12, map21, map22;
  cv::Mat depth_32F;
  cv::Mat bgr[3];

  cv::Rect roi1, roi2;
  cv::Mat Q;
  cv::Mat R1, P1, R2, P2;
  cv::Mat output, output8;

  int disp_size;
  float depth_center;

  public:
  stereo_disparity() : 	it(nh) {
	output = cv::Mat::zeros(HEIGHT/2,WIDTH, CV_8UC1);
	roi_left = cv::Rect(0, 0, WIDTH, HEIGHT);
	roi_right = cv::Rect(WIDTH, 0, WIDTH, HEIGHT);
	roi_half = cv::Rect(0, HEIGHT/4, WIDTH, HEIGHT/2);
	img_size = cv::Size(WIDTH, HEIGHT);

    	M1= cv::Mat(3, 3, CV_64FC1, &M_1);
    	M2 = cv::Mat(3, 3, CV_64FC1, &M_2);
    	D1 = cv::Mat(1, 4, CV_64FC1, &D_1);
    	D2 = cv::Mat(1, 4, CV_64FC1, &D_2);
    	R = cv::Mat(3, 3, CV_64FC1, &r_data);
    	T = cv::Mat(3, 1, CV_64FC1, &t_data);

	disp_size = 64;
	depth_center =0;

	cv::stereoRectify( M1, D1, M2, D2, img_size, R, T, R1, R2, P1, P2, Q, cv::CALIB_ZERO_DISPARITY, 0, img_size, &roi1, &roi2 );
	cv::initUndistortRectifyMap(M1, D1, R1, P1, img_size, CV_16SC2, map11, map12);
	cv::initUndistortRectifyMap(M2, D2, R2, P2, img_size, CV_16SC2, map21, map22);
//cout << "Q = " << Q << endl;
  	img_combine = it.subscribe("/camera/image_combine", 1, &stereo_disparity::imageCallback, this);
	img_disparity = it.advertise("/stereo/disparity", 1);
	img_depth = it.advertise("/stereo/depth", 1);
  }

  ~stereo_disparity(){
  }

  void imageCallback(const sensor_msgs::ImageConstPtr& msg){

	int64 t = cv::getTickCount();
	cv_bridge::CvImagePtr cv_ptr;	// opencv Mat pointer;

    	try {
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::TYPE_8UC1);
    	}

    	catch (cv_bridge::Exception& e) {
      	  	ROS_ERROR("cv_bridge exception: %s", e.what());
          	return;
    	}

	// cv_ptr->image   ----> Mat image in opencv

	img_left = cv_ptr->image(roi_left);
	img_right = cv_ptr->image(roi_right);
	cv::remap(img_left, img_left_rect, map11, map12, cv::INTER_LINEAR);
	cv::remap(img_right, img_right_rect, map21, map22, cv::INTER_LINEAR);
	img_left_half = img_left_rect(roi_half);
	img_right_half = img_right_rect(roi_half);

//	cudaEvent_t start, stop;
//	float time;
//	cudaEventCreate(&start);
//	cudaEventCreate(&stop);
//	cudaEventRecord( start, 0 );

	sgm::StereoSGM ssgm(img_left_half.cols, img_left_half.rows, disp_size, sgm::EXECUTE_INOUT_HOST2HOST);
	ssgm.execute(img_left_half.data, img_right_half.data, (void**)&output.data);

//	cudaEventRecord( stop, 0 );
//	cudaEventSynchronize( stop );
//	cudaEventElapsedTime( &time, start, stop );
//	cudaEventDestroy( start );
//	cudaEventDestroy( stop );

	output.convertTo(output8, CV_8U, 255/disp_size);

	cv::reprojectImageTo3D(output, depth_32F, Q);
	cv::split(depth_32F, bgr);
	bgr[2] = bgr[2] / 1000;
	depth_center = bgr[2].at<float>(HEIGHT/4, WIDTH/2);

	ROS_INFO_STREAM("depth of the center point is " << depth_center << " m." );

	sensor_msgs::ImagePtr disparity = cv_bridge::CvImage(std_msgs::Header(), "mono8", output8).toImageMsg();
        img_disparity.publish(disparity);
	sensor_msgs::ImagePtr depth = cv_bridge::CvImage(std_msgs::Header(), "32FC1",bgr[2]).toImageMsg();
        img_depth.publish(depth);
	t = cv::getTickCount() - t;
	ROS_INFO_STREAM("Stereo Matching time: " << t*1000/cv::getTickFrequency() << " miliseconds.");

  }	// for imageCallback
};	// for the class

int main(int argc, char** argv){

	ros::init(argc, argv, "stereo_gpu");	// The third argument is the cpp filename
	stereo_disparity stereo;
	ros::spin();
	return 0;
}
