// ueye_cameras.cpp

#include "ueye2/ueye_camera.h"

using namespace std;

// _____________________________________________________________________________
//
//                          CUeye_Camera
// _____________________________________________________________________________

//  CONSTRUCTOR
// -----------------------------------------------------------------------------
CUeye_Camera::CUeye_Camera()
{
  // Default configuration
  this->params_.cameraid = 0;
  this->params_.img_width = 752;
  this->params_.img_height = 480;
  this->params_.img_left = -1;
  this->params_.img_top = -1;
  this->params_.img_bpp = 8;
  this->params_.fps = 20;
  this->params_.param_mode = 0;
  this->params_.file_str = "";
  this->params_.pixel_clock = 20;
  this->params_.exposure = 10;
  this->params_.mirror_updown = false;
  this->params_.mirror_leftright = false;
}

//  DESTRUCTOR
// -----------------------------------------------------------------------------
CUeye_Camera::~CUeye_Camera()
{
}

//  CAMERA INITIALIZATION
// -----------------------------------------------------------------------------
void CUeye_Camera::init_camera()
{
  // Camera handle
  this->hCam_ = this->params_.cameraid;

  // Initialize camera
  if(is_InitCamera (&this->hCam_, NULL)!= IS_SUCCESS)
    is_cam_exception("Could not initialize camera");

  // Get sensor information
  get_sensor_info();

  // Set initial parameters
  set_params();

  // Allocate memory
  alloc_memory();

  // Get the line increment (in bytes)
  is_GetImageMemPitch(this->hCam_, &this->img_step_);

  this->img_data_size_= this->params_.img_height*this->img_step_;
  this->camera_open_=true;

  print_params();

  if ((is_SetExternalTrigger(this->hCam_, IS_SET_TRIGGER_HI_LO)) != IS_SUCCESS) {
      cout << "Could not enable falling-edge external trigger mode"<<endl;
  }
}

//  LIST AVAILABLE UEYE CAMERA
// -----------------------------------------------------------------------------
void CUeye_Camera::list_cameras()
{
  // Get number of cameras
  int nNumCam;
  UEYE_CAMERA_LIST* pucl;
  if( is_GetNumberOfCameras( &nNumCam ) == IS_SUCCESS) {
    if(nNumCam>0){
      if( nNumCam >= 1 ) {
        // Create new list with suitable size
        pucl = (UEYE_CAMERA_LIST*) new BYTE [sizeof (DWORD) + nNumCam * sizeof (UEYE_CAMERA_INFO)];
        pucl->dwCount = nNumCam;

        //Retrieve camera info
        if (is_GetCameraList(pucl) == IS_SUCCESS) {
          int iCamera;
          cout << "__UEYE connected cameras ___"<<endl;
          for (iCamera = 0; iCamera < (int)pucl->dwCount; iCamera++) {
            cout << "  Camera " << iCamera << " Id: " << pucl->uci[iCamera].dwCameraID << endl;
          }
          cout << "____________________________"<<endl;
        }
        else is_cam_exception("Could not list cameras");
      }
      delete [] pucl;
    }
    else is_cam_exception("No cameras connected");
  }
}
void CUeye_Camera::Enable_Event() {
    is_EnableEvent(this->hCam_, IS_SET_EVENT_FRAME);
}


bool CUeye_Camera::Wait_next_image() {
    int ret = is_WaitEvent(this->hCam_, IS_SET_EVENT_FRAME, 50);
    return (ret==IS_SUCCESS); 
}

//  GET IMAGE
// -----------------------------------------------------------------------------
bool CUeye_Camera::get_image()
{
  // Check if camera is open
  if(!this->camera_open_)
    is_cam_exception("Camera not initialized");
  // Acquire a single image
  //is_SetExternalTrigger(this->hCam_, IS_SET_TRIGGER_HI_LO);
  //cout << "break point"<<endl;
  if (is_FreezeVideo(this->hCam_,IS_DONT_WAIT) == IS_SUCCESS)
  {
    this->image_data_.resize(this->img_data_size_);
    copy((char*) this->imgMem_, ((char*)this->imgMem_) + this->img_data_size_, this->image_data_.begin());
    return true;
  }
  else {
    // TODO: Show the lost frames in a "light" way
    // is_feature_exception("Error freezing video","Get image");
    return false;
  }
}

//  ALLOCATE MEMORY
// -----------------------------------------------------------------------------
void CUeye_Camera::alloc_memory()
{
  // Allocate memory for the next image
  if(is_AllocImageMem(this->hCam_, this->params_.img_width, this->params_.img_height, this->params_.img_bpp, &this->imgMem_, &this->memId_)!=IS_SUCCESS)
    is_cam_exception("Could not allocate image memory");

  // Activate allocated memory
  if(is_SetImageMem (this->hCam_, this->imgMem_, this->memId_)!=IS_SUCCESS)
    is_cam_exception("Could not activate allocated image memory");
}

//  GET SENSOR INFORMATION
// -----------------------------------------------------------------------------
void CUeye_Camera::get_sensor_info()
{
  // Get sensor information
  if(is_GetSensorInfo (this->hCam_, &this->sensor_info_)!=IS_SUCCESS)
    is_cam_exception("Could not get sensor information");

  // Set default image left offset
  if(this->params_.img_width <= int(this->sensor_info_.nMaxWidth) && this->params_.img_left == -1){
    cerr<< "[Camera "<<this->params_.cameraid<<"]: Left not specified. Assuming centering the sensor"<<endl;
    this->params_.img_left = (this->sensor_info_.nMaxWidth - this->params_.img_width)/2;
  }
  // Set default image right offset
  if(this->params_.img_height <= int(this->sensor_info_.nMaxHeight) && this->params_.img_top == -1){
    cerr<< "[Camera "<<this->params_.cameraid<<"]: Top not specified. Assuming centering the sensor"<<endl;
    this->params_.img_top = (this->sensor_info_.nMaxHeight - this->params_.img_height)/2;
  }

  // Get color mode
  get_color_mode();
}

//  GET COLOR MODE
// -----------------------------------------------------------------------------
void CUeye_Camera::get_color_mode()
{
  // Get color mode
  int nRet;
  nRet=is_SetColorMode (this->hCam_, IS_GET_COLOR_MODE);

  // Set bits per pixel (bpp) depending on color mode
  if(nRet==IS_CM_MONO8 || nRet==IS_CM_SENSOR_RAW8)
    this->params_.img_bpp=8;
  else if(nRet==IS_CM_RGB8_PACKED || nRet==IS_CM_RGBA8_PACKED || nRet==IS_CM_BGR8_PACKED || nRet==IS_CM_BGRA8_PACKED || nRet==IS_CM_BGRY8_PACKED)
    this->params_.img_bpp=24;
  else is_feature_exception("Different than (gray 8) or (rgb 8 8 8)","Current Color mode");
}


// -----------------------------------------------------------------------------
void CUeye_Camera::set_params()
{
  // Set camera parameters
  double enable = 1;
  switch (this->params_.param_mode)
  {
    // Default parameters
    case 0:
            cout<< "[Camera "<<this->params_.cameraid<<"]: Loading camera DEFAULT parameters..."<<endl;

            if(strcmp(this->sensor_info_.strSensorName,"XS")==0){
               is_feature_exception("Could not be set","Auto shutter");
              if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_SENSOR_GAIN_SHUTTER, &enable, 0)!=IS_SUCCESS)
                is_feature_exception("Could not be set","Auto shutter");
            }
            else{
              if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_GAIN, &enable, 0)!=IS_SUCCESS)
                is_feature_exception("Could not be set","Auto shutter");
              //if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_SHUTTER, &enable, 0)!=IS_SUCCESS)
                //is_feature_exception("Could not be set","Auto shutter");
            }

            //get_params();

            set_exposure(this->params_.exposure);
            //set_HW_gain_factor(100);
            break;
    // User defined parameters
    case 1:
            cout<< "[Camera "<<this->params_.cameraid<<"]: Loading camera USER DEFINED parameters..."<<endl;

            // Set color mode
            set_color_mode();
            // Set display mode
            set_display_mode();
            // Set pixel clock
            set_pixel_clock(this->params_.pixel_clock);
            // Set frame rate
            set_frame_rate(this->params_.fps);
            // Set exposure
            set_exposure(this->params_.exposure);
            // Set image format
            //set_img_format();
            // Set mirroring
            set_mirror();
            //set_HW_gain_factor(100);

            if(strcmp(this->sensor_info_.strSensorName,"XS")==0){
              //if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_SENSOR_GAIN_SHUTTER, &enable, 0)!=IS_SUCCESS)
                //is_feature_exception("Could not be set","Auto shutter");
            }
            else{
              //if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_GAIN, &enable, 0)!=IS_SUCCESS)
               // is_feature_exception("Could not be set","Auto gain");
            }

            break;
    // EEPROM parameters
    case 2:
            cout<< "[Camera "<<this->params_.cameraid<<"]: Loading camera EEPROM parameters..."<<endl;
            if(is_ParameterSet(this->hCam_, IS_PARAMETERSET_CMD_LOAD_EEPROM, NULL, unsigned(NULL))!=IS_SUCCESS)
              is_cam_exception("Could not load camera EEPROM parameters");

            // The ueye camera manager has some bugs working with XS camera and the shutter cannot be saved properly, thus we set it auto
            if(strcmp(this->sensor_info_.strSensorName,"XS")==0){
              if(is_SetAutoParameter (this->hCam_, IS_SET_ENABLE_AUTO_SENSOR_GAIN_SHUTTER, &enable, 0)!=IS_SUCCESS)
                is_feature_exception("Could not be set","Auto shutter");
            }
            // Get parameters to the class struct
            get_params();
            break;
    // File parameters
    case 3:
            cout<< "[Camera "<<this->params_.cameraid<<"]:Loading camera parameters from FILE..."<<endl;
            // String convertion to wide string pointer
            wstring wide_str;
            for(unsigned int ii = 0; ii < this->params_.file_str.length(); ++ii)
              wide_str += wchar_t(this->params_.file_str[ii]);
            const wchar_t* str_result = wide_str.c_str();
            // Load from file
            cout << this->params_.file_str.c_str() << endl;
            if(is_ParameterSet(this->hCam_, IS_PARAMETERSET_CMD_LOAD_FILE, (void*)str_result, unsigned(NULL))!=IS_SUCCESS)
              is_cam_exception("Could not load camera parameters from FILE");
            // Get parameters to the class struct
            get_params();
            break;
  }
  get_color_mode();
}

//  SET DISPLAY MODE
// -----------------------------------------------------------------------------
void CUeye_Camera::set_display_mode()
{
  // Set display mode to DIB
  int nRet = is_SetDisplayMode (this->hCam_, IS_GET_DISPLAY_MODE);
  if(nRet!=IS_SET_DM_DIB) is_SetDisplayMode (this->hCam_, IS_SET_DM_DIB);
}

//  SET COLOR MODE
// -----------------------------------------------------------------------------
void CUeye_Camera::set_color_mode()
{
  // Get color mode
  get_color_mode();

  // Set color mode depending on bits per pixel
  int nRet;
  if (this->params_.img_bpp==24)
    nRet = IS_CM_BGR8_PACKED;
  else if (this->params_.img_bpp==8)
    nRet = IS_CM_MONO8;
  else if (this->params_.img_bpp!=24 && this->params_.img_bpp!=8)
    is_cam_exception("Wrong img_bpp configuration");

  if(is_SetColorMode (this->hCam_, nRet)!=IS_SUCCESS)
    is_cam_exception("Could not set color mode");
}



//  SET IMAGE FORMAT
// -----------------------------------------------------------------------------
void CUeye_Camera::set_img_format()
{
  // Get number of available formats and size of list
  uint count;
  uint bytesNeeded = sizeof(IMAGE_FORMAT_LIST);
  if(is_ImageFormat(this->hCam_, IMGFRMT_CMD_GET_NUM_ENTRIES, &count, 4)!=IS_SUCCESS)
    is_feature_exception("Could not get number of available formats","Set image format");
  bytesNeeded += (count - 1) * sizeof(IMAGE_FORMAT_INFO);
  void* ptr = malloc(bytesNeeded);

  // Create and fill list
  IMAGE_FORMAT_LIST* pformatList = (IMAGE_FORMAT_LIST*) ptr;
  pformatList->nSizeOfListEntry = sizeof(IMAGE_FORMAT_INFO);
  pformatList->nNumListElements = count;
  if(is_ImageFormat(this->hCam_, IMGFRMT_CMD_GET_LIST, pformatList, bytesNeeded)!=IS_SUCCESS)
    is_feature_exception("Could not get format list","Set image format");

  // Check the best image format
  IMAGE_FORMAT_INFO formatInfo;
  int width,height,error,best;
  int error_best=5000;
  for (unsigned int ii = 0; ii < count; ii++)
  {
    formatInfo = pformatList->FormatInfo[ii];
    width = formatInfo.nWidth;
    height = formatInfo.nHeight;

    error = (abs(this->params_.img_width - width) + abs(this->params_.img_height - height));
    if(error_best>error){
      error_best = error;
      best = ii;
    }
  }

  // Set image format
  IMAGE_FORMAT_INFO formatInfo_std = pformatList->FormatInfo[best];
  this->params_.img_width = formatInfo_std.nWidth;
  this->params_.img_height = formatInfo_std.nHeight;
  cout << "[Camera "<<this->params_.cameraid<<"]: Desired IMAGE SIZE is not allowed by the model. " << "Setting best suitable size." << endl;

  // Allocate image mem for current format, set format
  alloc_memory();
  if(is_ImageFormat(this->hCam_, IMGFRMT_CMD_SET_FORMAT, &formatInfo_std.nFormatID, 4)!=IS_SUCCESS)
    is_feature_exception("Could not be set","Image format");
}

//  GET CURRENT PARAMETERS
// -----------------------------------------------------------------------------
void CUeye_Camera::get_params()
{
  get_AOI();

  // Get default pixel clock
  uint nPixelClockDefault;
  if(is_PixelClock(this->hCam_, IS_PIXELCLOCK_CMD_GET,(void*)&nPixelClockDefault, sizeof(nPixelClockDefault))!=IS_SUCCESS)
    is_feature_exception("Could not be obtained","Pixel clock");
  this->params_.pixel_clock=nPixelClockDefault;

  // Set frame rate due to internal parameter change
  set_frame_rate(0);
  // Set expoduse due to internal paramter change
  set_exposure(0);
}

//  SET DEFAULT PIXEL CLOCK
// -----------------------------------------------------------------------------
void CUeye_Camera::set_default_pixel_clock()
{
  // Get default pixel clock
  uint nPixelClockDefault;
  if(is_PixelClock(this->hCam_, IS_PIXELCLOCK_CMD_GET_DEFAULT,(void*)&nPixelClockDefault, sizeof(nPixelClockDefault))!=IS_SUCCESS)
    is_feature_exception("Could not be obtained","Pixel Clock");

  // Set default pixel clock
  set_pixel_clock(nPixelClockDefault);
}

//  SET PIXEL CLOCK
// -----------------------------------------------------------------------------
void CUeye_Camera::set_pixel_clock(const uint& pixel_clock)
{
  //Check pixel clock suggested is inside the camera possible values
  UINT nRange[3];
  UINT nMin, nMax;
  ZeroMemory(nRange, sizeof(nRange));

  // Get pixel clock range
  INT nRet = is_PixelClock(this->hCam_, IS_PIXELCLOCK_CMD_GET_RANGE, (void*)nRange, sizeof(nRange));
  if (nRet == IS_SUCCESS)
  {
    nMin = nRange[0];
    nMax = nRange[1];
    //UINT nInc = nRange[2]; //Not needed
  }
  else is_feature_exception("Could not be obtained","Pixel Clock range");

  //Ueye XS pixel clock possibilities: 9,18,30,35
  UINT new_pixel_clock;
  if(pixel_clock<nMax && pixel_clock>nMin){
    if(strcmp(this->sensor_info_.strSensorName,"XS")==0){
      if(pixel_clock!=9 || pixel_clock!=18 || pixel_clock!=30 || pixel_clock!=35){
        if (9 < pixel_clock && pixel_clock < 18) new_pixel_clock = 18;
        else if(18 < pixel_clock && pixel_clock < 30) new_pixel_clock = 30;
        else if(30 < pixel_clock && pixel_clock < 35) new_pixel_clock = 35;
        cout << "[Camera " << this->params_.cameraid <<"]: " << "Desired PIXEL CLOCK not allowed. Pixel clock set to the highest close allowed value: " << new_pixel_clock << ". " << endl;
      }
      else new_pixel_clock = pixel_clock;
    }
    else new_pixel_clock = pixel_clock;
  }
  else if (pixel_clock<nMin) {
    new_pixel_clock = nMin;
    cout << "[Camera " << this->params_.cameraid <<"]: " << "Desired PIXEL CLOCK out of range. Pixel clock set to the lowest allowed value: " << nMin << ". " << endl;
  }
  else if (pixel_clock>nMax) {
    new_pixel_clock = nMax;
    cout << "[Camera " << this->params_.cameraid <<"]: " << "Desired PIXEL CLOCK out of range. Pixel clock set to the highest allowed value: " << nMax << ". " << endl;
  }

  this->params_.pixel_clock=new_pixel_clock;

  // Set Pixel Clock
  if (is_PixelClock(this->hCam_, IS_PIXELCLOCK_CMD_SET,(void*)&new_pixel_clock, sizeof(new_pixel_clock))!=IS_SUCCESS)
    is_feature_exception("Could not be set","Pixel Clock");

  // Set frame rate
  set_frame_rate(0);
  // Set exposure
  set_exposure(0);
}

//  SET FRAME RATE
// -----------------------------------------------------------------------------
void CUeye_Camera::set_frame_rate(const double& fps)
{

  // Query frame rate range
  double min_fps,max_fps,interval;
  if (is_GetFrameTimeRange(this->hCam_,&min_fps,&max_fps,&interval)!=IS_SUCCESS)
    is_feature_exception("Could not be obtained","Frame time range");

  // If the change is a consequence of an other request (fps=0)
  // set maximum frame rate. Otherwise, check if it is allowed and set it.
  double desired_fps;
  if (fps==0) {
    if((1/max_fps)<this->params_.fps && this->params_.fps<(1/min_fps))
      desired_fps = this->params_.fps;
    else {
      desired_fps = 1/min_fps;
      cout << "[Camera "<<this->params_.cameraid<<"]: Desired FPS out of range. Fps set to the highest allowed value: " << desired_fps << "." << endl;
    }
  }
  else{
    if ((1/max_fps)<fps && fps<(1/min_fps)) desired_fps = fps;
    else {
      desired_fps = 1/min_fps;
      cout << "[Camera "<<this->params_.cameraid<<"]: Desired FPS out of range. Fps set to the highest allowed value: " << desired_fps << "." << endl;
    }
  }

  // Set frame rate
  double current_fps;
  if(is_SetFrameRate (this->hCam_,desired_fps,&current_fps)!=IS_SUCCESS)
    is_feature_exception("Could not be set","New frame rate");

  this->params_.fps = current_fps;
}

//  SET EXPOSURE
// -----------------------------------------------------------------------------
void CUeye_Camera::set_exposure(const double& exp)
{
  // Query exposure time range
  double min_exp = 0;
  double max_exp = 0;
  if(is_Exposure (this->hCam_,IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE_MIN,(void*) &min_exp,8)!=IS_SUCCESS)
    is_feature_exception("Could not be obtained","Min exposure time range");
  if(is_Exposure (this->hCam_,IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE_MAX,(void*) &max_exp,8)!=IS_SUCCESS)
    is_feature_exception("Could not be obtained","Max exposure time range");

  // If the change is a consequence of an other request (exp=0)
  // set minimum exposure time. Otherwise, check if it is allowed and set it.
  double desired_exp;
  if (exp==0) {
    if(min_exp<this->params_.exposure && this->params_.exposure<max_exp)
      desired_exp = this->params_.exposure;
    else {
      desired_exp = min_exp;
      cout << "[Camera "<<this->params_.cameraid<<"]: Desired EXPOSURE out of range. Exposure set to the lowest allowed value: " << desired_exp << "." << endl;
    }
  }
  else{
    if (min_exp<exp && exp<max_exp) desired_exp = exp;
    else
      desired_exp = min_exp;
      cout << "[Camera "<<this->params_.cameraid<<"]: Desired EXPOSURE out of range. Exposure set to the lowest allowed value: " << desired_exp << "." << endl;
  }

  // Set the exposure time
  if(is_Exposure (this->hCam_,IS_EXPOSURE_CMD_SET_EXPOSURE,(void*) &desired_exp,8)!=IS_SUCCESS)
    is_feature_exception("Could not be set","New exposure time");

  this->params_.exposure = desired_exp;
}

//  SET CAMERA PARAMETERS
// void CUeye_Camera::setExposure(double *time_ms)
void CUeye_Camera::get_Exposure()
{
   double time_ms;
   int error = is_Exposure(this->hCam_, IS_EXPOSURE_CMD_GET_EXPOSURE, &time_ms, sizeof(double));

   if (error != IS_SUCCESS) {
        is_feature_exception("Could not be set","New exposure time");
   }
   input_exposure = time_ms;
}

//Set and Get Gain Added Part
int CUeye_Camera::set_hardware_gain(int gain)
{

   return is_SetHardwareGain(this->hCam_, gain, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);
}
int CUeye_Camera::get_hardware_gain()
{
   return is_SetHardwareGain (this->hCam_, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);

}

/*
//Set and Get Gain Factor Added Part
int CUeye_Camera::set_HW_gain_factor(int gain_factor)
{
    return is_SetHWGainFactor(this->hCam_, IS_SET_MASTER_GAIN_FACTOR, gain_factor);
}

int CUeye_Camera::query_gain_factor()
{
    return is_SetHWGainFactor(this->hCam_, IS_INQUIRE_MASTER_GAIN_FACTOR, 100);
}
*/
//inline int		setGainMaster(int gain){return is_SetHardwareGain(m_hCam, gain, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);};
//inline int		getGainMaster(){return is_SetHardwareGain (m_hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);};





//  SET AREA OF INTEREST (AOI)
// -----------------------------------------------------------------------------
void CUeye_Camera::set_AOI(const int& x,const int & y,const int& width,const int& height)
{
  // Select region of intereset
  IS_RECT rectAOI;
  rectAOI.s32X = x;
  rectAOI.s32Y = y;
  rectAOI.s32Width = width;
  rectAOI.s32Height = height;
  if(is_AOI(this->hCam_, IS_AOI_IMAGE_SET_AOI, (void*)&rectAOI, sizeof(rectAOI))!=IS_SUCCESS)
    is_cam_exception("Could not set AOI");

  // Set frame rate due to internal parameter change
  set_frame_rate(0);

  // update image parameters
  this->params_.img_width=width;
  this->params_.img_height=height;

  // allocate memory
  alloc_memory();
}

// GET AREA OF INTEREST (AOI)
// -----------------------------------------------------------------------------
void CUeye_Camera::get_AOI()
{
  // Get width and height (function not available for Ueye XS)
  IS_RECT rectAOI;
  if(is_AOI(this->hCam_, IS_AOI_IMAGE_GET_AOI, (void*)&rectAOI, sizeof(rectAOI))!=IS_SUCCESS)
    is_cam_exception("Could not get AOI");

  this->params_.img_width = rectAOI.s32Width;
  this->params_.img_height = rectAOI.s32Height;
}

//  SET IMAGE MIRRORING
// -----------------------------------------------------------------------------
void CUeye_Camera::set_mirror()
{
  // Updown mirroring
  if(this->params_.mirror_updown)
    if(is_SetRopEffect (this->hCam_, IS_SET_ROP_MIRROR_UPDOWN, 1, 0)!=IS_SUCCESS)
      is_feature_exception("Could not be set.","Mirror updown");
  // Leftright mirroring
  if(this->params_.mirror_leftright)
    if(is_SetRopEffect (this->hCam_, IS_SET_ROP_MIRROR_LEFTRIGHT, 1, 0)!=IS_SUCCESS)
      is_feature_exception("Could not be set.","Mirror leftright");
}

//  CAMERA EXCEPTION
// -----------------------------------------------------------------------------
void CUeye_Camera::is_cam_exception(const string& text)
{
  // Camera exception message
  is_GetError(this->hCam_,&this->error_,&this->error_msg_);
  stringstream error;
  error << "[Camera " << this->params_.cameraid <<"]: " << text << ". Driver error: " << this->error_ << " " << this->error_msg_;
  throw CUeyeCameraException(_HERE_, error.str());
}

//  FEATURE EXCEPTION
// -----------------------------------------------------------------------------
void CUeye_Camera::is_feature_exception(const string& text,const string& feature)
{
  // Feature exception message
  is_GetError(this->hCam_,&this->error_,&this->error_msg_);
  stringstream error,error_feat;
  error << text << ". Driver error: " << this->error_ << " " << this->error_msg_;
  error_feat << "[Camera " << this->params_.cameraid <<"]: " << " " << feature << ". ";
  throw CUeyeFeatureException(_HERE_, error.str(),error_feat.str());
}

//  PRINT PARAMETERS ON SCREEN
// -----------------------------------------------------------------------------
void CUeye_Camera::print_params()
{
  // Print parameters
  cout<<"[Camera "<<this->params_.cameraid<<"]:"<<endl
      <<"           model: " <<this->sensor_info_.strSensorName<<endl
      <<"       max width: "<<this->sensor_info_.nMaxWidth<<endl
      <<"      max height: "<<this->sensor_info_.nMaxHeight<<endl
      <<"   current width: "<<this->params_.img_width<<endl
      <<"  current height: "<<this->params_.img_height<<endl
      <<"      frame rate: "<<this->params_.fps<<endl
      <<"       image bpp: "<<this->params_.img_bpp<<endl
      <<" parameters mode: "<<this->params_.param_mode<<endl
      <<"     pixel clock: "<<this->params_.pixel_clock<<endl
      <<"        exposure: "<<this->params_.exposure<<endl
      <<"   mirror updown: "<<this->params_.mirror_updown<<endl
      <<"mirror leftright: "<<this->params_.mirror_leftright<<endl;
}

//  CLOSE CAMERA
// -----------------------------------------------------------------------------
void CUeye_Camera::close_camera()
{
  // Close the camera
  if(!this->camera_open_)
    is_cam_exception("Camera not initialized");
  if(is_ExitCamera(this->hCam_)!=IS_SUCCESS)
    is_cam_exception("Could not exit camera");
  return;
}



