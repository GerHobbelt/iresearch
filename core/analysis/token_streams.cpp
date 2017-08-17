//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "token_streams.hpp"
#include "utils/bit_utils.hpp"

NS_LOCAL

irs::bstring const& value_empty() {
  static const irs::bstring value(0, irs::byte_type(0));
  return value;
}

irs::bstring const& value_false() {
  static const irs::bstring value(1, irs::byte_type(0));
  return value;
}

irs::bstring const& value_true() {
  static const irs::bstring value(1, irs::byte_type(~0));
  return value;
}

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                               boolean_token_stream implementation
// -----------------------------------------------------------------------------

boolean_token_stream::boolean_token_stream(bool value /*= false*/) 
  : attrs_(2), // increment + term
    in_use_(false), 
    value_(value) {
  init_attributes();
}

boolean_token_stream::boolean_token_stream(
    boolean_token_stream&& other) NOEXCEPT
  : term_(std::move(other.term_)),
    in_use_(std::move(other.in_use_)),
    value_(std::move(other.value_)) {
  init_attributes();
}

bool boolean_token_stream::next() {
  if (in_use_) {
    return false;
  }

  in_use_ = true;
  term_.value(value_ ? value_true() : value_false());

  return true;
}

/*static*/ const bytes_ref& boolean_token_stream::value_false() {
  static const bytes_ref value(::value_false());

  return value;
}

/*static*/ const bytes_ref& boolean_token_stream::value_true() {
  static const bytes_ref value(::value_true());

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                string_token_stream implementation
// -----------------------------------------------------------------------------

string_token_stream::string_token_stream()
  : attrs_(3), // offset + basic_term + increment
    in_use_(false) {
  init_attributes();
}

string_token_stream::string_token_stream(string_token_stream&& other) NOEXCEPT
  : offset_(std::move(other.offset_)),
    inc_(std::move(other.inc_)),
    term_(std::move(other.term_)),
    value_(std::move(other.value_)),
    in_use_(std::move(other.in_use_)) {
  init_attributes();
}

bool string_token_stream::next() {
  if (in_use_) {
    return false;
  }

  term_.value(value_);
  offset_.start = 0;
  offset_.end = static_cast<uint32_t>(value_.size());
  return (in_use_ = true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                       numeric_term implementation
// -----------------------------------------------------------------------------

bytes_ref numeric_token_stream::numeric_term::value(
    bstring& buf,
    NumericType type,
    decltype(numeric_token_stream::numeric_term::val_) val,
    uint32_t shift) {
  switch (type) {
    case NT_LONG: {
      typedef numeric_utils::numeric_traits<int64_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i64, &(buf[0]), shift));
    }
    case NT_DBL: {
      typedef numeric_utils::numeric_traits<double_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i64, &(buf[0]), shift));
    }
    case NT_INT: {
      typedef numeric_utils::numeric_traits<int32_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i32, &(buf[0]), shift));
    }
    case NT_FLOAT: {
      typedef numeric_utils::numeric_traits<float_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i32, &(buf[0]), shift));
    }
  }

  return bytes_ref::nil;
}

// -----------------------------------------------------------------------------
// --SECTION--                               numeric_token_stream implementation
// -----------------------------------------------------------------------------

numeric_token_stream::numeric_token_stream() 
  : attrs_(2) { // numeric_term + increment
  init_attributes();
}

numeric_token_stream::numeric_token_stream(
  numeric_token_stream&& other) NOEXCEPT
  : num_(std::move(other.num_)),
    inc_(std::move(other.inc_)) {
  init_attributes();
}

bool numeric_token_stream::next() {
  return num_.next(inc_);
}

void numeric_token_stream::reset(
    int32_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

void numeric_token_stream::reset(
    int64_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

#ifndef FLOAT_T_IS_DOUBLE_T
void numeric_token_stream::reset(
    float_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}
#endif

void numeric_token_stream::reset(
    double_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, int32_t value) {
  return numeric_term::value(buf, value);
}

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, int64_t value) {
  return numeric_term::value(buf, value);
}

#ifndef FLOAT_T_IS_DOUBLE_T
  /*static*/ bytes_ref numeric_token_stream::value(bstring& buf, float_t value) {
    return numeric_term::value(buf, value);
  }
#endif

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, double_t value) {
  return numeric_term::value(buf, value);
}

// -----------------------------------------------------------------------------
// --SECTION--                                  null_token_stream implementation
// -----------------------------------------------------------------------------

null_token_stream::null_token_stream()
  : attrs_(2), // basic_term + increment
    in_use_(false) {
  init_attributes();
}

null_token_stream::null_token_stream(null_token_stream&& other) NOEXCEPT
  : term_(std::move(other.term_)),
    in_use_(std::move(other.in_use_)) {
  init_attributes();
}

bool null_token_stream::next() {
  if (in_use_) {
    return false;
  }

  in_use_ = true;
  term_.value(value_null());

  return true;
}

/*static*/ const bytes_ref& null_token_stream::value_null() {
  // data pointer != nullptr or assert failure in bytes_hash::insert(...)
  static const bytes_ref value(::value_empty());

  return value;
}

NS_END
