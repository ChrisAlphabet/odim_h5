/*------------------------------------------------------------------------------
 * Rainfields ODIM HDF5 Library (rainHDF)
 *
 * Copyright (C) 2011 Commonwealth of Australia, Bureau of Meteorology
 * See COPYING for licensing and warranty details
 *----------------------------------------------------------------------------*/
#include "scan.h"

using namespace rainhdf;

scan::~scan()
{

}

scan::scan(
      const base &parent
    , size_t index
    , double elevation
    , size_t azimuth_count
    , size_t range_bin_count
    , size_t first_azimuth
    , double range_start
    , double range_scale
    , time_t start_time
    , time_t end_time)
  : base(parent, grp_dataset, index, create)
  , azi_count_(azimuth_count)
  , bin_count_(range_bin_count)
{
  new_att(hnd_what_, atn_product, pt_scan);
  new_att(hnd_what_, atn_start_date, atn_start_time, start_time);
  new_att(hnd_what_, atn_end_date, atn_end_time, end_time);

  new_att(hnd_where_, atn_elevation, elevation);
  new_att(hnd_where_, atn_range_count, (long) bin_count_);
  new_att(hnd_where_, atn_range_start, range_start / 1000.0);
  new_att(hnd_where_, atn_range_scale, range_scale);
  new_att(hnd_where_, atn_azimuth_count, (long) azi_count_);
  new_att(hnd_where_, atn_first_azimuth, (long) first_azimuth);
}

scan::scan(const base &parent, size_t index)
  : base(parent, grp_dataset, index, open)
  , azi_count_(get_att<long>(hnd_where_, atn_azimuth_count))
  , bin_count_(get_att<long>(hnd_where_, atn_range_count))
{
  hsize_t obj_count;
  char name[32];

  // Verify that this dataset is indeed a scan
  if (get_att<product_type>(hnd_what_, atn_product) != pt_scan)
    throw error(hnd_this_, "Scan product code mismatch");

  // Reserve some space in our data info vector for efficency sake
  if (H5Gget_num_objs(hnd_this_, &obj_count) < 0)
    throw error(hnd_this_, "Failed to determine number of objects in group");
  data_info_.reserve(obj_count);

  // Check for any data layers
  for (size_t i = 1; true; ++i)
  {
    // Do we have this 'dataX'?
    sprintf(name, "%s%d", grp_data, i);
    htri_t ret = H5Lexists(hnd_this_, name, H5P_DEFAULT);
    if (ret < 0)
      throw error(hnd_this_, "Failed to verify existence of group '%s'", name);
    if (!ret)
      break;

    // Yes - open it up for inspection
    hid_handle hnd_data(hid_group, hnd_this_, name, open);
    hid_handle hnd_data_what(hid_group, hnd_data, grp_what, open);

    // Store some vitals
    data_info li;
    li.is_quality_ = false;
    li.index_ = i;
    li.quantity_ = get_att<quantity>(hnd_data_what, atn_quantity);
    data_info_.push_back(li);
  }

  // Check for any quality layers
  for (size_t i = 1; true; ++i)
  {
    // Do we have this 'dataX'?
    sprintf(name, "%s%d", grp_quality, i);
    htri_t ret = H5Lexists(hnd_this_, name, H5P_DEFAULT);
    if (ret < 0)
      throw error(hnd_this_, "Failed to verify existence of group '%s'", name);
    if (!ret)
      break;

    // Yes - open it up for inspection
    hid_handle hnd_data(hid_group, hnd_this_, name, open);
    hid_handle hnd_data_what(hid_group, hnd_data, grp_what, open);

    // Store some vitals
    data_info li;
    li.is_quality_ = true;
    li.index_ = i;
    li.quantity_ = get_att<quantity>(hnd_data_what, atn_quantity);
    data_info_.push_back(li);
  }
}

data::ptr scan::layer(size_t i)
{
  return data::ptr(
      new data(
          *this, 
          data_info_[i].is_quality_,
          data_info_[i].index_,
          data_info_[i].quantity_,
          &azi_count_));
}

data::const_ptr scan::layer(size_t i) const
{
  return data::const_ptr(
      new data(
          *this, 
          data_info_[i].is_quality_,
          data_info_[i].index_,
          data_info_[i].quantity_,
          &azi_count_));
}

data::ptr scan::layer(quantity _quantity)
{
  for (data_info_store::iterator i = data_info_.begin(); i != data_info_.end(); ++i)
    if (i->quantity_ == _quantity)
      return data::ptr(
          new data(
              *this, 
              i->is_quality_, 
              i->index_,
              i->quantity_,
              &azi_count_));
  return data::ptr(NULL);
}

data::const_ptr scan::layer(quantity _quantity) const
{
  for (data_info_store::const_iterator i = data_info_.begin(); i != data_info_.end(); ++i)
    if (i->quantity_ == _quantity)
      return data::const_ptr(
          new data(
              *this, 
              i->is_quality_, 
              i->index_,
              i->quantity_,
              &azi_count_));
  return data::const_ptr(NULL);
}

data::ptr scan::add_layer(
      quantity _quantity
    , bool is_quality
    , const float *raw
    , float no_data
    , float undetect)
{
  data_info li;
  li.is_quality_ = is_quality;
  li.index_ = 1;
  for (data_info_store::reverse_iterator i = data_info_.rbegin(); 
       i != data_info_.rend(); 
       ++i)
  {
    if (i->is_quality_ == li.is_quality_)
    {
      li.index_ = i->index_ + 1;
      break;
    }
  }
  li.quantity_ = _quantity;

  data::ptr ret(
      new data(
          *this,
          li.is_quality_,
          li.index_,
          li.quantity_,
          &azi_count_,
          raw,
          no_data,
          undetect));

  // Must do the push_back last so that exceptions don't screw with our 
  // layer count
  data_info_.push_back(li);
  return ret;
}
