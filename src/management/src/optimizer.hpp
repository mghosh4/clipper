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
#include <redox.hpp>

using clipper::AppMetricEntry;

namespace management {

// TODO(avjykmr2): Add destructors

    struct Optimizer {

        virtual void push_qr_event(std::vector<AppMetricEntry> query_rates);

        virtual void start();

        virtual void stop();
    };

    struct DummyOptimizer : public Optimizer {
        void push_qr_event(std::vector<AppMetricEntry> query_rates) {
          // no-op
        }

        void start() {}

        void stop() {}
    };

    struct AMEvent {
        std::vector<AppMetricEntry> app_metric_entries;
        bool poison_pill = false;
    };

    class ResourceAllocationEntry {
    public:
        ResourceAllocationEntry(unsigned int num_containers, unsigned int batch_size) :
            num_containers_(num_containers), batch_size_(batch_size) {}

        unsigned int num_containers() const { return num_containers_; }

        unsigned int batch_size() const { return batch_size_; }

    private:
        unsigned int num_containers_;
        unsigned int batch_size_;
    };

    class IterativeLatencyOptimizer : public Optimizer {

    public:
        IterativeLatencyOptimizer(redox::Redox & redis_connection) {
          redis_connection_ = redis_connection;
        }

        void push_qr_event(std::vector<AppMetricEntry> app_metric_entries) {
          AMEvent qr_event;
          qr_event.app_metric_entries = app_metric_entries;
          q.enqueue(qr_event);
        }

        void start();

        void stop();

    private:
        redox::Redox redis_connection_;
        // TODO(avjykmr2): Finalize choice of blocking queue implementation
        moodycamel::BlockingConcurrentQueue<AMEvent> q;

//        folly::USPSCQueue<QREvent, true> q;
        static std::unordered_map<std::string, ResourceAllocationEntry> compute_resource_allocation(
            unsigned int num_containers, std::vector<AppMetricEntry> app_metric_entries);
    };

};

#endif //CLIPPER_OPTIMIZER_H