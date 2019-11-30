// ---------------------------------------------------------------------
//
// Copyright (C) 2019 by the SampleFlow authors.
//
// This file is part of the SampleFlow library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#ifndef SAMPLEFLOW_CONSUMER_H
#define SAMPLEFLOW_CONSUMER_H

#include <sampleflow/auxiliary_data.h>
#include <sampleflow/producer.h>
#include <boost/signals2.hpp>

#include <list>
#include <utility>
#include <future>


namespace SampleFlow
{
  /**
   * This is the base class for classes that *consume* samples, i.e. react
   * in some way to a sample produced by a Producer object. Examples of
   * consumers are classes that compute the average $\left<x\right>$ over
   * the samples $x_k$ they are sent (implemented in Consumers::MeanValue),
   * the standard deviation or covariance matrix (implemented in
   * Consumers::StandardDeviation and Consumers::Covariance), or that
   * simply output each sample to a `std::ostream` (implemented in
   * Consumers::StreamOutput). A special type of consumers are classes
   * derived from the Filter class: These generally don't carry any significant
   * state around (such as the mean value of previously received samples) but
   * instead transform each sample $x_k$ into a different $y_k=f(x_k)$.
   * The Filter class has more examples on this.
   *
   * The current class has a rather minimalist interface: Its
   * connect_to_producer() member function is used to attach a Consumer
   * object to a Producer object; whenever the producer then generates
   * a sample, it indicates the value of the sample to all connected
   * consumers through a signal that in connect_to_producer() is set to
   * call the consumer() member function of this class. The consume()
   * member function is `virtual` and abstract and needs to be implemented
   * in derived classes to do whatever that derive class wants to do with
   * each sample.
   *
   *
   * ### Threading model ###
   *
   * Consumers can be attached to multiple producers (see the example in
   * the documentation of the connect_to_producer() function), and these
   * producers may be running on separate threads. As a consequence,
   * implementations of classes derived from the Consumer base class need
   * to expect that their member functions can be called from different
   * threads, and, more importantly, concurrently. Thus, it
   * is important that all member functions of derived classes use
   * appropriate strategies for dealing with concurrency. Principally,
   * this implies that all functions that access the current state of
   * their object need to use `std::mutex` and `std::lock_guard` objects
   * appropriately.
   *
   *
   * @tparam InputType The C++ type used to describe samples. For example,
   *   if one samples from a continuous, one-dimensional distribution, then
   *   an appropriate type may be `double`. If one samples from the two
   *   sides of a coin, then `bool` may be the appropriate choice.
   */
  template <typename InputType>
  class Consumer
  {
    public:
      Consumer (const bool use_separate_task = true);

      /*
       * The destructor. It disconnects this consumer object from
       * all producers it was connected to.
       */
      virtual
      ~Consumer ();

      /**
       * A member function typically called from user code to connect
       * this consumer object to a producer object. As a consequence,
       * every time the producer generates a sample, it will call an internal
       * function of the current object which in turn then calls the
       * consumer() member function that needs to be implemented in derived
       * classes.
       *
       * Note that this function can be called for multiple producers. This
       * would connect the same consumer to multiple producers -- an
       * application of this facility would be if a program were to run
       * multiple sampling algorithms in parallel on separate threads. In
       * such cases, it might still be useful to compute the mean value
       * over all samples produced by all of the sampling objects. One
       * would do this by connecting the same mean value consumer object
       * to all samplers by calling this function several times with
       * different arguments.
       *
       * @param[in] producer A reference to the producer object whose
       *   samples we want to consumer in the current object.
       */
      void
      connect_to_producer (Producer<InputType> &producer);

      /**
       * The main function of this class. It is the only function a
       * derived class needs to implement. It receives both the sample
       * and some additional information about this sample as argument.
       *
       * @param[in] sample A sample $x_k$.
       * @param[in] aux_data Additional information the producer that
       *   generated the sample may have wanted to convey along with
       *   the same value itself.
       */
      virtual
      void
      consume (InputType sample,
               AuxiliaryData aux_data) = 0;

    private:
      const bool use_separate_task;

      std::future<void> background_task;

      /**
       * A list of connections created by calling connect_to_producer().
       * We store this list so that we can terminate the connection once
       * the current object is destroyed, in order to avoid triggering
       * a slot that no longer exists if the originally connected
       * producer decides to generate a sample after the current object
       * has been destroyed.
       */
      std::list<boost::signals2::connection> connections_to_producers;
  };



  template <typename InputType>
  Consumer<InputType>::Consumer (const bool use_separate_task)
    : use_separate_task (use_separate_task)
  {}



  template <typename InputType>
  Consumer<InputType>::~Consumer ()
  {
	  // Disconnect from anything that could submit more samples
	  // to the current class.
    for (auto &connection : connections_to_producers)
      connection.disconnect ();


	  // Then wait for any still pending tasks to finish.
    if (background_task.valid())
      background_task.wait();

    // PROBLEM: When we get to the wait() above, we're in the destructor
    // of this class. This means that the destructor of the derived class
    // has already run. So if there is still a task that hasn't been
    // taken care of and that we're waiting for here, whenever the
    // run-time system gets around to scheduling it, then it will try
    // to run on an object of which only the base class is left. That's
    // not likely going to work. In practice, the derived class destructor
    // resets the vtable and the call to the `consume` function will run
    // in an pure virtual function call.
  }



  template <typename InputType>
  void
  Consumer<InputType>::
  connect_to_producer (Producer<InputType> &producer)
  {
    // Create a connection to a lambda function that in turn calls
    // the consume() member function of the current object.
    //
    // If 'use_separate_task' was not set in the communicator,
    // then the lambda function simply calls the 'consume()'
    // function that derived classes need to implement.
    if (use_separate_task == false)
      {
        connections_to_producers.push_back (
          producer.connect_to_signal (
            [&](InputType sample, AuxiliaryData aux_data)
        {
          this->consume (std::move(sample), std::move(aux_data));
        }));
      }
    else
      // On the other hand, if 'use_separate_task' was set in the constructor,
      // then we create a lambda that when executed creates a task
      // that can be executed whenever the run-time system of the compiler
      // thinks is appropriate.
      {
        connections_to_producers.push_back (
          producer.connect_to_signal (
            [&](InputType sample, AuxiliaryData aux_data)
        {
          // Before we set up another task, make sure that any previous
          // one has returned.
          if (background_task.valid())
            background_task.wait();

          // Copy the sample and aux data sent in to a memory
          // location inside the following lambda, so that
          // they are available whenever the task is actually
          // executed. This makes sure that we can exit whenever
          // we want.
          auto deferred_worker
            = [=]()
          {
            this->consume (std::move(sample), std::move(aux_data));
          };

          background_task = std::async (std::launch::async, deferred_worker);
        }));
      }
  }


  /**
   * A namespace for the implementation of consumers, i.e., classes
   * derived from the Consumer class.
   *
   * Strictly speaking, this should also include filters (i.e., classes
   * derived from the Filter class), but since these are in addition
   * derived from the Producer class, they are in their own namespace
   * Filters.
   */
  namespace Consumers
  {}
}

#endif
