//
// Created by Arvind Vijayakumar on 5/1/18.
//

#ifndef CLIPPER_OPTIMIZER_H
#define CLIPPER_OPTIMIZER_H

#include <unordered_map>
#include <folly/concurrency/UnboundedQueue.h>
#include <folly/MPMCQueue.h>
#include <blockingconcurrentqueue.h>
#include <clipper/datatypes.hpp>

using clipper::QueryRateEntry;

struct Optimizer {

  virtual void push_qr_event(std::vector<QueryRateEntry> query_rates);
  virtual void start();
  virtual void stop();
};

struct DummyOptimizer : public Optimizer {
    void push_qr_event(std::vector<QueryRateEntry> query_rates) {
      // no-op
    }
    void start() {}
    void stop() {}
};

struct QREvent {
    std::vector<QueryRateEntry> query_rates;
    bool poison_pill = false;
};

class IterativeLatencyOptimizer : public Optimizer {

  public:
    IterativeLatencyOptimizer(int portno, redox::Redox redis_connection, unsigned int polling_interval) :
        portno_(portno), redis_connection_(redis_connection), polling_interval_(polling_interval) {}
    void push_qr_event(std::vector<QueryRateEntry> query_rates) {
      QREvent qr_event;
      qr_event.query_rates = query_rates;
      q.enqueue(qr_event);
    }
    void start();
    void stop();
  private:
    int portno_;
    unsigned int polling_interval_;
    redox::Redox redis_connection_;
    bool shutdown_ = false;
    // TODO(avjykmr2): Finalize choice of blocking queue implementation
    moodycamel::BlockingConcurrentQueue<QREvent> q;
//    folly::USPSCQueue<QREvent, true> q;
    void runtime_execution();
    static void compute_resource_allocation(unsigned int num_containers, );
    void request_query_rates();
};

#endif //CLIPPER_OPTIMIZER_H
