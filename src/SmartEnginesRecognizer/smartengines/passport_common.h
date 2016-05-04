/*
Copyright (c) 2012-2015, Smart Engines Ltd
All rights reserved.
*/

/**
 * @file
 * @brief Common classes used in PassportEngine
 */

#ifndef PASSPORT_ENGINE_PASSPORT_COMMON_H_INCLUDED
#define PASSPORT_ENGINE_PASSPORT_COMMON_H_INCLUDED

#if defined _MSC_VER && PASSPORT_ENGINE_EXPORTS
#  define DllExport   __declspec( dllexport )
#else
#  define DllExport
#endif 

#include <string>

/**
 * @brief  Exception class to raise from engine classes and functions
 */
class DllExport PassportException {
public:
  /**
   * @brief  Constructor
   * @param  what Description message
   */
  PassportException(const std::string &what);
  /**
   * @brief  Returns string identifying exception
   */
  const std::string & what() const;

private:
  std::string what_;  ///< Description message
};

/**
 * @brief  Class for representing a point
 */
class DllExport PassportPoint {
public:
  double x;   ///< Top-left x coordinate
  double y;   ///< Top-left y coordinate

  /**
   * @brief  Constructor
   */
  PassportPoint();
  /**
   * @brief  Constructor
   * @param  x Top-left x coordinate
   * @param  y Top-left x coordinate
   */
  PassportPoint(double x, double y);
};

/**
 * @brief  Class for representing a quadrangle
 */
class DllExport PassportQuadrangle {
public:

  /**
   * @brief  Returns the quadrangle vertex at the given @p index as a modifiable reference
   * @param  index Index position for quadrangle vertex, from 0 till 3
   */
  PassportPoint & operator[](int index);
  /**
   * @brief  Returns the quadrangle vertex at the given @p index as a constant reference
   * @param  index Index position for quadrangle vertex, from 0 till 3
   */
  const PassportPoint & operator[](int index) const;
  /**
   * @brief  Returns the quadrangle vertex at the given @p index as a constant reference
   * @param  index Index position for quadrangle vertex, from 0 till 3
   */
  const PassportPoint & GetPoint(int index) const;
  /**
   * @brief  Sets the quadrangle vertex at the given @p index to specified @p value
   * @param  index Index position for quadrangle vertex, from 0 till 3
   * @param  value New value for quadrangle vertex
   */
  void SetPoint(int index, const PassportPoint &value);

  /**
   * @brief  Constructor
   */
  PassportQuadrangle();
  /**
   * @brief  Constructor
   * @param  a Top-left vertex of the quadrangle
   * @param  b Top-right vertex of the quadrangle
   * @param  c Bottom-right vertex of the quadrangle
   * @param  d Bottom-left vertex of the quadrangle
   */
  PassportQuadrangle(PassportPoint a, PassportPoint b,
                     PassportPoint c, PassportPoint d);
  
private:
  PassportPoint points[4];  ///< Vector of quadrangle vertices
};

/**
 * @brief  Class for representing a size
 */
class DllExport PassportSize
{
public:
  int width;      ///< The width value
  int height;     ///< The hesight value

  /**
   * @brief  Constructor
   */
  PassportSize();

  /**
   * @brief  Constructor
   * @param  width  Width value
   * @param  height Height value
   */
  PassportSize(int width, int height);
};

/**
 * @brief  Class provides an 8bit image representation that
 *         allows direct access to the pixel data
 */
class DllExport PassportImage {
public:
  unsigned char *data;  ///< Pointer to the first pixel of the first row
  int width;            ///< Width of the image in pixels
  int height;           ///< Height of the image in pixels
  int stride;           ///< Difference in bytes between addresses of adjacent rows
  int channels;         ///< Number of image channels

  /**
   * @brief  Returns required buffer size for coping image data
   */
  int GetRequiredBufferLength() const;
  /**
   * @brief  Copies the image data to specified buffer
   * @param  out_buffer     Destination buffer, must be preallocated
   * @param  buffer_length  Size of buffer @p out_buffer
   */
  void CopyToBuffer(char *out_buffer, int buffer_length) const;

  /**
   * @brief  Constructor
   */
  PassportImage();
};

namespace psp { class PassportRecognizer; };

#endif // PASSPORT_ENGINE_PASSPORT_COMMON_H_INCLUDED