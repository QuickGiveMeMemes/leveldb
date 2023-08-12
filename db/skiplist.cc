
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <set>
#include <string>
#include <iterator>
#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "util/arena.h"
#include "util/random.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/no_destructor.h"
#include "skiplist.h"

namespace leveldb {

DefaultCmpI::DefaultCmpI() = default;
const char* DefaultCmpI::Name() const { return "leveldb.DefaultCmp"; }
int DefaultCmpI::Compare(const Slice& a, const Slice& b) const {
    return a.compare(b);
}
void DefaultCmpI::FindShortestSeparator(std::string* start,const Slice& limit) const {
    // Find length of common prefix
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;
    }

    if (diff_index >= min_length) {
      // Do not shorten if one string is a prefix of the other
    } else {
      uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
      if (diff_byte < static_cast<uint8_t>(0xff) &&
          diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
        (*start)[diff_index]++;
        start->resize(diff_index + 1);
        assert(Compare(*start, limit) < 0);
      }
    }
  }
void DefaultCmpI::FindShortSuccessor(std::string* key) const {
    // Find first character that can be incremented
    size_t n = key->size();
    for (size_t i = 0; i < n; i++) {
    const uint8_t byte = (*key)[i];
    if (byte != static_cast<uint8_t>(0xff)) {
        (*key)[i] = byte + 1;
        key->resize(i + 1);
        return;
    }
    }
    // *key is a run of 0xffs.  Leave it alone.
}
const Comparator* DefaultCmp() {
  static NoDestructor<DefaultCmpI> singleton;
  return singleton.get();
}




SkipList::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  valid = false;
  itr_ = list_->set_->begin();
}

bool SkipList::Iterator::Valid() const {
  return valid;
}


const char* SkipList::Iterator::key() const {
  assert(Valid());
  return (*itr_).data();
}


void SkipList::Iterator::Next() {
  assert(Valid());
  itr_++;
  if(itr_==list_->set_->end()) {
      valid=false;
  }
}


void SkipList::Iterator::Prev() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  if(itr_!=list_->set_->begin()) {
    itr_--;
  } else {
    valid=false;
  }
}

void SkipList::Iterator::Seek(const char * target) {
  SetCmp a;
  uint32_t len;
  GetVarint32Ptr(target, target + 5, &len);
  Slice b(target, len);
  while(a(*itr_, b)) {
    itr_++;
    if(itr_==list_->set_->end()) {
      itr_--;
      break;
    }
  }
}

void SkipList::Iterator::SeekToFirst() {
  if(list_->set_->empty()) {
    return;
  }
  itr_=list_->set_->begin();
  valid = true;
}

void SkipList::Iterator::SeekToLast() {
  if(list_->set_->empty()) {
    return;
  }
  itr_=list_->set_->end();
  itr_--;
  valid = true;
}
SkipList::SkipList(Arena* arena)
    : arena_(arena) {
        set_=new std::set<Slice, SetCmp>();
      }


void SkipList::Insert(const Slice key) {
  /**
  // TODO(opt): We can use a barrier-free variant of FindGreaterOrEqual()
  // here since Insert() is externally synchronized.
  Node* prev[kMaxHeight];
  Node* x = FindGreaterOrEqual(key, prev);

  // Our data structure does not allow duplicate insertion
  assert(x == nullptr || !Equal(key, x->key));

  int height = RandomHeight();
  if (height > GetMaxHeight()) {
    for (int i = GetMaxHeight(); i < height; i++) {
      prev[i] = head_;
    }
    // It is ok to mutate max_height_ without any synchronization
    // with concurrent readers.  A concurrent reader that observes
    // the new value of max_height_ will see either the old value of
    // new level pointers from head_ (nullptr), or a new value set in
    // the loop below.  In the former case the reader will
    // immediately drop to the next level since nullptr sorts after all
    // keys.  In the latter case the reader will use the new node.
    max_height_.store(height, std::memory_order_relaxed);
  }

  x = NewNode(key, height);
  for (int i = 0; i < height; i++) {
    // NoBarrier_SetNext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, x);
  }
  */
  set_->insert(key);
}


bool SkipList::Contains(const Slice& key) const {
  /**
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
  */
  return set_->count(key)!=0;
}
}