/*--------------------------------------------------------------
 *  new ueye camera drive to publish left and right images separately
!*/

#include "ueye2/ueye_camera.h"
#include "ueye2/ueye_exceptions.h"
#include "ueye2/exceptions.h"
#include "ueye2/my_ueye_functions.h"

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/Image.h>

using namespace std;
using namespace cv;

typedef unsigned int uint32;

int main(int argc, char** argv)
{
  ros::init(argc, argv, "ueye_node");
  ros::NodeHandle nh;
  ros::Rate loop_rate(20);

  //cv::Mat M1l,M2l,M1r,M2r; // for using the rectification function

  image_transport::ImageTransport it(nh);
  image_transport::Publisher pub1 = it.advertise("camera/left/image_raw", 1);
  image_transport::Publisher pub2 = it.advertise("camera/right/image_raw",1);
  //image_transport::Publisher pub3 = it.advertise("camera/left/image_rect", 1);
  //image_transport::Publisher pub4 = it.advertise("camera/right/image_rect",1);
  //ros::Publisher pub_info_camera_1 = nh.advertise<sensor_msgs::CameraInfo>("camera/left/camera_info", 1);
  //ros::Publisher pub_info_camera_2 = nh.advertise<sensor_msgs::CameraInfo>("camera/right/camera_info", 1);

  sensor_msgs::ImagePtr msg1;
  sensor_msgs::ImagePtr msg2;
  //sensor_msgs::CameraInfo info_camera_1;
  //sensor_msgs::CameraInfo info_camera_2;

// Camera class from library
  CUeye_Camera ueye_L;
  CUeye_Camera ueye_R;

  // Define Initial parameters
  setCameraParams(ueye_L);
  setCameraParams(ueye_R);

  // Initialize camera
  if(!init_camera(ueye_L) || !init_camera(ueye_R))
    return false;

  //List cameras to choose which one is going to be tested
  if(!list_cameras(ueye_L))
    return false;

  ueye_L.Enable_Event();
  ueye_R.Enable_Event();

  cv::Mat* frame_L = 0;
  cv::Mat* frame_R = 0;
  Mat img1;
  Mat img2;

  while (nh.ok()) {

	// require right camera gain to follow the left camera gain
    ueye_R.get_hardware_gain();
    int gain = ueye_L.get_hardware_gain();
    ueye_R.set_hardware_gain(gain);

    int flag1 = ueye_L.Wait_next_image();
//    int flag2 = ueye_R.Wait_next_image();
//    frame_L = get_img(ueye_L);
//    frame_R = get_img(ueye_R);

  boost::thread thread_1 = boost::thread(get_img_thread, ueye_L, &frame_L);
  boost::thread thread_2 = boost::thread(get_img_thread, ueye_R, &frame_R);
  thread_2.join();
  thread_1.join();

  if(frame_L!=NULL && frame_R!=NULL && flag1 ){

    img1 = *frame_L;
    cvtColor(img1,img1,CV_RGB2GRAY);
    img2 = *frame_R;
    cvtColor(img2,img2,CV_RGB2GRAY);

    #ifndef NDEBUG
  	   imshow("Left",img1);
       imshow("Right",img2);
       char button=waitKey(1);

       if (button == 'a' || button == 'A'){
          ueye_L.get_Exposure();
          ueye_R.get_Exposure();
          std::cout<<"exposure: "<<ueye_L.input_exposure<<"\t\t"<<ueye_R.input_exposure<<std::endl;
          std::cout<<"gain:\t\t"<<ueye_L.get_hardware_gain()<<"\t\t"<<ueye_R.get_hardware_gain()<<std::endl;
       }

       if (button == 's' || button == 'S'){
		      image_save(img1, img2);
       }
    #endif // NDEBUG

    try{
		    msg1 = cv_bridge::CvImage(std_msgs::Header(), "mono8", img1).toImageMsg();
	  }

    catch (cv_bridge::Exception& e){
		    ROS_ERROR("cv_bridge exception: %s", e.what());
        return 0;
    }

    try{
		    msg2 = cv_bridge::CvImage(std_msgs::Header(), "mono8", img2).toImageMsg();
    }

	  catch (cv_bridge::Exception& e){
		    ROS_ERROR("cv_bridge exception: %s", e.what());
        return 0;
	  }

	  static uint32 seq = 0;
	  ros::Time rosTimeStamp = ros::Time::now();
    msg1->header.stamp = rosTimeStamp;
    msg2->header.stamp = rosTimeStamp;
    msg1->header.seq = seq;
    msg2->header.seq = seq++;
    msg1->header.frame_id = "/camera_left";
    msg2->header.frame_id = "/camera_right";

    pub1.publish(msg1);
    pub2.publish(msg2);

    //ROS_INFO_STREAM("Sequence of left image: " << msg1->header.seq);
    //ROS_INFO_STREAM("Sequence of right image: " << msg2->header.seq);
    //ROS_INFO_STREAM("Time Stamp of left image: " << msg1->header.stamp);
    //ROS_INFO_STREAM("Time Stamp of right image: " << msg2->header.stamp);

    //info_camera_1.header.stamp = rosTimeStamp;
    //info_camera_2.header.stamp = rosTimeStamp;
    //pub_info_camera_1.publish(info_camera_1);
    //pub_info_camera_2.publish(info_camera_2);

    frame_L->release();
    frame_R->release();

  }
  else{
      #ifndef NDEBUG
      ROS_INFO_STREAM("Frame lost!");
      #endif // NDEBUG
  }

  ros::spinOnce();
  loop_rate.sleep();
 }//end of ROS
}
