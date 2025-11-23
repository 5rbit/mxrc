// MetricsCollector.cpp
// Copyright (C) 2025 MXRC Project

#include "MetricsCollector.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace mxrc::core::monitoring {

// Histogram 구현

Histogram::Histogram(const std::vector<double>& buckets)
    : buckets_(buckets)
    , bucket_counts_(buckets.size() + 1, 0) {
    // buckets는 정렬되어 있어야 함
    std::sort(buckets_.begin(), buckets_.end());
}

void Histogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    sum_ += value;
    count_++;

    // 적절한 bucket 찾기
    size_t bucket_idx = 0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            bucket_idx = i;
            break;
        }
    }
    if (value > buckets_.back()) {
        bucket_idx = buckets_.size();
    }

    bucket_counts_[bucket_idx]++;
}

double Histogram::sum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sum_;
}

uint64_t Histogram::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

std::vector<uint64_t> Histogram::bucketCounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bucket_counts_;
}

// MetricsCollector 구현

std::string MetricsCollector::labelsToString(const Labels& labels) const {
    if (labels.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : labels) {
        if (!first) oss << ",";
        oss << key << "=\"" << value << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::shared_ptr<Counter> MetricsCollector::getOrCreateCounter(
    const std::string& name,
    const Labels& labels,
    const std::string& help) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto& label_map = counters_[name];
    auto it = label_map.find(labels);
    if (it != label_map.end()) {
        return it->second;
    }

    if (!help.empty()) {
        help_texts_[name] = help;
    }

    auto counter = std::make_shared<Counter>();
    label_map[labels] = counter;
    return counter;
}

std::shared_ptr<Gauge> MetricsCollector::getOrCreateGauge(
    const std::string& name,
    const Labels& labels,
    const std::string& help) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto& label_map = gauges_[name];
    auto it = label_map.find(labels);
    if (it != label_map.end()) {
        return it->second;
    }

    if (!help.empty()) {
        help_texts_[name] = help;
    }

    auto gauge = std::make_shared<Gauge>();
    label_map[labels] = gauge;
    return gauge;
}

std::shared_ptr<Histogram> MetricsCollector::getOrCreateHistogram(
    const std::string& name,
    const Labels& labels,
    const std::vector<double>& buckets,
    const std::string& help) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto& label_map = histograms_[name];
    auto it = label_map.find(labels);
    if (it != label_map.end()) {
        return it->second;
    }

    if (!help.empty()) {
        help_texts_[name] = help;
    }

    auto histogram = std::make_shared<Histogram>(buckets);
    label_map[labels] = histogram;
    return histogram;
}

void MetricsCollector::incrementCounter(
    const std::string& name,
    const Labels& labels,
    uint64_t delta) {

    auto counter = getOrCreateCounter(name, labels);
    counter->increment(delta);
}

void MetricsCollector::setGauge(
    const std::string& name,
    double value,
    const Labels& labels) {

    auto gauge = getOrCreateGauge(name, labels);
    gauge->set(value);
}

void MetricsCollector::observeHistogram(
    const std::string& name,
    double value,
    const Labels& labels) {

    // 기본 buckets 사용
    auto histogram = getOrCreateHistogram(name, labels);
    histogram->observe(value);
}

std::string MetricsCollector::exportPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    // Counters
    for (const auto& [name, label_map] : counters_) {
        if (help_texts_.count(name)) {
            oss << "# HELP " << name << " " << help_texts_.at(name) << "\n";
        }
        oss << "# TYPE " << name << " counter\n";

        for (const auto& [labels, counter] : label_map) {
            oss << name << labelsToString(labels) << " " << counter->get() << "\n";
        }
    }

    // Gauges
    for (const auto& [name, label_map] : gauges_) {
        if (help_texts_.count(name)) {
            oss << "# HELP " << name << " " << help_texts_.at(name) << "\n";
        }
        oss << "# TYPE " << name << " gauge\n";

        for (const auto& [labels, gauge] : label_map) {
            oss << name << labelsToString(labels) << " " << std::fixed << std::setprecision(6) << gauge->get() << "\n";
        }
    }

    // Histograms
    for (const auto& [name, label_map] : histograms_) {
        if (help_texts_.count(name)) {
            oss << "# HELP " << name << " " << help_texts_.at(name) << "\n";
        }
        oss << "# TYPE " << name << " histogram\n";

        for (const auto& [labels, histogram] : label_map) {
            auto buckets = histogram->buckets();
            auto counts = histogram->bucketCounts();

            // Cumulative buckets
            uint64_t cumulative = 0;
            for (size_t i = 0; i < buckets.size(); ++i) {
                cumulative += counts[i];
                Labels bucket_labels = labels;
                bucket_labels["le"] = std::to_string(buckets[i]);
                oss << name << "_bucket" << labelsToString(bucket_labels) << " " << cumulative << "\n";
            }

            // +Inf bucket
            cumulative += counts.back();
            Labels inf_labels = labels;
            inf_labels["le"] = "+Inf";
            oss << name << "_bucket" << labelsToString(inf_labels) << " " << cumulative << "\n";

            // Sum and count
            oss << name << "_sum" << labelsToString(labels) << " " << std::fixed << std::setprecision(6) << histogram->sum() << "\n";
            oss << name << "_count" << labelsToString(labels) << " " << histogram->count() << "\n";
        }
    }

    return oss.str();
}

void MetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [name, label_map] : counters_) {
        for (auto& [labels, counter] : label_map) {
            counter->reset();
        }
    }

    gauges_.clear();
    histograms_.clear();
}

} // namespace mxrc::core::monitoring
