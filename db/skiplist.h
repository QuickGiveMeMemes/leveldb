// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...

#include <atomic>
#include <cstdlib>
#include <set>
#include <iterator>

#include "util/arena.h"
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "leveldb/export.h"
#include "util/coding.h"

namespace leveldb {

class DefaultCmpI : public Comparator {
    public:
        DefaultCmpI();
        const char* Name() const override;
        int Compare(const Slice& a, const Slice& b) const override;
        void FindShortestSeparator(std::string* start, const Slice& limit) const override;
        void FindShortSuccessor(std::string* key) const override;
};
LEVELDB_EXPORT const Comparator* DefaultCmp();

struct SetCmp {
  bool operator() (Slice a, Slice b) const {
    uint32_t lenA,lenB;
    const char * x = a.data();
    const char * y = b.data();
    x = GetVarint32Ptr(x,x+5,&lenA);
    y = GetVarint32Ptr(y,y+5,&lenB);
    Slice A(x,lenA);
    Slice B(y,lenB);
    int i=A.compare(B);
    if(A.compare(B)<0) {
        return true;
    }
    return false;
  }
};


class SkipList {
 private:
  std::set<Slice, SetCmp>* set_;
  Arena* const arena_;

 public:
  // Create a new SkipList object that will use "cmp" for comparing keys,
  // and will allocate memory using "*arena".  Objects allocated in the arena
  // must remain allocated for the lifetime of the skiplist object.
  explicit SkipList(Arena* arena);

  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  // Insert key into the list.
  // REQUIRES: nothing that compares equal to key is currently in the list.
  void Insert(const Slice key);

  // Returns true iff an entry that compares equal to key is in the list.
  bool Contains(const Slice& key) const;

  // Iteration over the contents of a skip list
  class Iterator {
   public:
    // Initialize an iterator over the specified list.
    // The returned iterator is not valid.
    explicit Iterator(const SkipList* list);

    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const;

    // Returns the key at the current position.
    // REQUIRES: Valid()
    const char* key() const;

    // Advances to the next position.
    // REQUIRES: Valid()
    void Next();

    // Advances to the previous position.
    // REQUIRES: Valid()
    void Prev();

    // Advance to the first entry with a key >= target
    void Seek(const char * target);

    // Position at the first entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToFirst();

    // Position at the last entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToLast();

   private:
    const SkipList* list_;
    typename std::set<Slice,SetCmp>::iterator itr_;
    bool valid;
    // Intentionally copyable
  };
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_SKIPLIST_H_
