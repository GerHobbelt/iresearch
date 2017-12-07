////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_INDEX_TESTS_H
#define IRESEARCH_INDEX_TESTS_H

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

  #include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

#include "tests_shared.hpp"
#include "assert_format.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "doc_generator.hpp"
#include "utils/locale_utils.hpp"
#include "utils/timer_utils.hpp"
#include "document/field.hpp"

NS_ROOT

struct term_attribute;

NS_END // NS_ROOT

namespace ir = iresearch;

NS_BEGIN(tests)

class index_test_base : public virtual test_base {
 protected:
  virtual ir::directory* get_directory() = 0;
  virtual ir::format::ptr get_codec() = 0;

  ir::directory& dir() const { return *dir_; }
  ir::format::ptr codec() { return codec_; }
  const index_t& index() const { return index_; }

  ir::index_writer::ptr open_writer(ir::OPEN_MODE mode = ir::OPEN_MODE::OM_CREATE) {
    return ir::index_writer::make(*dir_, codec_, mode);
  }

  ir::directory_reader open_reader() {
    return ir::directory_reader::open(*dir_, codec_);
  }

  void assert_index(const ir::flags& features, size_t skip = 0) const {
    tests::assert_index(dir(), codec_, index(), features, skip);
  }

  virtual void SetUp() {
    test_base::SetUp();

    // set directory
    dir_.reset(get_directory());

    // set codec
    codec_ = get_codec();

    assert(dir_);
    assert(codec_);
  }

  virtual void TearDown() {
    test_base::TearDown();
    iresearch::timer_utils::init_stats(); // disable profile state tracking

    u_cleanup(); // release/free all memory used by ICU
  }

  void write_segment( ir::index_writer& writer, tests::index_segment& segment, tests::doc_generator_base& gen ) {
    // add segment
    const document* doc;
    while ((doc = gen.next())) {
      segment.add(doc->indexed.begin(), doc->indexed.end());
      insert(writer, doc->indexed.begin(), doc->indexed.end(), doc->stored.begin(), doc->stored.end());
    }
  }

  void add_segment(ir::index_writer& writer, tests::doc_generator_base& gen) {
    index_.emplace_back();
    write_segment(writer, index_.back(), gen);
    writer.commit();
  }

  void add_segments(ir::index_writer& writer, std::vector<doc_generator_base::ptr>& gens) {
    for (auto& gen : gens) {
      index_.emplace_back();
      write_segment(writer, index_.back(), *gen);
    }
    writer.commit();
  }

  void add_segment(tests::doc_generator_base& gen, ir::OPEN_MODE mode = ir::OPEN_MODE::OM_CREATE) {
    auto writer = open_writer(mode);
    add_segment(*writer, gen);
  }

 private: 
  index_t index_;
  ir::directory::ptr dir_;
  ir::format::ptr codec_;
}; // index_test_base

NS_BEGIN(templates)

//////////////////////////////////////////////////////////////////////////////
/// @class token_stream_payload
/// @brief token stream wrapper which sets payload equal to term value
//////////////////////////////////////////////////////////////////////////////
class token_stream_payload : public ir::token_stream {
 public:
  explicit token_stream_payload(ir::token_stream* impl);
  bool next(); 

  const irs::attribute_view& attributes() const NOEXCEPT {
    return impl_->attributes();
  }

 private:
  const ir::term_attribute* term_;
  ir::payload pay_;
  ir::token_stream* impl_;
}; // token_stream_payload

//////////////////////////////////////////////////////////////////////////////
/// @class text_field
/// @brief field which uses text analyzer for tokenization and stemming
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class text_field : public tests::field_base {
 public:
  text_field(
      const ir::string_ref& name, 
      bool payload = false)
      : token_stream_(ir::analysis::analyzers::get("text", "{\"locale\":\"C\", \"ignored_words\":[]}")) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
         throw ir::illegal_state();
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
  }

  text_field(
      const ir::string_ref& name, 
      const T& value,
      bool payload = false)
      : token_stream_(ir::analysis::analyzers::get("text", "{\"locale\":\"C\", \"ignored_words\":[]}")),
      value_(value) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
        throw ir::illegal_state();
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
  }

  text_field(text_field&& other) NOEXCEPT
    : pay_stream_(std::move(other.pay_stream_)),
      token_stream_(std::move(other.token_stream_)),
      value_(std::move(other.value_)) {
  }

  ir::string_ref value() const { return value_; }
  void value(const T& value) { value = value; }
  void value(T&& value) { value_ = std::move(value); }

  const ir::flags& features() const {
    static ir::flags features{ 
      iresearch::frequency::type(), iresearch::position::type(), 
      iresearch::offset::type(), iresearch::payload::type() 
    };
    return features;
  }

  ir::token_stream& get_tokens() const {
    token_stream_->reset(value_);

    return pay_stream_
      ? static_cast<ir::token_stream&>(*pay_stream_)
      : *token_stream_;
  }

 private:
  virtual bool write(ir::data_output&) const { return false; }

  static std::locale& get_locale() {
    static auto locale = iresearch::locale_utils::locale(nullptr, true);
    return locale;
  }

  std::unique_ptr<token_stream_payload> pay_stream_;
  ir::analysis::analyzer::ptr token_stream_;
  T value_;
}; // text_field

//////////////////////////////////////////////////////////////////////////////
/// @class string field
/// @brief field which uses simple analyzer without tokenization
//////////////////////////////////////////////////////////////////////////////
class string_field : public tests::field_base {
 public:
  string_field(const irs::string_ref& name, const irs::flags& extra_features = irs::flags::empty_instance());
  string_field(const irs::string_ref& name, const irs::string_ref& value, const irs::flags& extra_features = irs::flags::empty_instance());

  void value(const ir::string_ref& str);
  ir::string_ref value() const { return value_; }

  virtual const ir::flags& features() const override;
  virtual ir::token_stream& get_tokens() const override;
  virtual bool write(ir::data_output& out) const override;

 private:
  irs::flags features_;
  mutable ir::string_token_stream stream_;
  std::string value_;
}; // string_field

//////////////////////////////////////////////////////////////////////////////
/// @class europarl_doc_template
/// @brief document template for europarl.subset.text
//////////////////////////////////////////////////////////////////////////////
class europarl_doc_template: public delim_doc_generator::doc_template {
 public:
  typedef templates::text_field<irs::string_ref> text_field;

  virtual void init();
  virtual void value(size_t idx, const std::string& value);
  virtual void end();
  virtual void reset();

 private:
  std::string title_; // current title
  std::string body_; // current body
  irs::doc_id_t idval_ = 0;
}; // europarl_doc_template

NS_END // templates

void generic_json_field_factory(
  tests::document& doc,
  const std::string& name,
  const json_doc_generator::json_value& data
);

void payloaded_json_field_factory(
  tests::document& doc,
  const std::string& name,
  const json_doc_generator::json_value& data
);

NS_END // tests

#endif // IRESEARCH_INDEX_TESTS_H
