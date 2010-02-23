/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#ifndef MESSAGE_FILTERS_TIME_SYNCHRONIZER_H
#define MESSAGE_FILTERS_TIME_SYNCHRONIZER_H

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/signals.hpp>
#include <boost/bind.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/noncopyable.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/is_nonmember_callable_builtin.hpp>

#include <roslib/Header.h>

#include "connection.h"
#include "signal9.h"
#include <ros/message_traits.h>
#include <ros/message_event.h>


namespace message_filters
{

namespace ft = boost::function_types;
namespace mpl = boost::mpl;

struct NullType
{
};
typedef boost::shared_ptr<NullType const> NullTypeConstPtr;

template<class M>
struct NullFilter
{
  template<typename C>
  Connection registerCallback(const C& callback)
  {
    return Connection();
  }

  template<typename P>
  Connection registerCallback(const boost::function<void(P)>& callback)
  {
    return Connection();
  }
};
}

namespace ros
{
namespace message_traits
{
template<>
struct TimeStamp<message_filters::NullType>
{
  static ros::Time value(const message_filters::NullType&)
  {
    return ros::Time();
  }
};
}
}

namespace message_filters
{

/**
 * \brief Synchronizes up to 9 messages by their timestamps.
 *
 * TimeSynchronizer synchronizes up to 9 incoming channels by the timestamps contained in their messages' headers.
 * TimeSynchronizer takes anywhere from 2 to 9 message types as template parameters, and passes them through to a
 * callback which takes a shared pointer of each.
 *
 * The required queue size parameter when constructing the TimeSynchronizer tells it how many sets of messages it should
 * store (by timestamp) while waiting for messages to arrive and complete their "set"
 *
 * \section connections CONNECTIONS
 *
 * The input connections for the TimeSynchronizer object is the same signature as for roscpp subscription callbacks, ie.
\verbatim
void callback(const boost::shared_ptr<M const>&);
\endverbatim
 * The output connection for the TimeSynchronizer object is dependent on the number of messages being synchronized.  For
 * a 3-message synchronizer for example, it would be:
\verbatim
void callback(const boost::shared_ptr<M0 const>&, const boost::shared_ptr<M1 const>&, const boost::shared_ptr<M2 const>&);
\endverbatim
 * \section usage USAGE
 * Example usage would be:
\verbatim
TimeSynchronizer<sensor_msgs::CameraInfo, sensor_msgs::Image, sensor_msgs::Image> sync(caminfo_sub, limage_sub, rimage_sub, 3);
sync.registerCallback(callback);
\endverbatim

 * The callback is then of the form:
\verbatim
void callback(const sensor_msgs::CameraInfo::ConstPtr&, const sensor_msgs::Image::ConstPtr&, const sensor_msgs::Image::ConstPtr&);
\endverbatim
 *
 */
template<class M0, class M1, class M2 = NullType, class M3 = NullType, class M4 = NullType,
         class M5 = NullType, class M6 = NullType, class M7 = NullType, class M8 = NullType>
class TimeSynchronizer : public boost::noncopyable
{
public:
  typedef boost::shared_ptr<M0 const> M0ConstPtr;
  typedef boost::shared_ptr<M1 const> M1ConstPtr;
  typedef boost::shared_ptr<M2 const> M2ConstPtr;
  typedef boost::shared_ptr<M3 const> M3ConstPtr;
  typedef boost::shared_ptr<M4 const> M4ConstPtr;
  typedef boost::shared_ptr<M5 const> M5ConstPtr;
  typedef boost::shared_ptr<M6 const> M6ConstPtr;
  typedef boost::shared_ptr<M7 const> M7ConstPtr;
  typedef boost::shared_ptr<M8 const> M8ConstPtr;
  typedef ros::MessageEvent<M0 const> M0Event;
  typedef ros::MessageEvent<M1 const> M1Event;
  typedef ros::MessageEvent<M2 const> M2Event;
  typedef ros::MessageEvent<M3 const> M3Event;
  typedef ros::MessageEvent<M4 const> M4Event;
  typedef ros::MessageEvent<M5 const> M5Event;
  typedef ros::MessageEvent<M6 const> M6Event;
  typedef ros::MessageEvent<M7 const> M7Event;
  typedef ros::MessageEvent<M8 const> M8Event;
  typedef boost::tuple<M0Event, M1Event, M2Event, M3Event, M4Event, M5Event, M6Event, M7Event, M8Event> Tuple;
  typedef Signal9<M0, M1, M2, M3, M4, M5, M6, M7, M8> Signal;
  typedef boost::signal<void(const Tuple&)> DropSignal;
  typedef boost::function<void(const Tuple&)> DropCallback;
  typedef const boost::shared_ptr<NullType const>& NullP;

  static const uint8_t MAX_MESSAGES = 9;

  template<class F0, class F1>
  TimeSynchronizer(F0& f0, F1& f1, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1);
  }

  template<class F0, class F1, class F2>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2);
  }

  template<class F0, class F1, class F2, class F3>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3);
  }

  template<class F0, class F1, class F2, class F3, class F4>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3, f4);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3, f4, f5);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3, f4, f5, f6);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6, class F7>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6, F7& f7, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3, f4, f5, f6, f7);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6, class F7, class F8>
  TimeSynchronizer(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6, F7& f7, F8& f8, uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
    connectInput(f0, f1, f2, f3, f4, f5, f6, f7, f8);
  }

  TimeSynchronizer(uint32_t queue_size)
  : queue_size_(queue_size)
  {
    determineRealTypeCount();
  }

  ~TimeSynchronizer()
  {
    disconnectAll();
  }

  template<class F0, class F1>
  void connectInput(F0& f0, F1& f1)
  {
    NullFilter<M2> f2;
    connectInput(f0, f1, f2);
  }

  template<class F0, class F1, class F2>
  void connectInput(F0& f0, F1& f1, F2& f2)
  {
    NullFilter<M3> f3;
    connectInput(f0, f1, f2, f3);
  }

  template<class F0, class F1, class F2, class F3>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3)
  {
    NullFilter<M4> f4;
    connectInput(f0, f1, f2, f3, f4);
  }

  template<class F0, class F1, class F2, class F3, class F4>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4)
  {
    NullFilter<M5> f5;
    connectInput(f0, f1, f2, f3, f4, f5);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5)
  {
    NullFilter<M6> f6;
    connectInput(f0, f1, f2, f3, f4, f5, f6);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6)
  {
    NullFilter<M7> f7;
    connectInput(f0, f1, f2, f3, f4, f5, f6, f7);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6, class F7>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6, F7& f7)
  {
    NullFilter<M8> f8;
    connectInput(f0, f1, f2, f3, f4, f5, f6, f7, f8);
  }

  template<class F0, class F1, class F2, class F3, class F4, class F5, class F6, class F7, class F8>
  void connectInput(F0& f0, F1& f1, F2& f2, F3& f3, F4& f4, F5& f5, F6& f6, F7& f7, F8& f8)
  {
    disconnectAll();

    input_connections_[0] = f0.registerCallback(boost::function<void(const ros::MessageEvent<M0 const>&)>(boost::bind(&TimeSynchronizer::cb0, this, _1)));
    input_connections_[1] = f1.registerCallback(boost::function<void(const ros::MessageEvent<M1 const>&)>(boost::bind(&TimeSynchronizer::cb1, this, _1)));
    input_connections_[2] = f2.registerCallback(boost::function<void(const ros::MessageEvent<M2 const>&)>(boost::bind(&TimeSynchronizer::cb2, this, _1)));
    input_connections_[3] = f3.registerCallback(boost::function<void(const ros::MessageEvent<M3 const>&)>(boost::bind(&TimeSynchronizer::cb3, this, _1)));
    input_connections_[4] = f4.registerCallback(boost::function<void(const ros::MessageEvent<M4 const>&)>(boost::bind(&TimeSynchronizer::cb4, this, _1)));
    input_connections_[5] = f5.registerCallback(boost::function<void(const ros::MessageEvent<M5 const>&)>(boost::bind(&TimeSynchronizer::cb5, this, _1)));
    input_connections_[6] = f6.registerCallback(boost::function<void(const ros::MessageEvent<M6 const>&)>(boost::bind(&TimeSynchronizer::cb6, this, _1)));
    input_connections_[7] = f7.registerCallback(boost::function<void(const ros::MessageEvent<M7 const>&)>(boost::bind(&TimeSynchronizer::cb7, this, _1)));
    input_connections_[8] = f8.registerCallback(boost::function<void(const ros::MessageEvent<M8 const>&)>(boost::bind(&TimeSynchronizer::cb8, this, _1)));
  }

  template<typename P0, typename P1>
  Connection registerCallback(void(*callback)(P0, P1))
  {
    return registerCallback(boost::function<void(P0, P1, NullP, NullP, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, _1, _2)));
  }

  template<typename P0, typename P1, typename P2>
  Connection registerCallback(void(*callback)(P0, P1, P2))
  {
    return registerCallback(boost::function<void(P0, P1, P2, NullP, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, _1, _2, _3)));
  }

  template<typename P0, typename P1, typename P2, typename P3>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, _1, _2, _3, _4)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3, P4))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, NullP, NullP, NullP, NullP)>(boost::bind(callback, _1, _2, _3, _4, _5)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3, P4, P5))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, NullP, NullP, NullP)>(boost::bind(callback, _1, _2, _3, _4, _5, _6)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3, P4, P5, P6))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, P6, NullP, NullP)>(boost::bind(callback, _1, _2, _3, _4, _5, _6, _7)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3, P4, P5, P6, P7))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, P6, P7, NullP)>(boost::bind(callback, _1, _2, _3, _4, _5, _6, _7, _8)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  Connection registerCallback(void(*callback)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, P6, P7, P8)>(boost::bind(callback, _1, _2, _3, _4, _5, _6, _7, _8, _9)));
  }

  template<typename T, typename P0, typename P1>
  Connection registerCallback(void(T::*callback)(P0, P1), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, NullP, NullP, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, t, _1, _2)));
  }

  template<typename T, typename P0, typename P1, typename P2>
  Connection registerCallback(void(T::*callback)(P0, P1, P2), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, NullP, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, t, _1, _2, _3)));
  }

  template<typename T, typename P0, typename P1, typename P2, typename P3>
  Connection registerCallback(void(T::*callback)(P0, P1, P2, P3), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, NullP, NullP, NullP, NullP, NullP)>(boost::bind(callback, t, _1, _2, _3, _4)));
  }

  template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4>
  Connection registerCallback(void(T::*callback)(P0, P1, P2, P3, P4), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, NullP, NullP, NullP, NullP)>(boost::bind(callback, t, _1, _2, _3, _4, _5)));
  }

  template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
  Connection registerCallback(void(T::*callback)(P0, P1, P2, P3, P4, P5), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, NullP, NullP, NullP)>(boost::bind(callback, t, _1, _2, _3, _4, _5, _6)));
  }

  template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  Connection registerCallback(void(T::*callback)(P0, P1, P2, P3, P4, P5, P6), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, P6, NullP, NullP)>(boost::bind(callback, t, _1, _2, _3, _4, _5, _6, _7)));
  }

  template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  Connection registerCallback(void(T::*callback)(P0, P1, P2, P3, P4, P5, P6, P7), T* t)
  {
    return registerCallback(boost::function<void(P0, P1, P2, P3, P4, P5, P6, P7, NullP)>(boost::bind(callback, t, _1, _2, _3, _4, _5, _6, _7, _8)));
  }

  template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  Connection registerCallback(const boost::function<void(P0, P1, P2, P3, P4, P5, P6, P7, P8)>& callback)
  {
    typename CallbackHelper9<M0, M1, M2, M3, M4, M5, M6, M7, M8>::Ptr helper =
        signal_.template addCallback<P0, P1, P2, P3, P4, P5, P6, P7, P8>(callback);

    return Connection(boost::bind(&Signal::removeCallback, &signal_, helper));
  }

  template<class C>
  Connection registerCallback(const C& callback)
  {
    typename CallbackHelper9<M0, M1, M2, M3, M4, M5, M6, M7, M8>::Ptr helper =
        signal_.template addCallback<const M0ConstPtr&,
                                     const M1ConstPtr&,
                                     const M2ConstPtr&,
                                     const M3ConstPtr&,
                                     const M4ConstPtr&,
                                     const M5ConstPtr&,
                                     const M6ConstPtr&,
                                     const M7ConstPtr&,
                                     const M8ConstPtr&>(boost::bind(callback, _1, _2, _3, _4, _5, _6, _7, _8, _9));

    return Connection(boost::bind(&Signal::removeCallback, &signal_, helper));
  }

  /**
   * \brief Register a callback to be called whenever a set of messages is removed from our queue
   *
   * The drop callback takes the form:
\verbatim
void callback(const TimeSynchronizer<M0, M1,...>::Tuple& tuple);
\endverbatim
   */
  Connection registerDropCallback(const DropCallback& callback)
  {
    boost::mutex::scoped_lock lock(drop_signal_mutex_);
    return Connection(boost::bind(&TimeSynchronizer::disconnectDrop, this, _1), drop_signal_.connect(callback));
  }

  void add0(const M0ConstPtr& msg)
  {
    add0(M0Event(msg));
  }

  void add1(const M1ConstPtr& msg)
  {
    add1(M1Event(msg));
  }

  void add2(const M2ConstPtr& msg)
  {
    add2(M2Event(msg));
  }

  void add3(const M3ConstPtr& msg)
  {
    add3(M3Event(msg));
  }

  void add4(const M4ConstPtr& msg)
  {
    add4(M4Event(msg));
  }

  void add5(const M5ConstPtr& msg)
  {
    add5(M5Event(msg));
  }

  void add6(const M6ConstPtr& msg)
  {
    add6(M6Event(msg));
  }

  void add7(const M7ConstPtr& msg)
  {
    add7(M7Event(msg));
  }

  void add8(const M8ConstPtr& msg)
  {
    add8(M8Event(msg));
  }

  void add0(const M0Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M0>::value(*evt.getMessage())];
    boost::get<0>(t) = evt;

    checkTuple(t);
  }

  void add1(const M1Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M1>::value(*evt.getMessage())];
    boost::get<1>(t) = evt;

    checkTuple(t);
  }

  void add2(const M2Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M2>::value(*evt.getMessage())];
    boost::get<2>(t) = evt;

    checkTuple(t);
  }

  void add3(const M3Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M3>::value(*evt.getMessage())];
    boost::get<3>(t) = evt;

    checkTuple(t);
  }

  void add4(const M4Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M4>::value(*evt.getMessage())];
    boost::get<4>(t) = evt;

    checkTuple(t);
  }

  void add5(const M5Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M5>::value(*evt.getMessage())];
    boost::get<5>(t) = evt;

    checkTuple(t);
  }

  void add6(const M6Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M6>::value(*evt.getMessage())];
    boost::get<6>(t) = evt;

    checkTuple(t);
  }

  void add7(const M7Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M7>::value(*evt.getMessage())];
    boost::get<7>(t) = evt;

    checkTuple(t);
  }

  void add8(const M8Event& evt)
  {
    namespace mt = ros::message_traits;

    boost::mutex::scoped_lock lock(tuples_mutex_);

    Tuple& t = tuples_[mt::TimeStamp<M8>::value(*evt.getMessage())];
    boost::get<8>(t) = evt;

    checkTuple(t);
  }

  void setName(const std::string& name) { name_ = name; }
  const std::string& getName() { return name_; }

private:

  void disconnectAll()
  {
    for (int i = 0; i < MAX_MESSAGES; ++i)
    {
      input_connections_[i].disconnect();
    }
  }

  void determineRealTypeCount()
  {
    real_type_count_ = 2;

    if (!boost::is_same<M2, NullType>::value)
    {
      ++real_type_count_;

      if (!boost::is_same<M3, NullType>::value)
      {
        ++real_type_count_;

        if (!boost::is_same<M4, NullType>::value)
        {
          ++real_type_count_;

          if (!boost::is_same<M5, NullType>::value)
          {
            ++real_type_count_;

            if (!boost::is_same<M6, NullType>::value)
            {
              ++real_type_count_;

              if (!boost::is_same<M7, NullType>::value)
              {
                ++real_type_count_;

                if (!boost::is_same<M8, NullType>::value)
                {
                  ++real_type_count_;
                }
              }
            }
          }
        }
      }
    }
  }

  void cb0(const M0Event& evt)
  {
    add0(evt);
  }

  void cb1(const M1Event& evt)
  {
    add1(evt);
  }

  void cb2(const M2Event& evt)
  {
    add2(evt);
  }

  void cb3(const M3Event& evt)
  {
    add3(evt);
  }

  void cb4(const M4Event& evt)
  {
    add4(evt);
  }

  void cb5(const M5Event& evt)
  {
    add5(evt);
  }

  void cb6(const M6Event& evt)
  {
    add6(evt);
  }

  void cb7(const M7Event& evt)
  {
    add7(evt);
  }

  void cb8(const M8Event& evt)
  {
    add8(evt);
  }

  // assumes tuples_mutex_ is already locked
  void checkTuple(Tuple& t)
  {
    namespace mt = ros::message_traits;

    bool full = true;
    full &= (bool)boost::get<0>(t).getMessage();
    full &= (bool)boost::get<1>(t).getMessage();
    full &= real_type_count_ > 2 ? (bool)boost::get<2>(t).getMessage() : true;
    full &= real_type_count_ > 3 ? (bool)boost::get<3>(t).getMessage() : true;
    full &= real_type_count_ > 4 ? (bool)boost::get<4>(t).getMessage() : true;
    full &= real_type_count_ > 5 ? (bool)boost::get<5>(t).getMessage() : true;
    full &= real_type_count_ > 6 ? (bool)boost::get<6>(t).getMessage() : true;
    full &= real_type_count_ > 7 ? (bool)boost::get<7>(t).getMessage() : true;
    full &= real_type_count_ > 8 ? (bool)boost::get<8>(t).getMessage() : true;

    if (full)
    {
      signal_.call(boost::get<0>(t), boost::get<1>(t), boost::get<2>(t),
                   boost::get<3>(t), boost::get<4>(t), boost::get<5>(t),
                   boost::get<6>(t), boost::get<7>(t), boost::get<8>(t));

      last_signal_time_ = mt::TimeStamp<M0>::value(*boost::get<0>(t).getMessage());

      tuples_.erase(last_signal_time_);

      clearOldTuples();
    }

    if (queue_size_ > 0)
    {
      boost::mutex::scoped_lock lock(drop_signal_mutex_);
      while (tuples_.size() > queue_size_)
      {
        drop_signal_(tuples_.begin()->second);
        tuples_.erase(tuples_.begin());
      }
    }
  }

  // assumes tuples_mutex_ is already locked
  void clearOldTuples()
  {
    boost::mutex::scoped_lock lock(drop_signal_mutex_);

    typename M_TimeToTuple::iterator it = tuples_.begin();
    typename M_TimeToTuple::iterator end = tuples_.end();
    for (; it != end;)
    {
      if (it->first <= last_signal_time_)
      {
        typename M_TimeToTuple::iterator old = it;
        ++it;

        drop_signal_(old->second);
        tuples_.erase(old);
      }
      else
      {
        // the map is sorted by time, so we can ignore anything after this if this one's time is ok
        break;
      }
    }
  }

  void disconnectDrop(const Connection& c)
  {
    boost::mutex::scoped_lock lock(drop_signal_mutex_);
    c.getBoostConnection().disconnect();
  }

  uint32_t queue_size_;

  typedef std::map<ros::Time, Tuple> M_TimeToTuple;
  M_TimeToTuple tuples_;
  boost::mutex tuples_mutex_;

  Signal signal_;
  ros::Time last_signal_time_;

  boost::mutex drop_signal_mutex_;
  DropSignal drop_signal_;

  Connection input_connections_[MAX_MESSAGES];

  uint32_t real_type_count_;

  std::string name_;
};

}

#endif // MESSAGE_FILTERS_TIME_SYNCHRONIZER_H