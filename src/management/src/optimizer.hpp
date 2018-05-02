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
#include <clipper/redis.hpp>
#include <clipper/datatypes.hpp>

using clipper::AppMetricEntry;

namespace management {

typedef std::vector<std::string, AppMetricEntry> AMEvent;

// TODO(avjykmr2): Add destructors

    struct Optimizer {

        virtual void start();

        virtual void stop();
    };

    struct DummyOptimizer : public Optimizer {

        void start() {}

        void stop() {}
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
        IterativeLatencyOptimizer(unsigned int polling_interval, redox::Redox & redis_connection) :
            polling_interval_(polling_interval) {
          // TODO(avjykmr2): Questions about redox usage:
          //  1) By reference or by value?
          //  2) Naturally thread-safe or synchronization required?
          redis_connection_ = redis_connection;
        }

        void start();

        void stop();

      private:
        unsigned int polling_interval_;
        bool shutdown_ = false;
        redox::Redox redis_connection_;
        // TODO(avjykmr2): Finalize choice of blocking queue implementation
        moodycamel::BlockingConcurrentQueue<AMEvent> q;
        //folly::USPSCQueue<QREvent, true> q;
        static std::unordered_map<std::string, ResourceAllocationEntry> compute_resource_allocation(
            unsigned int num_containers, std::vector<AppMetricEntry> app_metric_entries);
        void runtime_execution();
        std::vector<AppMetricEntry> request_application_metrics();
        /**
         * Fetches list of containers from redis.
         */
        std::vector<std::pair<clipper::VersionedModelId, int>> get_containers() {
          return clipper::redis::get_all_containers(redis_connection_);
        }
    };

};

#endif //CLIPPER_OPTIMIZER_H