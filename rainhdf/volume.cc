/*------------------------------------------------------------------------------
 * Rainfields ODIM HDF5 Library (rainHDF)
 *
 * Copyright (C) 2011 Commonwealth of Australia, Bureau of Meteorology
 * See COPYING for licensing and warranty details
 *----------------------------------------------------------------------------*/
#include "volume.h"

#include <cstring>
#include <cmath>

using namespace RainHDF;

namespace RainHDF
{
  /// Compression factor used to compress layers (0 - 9, 9 = max compression)
  static const int kDefCompression = 6;
}

Volume::Scan::Layer::Layer(
      hid_t hParent
    , bool bIsQuality
    , size_t nIndex
    , Quantity eQuantity
    , const hsize_t *pDims)
  : m_bIsQuality(bIsQuality)
  , m_eQuantity(eQuantity)
  , m_hLayer(hParent, m_bIsQuality ? kGrp_Quality : kGrp_Data, nIndex, kOpen)
  , m_hWhat(m_hLayer, kGrp_What, kOpen)
  , m_hHow(m_hLayer, kGrp_How, kOpen, true)
  , m_fGain(GetAtt<double>(m_hWhat, kAtn_Gain))
  , m_fOffset(GetAtt<double>(m_hWhat, kAtn_Offset))
  , m_nSize(pDims[0] * pDims[1])
{

}

Volume::Scan::Layer::Layer(
      hid_t hParent
    , bool bIsQuality
    , size_t nIndex
    , Quantity eQuantity
    , const hsize_t *pDims
    , const float *pData
    , float fNoData
    , float fUndetect)
  : m_bIsQuality(bIsQuality)
  , m_eQuantity(eQuantity)
  , m_hLayer(hParent, m_bIsQuality ? kGrp_Quality : kGrp_Data, nIndex, kCreate)
  , m_hWhat(m_hLayer, kGrp_What, kCreate)
  , m_fGain(1.0f)
  , m_fOffset(0.0f)
  , m_nSize(pDims[0] * pDims[1])
{
  // Fill in the 'what' parameters
  NewAtt(m_hWhat, kAtn_Quantity, m_eQuantity);
  NewAtt(m_hWhat, kAtn_Gain, m_fGain);
  NewAtt(m_hWhat, kAtn_Offset, m_fOffset);
  NewAtt(m_hWhat, kAtn_NoData, fNoData);
  NewAtt(m_hWhat, kAtn_Undetect, fUndetect);

  // Create the HDF dataset
  HID_Space hSpace(2, pDims, kCreate);
  HID_PList hPList(H5P_DATASET_CREATE, kCreate);
  if (H5Pset_chunk(hPList, 2, pDims) < 0)
    throw Error(m_hLayer, "Failed to set chunk parameters for layer");
  if (H5Pset_deflate(hPList, kDefCompression) < 0)
    throw Error(m_hLayer, "Failed to set compression level for layer");
  HID_Data hData(m_hLayer, kDat_Data, H5T_NATIVE_FLOAT, hSpace, hPList, kCreate);
  NewAtt(hData, kAtn_Class, kVal_Class);
  NewAtt(hData, kAtn_ImageVersion, kVal_ImageVersion);

  // Write the actual image data
  if (H5Dwrite(hData, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, pData) < 0)
    throw Error("Failed to write layer data");
}

Volume::Scan::Layer::Layer(const Layer &layer)
{
  throw Error("ASSERT: Copy constructor called on Volume::Scan::Layer");
}

Volume::Scan::Layer & Volume::Scan::Layer::operator=(const Layer &layer)
{
  throw Error("ASSERT: Assignment operator called on Volume::Scan::Layer");
}

void Volume::Scan::Layer::Read(float *pData, float &fNoData, float &fUndetect) const
{
  HID_Data hData(m_hLayer, kDat_Data, kOpen);

  // Verify the correct dimension to prevent memory corruption
  HID_Space hSpace(H5Dget_space(hData));
  if (H5Sget_simple_extent_npoints(hSpace) != m_nSize)
    throw Error(hData, "Dataset dimension mismatch");

  // Read the raw data
  fNoData = GetAtt<double>(m_hWhat, kAtn_NoData);
  fUndetect = GetAtt<double>(m_hWhat, kAtn_Undetect);
  if (H5Dread(hData, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, pData) < 0)
    throw Error(m_hLayer, "Failed to read layer data");

  // Convert using gain and offset?
  if (   std::fabs(m_fGain - 1.0) > 0.000001
      || std::fabs(m_fOffset) > 0.000001)
  {
    fNoData = (fNoData * m_fGain) + m_fOffset;
    fUndetect = (fUndetect * m_fGain) + m_fOffset;
    for (size_t i = 0; i < m_nSize; ++i)
      pData[i] = (pData[i] * m_fGain) + m_fOffset;
  }
}

void Volume::Scan::Layer::Write(const float *pData, float fNoData, float fUndetect)
{
  HID_Data hData(m_hLayer, kDat_Data, kOpen);

  // Verify the correct dimension to prevent memory corruption
  HID_Space hSpace(H5Dget_space(hData));
  if (H5Sget_simple_extent_npoints(hSpace) != m_nSize)
    throw Error(hData, "Dataset dimension mismatch");

  // Do we have to convert the data?
  if (   std::fabs(m_fGain - 1.0f) > 0.000001
      || std::fabs(m_fOffset) > 0.000001)
  {
    float fGainMult = 1.0f / m_fGain;
    fNoData = (fNoData - m_fOffset) * fGainMult;
    fUndetect = (fUndetect - m_fOffset) * fGainMult;

    std::vector<float> vecData;
    vecData.reserve(m_nSize);
    vecData.assign(pData, pData + m_nSize);
    for (std::vector<float>::iterator i = vecData.begin(); i != vecData.end(); ++i)
      *i = (*i - m_fOffset) * fGainMult;

    // Write the converted data
    SetAtt(m_hWhat, kAtn_NoData, fNoData);
    SetAtt(m_hWhat, kAtn_Undetect, fUndetect);
    if (H5Dwrite(hData, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &vecData[0]) < 0)
      throw Error("Failed to write layer data");
  }
  else
  {
    // Write the raw data
    SetAtt(m_hWhat, kAtn_NoData, fNoData);
    SetAtt(m_hWhat, kAtn_Undetect, fUndetect);
    if (H5Dwrite(hData, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, pData) < 0)
      throw Error("Failed to write layer data");
  }
}

Volume::Scan::Scan(
      hid_t hParent
    , size_t nIndex
    , double fElevation
    , size_t nAzimuths
    , size_t nRangeBins
    , size_t nFirstAzimuth
    , double fRangeStart
    , double fRangeScale
    , time_t tStart
    , time_t tEnd)
  : m_hScan(hParent, kGrp_Dataset, nIndex, kCreate)
  , m_hWhat(m_hScan, kGrp_What, kCreate)
  , m_hWhere(m_hScan, kGrp_Where, kCreate)
  , m_nAzimuthCount(nAzimuths)
  , m_nRangeCount(nRangeBins)
{
  NewAtt(m_hWhat, kAtn_Product, kProd_Scan);
  NewAtt(m_hWhat, kAtn_StartDate, kAtn_StartTime, tStart);
  NewAtt(m_hWhat, kAtn_EndDate, kAtn_EndTime, tEnd);

  NewAtt(m_hWhere, kAtn_Elevation, fElevation);
  NewAtt(m_hWhere, kAtn_RangeCount, (long) m_nRangeCount);
  NewAtt(m_hWhere, kAtn_RangeStart, fRangeStart / 1000.0);
  NewAtt(m_hWhere, kAtn_RangeScale, fRangeScale);
  NewAtt(m_hWhere, kAtn_AzimuthCount, (long) m_nAzimuthCount);
  NewAtt(m_hWhere, kAtn_FirstAzimuth, (long) nFirstAzimuth);
}

Volume::Scan::Scan(hid_t hParent, size_t nIndex)
  : m_hScan(hParent, kGrp_Dataset, nIndex, kOpen)
  , m_hWhat(m_hScan, kGrp_What, kOpen)
  , m_hWhere(m_hScan, kGrp_Where, kOpen)
  , m_hHow(m_hScan, kGrp_How, kOpen, true)
  , m_nAzimuthCount(GetAtt<long>(m_hWhere, kAtn_AzimuthCount))
  , m_nRangeCount(GetAtt<long>(m_hWhere, kAtn_RangeCount))
{
  hsize_t nObjs;
  char pszName[32];

  // Verify that this dataset is indeed a scan
  if (GetAtt<ProductType>(m_hWhat, kAtn_Product) != kProd_Scan)
    throw Error(m_hScan, "Scan product code mismatch");

  // Reserve some space in our layer info vector for efficency sake
  if (H5Gget_num_objs(m_hScan, &nObjs) < 0)
    throw Error(m_hScan, "Failed to determine number of objects in group");
  m_LayerInfos.reserve(nObjs);

  // Check for any data layers
  for (size_t i = 1; true; ++i)
  {
    // Do we have this 'dataX'?
    sprintf(pszName, "%s%d", kGrp_Data, i);
    htri_t ret = H5Lexists(m_hScan, pszName, H5P_DEFAULT);
    if (ret < 0)
      throw Error(m_hScan, "Failed to verify existence of group '%s'", pszName);
    if (!ret)
      break;

    // Yes - open it up for inspection
    HID_Group hLayer(m_hScan, pszName, kOpen);
    HID_Group hLayerWhat(hLayer, kGrp_What, kOpen);

    // Store some vitals
    LayerInfo li;
    li.m_bIsQuality = false;
    li.m_nIndex = i;
    li.m_eQuantity = GetAtt<Quantity>(hLayerWhat, kAtn_Quantity);
    m_LayerInfos.push_back(li);
  }

  // Check for any quality layers
  for (size_t i = 1; true; ++i)
  {
    // Do we have this 'dataX'?
    sprintf(pszName, "%s%d", kGrp_Quality, i);
    htri_t ret = H5Lexists(m_hScan, pszName, H5P_DEFAULT);
    if (ret < 0)
      throw Error(m_hScan, "Failed to verify existence of group '%s'", pszName);
    if (!ret)
      break;

    // Yes - open it up for inspection
    HID_Group hLayer(m_hScan, pszName, kOpen);
    HID_Group hLayerWhat(hLayer, kGrp_What, kOpen);

    // Store some vitals
    LayerInfo li;
    li.m_bIsQuality = true;
    li.m_nIndex = i;
    li.m_eQuantity = GetAtt<Quantity>(hLayerWhat, kAtn_Quantity);
    m_LayerInfos.push_back(li);
  }
}

Volume::Scan::Scan(const Scan &scan)
{
  throw Error("ASSERT: Copy constructor called on Volume::Scan");
}

Volume::Scan & Volume::Scan::operator=(const Scan &scan)
{
  throw Error("ASSERT: Assignment operator called on Volume::Scan");
}

Volume::Scan::LayerPtr Volume::Scan::GetLayer(size_t nLayer)
{
  return LayerPtr(
      new Layer(
          m_hScan, 
          m_LayerInfos[nLayer].m_bIsQuality,
          m_LayerInfos[nLayer].m_nIndex,
          m_LayerInfos[nLayer].m_eQuantity,
          &m_nAzimuthCount));
}

Volume::Scan::LayerConstPtr Volume::Scan::GetLayer(size_t nLayer) const
{
  return LayerConstPtr(
      new Layer(
          m_hScan, 
          m_LayerInfos[nLayer].m_bIsQuality,
          m_LayerInfos[nLayer].m_nIndex,
          m_LayerInfos[nLayer].m_eQuantity,
          &m_nAzimuthCount));
}

Volume::Scan::LayerPtr Volume::Scan::GetLayer(Quantity eQuantity)
{
  for (LayerInfoStore_t::iterator i = m_LayerInfos.begin(); i != m_LayerInfos.end(); ++i)
    if (i->m_eQuantity == eQuantity)
      return LayerPtr(
          new Layer(
              m_hScan, 
              i->m_bIsQuality, 
              i->m_nIndex,
              i->m_eQuantity,
              &m_nAzimuthCount));
  return LayerPtr(NULL);
}

Volume::Scan::LayerConstPtr Volume::Scan::GetLayer(Quantity eQuantity) const
{
  for (LayerInfoStore_t::const_iterator i = m_LayerInfos.begin(); i != m_LayerInfos.end(); ++i)
    if (i->m_eQuantity == eQuantity)
      return LayerConstPtr(
          new Layer(
              m_hScan, 
              i->m_bIsQuality, 
              i->m_nIndex,
              i->m_eQuantity,
              &m_nAzimuthCount));
  return LayerConstPtr(NULL);
}

Volume::Scan::LayerPtr Volume::Scan::AddLayer(
      Quantity eQuantity
    , bool bIsQuality
    , const float *pData
    , float fNoData
    , float fUndetect)
{
  LayerInfo li;
  li.m_bIsQuality = bIsQuality;
  li.m_nIndex = 1;
  for (LayerInfoStore_t::reverse_iterator i = m_LayerInfos.rbegin(); 
       i != m_LayerInfos.rend(); 
       ++i)
  {
    if (i->m_bIsQuality == li.m_bIsQuality)
    {
      li.m_nIndex = i->m_nIndex + 1;
      break;
    }
  }
  li.m_eQuantity = eQuantity;

  LayerPtr pLayer(
      new Layer(
          m_hScan,
          li.m_bIsQuality,
          li.m_nIndex,
          li.m_eQuantity,
          &m_nAzimuthCount,
          pData,
          fNoData,
          fUndetect));

  // Must do the push_back last so that exceptions don't screw with our 
  // layer count
  m_LayerInfos.push_back(li);
  return pLayer;
}

Volume::Volume(
      const std::string &strFilename
    , time_t tValid
    , double fLatitude
    , double fLongitude
    , double fHeight)
  : Base(strFilename, kObj_VolumePolar, tValid)
  , m_nScanCount(0)
{
  NewAtt(m_hWhere, kAtn_Latitude, fLatitude);
  NewAtt(m_hWhere, kAtn_Longitude, fLongitude);
  NewAtt(m_hWhere, kAtn_Height, fHeight);
}

Volume::Volume(const std::string &strFilename, bool bReadOnly)
  : Base(strFilename, bReadOnly, kObj_VolumePolar)
{
  // Determine the number of scans
  hsize_t nObjs;
  if (H5Gget_num_objs(m_hFile, &nObjs) < 0)
    throw Error(m_hFile, "Failed to determine number of objects in group");
  for (nObjs; nObjs > 0; --nObjs)
  {
    char pszName[32];
    sprintf(pszName, "%s%d", kGrp_Dataset, (int) nObjs);
    htri_t ret = H5Lexists(m_hFile, pszName, H5P_DEFAULT);
    if (ret < 0)
      throw Error(m_hFile, "Failed to verify existance of group '%s'", pszName);
    else if (ret)
      break;
  }
  m_nScanCount = nObjs;
}

Volume & Volume::operator=(const Volume &vol)
{
  throw Error("ASSERT: Assignment operator called on Volume");
}

