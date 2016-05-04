/*
Copyright (c) 2012-2015, Smart Engines Ltd
All rights reserved.
*/

/**
 * @file
 * @brief Main processing classes
 */

#ifndef PASSPORT_ENGINE_PASSPORT_ENGINE_H_INCLUDED
#define PASSPORT_ENGINE_PASSPORT_ENGINE_H_INCLUDED

#include "passport_result.h"
#include "passport_common.h"

#include <memory>

/**
 * @mainpage Overview
 * The Smart PassportReader Library allows to recognize Internal passport of Russia 
 * on images or video data obtained either from cameras or from scanners.
 * 
 * This file contains a brief description of classes and members of the Library. 
 * Sample usage is shown in the @c passportengine_sample.cpp.
 * 
 * Feel free to send any questions about the Library on support@smartengines.biz.
 */

/**
 * @brief Main processing class
 */
class DllExport PassportEngine {
public:
  /**
   * @brief The ImageOrientation enum
   *
   * The document quadrangle which is set with the PassportQuadrangle is set for the rotated picture.
   */
  enum ImageOrientation {
    Landscape, ///< image is in the proper orientation, nothing needs to be done
    Portrait, ///< image is in portrait, needs to be rotated 90° clockwise
    InvertedLandscape, ///< image is upside-down, needs to be rotated 180°
    InvertedPortrait ///< image is in portrait, needs to be rotated 90° counter-clockwise
  };
  
  /**
   * @brief  Constructor
   */
  PassportEngine();

  /**
   * @brief  Configures the recognition engines
   * @param  config_path Configuration file path
   *
   * The SDK contains different configuration files for different task. At the present time we provide following configuration files:
   * * *data/passport_anywhere.json* - Use this file to prepare engine to recognize passport on the image obtained under uncontrolled conditions
   * * *data/passport_webcam.json* - Use this file to prepare engine to recognize passport on video stream obtained from web cameras
   * * *data/passport_mobile.json* - Use this file to prepare engine to recognize passport on video stream obtained from mobile device
   * * *data/page3_scanner.json* - Use this file to prepare engine to recognize only page 3 (which contains name and photo) on the image obtained via special passport scanner
   * * *data/page3_webcam.json* - Use this file to prepare engine to recognize only page 3 on video stream obtained from web cameras
   * * *data/page_mobile.json* - Use this file to prepare engine to recognize only page 3 on video stream obtained from mobile device
   */
  void Configure(const std::string &config_path);
  
  /**
   * @brief Initializes recognition session to enable image feeding
   * @param result_reporter Instance to callback processing progress
   */
  void InitializeSession(PassportResultReporterInterface &result_reporter);
  
  /**
   * @brief Terminates recognition session, cleans up integrated OCR results
   */
  void TerminateSession();

  /**
   * @brief  Processes the uncompressed RGB image stored in memory line by line
   * @param  image                Image
   * @param  image_orientation    Current image orientation to perform proper rotation to landscape
   * @param  document_quadrangle  Passport quadrangle on the image (if known)
   * 
   * Each data line looks like RGBRGBRGBRGB...RGB or BGRABGRABGRA... and so on. <br>
   * Throws @c PassportException if processing error occurs
   */
  void ProcessSnapshot(const PassportImage &image,
                       ImageOrientation image_orientation = Landscape,
                       const PassportQuadrangle *document_quadrangle = 0);

  /**
   * @brief  Processes the uncompressed RGB image stored in memory line by line
   * @param  data                 Pointer to the data start
   * @param  width                Image width
   * @param  height               Image height
   * @param  stride               Difference between the pointers to the consequence image lines, in bytes
   * @param  channels             Number of channels (4 means that the last one is alpha channel)
   * @param  image_orientation    Current image orientation to perform proper rotation to landscape
   * @param  document_quadrangle  Passport quadrangle on the image (if known)
   * 
   * Each data line looks like RGBRGBRGBRGB...RGB or BGRABGRABGRA... and so on. <br>
   * Throws @c PassportException if processing error occurs
   */
  void ProcessSnapshot(unsigned char *data, int width, int height, int stride, 
                       int channels,
                       ImageOrientation image_orientation = Landscape,
                       const PassportQuadrangle *document_quadrangle = 0);

  /**
   * @brief  Processes the uncompressed YUV image stored in memory line by line
   * @param  yuv_data             Pointer to the data start
   * @param  yuv_data_length      Total length of image data
   * @param  width                Image width
   * @param  height               Image height
   * @param  image_orientation    Current image orientation to perform proper rotation to landscape
   * @param  document_quadrangle  Passport quadrangle on the image (if known)
   *
   * Throws @c PassportException if processing error occurs
   */
  void ProcessYUVSnapshot(char *yuv_data, size_t yuv_data_length,
                          int width, int height,
                          ImageOrientation image_orientation = Landscape,
                          const PassportQuadrangle *document_quadrangle = 0);
  
  /**
   * @brief  Runs recognition process on the specified file
   * @param  image_file           Image file path
   * @param  image_orientation    Current image orientation to perform proper rotation to landscape
   * @param  document_quadrangle  Passport quadrangle on the image (if known)
   *
   * Throws @c PassportException if file does not exist or can't be processed, 
   * or some processing error occurred
   */
  void ProcessImageFile(const std::string &image_file,
                        ImageOrientation image_orientation = Landscape,
                        const PassportQuadrangle *document_quadrangle = 0);

private:
  /**
   * @brief  Disabled copy constructor
   */
  PassportEngine(const PassportEngine &);
  /**
   * @brief  Disabled assignment operator
   */
  PassportEngine &operator=(const PassportEngine &);

private:
  std::auto_ptr<psp::PassportRecognizer> passport_recognizer_; ///< Encapsulated engine implementation
  std::auto_ptr<struct ProxyResultReporter> proxy_reporter_;   ///< Core result reporter proxy
};

#endif // PASSPORT_ENGINE_PASSPORT_ENGINE_H_INCLUDED