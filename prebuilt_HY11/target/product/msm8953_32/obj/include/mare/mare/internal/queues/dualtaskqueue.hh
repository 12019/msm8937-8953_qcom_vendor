// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_set>

#include <mare/internal/atomic/alignedatomic.hh>
#include <mare/internal/queues/hazardpointers.hh>
#include <mare/internal/synchronization/mutex_cv.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>

#ifdef MARE_DQ_DEBUG_ELEMENTS
#define MARE_DQ_DEBUG
#endif

#define MARE_CPTR_DELETEME_PTR (reinterpret_cast<qnode*>(0x1))
#define MARE_CPTR_CLAIMDELETION_PTR (reinterpret_cast<qnode*>(0x2))

namespace mare {

namespace internal {

template <typename T>
class DualTaskQueue {
public:
  struct cptr;
  struct ctptr;
  struct qnode;
  typedef hp::Record<qnode,2> HPRecord;

public:

  DualTaskQueue():
#ifdef MARE_DQ_DEBUG
    _count(0),
    _waiting(0),
    _count_push_fastPass(0),
    _count_push_slowPass(0),
    _count_pop_fastPass(0),
    _count_pop_slowPass(0),
#endif
    _head(),
    _tail(),
    _mutex(),
    _HPManager()
  {

    qnode* qn = new qnode();
    MARE_INTERNAL_ASSERT(qn, "Node is null");

    head().storeNA(cptr(qn));
    tail().storeNA(ctptr(qn,false));
#ifdef MARE_DQ_DEBUG

    qn->_dataObtainedFast = true;
#endif
  }

  virtual ~DualTaskQueue() {
    qnode* node = head().load(mare::mem_order_relaxed).ptr();
    while(node) {
      qnode* delNode = node;
      node = node->_next.load(mare::mem_order_relaxed).ptr();
      delete delNode;
    }
  }

  void push(T element, bool do_notify = true) {

    qnode* node = new qnode(element);
    MARE_INTERNAL_ASSERT(node, "Node is null");
    MARE_INTERNAL_ASSERT(
      node->_next.load(mare::mem_order_relaxed).ptr()==nullptr,
      "node init failure");
    MARE_INTERNAL_ASSERT(
      node->_request.load(mare::mem_order_relaxed).ptr()==nullptr,
      "node init failure");

#ifdef MARE_DQ_DEBUG
    typename hp::Manager<qnode,2>::CheckCleanOnDestroy checkClean(_HPManager);
#endif

    while (1) {
      ctptr myTail = tail().load(mare::mem_order_relaxed);
      cptr myHead = head().load(mare::mem_order_relaxed);

      if ((myTail.ptr() == myHead.ptr()) || !myTail.isRequest()) {

        _HPManager.setMySlot(0,myTail.ptr());

        if(myTail != tail().load(mare::mem_order_relaxed)) continue;

        if(myHead != head().load(mare::mem_order_relaxed)) continue;

#ifdef MARE_DQ_DEBUG
        incrRefCount(myTail.ptr(), __LINE__);
#endif

        ctptr next = myTail.ptr()->_next.load(mare::mem_order_relaxed);

        if (true) {

          if (next.ptr() != nullptr) {

            (void) cas(tail(), myTail, ctptr(next.ptr(), next.isRequest()));
          } else {
            if (cas(myTail.ptr()->_next, next, ctptr(node, false))) {
              (void) cas(tail(), myTail, ctptr(node, false));
#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif
#ifdef MARE_DQ_DEBUG
              _count_push_fastPass++;
#endif
#ifdef MARE_DQ_DEBUG
              decrRefCount(myTail.ptr(), __LINE__);
#endif

              _HPManager.clearMySlot(0);

              return;
            }
          }
        }
#ifdef MARE_DQ_DEBUG
        decrRefCount(myTail.ptr(), __LINE__);
#endif
      } else {

        _HPManager.setMySlot(0,myHead.ptr());

        if(myHead != head().load(mare::mem_order_relaxed)) continue;

#ifdef MARE_DQ_DEBUG
        incrRefCount(myHead.ptr(), __LINE__);
#endif

        ctptr next = myHead.ptr()->_next.load(mare::mem_order_relaxed);

        if (myTail == tail().load(mare::mem_order_relaxed) &&
            next.ptr() != nullptr) {

          cptr req = myHead.ptr()->_request.load(mare::mem_order_relaxed);
          if (myHead == head().load(mare::mem_order_relaxed)) {

            bool success = (req.ptr() == nullptr &&
                            cas_strong(myHead.ptr()->_request,
                                       req, cptr(node)));

#ifdef MARE_DQ_DEBUG
            MARE_INTERNAL_ASSERT(node, "Node is null");
            verifyQNode(myHead.ptr(), __LINE__);
            if ((myHead.ptr()->
                 _request.load(mare::mem_order_relaxed).ptr() ==
                 nullptr)) {
              std::stringstream s;
              _HPManager.print(s);
              MARE_FATAL("Fulfilling request failed! head:%p "
                         "success:%d, req:%p\n%s",
                         myHead.ptr(),success, req.ptr(), s.str().c_str());
            }
            MARE_INTERNAL_ASSERT(next.ptr(), "tried to set head() to null!");
#endif

            (void)advanceHeadStrong(myHead, next);

            if (success) {
#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif
#ifdef MARE_DQ_DEBUG
              if(!next.isRequest()) {
                MARE_FATAL("linked data to node that is not a request! "
                           "DATA LOST!");
              }
#endif

              if (do_notify)
                notify(myHead.ptr());
#ifdef MARE_DQ_DEBUG
              _count_push_slowPass++;
#endif
#ifdef MARE_DQ_DEBUG
              decrRefCount(myHead.ptr(), __LINE__);
#endif

              _HPManager.clearMySlot(0);

              return;

            } else {
              req = myHead.ptr()->_request.load(mare::mem_order_relaxed);

              if(req.ptr() == MARE_CPTR_DELETEME_PTR &&
                 cas_strong(myHead.ptr()->_request, req,
                            cptr(MARE_CPTR_CLAIMDELETION_PTR))) {
#ifdef MARE_DQ_DEBUG
                myHead.ptr()->_deletedMark = 1;
                myHead.ptr()->_deletedThread = thread_id();
#endif

#ifdef MARE_DQ_DEBUG
                decrRefCount(myHead.ptr(), __LINE__);
#endif

                _HPManager.clearMySlot(0);

#ifdef MARE_DQ_DEBUG
                verifyQNode(myHead.ptr(), __LINE__);
#endif
                _HPManager.retireNode(myHead.ptr());

              } else {
#ifdef MARE_DQ_DEBUG
                decrRefCount(myHead.ptr(), __LINE__);
#endif
              }
            }

          } else {
#ifdef MARE_DQ_DEBUG
            decrRefCount(myHead.ptr(), __LINE__);
#endif
          }

        } else {
#ifdef MARE_DQ_DEBUG
          decrRefCount(myHead.ptr(), __LINE__);
#endif
        }
      }
    }
  }

  template <typename Predicate>
  bool popIf(Predicate pred, T& element, paired_mutex_cv* synch) {

    qnode* node = new qnode();
    MARE_INTERNAL_ASSERT(node, "node init failure");

    node->_next.storeNA(ctptr(nullptr, true));

#ifdef MARE_DQ_DEBUG
    typename hp::Manager<qnode,2>::CheckCleanOnDestroy checkClean(_HPManager);
#endif

    while (1) {
      cptr myHead = head().load(mare::mem_order_relaxed);

      _HPManager.setMySlot(0,myHead.ptr());

      if(myHead != head().load(mare::mem_order_relaxed)) continue;

#ifdef MARE_DQ_DEBUG
      incrRefCount(myHead.ptr(), __LINE__);
#endif

      ctptr myTail = tail().load(mare::mem_order_relaxed);

      if ((myTail.ptr() == myHead.ptr()) || myTail.isRequest()) {

        _HPManager.setMySlot(1,myTail.ptr());

        if(myTail != tail().load(mare::mem_order_relaxed)) {
#ifdef MARE_DQ_DEBUG
          decrRefCount(myHead.ptr(), __LINE__);
#endif

          continue;
        }

#ifdef MARE_DQ_DEBUG
        incrRefCount(myTail.ptr(), __LINE__);
#endif

        ctptr next = myTail.ptr()->_next.load(mare::mem_order_relaxed);
        if(true) {

          if (next.ptr() != nullptr) {
            (void) cas(tail(), myTail, ctptr(next.ptr(), next.isRequest()));
          } else {
            if (cas(myTail.ptr()->_next, next, ctptr(node, true))) {

              myTail.ptr()->init_synch(synch);

              (void) cas(tail(), myTail, ctptr(node, true));

              if (myHead == head().load(mare::mem_order_relaxed) &&
                  myHead.ptr()->
                  _request.load(mare::mem_order_relaxed).ptr() != nullptr) {
                (void)advanceHead(myHead,
                                  myHead.ptr()->_next.load(
                                    mare::mem_order_relaxed));
              }

              cptr req(MARE_CPTR_DELETEME_PTR);

              bool dataOK = true;

              {
                std::unique_lock<std::mutex> lock(myTail.ptr()->_synch->first);
                while(1) {

                  bool empty =
                    myTail.ptr()->
                    _request.load(mare::mem_order_relaxed).ptr() == nullptr;

                  if(pred() && empty) {
#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif

#ifdef MARE_DQ_DEBUG
                    if(myTail.ptr()->_waiter) {
                      MARE_FATAL("Already someone waiting on node!");
                    }
                    if(myTail.ptr()->_dataObtained) {
                      MARE_FATAL("Data already obtained! (line:%d)",__LINE__);
                    }
                    if(myTail.ptr()->_deleted) {
                      MARE_FATAL("Node deleted!");
                    }
                    myTail.ptr()->_waiter = thread_id();
#endif
                    myTail.ptr()->_synch->second.wait(lock);
#ifdef MARE_DQ_DEBUG
                    MARE_INTERNAL_ASSERT(
                      myTail.ptr()->_waiter == thread_id(),
                      "For some reason this is not the node I "
                      "was waiting on!");
                    myTail.ptr()->_waiter = {0};
#endif

#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif

                  } else if(empty) {

                    req = myTail.ptr()->_request.load(
                            mare::mem_order_relaxed);
                    if(req.ptr()==nullptr) {
                      dataOK = !cas_strong(myTail.ptr()->_request, req,
                                           cptr(MARE_CPTR_DELETEME_PTR));
                    }
                    break;

                  } else {
                    break;
                  }
                }
              }

              qnode* dataNode = dataOK ?
                (myTail.ptr()->
                 _request.load(mare::mem_order_relaxed).ptr()) :
                nullptr;

#ifdef MARE_DQ_DEBUG
              _count_pop_slowPass++;
#endif

              if(dataNode>MARE_CPTR_CLAIMDELETION_PTR) {
#ifdef MARE_DQ_DEBUG
                bool b = false;
                if(!myTail.ptr()->
                   _dataObtained.compare_exchange_strong(b,true)) {
                  MARE_FATAL("Data already obtained! (line:%d)", __LINE__);
                }
#endif

                element = dataNode->_element;

#ifdef MARE_DQ_DEBUG
                cptr origHead = myHead;
#endif

                myHead = head().load(mare::mem_order_relaxed);
                if (myHead.ptr() == myTail.ptr()) {

                  (void)advanceHeadStrong(myHead, cptr(node));
                }

                delete dataNode;

#ifdef MARE_DQ_DEBUG
                myTail.ptr()->_deletedMark = 2;
                myTail.ptr()->_deletedThread = thread_id();
                decrRefCount(myTail.ptr(), __LINE__);
                decrRefCount(origHead.ptr(), __LINE__);
#endif

                _HPManager.clearMySlot(0);
                _HPManager.clearMySlot(1);

#ifdef MARE_DQ_DEBUG
                verifyQNode(myTail.ptr(), __LINE__);
#endif
                _HPManager.retireNode(myTail.ptr());

#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif

                return true;
              } else {

#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif
#ifdef MARE_DQ_DEBUG
                decrRefCount(myTail.ptr(), __LINE__);
                decrRefCount(myHead.ptr(), __LINE__);
#endif

                _HPManager.clearMySlot(0);
                _HPManager.clearMySlot(1);

                return false;
              }
            }
          }
        }
#ifdef MARE_DQ_DEBUG
        decrRefCount(myTail.ptr(), __LINE__);
#endif
      } else {

        ctptr next = myHead.ptr()->_next.load(mare::mem_order_relaxed);

        _HPManager.setMySlot(1,next.ptr());

        if (myHead == head().load(mare::mem_order_relaxed) && myTail ==
            tail().load(mare::mem_order_relaxed) && next.ptr() != nullptr) {

          MARE_INTERNAL_ASSERT(
            next.ptr() != nullptr,
            "next should not be null");

#ifdef MARE_DQ_DEBUG
          incrRefCount(next.ptr(), __LINE__);
#endif

          if (advanceHead(myHead, next)) {

#ifdef MARE_DQ_DEBUG
            MARE_INTERNAL_ASSERT(
              myHead.ptr()->_request.load(mare::mem_order_relaxed)._ptr <=
              MARE_CPTR_CLAIMDELETION_PTR,
              "Dequeuer snipped a data node WITH a data node!");
            MARE_INTERNAL_ASSERT(
              !next.isRequest(),
              "Dequeur snipped a request node");
            verifyQNode(next.ptr(), __LINE__);
#endif

#ifdef MARE_DQ_DEBUG
            myHead.ptr()->_deletedMark = 3;
            myHead.ptr()->_deletedThread = thread_id();
            decrRefCount(myHead.ptr(), __LINE__);
#endif

            _HPManager.clearMySlot(0);

#ifdef MARE_DQ_DEBUG
            verifyQNode(myHead.ptr(), __LINE__);
#endif
            _HPManager.retireNode(myHead.ptr());

            delete node;

#ifdef MARE_DQ_DEBUG_ELEMENTS

#endif
#ifdef MARE_DQ_DEBUG
            _count_pop_fastPass++;
#endif
#ifdef MARE_DQ_DEBUG
            bool b = false;
            if(!next.ptr()->
               _dataObtainedFast.compare_exchange_strong(b,true)) {
              MARE_FATAL("Data already obtained! (line:%d)",__LINE__);
            }
#endif

            element = next.ptr()->_element;

#ifdef MARE_DQ_DEBUG
            decrRefCount(next.ptr(), __LINE__);
#endif

            _HPManager.clearMySlot(1);

            return true;
          }
#ifdef MARE_DQ_DEBUG
          decrRefCount(next.ptr(), __LINE__);
#endif
        }

        _HPManager.clearMySlot(1);
      }
#ifdef MARE_DQ_DEBUG
      decrRefCount(myHead.ptr(), __LINE__);
#endif
    }
  }

  T pop(paired_mutex_cv* synch)
  {
    T element;
    if (popIf([] { return true; }, element, synch))
      return element;
    return nullptr;
  }

  bool pop(T& element, paired_mutex_cv* synch)
  {
    if (popIf([] { return true; }, element, synch))
      return true;
    return false;
  }

  void notify(struct qnode* node) {
    if(node->_synch != nullptr){
      std::unique_lock<std::mutex> lock(node->_synch->first);
      node->_synch->second.notify_one();
    }
  }

  void threadInit() {
    _HPManager.threadInit();
  }

  void threadInitAuto() {
    _HPManager.threadInitAuto();
  }

  void threadFinish() {
    _HPManager.threadFinish();
  }

#ifdef MARE_DQ_DEBUG
  void verifyQNode(struct qnode* node, int line) {
    if(node->_deleted) {
      std::stringstream s;
      s << "Node should not be deleted at line " << line << "! node: " <<
        node << " del: " << node->_deleted << " mark: " <<
        node->_deletedMark << " retiredBy: " << node->_deletedThread;
      _HPManager.print(s);
      MARE_FATAL("%s",s.str().c_str());
    }
  }

  void incrRefCount(struct qnode* node, int line) {
    MARE_INTERNAL_ASSERT(
      node->_refCount>=0,
      "Reference count of node %p is negative",
      this);
    node->_refCount++;
    verifyQNode(node, line);
  }

  void decrRefCount(struct qnode* node, int line) {
    verifyQNode(node, line);
    node->_refCount--;
    MARE_INTERNAL_ASSERT(
      node->_refCount>=0,
      "Reference count of node %p is negative",
      this);
  }
#endif

public:

  struct cptr {
    struct qnode* _ptr;

    cptr() MARE_NOEXCEPT:
    _ptr(nullptr)
    {
    }

    explicit cptr(struct qnode* const& p):
      _ptr(p)
    {
    }

    cptr(const cptr& other):
      _ptr(other._ptr)
    {
    }

    inline qnode* ptr() const {
      return _ptr;
    }

    inline qnode* ptr(qnode* p) {
      _ptr = p;
      return _ptr;
    }

    bool operator==(const cptr& other) const {
      return   _ptr == other._ptr;
    }

    bool operator!=(const cptr& other) const {
      return !(*this==other);
    }

    const cptr& operator=(const cptr& other) {
      if(this!=&other) {
        this->_ptr = other._ptr;
      }
      return *this;
    }
  }
    ;

  struct ctptr {
  private:
    struct qnode* _ptr;
  public:

    static MARE_CONSTEXPR_CONST intptr_t s_flag_isrequest = 1;

    inline qnode* ptr() const {
      return reinterpret_cast<qnode*>
        (reinterpret_cast<intptr_t>(_ptr) & (~s_flag_isrequest));
    }

    inline qnode* ptr(qnode* p) {
      _ptr = reinterpret_cast<qnode*>(reinterpret_cast<intptr_t>(p) |
                                      (reinterpret_cast<intptr_t>(_ptr) &
                                       (~s_flag_isrequest)));
      return _ptr;
    }

    inline bool isRequest() const {
      return (reinterpret_cast<intptr_t>(_ptr)&s_flag_isrequest);
    }

    inline bool isRequest(bool isReq) const {
      _ptr = reinterpret_cast<qnode*>((reinterpret_cast<intptr_t>(_ptr)&(~s_flag_isrequest)) |
                                      (isReq != false));
      MARE_INTERNAL_ASSERT(isReq == this->isRequest(),
                           "bit stuffing error");
      return isReq;
    }

    ctptr() MARE_NOEXCEPT:
    _ptr(nullptr)
    {
    }

    ctptr(qnode* ptrb, bool isReq):
      _ptr(reinterpret_cast<qnode*>(reinterpret_cast<intptr_t>(ptrb) |
                                    (isReq != false)))
    {
      MARE_INTERNAL_ASSERT(isReq == this->isRequest(),
                           "bit stuffing error");
    }

    ctptr(const ctptr& other):
      _ptr(other._ptr)
    {
    }

    bool operator==(const ctptr& other) const {
      return   _ptr == other._ptr;
    }

    bool operator!=(const ctptr& other) const {
      return !(*this==other);
    }

    const ctptr& operator=(const ctptr& other) {
      if(this!=&other) {
        this->_ptr = other._ptr;
      }
      return *this;
    }
  }
    ;

  struct qnode {
    alignedatomic<cptr> _request;
    alignedatomic<ctptr> _next;
#ifdef MARE_DQ_DEBUG
    uintptr_t _waiter;
    std::atomic<bool> _dataObtained;
    std::atomic<bool> _dataObtainedFast;
    bool _deleted;
    int _deletedMark;
    uintptr_t _deletedThread;
    std::atomic<int> _refCount;
#endif
    paired_mutex_cv* _synch;
    T _element;
#ifdef MARE_DQ_DEBUG_MEMORY
    static std::atomic<size_t> _allocs;
    static std::atomic<size_t> _deletes;
#endif

    MARE_DELETE_METHOD(qnode& operator=(qnode const&));

    MARE_DELETE_METHOD(qnode(qnode const&));
    qnode():
      _request(),
      _next(),
#ifdef MARE_DQ_DEBUG
      _waiter(0),
      _dataObtained(false),
      _dataObtainedFast(false),
      _deleted(false),
      _deletedMark(0),
      _deletedThread(0),
      _refCount(0),
#endif
      _synch(nullptr),
      _element() {
    }
    explicit qnode(const T& element):
      _request(),
      _next(),
#ifdef MARE_DQ_DEBUG
      _waiter(0),
      _dataObtained(false),
      _dataObtainedFast(false),
      _deleted(false),
      _deletedMark(0),
      _deletedThread(0),
      _refCount(0),
#endif
      _synch(nullptr),
      _element(element) {
    }

#ifdef MARE_DQ_DEBUG
    ~qnode() {
      int rc = _refCount.load(mare::mem_order_relaxed);
      if(rc>0) {
        MARE_FATAL("QNode still in use, but deleted! "
                   "node: %p rc: %d mark: %i", this, rc, _deletedMark);
      } else if(rc<0) {
        MARE_FATAL("QNode has negative refCount! "
                   "node: %p rc: %d mark: %i", this, rc, _deletedMark);
      }
      if(_deleted) {
        MARE_FATAL("QNode already deleted!? Marked: %d", _deletedMark);
      }
      _deleted = true;
    }
#endif

    void init_synch(paired_mutex_cv* synch){
      if(_synch == nullptr){
        _synch = synch;
      }
    }

#ifdef MARE_DQ_DEBUG_MEMORY
    void* operator new(std::size_t size) {
      void* mem = malloc(size);
      if(mem) {
        _allocs++;
        return mem;
      } else {
        throw std::bad_alloc();
      }
    }

    void* operator new(std::size_t size,
                       const std::nothrow_t& nothrow_constant) {
      _allocs++;
      return malloc(size);
    }

    void* operator new(std::size_t size, void* ptr) {
      return ptr;
    }

    void operator delete(void* ptr) {
      MARE_INTERNAL_ASSERT(ptr, "ptr is null");
      _deletes++;
      free(ptr);
    }
    void operator delete(void* ptr,
                         const std::nothrow_t& nothrow_constant) {
      MARE_INTERNAL_ASSERT(ptr, "ptr is null");
      _deletes++;
      free(ptr);
    }
    void operator delete (void* ptr, void* voidptr2) {
    }
#endif

  };

private:

  inline bool cas_strong(alignedatomic<ctptr>& ptr, ctptr expected,
                         const ctptr& newPtr) {
    return ptr.compare_exchange_strong(expected,newPtr);
  }
  inline bool cas_strong(alignedatomic<cptr>& ptr, cptr expected,
                         const cptr& newPtr) {
    return ptr.compare_exchange_strong(expected,newPtr);
  }

  inline bool cas(alignedatomic<ctptr>& ptr, ctptr expected,
                  const ctptr& newPtr) {
    return ptr.compare_exchange_weak(expected,newPtr);

  }
  inline bool cas(alignedatomic<cptr>& ptr, cptr expected,
                  const cptr& newPtr) {
    return ptr.compare_exchange_weak(expected,newPtr);

  }

  inline alignedatomic<cptr>& head() {
    return _head;
  }

  inline alignedatomic<ctptr>& tail() {
    return _tail;
  }

  bool advanceHead(const cptr& expectedHead, const cptr& newHead) {
    return cas(head(),expectedHead,newHead);
  }
  bool advanceHead(const cptr& expectedHead, const ctptr& newHead) {
    return advanceHead(expectedHead,cptr(newHead.ptr()));
  }

  bool advanceHeadStrong(const cptr& expectedHead, const cptr& newHead) {
    return cas_strong(head(),expectedHead,newHead);
  }

  bool advanceHeadStrong(const cptr& expectedHead, const ctptr& newHead) {
    return cas_strong(head(),expectedHead,cptr(newHead.ptr()));
  }

private:
#ifdef MARE_DQ_DEBUG
public:
  std::atomic<size_t> _count;
  std::atomic<size_t> _waiting;
  std::atomic<size_t> _count_push_fastPass;
  std::atomic<size_t> _count_push_slowPass;
  std::atomic<size_t> _count_pop_fastPass;
  std::atomic<size_t> _count_pop_slowPass;
#endif
  alignedatomic<cptr> _head;
  alignedatomic<ctptr> _tail;
#if defined(MARE_DQ_OPTION_SINGLECONDVAR) || \
  defined(MARE_DQ_OPTION_EXTERNCONDVAR)
  std::condition_variable _condvar;
#endif
  std::mutex _mutex;
  hp::Manager<qnode,2> _HPManager;

};

#ifdef MARE_DQ_DEBUG_MEMORY
template<typename T>
typename std::atomic<size_t> DualTaskQueue<T>::qnode::_allocs;
template<typename T>
typename std::atomic<size_t> DualTaskQueue<T>::qnode::_deletes;
#endif

};

};
