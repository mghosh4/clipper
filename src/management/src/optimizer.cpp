//
// Created by Arvind Vijayakumar on 5/1/18.
//

#include "optimizer.hpp"

#include <thread>
#include <chrono>

using std::chrono::milliseconds;

void IterativeLatencyOptimizer::start() {
  std::thread runtime_execution;
  runtime_execution.detach();
}

void IterativeLatencyOptimizer::runtime_execution() {
  while(!shutdown_) {
    request_query_rates();
    QREvent qr_event;
    q.wait_dequeue(qr_event);


    std::this_thread::sleep_for(milliseconds(polling_interval_));
  }
}

void IterativeLatencyOptimizer::compute_resource_allocation() {

}

void IterativeLatencyOptimizer::request_query_rates() {
  // TODO(avjykmr2): Implement send request logic
  // push_qr_event(response)
}

void IterativeLatencyOptimizer::stop() {
  shutdown_ = true;
}

//void IterativeLatencyOptimizer::stop() {
//  QREvent poison_pill;
//  poison_pill.poison_pill = true;
//  q.enqueue(poison_pill);
//}

/*void IterativeLatencyOptimizer::poll_response() {
  QREvent qr_event;
  do {
    q.wait_dequeue(qr_event);
  } while(!qr_event.poison_pill);
}*/