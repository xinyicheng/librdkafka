/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2012-2015, Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include "testcpp.h"

/**
 * Test offset_for_times (KIP-79): time-based offset lookups.
 */


static int verify_offset (const RdKafka::TopicPartition *tp,
                          int64_t timestamp, int64_t exp_offset,
                          RdKafka::ErrorCode exp_err) {
  int fails = 0;
  if (tp->err() != exp_err) {
    Test::FailLater(tostr() << " " << tp->topic() <<
                    " [" << tp->partition() << "] " <<
                    "expected error " << RdKafka::err2str(exp_err) << ", got " <<
                    RdKafka::err2str(tp->err()) << "\n");
    fails++;
  }

  if (tp->offset() != exp_offset) {
    Test::FailLater(tostr() << " " << tp->topic() <<
                    " [" << tp->partition() << "] " <<
                    "expected offset " << exp_offset << " for timestamp " <<
                    timestamp << ", got " << tp->offset() << "\n");
    fails++;
  }

  return fails;
}


static void test_offset_time (void) {
  std::vector<RdKafka::TopicPartition*> query_parts;
  std::string topic = Test::mk_topic_name("0054-offset_time", 1);
  RdKafka::Conf *conf;
  int64_t timestamps[] = {
    /* timestamp, expected offset */
    1234, 0,
    999999999999, 1,
  };
  const int timestamp_cnt = 2;
  int fails = 0;

  Test::conf_init(&conf, NULL, 0);

  Test::conf_set(conf, "api.version.request", "true");

  std::string errstr;
  RdKafka::Producer *p = RdKafka::Producer::create(conf, errstr);
  if (!p)
    Test::Fail("Failed to create Producer: " + errstr);

  query_parts.push_back(RdKafka::TopicPartition::create(topic, 0, timestamps[0]));
  query_parts.push_back(RdKafka::TopicPartition::create(topic, 1, timestamps[0]));
  query_parts.push_back(RdKafka::TopicPartition::create(topic, 99, timestamps[0]));

  /* First query timestamps before topic exists, should fail. */
  Test::Say("Attempting first offsetsForTimes() query (should fail)\n");
  RdKafka::ErrorCode err = p->offsetsForTimes(query_parts, 10000);
  Test::Say(tostr() << "offsetsForTimes #1 with non-existing topic returned " <<
            RdKafka::err2str(err) << "\n");
  Test::print_TopicPartitions("offsetsForTimes #1", query_parts);

  if (err == RdKafka::ERR_NO_ERROR)
    Test::Fail("offsetsForTimes #1 should have failed");

  Test::Say("Producing to " + topic + "\n");
  for (int partition = 0 ; partition < 2 ; partition++) {
    for (int ti = 0 ; ti < timestamp_cnt*2 ; ti += 2) {
      err = p->produce(topic, partition, RdKafka::Producer::RK_MSG_COPY,
                       (void *)topic.c_str(), topic.size(), NULL, 0,
                       timestamps[ti], NULL);
      if (err != RdKafka::ERR_NO_ERROR)
        Test::Fail("Produce failed: " + RdKafka::err2str(err));
    }
  }

  if (p->flush(5000) != 0)
    Test::Fail("Not all messages flushed");


  for (int ti = 0 ; ti < timestamp_cnt*2 ; ti += 2) {
    query_parts.clear();
    query_parts.push_back(RdKafka::TopicPartition::create(topic, 0, timestamps[ti]));
    query_parts.push_back(RdKafka::TopicPartition::create(topic, 1, timestamps[ti]));

    Test::Say(tostr() << "Attempting offsetsForTimes() for timestamp " << timestamps[ti] << "\n");
    err = p->offsetsForTimes(query_parts, 5000);
    Test::print_TopicPartitions("offsetsForTimes", query_parts);
    if (err != RdKafka::ERR_NO_ERROR)
      Test::Fail("offsetsForTimes failed: " + RdKafka::err2str(err));

    fails += verify_offset(query_parts[0], timestamps[ti], timestamps[ti+1], RdKafka::ERR_NO_ERROR);
    fails += verify_offset(query_parts[1], timestamps[ti], timestamps[ti+1], RdKafka::ERR_NO_ERROR);
  }

  if (fails > 0)
    Test::Fail(tostr() << "See " << fails << " previous error(s)");

  delete p;
  delete conf;
}

extern "C" {
  int main_0054_offset_time (int argc, char **argv) {
    test_offset_time();
    return 0;
  }
}
