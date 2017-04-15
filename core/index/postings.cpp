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

#include "utils/map_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "postings.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                           postings implementation
// -----------------------------------------------------------------------------

postings::postings(writer_t& writer):
  writer_(writer) {
}

postings::emplace_result postings::emplace(const bytes_ref& term) {
  REGISTER_TIMER_DETAILED();
  auto& parent = writer_.parent();
 
  // maximum number to bytes needed for storage of term length and data
  const auto max_term_len = term.size(); // + vencode_size(term.size());

  if (writer_t::container::block_type::SIZE < max_term_len) {
    // TODO: maybe move big terms it to a separate storage
    // reject terms that do not fit in a block
    return std::make_pair(map_.end(), false);
  }

  const auto slice_end = writer_.pool_offset() + max_term_len;
  const auto next_block_start = writer_.pool_offset() < parent.size()
                        ? writer_.position().block_offset() + writer_t::container::block_type::SIZE
                        : writer_t::container::block_type::SIZE * parent.count();

  // do not span slice over 2 blocks, start slice at the start of the next block
  if (slice_end > next_block_start) {
    writer_.seek(next_block_start);
  }

  assert(size() < type_limits<type_t::doc_id_t>::eof()); // not larger then the static flag

  struct generator_t {
    const bytes_ref& term;
    writer_t& writer;
    generator_t(const bytes_ref& v_term, writer_t& v_writer)
      : term(v_term), writer(v_writer) {
    }
    hashed_bytes_ref operator()(
      const hashed_bytes_ref& key, const posting&
    ) const {
      // for new terms also write out their value
      writer.write(term.c_str(), term.size());

      // reuse hash but point ref at data in pool
      return hashed_bytes_ref(key.hash(), (writer.position() - term.size()).buffer(), term.size());
    }
  } generator(term, writer_);

  // replace original reference to 'term' provided by the caller
  // with a reference to the cached copy in 'writer_'
  return map_utils::try_emplace_update_key(
    map_,                                     // container
    generator,                                // key generator
    make_hashed_ref(term, std::hash<irs::bytes_ref>()) // key
  );
}

NS_END