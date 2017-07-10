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

#ifndef IRESEARCH_SORT_H
#define IRESEARCH_SORT_H

#include "shared.hpp"
#include "index/iterators.hpp"
#include "utils/memory.hpp"
#include "utils/string.hpp"
#include "utils/attributes.hpp"
#include "utils/type_id.hpp"
#include "utils/iterator.hpp"

#include <vector>
#include <functional>
#include <boost/iterator/iterator_facade.hpp>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief represents a boost related to the particular query
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API boost : basic_attribute<float_t> {
  typedef float_t boost_t;

  static CONSTEXPR boost_t no_boost() { return 1.f; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief applies boost to the specified attributes collection ("src")
  //////////////////////////////////////////////////////////////////////////////
  static void apply(attribute_store& src, boost_t value) {
    if (boost::no_boost() == value) {
      return;
    }

    src.emplace<irs::boost>()->value *= value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns a value of the "boost" attribute in the specified "src"
  /// collection, or NO_BOOST value if there is no "boost" attribute in "src"
  //////////////////////////////////////////////////////////////////////////////
  static boost_t extract(const attribute_store& src) {
    boost_t value = no_boost();
    auto& attr = src.get<iresearch::boost>();

    if (attr) value = attr->value;

    return value;
  }

  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();

  boost();

  void clear() {
    value = no_boost();
  }
}; // boost

struct collector;
struct index_reader;
struct sub_reader;
struct term_reader;

////////////////////////////////////////////////////////////////////////////////
/// @class sort
/// @brief base class for all user-side sort entries
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API sort {
 public:
  DECLARE_SPTR(sort);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief object used for collecting index statistics required to be used by
  ///        the scorer for scoring individual documents
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API collector {
   public:
    DECLARE_PTR(collector);
    DECLARE_FACTORY(collector);

    virtual ~collector();
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compute term level statistics, e.g. from current attribute values
    ////////////////////////////////////////////////////////////////////////////////
    virtual void field(
      const sub_reader& /* segment */,
      const term_reader& /* field */) {
    } 

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compute term level statistics, e.g. from current attribute values
    ////////////////////////////////////////////////////////////////////////////////
    virtual void term(const attribute_store& /*term*/) {
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief store collected index statistics into attributes for the current
    ///        query node
    ////////////////////////////////////////////////////////////////////////////////
    virtual void finish(
      const iresearch::index_reader& /* index */,
      attribute_store& /*query_context*/
    ) {
    }
  }; // collector

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief stateful object used for computing the document score based on the
  ///        stored state
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API scorer {
   public:
    DECLARE_PTR(scorer);
    DECLARE_FACTORY(scorer);

    virtual ~scorer();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set the document score based on the stored state
    ////////////////////////////////////////////////////////////////////////////////
    virtual void score(byte_type* score_buf) = 0;
  }; // scorer

  template <typename T>
  class scorer_base : public scorer {
   public:
    typedef T score_t;

    FORCE_INLINE static T& score_cast(byte_type* score_buf) {
      return *reinterpret_cast<T*>(score_buf);
    }
  }; // scorer_base

  ////////////////////////////////////////////////////////////////////////////////
  /// @class sort::prepared
  /// @brief base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared {
   public:
    DECLARE_PTR(prepared);

    prepared() = default;
    virtual ~prepared();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief the features required for proper operation of this sort::prepared
    ////////////////////////////////////////////////////////////////////////////////
    virtual const flags& features() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics
    ////////////////////////////////////////////////////////////////////////////////
    virtual collector::ptr prepare_collector() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create a stateful scorer used for computation of document scores
    ////////////////////////////////////////////////////////////////////////////////
    virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& query_attrs,
      const attribute_store& doc_attrs
    ) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score container and prepare it for add(...) calls
    ////////////////////////////////////////////////////////////////////////////////
    virtual void prepare_score(byte_type* score) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief add the score from 'src' to the score in 'dst', i.e. +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual void add(byte_type* dst, const byte_type* src) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual bool less(const byte_type* lhs, const byte_type* rhs) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score type (i.e. sizeof(score))
    ////////////////////////////////////////////////////////////////////////////////
    virtual size_t size() const = 0;
  }; // prepared

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename T>
  class prepared_base: public prepared {
   public:
    typedef T score_t;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score container and prepare it for add(...) calls
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline void prepare_score(byte_type* score) const final override {
      assert(score);
      prepare_score(*reinterpret_cast<T*>(score));
    }

    virtual void prepare_score(T& score) const {
      std::memset(&score, 0, size());
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief add the score from 'src' to the score in 'dst', i.e. +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline void add(
      byte_type* dst, const byte_type* src
    ) const final override {
      assert(dst);
      assert(src);
      add(*reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
    }

    virtual void add(score_t& dst, const score_t& src) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline bool less(
      const byte_type* lhs, const byte_type* rhs
    ) const final override {
      assert(lhs);
      assert(rhs);
      return less(*reinterpret_cast<const T*>(lhs), *reinterpret_cast<const T*>(rhs));
    }

    virtual bool less(const score_t& lhs, const score_t& rhs) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score type (i.e. sizeof(score))
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline size_t size() const final override { return sizeof(score_t); }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @class type_id
  //////////////////////////////////////////////////////////////////////////////
  class type_id: public iresearch::type_id, util::noncopyable {
   public:
    type_id(const string_ref& name): name_(name) {}
    operator const type_id*() const { return this; }
    const string_ref& name() const { return name_; }

   private:
    string_ref name_;
  };

  explicit sort(const type_id& id);
  virtual ~sort();

  bool reverse() const { return rev_; }
  void reverse( bool rev ) { rev_ = rev; }

  const type_id& type() const { return *type_; }

  virtual prepared::ptr prepare() const = 0;

 private:
  const type_id* type_;
  bool rev_;
}; // sort

////////////////////////////////////////////////////////////////////////////////
/// @class sort
/// @brief base class for all user-side sort entries
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API order final {
public:
  typedef std::vector<sort::ptr> order_t;
  typedef ptr_iterator< order_t::const_iterator > const_iterator;
  typedef ptr_iterator< order_t::iterator > iterator;

  ////////////////////////////////////////////////////////////////////////////////
  /// @class sort
  /// @brief base class for all compiled sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared final : private util::noncopyable {
   public:
    struct prepared_sort : private util::noncopyable {
      explicit prepared_sort(sort::prepared::ptr&& bucket)
        : bucket(std::move(bucket)), offset(0) {
      }

      prepared_sort(prepared_sort&& rhs) NOEXCEPT
        : bucket(std::move(rhs.bucket)), offset(rhs.offset) {
        rhs.offset = 0;
      }

      prepared_sort& operator=(prepared_sort&& rhs) NOEXCEPT {
        if (this != &rhs) {
          bucket = std::move(rhs.bucket);
          offset = rhs.offset;
          rhs.offset = 0;
        }
        return *this;
      }

      sort::prepared::ptr bucket;
      size_t offset;
    }; // prepared_sort

    class IRESEARCH_API stats final : private util::noncopyable {
     public:
      typedef std::vector<sort::collector::ptr> collectors_t;

      explicit stats(collectors_t&& colls);
      stats(stats&& rhs) NOEXCEPT;

      stats& operator=(stats&& rhs) NOEXCEPT;

      void field(const sub_reader& segment, const term_reader& field) const;

      void term(const attribute_store& term) const;

      void finish(const index_reader& index, attribute_store& query_context) const;

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      collectors_t colls_;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // collectors

    class IRESEARCH_API scorers final : private util::noncopyable {
     public:
      typedef std::vector<sort::scorer::ptr> scorers_t;

      scorers() = default;
      explicit scorers(scorers_t&& scorers);
      scorers(scorers&& rhs) NOEXCEPT;

      scorers& operator=(scorers&& rhs) NOEXCEPT;

      void score(const prepared& ord, byte_type* score) const;

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      scorers_t scorers_;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // scorers

    typedef std::vector<prepared_sort> prepared_order_t;

    static const prepared& unordered();

    prepared();
    prepared(prepared&& rhs) NOEXCEPT;

    prepared& operator=(prepared&& rhs) NOEXCEPT;

    const flags& features() const { return features_; }
    size_t size() const { return size_; }
    bool empty() const { return order_.empty(); }

    prepared_order_t::const_iterator begin() const { 
      return prepared_order_t::const_iterator(order_.begin()); 
    }

    prepared_order_t::const_iterator end() const { 
      return prepared_order_t::const_iterator(order_.end()); 
    }

    const prepared_sort& operator[]( size_t i ) const { return order_[i]; }

    prepared::stats prepare_stats() const;

    prepared::scorers prepare_scorers(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& stats,
      const attribute_store& doc
    ) const;

    bool less(const byte_type* lhs, const byte_type* rhs) const;
    void add(byte_type* lhs, const byte_type* rhs) const;

  private:
    friend class order;

    template<typename Func>
    inline void for_each( const Func& func ) const {
      std::for_each( order_.begin(), order_.end(), func );
    }

    IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
    prepared_order_t order_;
    flags features_;
    size_t size_;
    IRESEARCH_API_PRIVATE_VARIABLES_END
  }; // prepared

  static const order& unordered();

  order() = default;
  order(order&& rhs) NOEXCEPT;

  order& operator=(order&& rhs) NOEXCEPT;

  bool operator==(const order& other) const;

  bool operator!=(const order& other) const;

  prepared prepare() const;

  order& add(sort::ptr const& sort);

  template<typename T, typename... Args>
  T& add(Args&&... args) {
    typedef typename std::enable_if <
      std::is_base_of< sort, T >::value, T
    >::type type;

    order_.emplace_back(type::make(std::forward<Args>(args)...));
    return static_cast< type& >( *order_.back() );
  }

  template<typename T>
  void remove() {
    typedef typename std::enable_if <
      std::is_base_of< sort, T >::value, T
    >::type type;

    remove( type::type() );
  }

  void remove( const type_id& id );
  void clear() { order_.clear(); }

  size_t size() const { return order_.size(); }
  bool empty() const { return order_.empty(); }

  const_iterator begin() const { return const_iterator( order_.begin() ); }
  const_iterator end() const { return const_iterator( order_.end() ); }

  iterator begin() { return iterator( order_.begin() ); }
  iterator end() { return iterator( order_.end() ); }

private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  order_t order_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // order

NS_END

#endif
