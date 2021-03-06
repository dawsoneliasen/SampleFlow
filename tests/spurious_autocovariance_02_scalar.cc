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


// Check the SpuriousAutocovariance consumer. Do so with a sequence of
// samples that consists of {0, 1, 0, 1, ...}
//
// While the _02 test uses vectors of length 1 to encode each of these
// samples, the current test just uses scalar values.

#include <iostream>
#include <valarray>

#include <sampleflow/producers/range.h>
#include <sampleflow/consumers/spurious_autocovariance.h>


int main ()
{
  using SampleType = double;

  SampleFlow::Producers::Range<SampleType> range_producer;

  const unsigned int AC_length = 10;
  SampleFlow::Consumers::SpuriousAutocovariance<SampleType> autocovariance(AC_length);
  autocovariance.connect_to_producer (range_producer);

  std::vector<SampleType> samples(1000);
  for (unsigned int i=0; i<1000; ++i)
    samples[i] = (i % 2 == 0 ? 0. : 1.);

  range_producer.sample (samples);

  for (const auto v : autocovariance.get())
    std::cout << v << std::endl;
}
