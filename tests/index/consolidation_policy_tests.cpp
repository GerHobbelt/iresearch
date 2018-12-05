////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "index/index_writer.hpp"
#include "utils/index_utils.hpp"

NS_LOCAL

void print_consolidation(
    const irs::index_meta& meta,
    const irs::index_writer::consolidation_policy_t& policy
) {
  auto less = [](const irs::segment_meta* lhs, const irs::segment_meta* rhs) {
    if (lhs->size == rhs->size) {
      return lhs->name < rhs->name;
    }

    return lhs->size < rhs->size;
  };

  std::set<const irs::segment_meta*> candidates;
  irs::index_writer::consolidating_segments_t consolidating_segments;

  while (true) {
    candidates.clear();
    policy(candidates, meta, consolidating_segments);

    if (candidates.empty()) {
      break;
    }

    std::set<const irs::segment_meta*, decltype(less)> sorted_candidates(
      candidates.begin(), candidates.end(), less
    );

    std::cerr << "Consolidation: ";
    for (auto* segment : sorted_candidates) {
      std::cerr << segment->size << ", ";
    }
    std::cerr << std::endl;

    consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
  }
}

void assert_candidates(
    const irs::index_meta& meta,
    const std::vector<size_t>& expected_candidates,
    const std::set<const irs::segment_meta*>& actual_candidates
) {
  ASSERT_EQ(expected_candidates.size(), actual_candidates.size());

  for (const size_t expected_candidate_idx : expected_candidates) {
    const auto& expected_candidate = meta[expected_candidate_idx];
    ASSERT_NE(actual_candidates.end(), actual_candidates.find(&expected_candidate.meta));
  }
}

NS_END

TEST(consolidation_test_tier, test_max_consolidation_size) {
  // generate meta
  irs::index_meta meta;
  for (size_t i = 0; i < 22; ++i) {
    meta.add(irs::segment_meta(std::to_string(i), nullptr, 1, 1, false, irs::segment_meta::file_set(), 1));
  }

  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = irs::integer_traits<size_t>::const_max;
    options.min_segments = 1;
    options.max_segments_bytes = 10;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments_bytes, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments_bytes, candidates.size());
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size() - 2*options.max_segments_bytes, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: max_segments_bytes == 0
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = irs::integer_traits<size_t>::const_max;
    options.min_segments = 1;
    options.max_segments_bytes = 0;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // all segments are too big
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_max_consolidation_count) {
  // generate meta
  irs::index_meta meta;
  for (size_t i = 0; i < 22; ++i) {
    meta.add(irs::segment_meta(std::to_string(i), nullptr, 1, 1, false, irs::segment_meta::file_set(), 1));
  }

  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = 10;
    options.min_segments = 1;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size() - 2*options.max_segments, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // max_segments == irs::integer_traits<size_t>::const_max
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = irs::integer_traits<size_t>::const_max;
    options.min_segments = 1;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size(), candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: max_segments == 0
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = 0;
    options.min_segments = 1;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    for (size_t i = 0; i < meta.size(); ++i) {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(1, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: floor_segments_bytes == 0
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 0;
    options.max_segments = 10;
    options.min_segments = 3;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_min_consolidation_count) {
  // generate meta
  irs::index_meta meta;
  for (size_t i = 0; i < 22; ++i) {
    meta.add(irs::segment_meta(std::to_string(i), nullptr, 1, 1, false, irs::segment_meta::file_set(), 1));
  }

  // min_segments == 3
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = 10;
    options.min_segments = 3;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: min_segments == 1
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = 10;
    options.min_segments = 0;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size() - 2*options.max_segments, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: min_segments > max_segments
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = 10;
    options.min_segments = irs::integer_traits<size_t>::const_max;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(options.max_segments, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: min_segments > max_segments
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 1;
    options.max_segments = irs::integer_traits<size_t>::const_max;
    options.min_segments = irs::integer_traits<size_t>::const_max;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // can't find anything
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_consolidation_floor) {
  // generate meta
  irs::index_meta meta;
  {
    size_t i = 0;
    for (; i < 5; ++i) {
      meta.add(irs::segment_meta(std::to_string(i), nullptr, 1, 1, false, irs::segment_meta::file_set(), 2*i));
    }
    for (; i < 22; ++i) {
      meta.add(irs::segment_meta(std::to_string(i), nullptr, 1, 1, false, irs::segment_meta::file_set(), 2*i));
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = 8;
    options.max_segments = meta.size();
    options.min_segments = 1;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(5, candidates.size());

      for (size_t i = 0; i < candidates.size(); ++i) {
        ASSERT_NE(candidates.end(), candidates.find(&meta[i].meta));
      }
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size()-5, candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // enormous floor value, treat all segments as equal
  {
    irs::index_utils::consolidate_tier options;
    options.floor_segment_bytes = irs::integer_traits<uint32_t>::const_max;
    options.max_segments = irs::integer_traits<size_t>::const_max;
    options.min_segments = 1;
    options.max_segments_bytes = irs::integer_traits<size_t>::const_max;

    irs::index_writer::consolidating_segments_t consolidating_segments;
    auto policy = irs::index_utils::consolidation_policy(options);

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
      ASSERT_EQ(meta.size(), candidates.size());
    }

    // last empty tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_defaults) {
  irs::index_utils::consolidate_tier options;
  auto policy = irs::index_utils::consolidation_policy(options);

  {
    irs::index_writer::consolidating_segments_t consolidating_segments;

    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100,  false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4 }, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4 }, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, {0}, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, {0}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_no_candidates) {
  irs::index_utils::consolidate_tier options;
  options.floor_segment_bytes = 2097152;
  options.max_segments_bytes = 4294967296;
  options.min_segments = 5;         // min number of segments per tier to merge at once
  options.max_segments = 10;        // max number of segments per tier to merge at once
  options.lookahead = irs::integer_traits<size_t>::const_max;
  auto policy = irs::index_utils::consolidation_policy(options);

  irs::index_writer::consolidating_segments_t consolidating_segments;
  irs::index_meta meta;
  meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 141747));
  meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1548373791));
  meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1699787770));
  meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1861963739));
  meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2013404723));

  std::set<const irs::segment_meta*> candidates;
  policy(candidates, meta, consolidating_segments);
  ASSERT_TRUE(candidates.empty()); // candidates too large
}

TEST(consolidation_test_tier, test_skewed_segments) {
  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;         // min number of segments per tier to merge at once
    options.max_segments = 10;        // max number of segments per tier to merge at once
    options.max_segments_bytes= 2500; // max size of the merge
    options.floor_segment_bytes = 50; // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 40));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 60));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 70));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 200));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 750));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1100));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 90));
    meta.add(irs::segment_meta("11", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 75));
    meta.add(irs::segment_meta("12", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1500));
    meta.add(irs::segment_meta("13", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10000));
    meta.add(irs::segment_meta("14", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 5000));
    meta.add(irs::segment_meta("15", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1750));
    meta.add(irs::segment_meta("16", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 690));

    const std::vector<std::vector<size_t>> expected_tiers {
      { 0, 1, 2, 3, 4, 10, 11 },
      { 5, 6 },
      { 7, 8, 16 },
      { 9 },
    };

    for (auto& expected_tier : expected_tiers) {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead =  irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));

    const std::vector<std::vector<size_t>> expected_tiers {
      { 0, 1 },
      { 2, 3 },
      { 4, 5 },
      { 6, 7, 8 },
      { 9 }
    };

    for (auto& expected_tier : expected_tiers) {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 3;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));

    const std::vector<std::vector<size_t>> expected_tiers {
      { 2, 3, 4 },
      { 6, 7, 8 }
      // no more candidates since 10, 100, 4000, 100000 means exponensial grow
    };

    for (auto& expected_tier : expected_tiers) {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 51));
    meta.add(irs::segment_meta("11", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 151));
    meta.add(irs::segment_meta("12", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 637));
    meta.add(irs::segment_meta("13", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 351));
    meta.add(irs::segment_meta("14", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2351));
    meta.add(irs::segment_meta("15", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1351));
    meta.add(irs::segment_meta("16", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1351));
    meta.add(irs::segment_meta("17", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 20));

    const std::vector<std::vector<size_t>> expected_tiers {
      { 0, 10, 17 },
      { 1, 11 },
      { 2, 3, 12, 13, 15, 16},
      { 4, 14 },
      { 5, 6 },
      { 7, 8 },
      { 9 }
    };

    for (auto& expected_tier : expected_tiers) {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      candidates.clear();
      policy(candidates, meta, consolidating_segments);
      assert_candidates(meta, expected_tier, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }
    ASSERT_EQ(meta.size(), consolidating_segments.size());

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

/*
TEST(consolidation_test_tier, print_consolidation) {
  const size_t sizes[] = {
    9067, 2228, 9023, 0, 9293, 2637, 7529, 291, 4816, 68, 11, 3582, 4298, 4590, 2772, 9021, 32, 1993, 340, 538, 8578, 258, 8731, 5180, 5708, 339, 3830, 1530, 3906, 8714, 3501,
    1767, 2695, 458, 286, 2506, 3454, 9191, 9368, 305, 17, 219, 6198, 1562, 6303, 7162, 4601, 2687, 8205, 8321, 4568, 2511, 6629, 9109, 9502, 1412, 357, 5235, 137, 9886, 5607,
    1359, 9174, 529, 7074, 8343, 8023, 1618, 6128, 1661, 515, 2388, 2549, 826, 180, 886, 4237, 317, 170, 1532, 1602, 1091, 8953, 1791, 8523, 130, 22, 6319, 6145, 7034, 2006, 52,
    9361, 3443, 8228, 1345, 95, 1940, 6432, 609
  };

//  const size_t sizes[] = {
//   27629, 67916, 23094, 66931, 24073, 34911, 59754, 19536, 28564, 49466, 77086, 36574, 67376, 23809, 78202, 46143, 25801, 20893, 26131, 32593, 7609, 15070, 694, 92066, 63555, 97486, 64172, 16209, 859, 79567,
//   60533, 17274, 17947, 56072, 40469, 98003, 19283, 77609, 1027, 72332, 15191, 42206, 98551, 65745, 88190, 60909, 97204, 54687, 47229, 38288, 6965, 62032, 66034, 26130, 4888, 33979, 28488, 59519, 16049, 44099,
//   45938, 71850, 86965, 33289, 13793, 7017, 25307, 18551, 96080, 23804, 69327, 7633, 16125, 18691, 14009
//  };

//  const size_t sizes[] = {
//    39925, 69814, 31158, 45391, 42108, 49794, 90810, 88669, 31632, 98881, 81698, 63634, 31758, 99815, 95303, 99964, 74580, 7634, 95940, 68600, 99504, 56998, 99933, 58606, 99834, 90622, 84503, 99882, 75774, 97943, 99818, 30614, 17753, 14212, 23742, 24788, 79865, 99529, 68948, 75311, 28023, 68965, 81220, 24513, 98869, 50843, 20845, 1423, 66032, 96528, 71077, 50124, 90054, 61266, 64006, 58311, 95182, 89368, 74420, 26147, 77032, 26783, 23523, 73613, 90597, 99851, 2703, 74494, 72453, 66344, 91411, 14495, 51000, 66759, 99783
//  };

//  const size_t sizes[] = {
//   65584, 34724, 27250, 77632, 65272, 91084, 46651, 50883, 10398, 41399, 55901, 66135, 60459, 30839, 18633, 55088, 9270, 35329, 42071, 48036, 49299, 38439, 43901, 71549, 47871, 82822, 18587, 64491, 67480, 50796, 38375, 40147, 87542, 82888, 44853, 75229, 48187, 24594, 37377, 61251, 57762, 81775, 84560, 26733, 27540, 38030, 46588, 44271, 80052, 21478, 26121, 22177, 92980, 15496, 18489, 55435, 44831, 35454, 55938, 57711, 78043, 77401, 14121, 45084, 91617, 11910, 49441, 58949, 32022, 36613, 44715, 39283, 50288, 70946
//  };

 // const size_t sizes[] = {
 //74604, 51462, 27638, 81295, 75957, 91440, 17068, 85996, 82025, 98362, 96854, 90636, 71054, 86661, 74262, 38172, 10466, 17682, 49883, 8338, 40183, 77155, 19723, 76477, 66132, 15475, 18601, 87714, 22698, 59075, 1573, 38357, 68392, 6167, 95481, 91022, 77503, 8948, 99338, 43697, 50419, 60041, 33054, 57314, 2088, 53208, 19883, 80704, 26602, 85861, 70361, 16845, 85436, 65456, 6352, 97385, 71451, 18428, 87989, 13448, 72374, 55228, 43709, 22734, 88412, 88552, 16858, 85550, 78397, 25945, 7546, 90200, 78385, 96336, 84866
 // };

//  const size_t sizes[] = {
//    90,
//    11000
//  };

//  const size_t sizes[] = {
//    90, 100, 110, 95, 105,
//    150, 145, 155, 160, 165,
//    1000, 900, 1100, 1150, 950,
//    10000, 10100, 9900, 10250, 9800,
//    110000, 110100, 19900, 110250, 19800,
//  };

  irs::index_meta meta;
  for (auto begin = std::begin(sizes), end = std::end(sizes); begin != end; ++begin) {
    const size_t i = std::distance(begin, end);
    meta.add(irs::segment_meta(std::to_string(i), nullptr, 100, 100, false, irs::segment_meta::file_set{}, *begin));
  }

  irs::index_utils::consolidate_tier options;
  options.min_segments = 1;           // min number of segments per tier to merge at once
  options.max_segments = 10;           // max number of segments per tier to merge at once
  options.max_segments_bytes = 2500; // max size of the merge
  options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
  options.lookahead = irs::integer_traits<size_t>::const_max;

  auto policy = irs::index_utils::consolidation_policy(options);

  print_consolidation(meta, policy);
}
*/

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
