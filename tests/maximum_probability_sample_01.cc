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


// Check the auxiliary data the MH sampler attaches to samples. In
// particular, check the information about repeated/non-repeated
// samples: Because every trial sample is definitely different from
// the previous sample, we know that the MH sampler should say that
// the sample is repeated if it has the same value as the previous
// sample: There is no possibility that an accepted trial sample *by
// chance* happens to be at the same location as the previous sample.
//
// Test the maximum probability sample consumer by observing the 
// auxiliary data from the MH producer.


#include <iostream>
#include <sampleflow/producers/metropolis_hastings.h>
#include <sampleflow/consumers/maximum_probability_sample.h>
#include <random>

using SampleType = int;

// Choose a probability distribution that is essentially zero on the
// left side of the real line, highest at x=0, and then decreases to
// the right. This will bring samples back to zero, but they can't go
// into negative territory.
double log_likelihood (const SampleType &x)
{
  return (x>=0 ? -x/100. : -1e10);
}

SampleType perturb (const SampleType &x)
{
  static std::mt19937 rng;
  // give "true" 1/2 of the time and
  // give "false" 1/2 of the time
  std::bernoulli_distribution distribution(0.5);

  if (distribution(rng) == true)
    return x-1;
  else
    return x+1;
}


int main ()
{

  SampleFlow::Producers::MetropolisHastings<SampleType> mh_sampler;

  SampleFlow::Consumers::MaximumProbabilitySample<SampleType> map;
  map.connect_to_producer(mh_sampler);

  // Sample, starting at zero and perform a random walk to the left
  // and right using the proposal distribution. But, because the
  // probability distribution is essentially zero on the left, never
  // take a step to the left side.
  mh_sampler.sample ({0},
                     &log_likelihood,
                     &perturb,
                     20);
  std::cout << "MAP = " << map.get() << std::endl;
  // printf("MAP = %f\n", map.get());
}