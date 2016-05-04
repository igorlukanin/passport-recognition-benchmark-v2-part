/*
Copyright (c) 2012-2015, Smart Engines Ltd
All rights reserved.
*/

/**
 * @file
 * @brief Recognition result classes
 */

#ifndef PASSPORT_ENGINE_PASSPORT_RESULT_H_INCLUDED
#define PASSPORT_ENGINE_PASSPORT_RESULT_H_INCLUDED

#include "passport_common.h"

#include <string>

/**
 * @brief The @c PassportField is the base class for representation of recognized field
 */
class DllExport PassportField 
{
public:
  bool is_accepted; ///< Specifies whether the system is confident in field recognition result

  /**
   * @brief  Returns string representation of @c PassportField
   */
  virtual std::string ToString() const = 0;

  /**
   * @brief  Constructor
   */
  PassportField();
  /**
   * @brief  Destructor
   */
  virtual ~PassportField();
};

/**
 * @brief Class represents implementation of @c PassportField for string fields
 */
class DllExport PassportStringField : public PassportField 
{
public:
  std::string value;  ///< String value

  /**
   * @brief  Returns string value
   */
  std::string ToString() const;
  /**
   * @brief  Constructor
   */
  PassportStringField();
  /**
   * @brief  Constructor
   * @param  value Input string value
   */
  PassportStringField(const std::string &value);
};

/**
 * @brief Class represents implementation of @c PassportField for date fields
 */
class DllExport PassportDateField : public PassportField
{
public:
  int day;    ///< Day of the month (1 to 31) of this date (0 if the date is invalid)
  int month;  ///< Number corresponding to the month of this date (0 if the date is invalid)
  int year;   ///< Year of this date (0 if the date is invalid)

  /**
   * @brief  Returns string representation of date field as DD.MM.YYYY
   */
  std::string ToString() const;
  /**
   * @brief  Constructor
   */
  PassportDateField();
  /**
   * @brief  Constructor
   * @param  day    Day of the month (1 to 31)
   * @param  month  Number corresponding to the month
   * @param  year   Year
   */
  PassportDateField(int day, int month, int year);
};

/**
 * @brief Class represents implementation of @c PassportField for gender field
 */
class DllExport PassportGenderField : public PassportField
{
public:
  /**
   * @brief Enum defines the valid gender value
   */
  enum Gender 
  { 
    Unknown,  ///< The gender is undefined (usually returned as an error condition, when recognition if failed)
    Male,     ///< Male gender
    Female    ///< Female gender
  };

  Gender value; ///< Gender value

  /**
   * @brief  Returns string representation of gender field
   */
  std::string ToString() const;
  /**
   * @brief  Constructor
   */
  PassportGenderField();
  /**
   * @brief  Constructor
   * @param  value Gender
   */
  PassportGenderField(Gender value);
};

/**
 * @brief Class represents implementation of @c PassportField for authority code field
 */
class DllExport PassportCodeField : public PassportField
{
public:
  int code0;    ///< The first part of code
  int code1;    ///< The second part of code

  /**
   * @brief  Returns string representation of code field
   */
  std::string ToString() const;
  /**
   * @brief  Constructor
   */
  PassportCodeField();
  /**
   * @brief  Constructor
   * @param  code0 The first part of code
   * @param  code1 The first part of code
   */
  PassportCodeField(int code0, int code1);
};

/**
 * @brief Class represents passport recognition result
 */
class DllExport PassportRecognitionResult 
{ 
public:
  PassportStringField series;           ///< Series
  PassportStringField number;           ///< Number

  PassportStringField surname;          ///< Surname
  PassportStringField name;             ///< Name
  PassportStringField patronymic;       ///< Patronymic
  PassportGenderField gender;           ///< Gender
  PassportDateField   birthdate;        ///< Birthdate
  PassportStringField birthplace;       ///< Birthplace

  PassportStringField authority;        ///< Authority
  PassportDateField   issue_date;       ///< Issue date
  PassportCodeField   authority_code;   ///< Authority code

  PassportStringField mrz_line1;        ///< The first line of MRZ (if exists)
  PassportStringField mrz_line2;        ///< The second line of MRZ (if exists)
};

/**
 * @brief Class represents passport match result
 */
class DllExport PassportMatchResult
{
public:
  double              score;               ///< Matching score
  std::string         type;                ///< Document type

  PassportQuadrangle  doc_quadrangle;      ///< Document quadrangle
  PassportQuadrangle  number_quadrangle;   ///< Number zone quadrangle
  PassportQuadrangle  data_quadrangle;     ///< Data zone quadrangle
  PassportQuadrangle  mrz_quadrangle;      ///< MRZ zone quadrangle
  PassportQuadrangle  photo_quadrangle;    ///< Photo quadrangle

  PassportMatchResult();
};

/**
 * @brief Class represents the request for passport images
 */
class DllExport PassportImageRequest
{
public:
  PassportSize        doc_image_size;     ///< Required document image size
  PassportSize        number_image_size;  ///< Required number zone image size
  PassportSize        data_image_size;    ///< Required data zone image size
  PassportSize        mrz_image_size;     ///< Required MRZ zone image size
  PassportSize        photo_image_size;   ///< Required person photo zone image size
};

/**
 * @brief Class represents passport image result
 */
class DllExport PassportImageResult
{
public:
  PassportImage       doc_image;          ///< Document image
  PassportImage       number_image;       ///< Number zone image
  PassportImage       data_image;         ///< Data zone image
  PassportImage       mrz_image;          ///< MRZ zone image
  PassportImage       photo_image;        ///< Person photo image
};

/**
 * @brief Callback interface to obtain recognition results. Must be implemented 
 *        to get the results as they appear during the stream processing
 */
class DllExport PassportResultReporterInterface 
{
public:
  /**
   * @brief  Callback tells that last snapshot is not going to be
   *         processed/recognized. Optional
   */
  virtual void SnapshotRejected();
  /**
   * @brief  Callback tells that last snapshot has valid document and
             contains document match result. Optional
   * @param  result   Document match result
   * @param  request  Request for document images
   */
  virtual void DocumentMatched(const PassportMatchResult &result, PassportImageRequest &request);

  /**
   * @brief  Callback tells that requested document images are cropped. Optional
   * @param  result Document images result
   */
  virtual void DocumentImageCropped(const PassportImageResult &result);

  /**
   * @brief  Callback tells that last snapshot was processed  
   *         successfully and returns current result. Required
   * @param  result       Current recognition result
   * @param  may_finish   Indicates whether you could stop feeding snapshots
   * @param  is_break     Indicates whether you must stop feeding snapshots
   * 
   * If @p may_finish flag is true it means that internal recognition engine
   * already has enough data to provide a result with needed quality
   * (but in some bad cases further feeding of images may still improve
   * the result).
   *
   * If @p is_break flag is true it means that internal recognition engine
   * finished recognition process (for example, timeout is reached) and is ready to return result.
   */
  virtual void SnapshotProcessed(const PassportRecognitionResult &result, bool may_finish, bool is_break) = 0;
  
  /**
   * @brief  Destructor
   */
  virtual ~PassportResultReporterInterface();
};


#endif // PASSPORT_ENGINE_PASSPORT_RESULT_H_INCLUDED