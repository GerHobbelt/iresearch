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

#include "tests_shared.hpp"
#include "index/index_tests.hpp"
#include "store/memory_directory.hpp"
#include "search/range_filter.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"
#include "search/term_filter.hpp"
#include "search/tfidf.hpp"
#include "utils/utf8_path.hpp"

NS_BEGIN(tests)

class tfidf_test: public index_test_base { 
 protected:
  virtual iresearch::directory* get_directory() {
    return new iresearch::memory_directory();
   }

  virtual iresearch::format::ptr get_codec() {
    return ir::formats::get("1_0");
  }
};

NS_END

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/////////////////
// Freq | Term //
/////////////////
// 4    | 0    //
// 3    | 1    //
// 10   | 2    //
// 7    | 3    //
// 5    | 4    //
// 4    | 5    //
// 3    | 6    //
// 7    | 7    //
// 2    | 8    //
// 7    | 9    //
/////////////////

//////////////////////////////////////////////////
// Stats                                        //
//////////////////////////////////////////////////
// TotalFreq = 52                               //
// DocsCount = 8                                //
// AverageDocLength (TotalFreq/DocsCount) = 6.5 //
//////////////////////////////////////////////////

TEST_F(tfidf_test, test_load) {
  irs::order order;
  auto scorer = irs::scorers::get("tfidf", irs::string_ref::nil);

  ASSERT_NE(nullptr, scorer);
  ASSERT_EQ(1, order.add(scorer).size());
}

TEST_F(tfidf_test, test_query) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    add_segment(gen);
  }

  irs::order order;

  order.add(irs::scorers::get("tfidf", irs::string_ref::nil));

  auto prepared_order = order.prepare();
  auto comparer = [&prepared_order](const irs::bstring& lhs, const irs::bstring& rhs)->bool {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  auto reader = iresearch::directory_reader::open(dir(), codec());
  auto& segment = *(reader.begin());
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // by_term
  {
    irs::by_term filter;

    filter.field("field").term("7");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    while(docs->next()) {
      docs->score();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score->value(), seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range single
  {
    irs::by_range filter;

    filter.field("field")
      .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    while(docs->next()) {
      docs->score();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score->value(), seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range single + scored_terms_limit
  {
    irs::by_range filter;

    filter.field("field").scored_terms_limit(0)
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("8")
      .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("9");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 3, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    while(docs->next()) {
      docs->score();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score->value(), seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range multiple
  {
    irs::by_range filter;

    filter.field("field")
      .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 7, 3, 0, 1, 5 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    while(docs->next()) {
      docs->score();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score->value(), seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
}

#ifndef IRESEARCH_DLL

TEST_F(tfidf_test, test_order) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    add_segment(gen);
  }

  auto reader = iresearch::directory_reader::open(dir(), codec());
  auto& segment = *(reader.begin());

  iresearch::by_term query;
  query.field("field");

  iresearch::order ord;
  ord.add<iresearch::tfidf_sort>();
  auto prepared_order = ord.prepare();

  auto comparer = [&prepared_order] (const iresearch::bstring& lhs, const iresearch::bstring& rhs) {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  uint64_t seq = 0;
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  {
    query.term("7");

    std::multimap<iresearch::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared = query.prepare(reader, prepared_order);
    auto docs = prepared->execute(segment, prepared_order);
    auto& score = docs->attributes().get<iresearch::score>();
    for (; docs->next();) {
      docs->score();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = iresearch::read_string<std::string>(in);
      seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score->value(), seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    const bool eq = std::equal(
      sorted.begin(), sorted.end(), expected.begin(),
      [](const std::pair<iresearch::bstring, uint64_t>& lhs, uint64_t rhs) {
        return lhs.second == rhs;
    });
    ASSERT_TRUE(eq);
  }
}

#endif
