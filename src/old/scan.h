/*------------------------------------------------------------------------------
 * Rainfields ODIM HDF5 Library (rainhdf)
 *
 * Copyright (C) 2011 Commonwealth of Australia, Bureau of Meteorology
 * See COPYING for licensing and warranty details
 *----------------------------------------------------------------------------*/
#ifndef RAINHDF_SCAN_H
#define RAINHDF_SCAN_H

#include "base.h"
#include "data.h"
#include <memory>
#include <vector>

namespace rainfields {
namespace hdf {
  /// Unique handle to a scan in the HDF5 file
  class scan : public base
  {
  public:
#if RAINHDF_CXX11
    typedef std::unique_ptr<scan> ptr;
    typedef std::unique_ptr<const scan> const_ptr;
#else
    typedef std::auto_ptr<scan> ptr;
    typedef std::auto_ptr<const scan> const_ptr;
#endif

  public:
    virtual ~scan();

    /// Get the elevation of this scan
    double elevation() const { return get_att<double>(hnd_where_, atn_elevation); }
    /// Get the number of azimuths in scan
    size_t azimuth_count() const { return azi_count_; }
    /// Get the number of range bins per azimuth
    size_t range_bin_count() const { return bin_count_; }
    /// Get the number of the first azimuth to be radiated
    size_t first_azimuth() const { return get_att<long>(hnd_where_, atn_first_azimuth); }
    /// Get the distance from sensor at the start of the first range bin in meters
    double range_start() const { return get_att<double>(hnd_where_, atn_range_start) * 1000.0; }
    /// Get the distance between consecutive range bins in meters
    double range_scale() const { return get_att<double>(hnd_where_, atn_range_scale); }
    /// Get the time this scan commenced
    time_t start_time() const { return get_att<time_t>(hnd_what_, atn_start_date, atn_start_time); }
    /// Get the time this scan completed
    time_t end_time() const { return get_att<time_t>(hnd_what_, atn_end_date, atn_end_time); }

    /// Get the number of data layers in the scan
    size_t layer_count() const { return data_info_.size(); }
    /// Get the quantity stored by the 'nth' layer
    const std::string layer_quantity(size_t i) const { return data_info_[i].quantity_; }
    /// Get the 'nth' data layer
    data::ptr layer(size_t i);
    /// Get the 'nth' data layer
    data::const_ptr layer(size_t i) const;
    /// Get a data layer based on it's quantity (or NULL if no such data)
    data::ptr layer(const std::string& quantity);
    data::const_ptr layer(const std::string& quantity) const;
    /// Add a new data or quality layer to the scan
    data::ptr add_layer(const std::string& quantity, bool is_quality, bool floating_point);

  private:
    struct data_info
    {
      bool        is_quality_;  ///< True if quality, false if data
      size_t      index_;       ///< Index of dataX/qualityX in file
      std::string quantity_;    ///< Quantity stored by data layer
    };
    typedef std::vector<data_info> data_info_store;

  private:
    /// Create new scan in file
    scan(
          const base& parent
        , size_t index            ///< Scan number in file (datasetX)
        , double elevation        ///< Scan elevation angle (degrees above horizon)
        , size_t azimuth_count    ///< Number of azimuths in scan
        , size_t range_bin_count  ///< Number of range bins per azimuth
        , size_t first_azimuth    ///< Index of first azimuth to be radiated
        , double range_start      ///< Range of start of first bin (m)
        , double range_scale      ///< Distance between bins (m)
        , time_t start_time       ///< Time scan started
        , time_t end_time         ///< Time scan ended
        );
    /// Create handle to a scan that is existing in the file
    scan(const base& parent, size_t index);

  private:
    // Size required by data layers
    // Note: Multiple variable declaration is deliberate - used as an array internally
    hsize_t azi_count_,   ///< Number of azimuths
            bin_count_;   ///< Number of range bins

    // Handles to any data layers that are present
    data_info_store   data_info_;       ///< Information about data/quality layers

    friend class volume;
  };
}}

#endif
