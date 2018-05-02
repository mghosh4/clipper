//
// Created by Arvind Vijayakumar on 5/1/18.
//

#include "optimizer.hpp"

#include <thread>
#include <chrono>
#include <xtensor/xarray.hpp>
#include <xtensor/xadapt.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor/xbuilder.hpp>
#include <xtensor/xexpression.hpp>
#include <xtensor/xreducer.hpp>
#include <xtensor/xsort.hpp>
#include <xtensor/xindex_view.hpp>

using std::chrono::milliseconds;
using std::vector;
using std::string;
using std::unordered_map;
using std::thread;

using namespace management;

// Sourced from https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
template<class T> vector<size_t> arg_sort(const vector<T> &v) {

  // Initialize original index locations
  vector<size_t> idx(v.size());
  iota(idx.begin(), idx.end(), 0);

  // Sort indexes based on comparing values in v
  sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1] < v[i2]; } );

  return idx;
}

template<class T, int N> vector<T> to_vector(xt::xtensor<T, N>& tensor) {
  vector<T> v(N);
  for (auto e : tensor) {
    v.push_back(e);
  }
  return v;
};

unordered_map<size_t, size_t> invert_index(vector<size_t> & sort_indices) {
  unordered_map<size_t, size_t> m;
  for (size_t i = 0; i < sort_indices.size(); i++) {
    m[sort_indices[i]] = i;
  }
  return m;
}

template<int N> auto revert_order(xt::xtensor<double, N> arr, vector<size_t> sort_indices) {
  unordered_map<size_t, size_t> inverse_index = invert_index(sort_indices);
  xt::xtensor<double, N> orig_order;
  for (size_t i = 0; i < N; i++) {
    orig_order[i] = arr[inverse_index[i]];
  }
  return orig_order;
}

template<int N> auto normalize_constrained_array(xt::xtensor<double, N> s_arr, size_t num_containers) {
  auto int_arr = xt::trunc(s_arr);
  auto int_sum = xt::cast<int>(xt::sum(int_arr));
  auto frac_weights = -((s_arr - int_arr) / s_arr);
  auto frac_weights_sortinds = arg_sort(to_vector(frac_weights));
  auto sorted_int_arr = xt::index_view(int_arr, frac_weights_sortinds);
  int rem_buf = xt::cast<int>(xt::round(xt::sum(s_arr) - int_sum));
  size_t i = 0;
  while (rem_buf > 0) {
    sorted_int_arr[i] += 1;
    rem_buf -= 1;
    i += 1;
  }
  while (i < N) {
    if (sorted_int_arr[i] < 1) {
      sorted_int_arr[i] = 1;
      rem_buf -= 1;
    } else if (rem_buf < 0) {
      sorted_int_arr[i] = -1;
      rem_buf += 1;
    }
    i += 1;
  }

  return xt::cast<int>(revert_order(sorted_int_arr, frac_weights_sortinds));
}

unordered_map<string, ResourceAllocationEntry> create_resource_allocation(
    const std::vector<AppMetricEntry> & app_metric_entries, const vector<int> & R, const vector<int> & B) {
  unordered_map<string, ResourceAllocationEntry> m;
  for (size_t i = 0; i < app_metric_entries.size(); i++) {
    auto entry = app_metric_entries[i];
    m[entry.get_app_name()] = ResourceAllocationEntry(R[i], B[i]);
  }
  return m;
}

unordered_map<string, ResourceAllocationEntry> IterativeLatencyOptimizer::compute_resource_allocation(
    unsigned int num_containers, std::vector<AppMetricEntry> app_metric_entries) {

  const size_t num_applications = app_metric_entries.size();
  xt::xtensor<double, num_applications> B_start({num_applications, 1});
  xt::xtensor<double, num_applications> Q({num_applications, 1});
  std::unordered_map<string, int> app_name_index;
  for (int i = 0; i < num_applications; i++) {
    auto entry = app_metric_entries[i];
    B[i] = entry.get_batch_size();
    Q[i] = entry.get_query_rate();
    app_name_index[entry.get_app_name()] = i;
  }

  auto R_start = Q / B_start;
  double total_projected_containers = xt::sum(R_start);
  if ((int) total_projected_containers > num_containers) {
    auto unit_R = R_start / total_projected_containers;
    auto norm_R = unit_R * num_containers;
    vector<int> int_R = to_vector(normalize_constrained_array(norm_R, num_containers));
    vector<int> int_B = xt::round(B_start);
    return create_resource_allocation(app_metric_entries, int_R, int_B);
  }

  // TODO(avjykmr2): Implement iterative assignment method
  return unordered_map<string, ResourceAllocationEntry>();

}

void IterativeLatencyOptimizer::stop() {
  shutdown_ = false;
}

void IterativeLatencyOptimizer::start() {
  std::thread runtime_execution_t(&IterativeLatencyOptimizer::runtime_execution, this);
  runtime_execution_t.detach();
}

/**
 * Blocking command to fetch latest query rates and batch sizes for all applications.
 */
std::vector<AppMetricEntry> IterativeLatencyOptimizer::request_application_metrics() {
  // TODO(avjykmr2): Implement send request logic
}

void IterativeLatencyOptimizer::runtime_execution() {
  while (!shutdown_) {
    vector<AppMetricEntry> app_metric_entries = request_application_metrics();
    size_t number_of_containers = get_containers().size();
    unordered_map<string, ResourceAllocationEntry> resource_allocation =
        compute_resource_allocation(number_of_containers, app_metric_entries);
    // TODO(avjykmr2): Publish resource allocation to kubernetes replica set / replica controller
    std::this_thread::sleep_for(milliseconds(polling_interval_));
  }
}

