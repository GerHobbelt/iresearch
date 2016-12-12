//
// IResearch search engine 
// 
// Copyright � 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "formats/formats_10.hpp" 
#include "store/memory_directory.hpp"
#include "filter_test_case_base.hpp"
#include "analysis/token_attributes.hpp"
#include "search/phrase_filter.hpp"

namespace ir = iresearch;

namespace tests {

void analyzed_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const tests::json::json_value& data) {
  typedef templates::text_field<std::string> text_field;
 
  class string_field : public templates::string_field {
   public:
    string_field(const ir::string_ref& name, const ir::string_ref& value)
      : templates::string_field(name, value, true, true) {
    }

    const ir::flags& features() const {
      static ir::flags features{ ir::frequency::type() };
      return features;
    }
  }; // string_field

  if (data.quoted) {
    // analyzed field
    doc.add(new text_field(
      std::string(name.c_str()) + "_anl",
      ir::string_ref(data.value),
      true
    ));

    // not analyzed field
    doc.add(new string_field(
      ir::string_ref(name),
      ir::string_ref(data.value)
    ));
  }
}

class phrase_filter_test_case : public filter_test_case_base {
 protected:
  void sequential() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("phrase_sequential.json"),
        &tests::analyzed_json_field_factory);
      add_segment(gen);
    }

    // read segment
    ir::index_reader::ptr rdr = open_reader();

    // empty field 
    {
      ir::by_phrase q;

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();

      auto docs = prepared->execute(*sub);
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // empty phrase
    {
      ir::by_phrase q;
      q.field("phrase_anl");

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();

      auto docs = prepared->execute(*sub);
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // equals to term_filter "fox"
    {
      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          iresearch::read_string<std::string>(in); // skip string
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("fox");

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();

      auto docs = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "A";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "G";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "I";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "K";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "L";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "fox" on field without positions
    {
      ir::by_phrase q;
      q.field("phrase")
       .push_back("fox");

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub);
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // equals to term_filter "fox" with phrase offset 
    // which is does not matter
    {
      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          iresearch::read_string<std::string>(in);
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("fox", ir::integer_traits<size_t>::const_max);

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();

      auto docs = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "A";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "G";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "I";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "K";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "L";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // "quick brown fox"
    {
      std::unordered_map<iresearch::string_ref, std::function<void(iresearch::data_input&)>> codecs{
        { "name", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
        { "phrase", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
      };

      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&codecs, &expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          auto it = codecs.find(field.name);
          if (codecs.end() == it) {
            return false; // can't find codec
          }
          it->second(in); // skip field
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("quick")
       .push_back("brown")
       .push_back("fox");

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();

      auto docs = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "A";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "G";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      expected_value = "I";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs_seek->seek(ir::type_limits<ir::type_t::doc_id_t>::eof())));
    }

    // "fox ... quick"
    {
      std::unordered_map<iresearch::string_ref, std::function<void(iresearch::data_input&)>> codecs{
        { "name", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
        { "phrase", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
      };

      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&codecs, &expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          auto it = codecs.find(field.name);
          if (codecs.end() == it) {
            return false; // can't find codec
          }
          it->second(in); // skip field
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("fox")
       .push_back("quick", 1);

      auto prepared = q.prepare(*rdr);

      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub); 
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "L";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs_seek->seek(ir::type_limits<ir::type_t::doc_id_t>::eof())));
    }

    // "fox ... quick" with phrase offset
    // which is does not matter
    {
      std::unordered_map<iresearch::string_ref, std::function<void(iresearch::data_input&)>> codecs{
        { "name", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
        { "phrase", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
      };

      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&codecs, &expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          auto it = codecs.find(field.name);
          if (codecs.end() == it) {
            return false; // can't find codec
          }
          it->second(in); // skip field
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("fox", ir::integer_traits<size_t>::const_max)
       .push_back("quick", 1);

      auto prepared = q.prepare(*rdr);

      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub); 
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "L";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs_seek->seek(ir::type_limits<ir::type_t::doc_id_t>::eof())));
    }

    // "fox ... ... quick"
    {
      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("fox")
       .push_back("quick", 2);

      auto prepared = q.prepare(*rdr);

      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub); 
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
    }

    // "eye ... eye"
    {
      std::unordered_map<iresearch::string_ref, std::function<void(iresearch::data_input&)>> codecs{
        { "name", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
        { "phrase", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
      };

      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&codecs, &expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          auto it = codecs.find(field.name);
          if (codecs.end() == it) {
            return false; // can't find codec
          }
          it->second(in); // skip field
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("eye")
       .push_back("eye", 1);

      auto prepared = q.prepare(*rdr);

      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub); 
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "C";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs_seek->seek(ir::type_limits<ir::type_t::doc_id_t>::eof())));
    }

    // "as in the past we are looking forward"
    {
      std::unordered_map<iresearch::string_ref, std::function<void(iresearch::data_input&)>> codecs{
        { "name", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
        { "phrase", [](iresearch::data_input& in)->void{ iresearch::read_string<std::string>(in); } },
      };

      const iresearch::string_ref expected_name = "name";
      iresearch::string_ref expected_value;
      iresearch::index_reader::document_visitor_f visitor = [&codecs, &expected_value, &expected_name](
        const iresearch::field_meta& field, iresearch::data_input& in
      ) {
        if (field.name != expected_name) {
          auto it = codecs.find(field.name);
          if (codecs.end() == it) {
            return false; // can't find codec
          }
          it->second(in); // skip field
          return true;
        }

        auto value = iresearch::read_string<std::string>(in);
        if (value != expected_value) {
          return false;
        }

        return true;
      };

      ir::by_phrase q;
      q.field("phrase_anl")
       .push_back("as")
       .push_back("in")
       .push_back("the")
       .push_back("past")
       .push_back("we")
       .push_back("are")
       .push_back("looking")
       .push_back("forward");

      auto prepared = q.prepare(*rdr);
      auto sub = rdr->begin();
      auto docs = prepared->execute(*sub); 
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::doc_id_t>::valid(docs_seek->value()));

      expected_value = "H";
      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(sub->document(docs->value(), visitor));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(sub->document(docs->value(), visitor));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(ir::type_limits<ir::type_t::doc_id_t>::eof(docs_seek->seek(ir::type_limits<ir::type_t::doc_id_t>::eof())));
    }  
  }
}; // phrase_filter_test_case 

} // tests

// ----------------------------------------------------------------------------
// --SECTION--                                             by_phrase base tests 
// ----------------------------------------------------------------------------

TEST(by_phrase_test, ctor) {
  ir::by_phrase q;
  ASSERT_EQ(ir::by_phrase::type(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
  ASSERT_EQ(q.begin(), q.end());
  ASSERT_EQ(ir::boost::no_boost(), q.boost());

  auto& features = ir::by_phrase::required();
  ASSERT_EQ(2, features.size());
  ASSERT_TRUE(features.check<ir::frequency>());
  ASSERT_TRUE(features.check<ir::position>());
}

TEST(by_phrase_test, boost) {
  // no boost
  {
    // no terms
    {
      ir::by_phrase q;
      q.field("field");

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(ir::boost::no_boost(), ir::boost::extract(prepared->attributes()));
    }

    // single term
    {
      ir::by_phrase q;
      q.field("field").push_back("quick");

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(ir::boost::no_boost(), ir::boost::extract(prepared->attributes()));
    }

    // multiple terms
    {
      ir::by_phrase q;
      q.field("field").push_back("quick").push_back("brown");

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(ir::boost::no_boost(), ir::boost::extract(prepared->attributes()));
    }
  }

  // with boost
  {
    iresearch::boost::boost_t boost = 1.5f;
    
    // no terms, return empty query
    {
      ir::by_phrase q;
      q.field("field");
      q.boost(boost);

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(ir::boost::no_boost(), ir::boost::extract(prepared->attributes()));
    }

    // single term
    {
      ir::by_phrase q;
      q.field("field").push_back("quick");
      q.boost(boost);

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(boost, ir::boost::extract(prepared->attributes()));
    }
    
    // single multiple terms 
    {
      ir::by_phrase q;
      q.field("field").push_back("quick").push_back("brown");
      q.boost(boost);

      auto prepared = q.prepare(tests::empty_index_reader::instance());
      ASSERT_EQ(boost, ir::boost::extract(prepared->attributes()));
    }
  }
}

TEST(by_phrase_test, push_back_insert) {
  ir::by_phrase q;

  // push_back 
  {
    q.push_back("quick");
    q.push_back(ir::ref_cast<ir::byte_type>(ir::string_ref("brown")), 1);
    q.push_back(ir::bstring(ir::ref_cast<ir::byte_type>(ir::string_ref("fox"))));
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(3, q.size());

    // check elements via positions
    {
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("quick")), q[0]); 
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("brown")), q[2]); 
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("fox")), q[3]); 
    }
    
    // check elements via iterators 
    {
      auto it = q.begin();
      ASSERT_NE(q.end(), it); 
      ASSERT_EQ(0, it->first);
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("quick")), it->second);

      ++it;
      ASSERT_NE(q.end(), it); 
      ASSERT_EQ(2, it->first);
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("brown")), it->second);

      ++it;
      ASSERT_NE(q.end(), it); 
      ASSERT_EQ(3, it->first);
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("fox")), it->second);

      ++it;
      ASSERT_EQ(q.end(), it); 
    }
   
    // push term 
    {
      q.push_back("squirrel", 0);
      ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("squirrel")), q[4]); 
    }
    ASSERT_EQ(4, q.size());
  }

  // insert
  {
    q[3] = ir::ref_cast<ir::byte_type>(ir::string_ref("jumps"));
    ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("jumps")), q[3]); 
    ASSERT_EQ(4, q.size());

    q.insert(5, "lazy");
    ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("lazy")), q[5]); 
    ASSERT_EQ(5, q.size());
    
    q.insert(28, ir::bstring(ir::ref_cast<ir::byte_type>(ir::string_ref("dog"))));
    ASSERT_EQ(ir::ref_cast<ir::byte_type>(ir::string_ref("dog")), q[28]); 
    ASSERT_EQ(6, q.size());
  }
}

TEST(by_phrase_test, equal) {
  ASSERT_EQ(ir::by_phrase(), ir::by_phrase());

  {
    ir::by_phrase q0;
    q0.field("name");
    q0.push_back("quick");
    q0.push_back("brown");
    
    ir::by_phrase q1;
    q1.field("name");
    q1.push_back("quick");
    q1.push_back("brown");
    ASSERT_EQ(q0, q1);
  }

  {
    ir::by_phrase q0;
    q0.field("name");
    q0.push_back("quick");
    q0.push_back("squirrel");
    
    ir::by_phrase q1;
    q1.field("name");
    q1.push_back("quick");
    q1.push_back("brown");
    ASSERT_NE(q0, q1);
  }
  
  {
    ir::by_phrase q0;
    q0.field("name1");
    q0.push_back("quick");
    q0.push_back("brown");
    
    ir::by_phrase q1;
    q1.field("name");
    q1.push_back("quick");
    q1.push_back("brown");
    ASSERT_NE(q0, q1);
  }
  
  {
    ir::by_phrase q0;
    q0.field("name");
    q0.push_back("quick");
    
    ir::by_phrase q1;
    q1.field("name");
    q1.push_back("quick");
    q1.push_back("brown");
    ASSERT_NE(q0, q1);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_phrase_filter_test_case : public tests::phrase_filter_test_case {
protected:
  virtual ir::directory* get_directory() override {
    return new ir::memory_directory();
  }

  virtual ir::format::ptr get_codec() override {
    static ir::version10::format FORMAT;
    return ir::format::ptr(&FORMAT, [](ir::format*)->void{});
  }
};

TEST_F(memory_phrase_filter_test_case, by_phrase) {
  sequential();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_phrase_filter_test_case : public tests::phrase_filter_test_case {
protected:
  virtual ir::directory* get_directory() override {
    return new ir::memory_directory();
  }

  virtual ir::format::ptr get_codec() override {
    static ir::version10::format FORMAT;
    return ir::format::ptr(&FORMAT, [](ir::format*)->void{});
  }
};

TEST_F(fs_phrase_filter_test_case, by_phrase) {
  sequential();
}