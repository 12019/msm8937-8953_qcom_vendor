// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <memory>

#include <mare/internal/legacy/task.hh>
#include <mare/internal/legacy/types.hh>
#include <mare/internal/synchronization/semaphore.hh>

namespace mare{

namespace internal{

class exectx;

class scheduler;

class blocked_task{
protected:
  task_shared_ptr _stub_task;

public:
  blocked_task() : _stub_task(nullptr){
  }

  explicit blocked_task(task_shared_ptr stub_task) : _stub_task(stub_task){
  }

  MARE_DELETE_METHOD(blocked_task(blocked_task&));
  MARE_DELETE_METHOD(blocked_task(blocked_task&&));
  MARE_DELETE_METHOD(blocked_task& operator=(blocked_task&));
  MARE_DELETE_METHOD(blocked_task& operator=(blocked_task&&));

  virtual ~blocked_task(){
  }

  virtual void wakeup(){

    legacy::launch(_stub_task);
  }
};

class blocked_mare_task : public blocked_task {
private:
  scheduler* _sched;
  scheduler* _this_sched;

public:
  blocked_mare_task(scheduler& sched, task_shared_ptr stub_task,
                    scheduler& this_sched) :
    blocked_task(stub_task), _sched(&sched), _this_sched(&this_sched) {
  }

  MARE_DELETE_METHOD(blocked_mare_task(blocked_mare_task&));
  MARE_DELETE_METHOD(blocked_mare_task(blocked_mare_task&&));
  MARE_DELETE_METHOD(blocked_mare_task& operator=(blocked_mare_task&));
  MARE_DELETE_METHOD(blocked_mare_task& operator=(blocked_mare_task&&));

  virtual ~blocked_mare_task(){
  }

  void block(exectx& cur_ctx);

  void cleanup();
};

class blocked_blocking_task : public blocked_task{
private:
  std::shared_ptr<semaphore> _sem;
public:
  blocked_blocking_task() : _sem(std::make_shared<semaphore>()){
    _stub_task = create_stub_task([this] {
        _sem->signal();
      });
  }

  blocked_blocking_task(task_shared_ptr stub_task, std::shared_ptr<semaphore> sem) :
    blocked_task(stub_task), _sem(sem){
  }

  MARE_DELETE_METHOD(blocked_blocking_task(blocked_blocking_task&));
  MARE_DELETE_METHOD(blocked_blocking_task(blocked_blocking_task&&));
  MARE_DELETE_METHOD(blocked_blocking_task& operator=(blocked_blocking_task&));
  MARE_DELETE_METHOD(blocked_blocking_task&
                     operator=(blocked_blocking_task&&));

  virtual ~blocked_blocking_task(){
  }

  void block();
};

class blocked_timed_task : public blocked_task {
private:
  std::atomic<int> * _state;

public:
  blocked_timed_task() : _state(new std::atomic<int>()){
    *_state = 0;
  }

  MARE_DELETE_METHOD(blocked_timed_task(blocked_timed_task&));
  MARE_DELETE_METHOD(blocked_timed_task(blocked_timed_task&&));
  MARE_DELETE_METHOD(blocked_timed_task& operator=(blocked_timed_task&));
  MARE_DELETE_METHOD(blocked_timed_task& operator=(blocked_timed_task&&));

  virtual ~blocked_timed_task(){
  }

  virtual void wakeup();

  std::atomic<int> * get_state()
  {
    return _state;
  }
};

};

};
