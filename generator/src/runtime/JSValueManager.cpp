#include "runtime/JSValueManager.h"

namespace SCXML {

// Static member definitions
thread_local int JSValueWrapper::creation_counter_ = 0;
std::unordered_map<void*, std::string> JSValueTracker::tracked_values_;
std::mutex JSValueTracker::tracker_mutex_;

// JSValueTracker implementation
void JSValueTracker::track(JSValue value, const std::string& name) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    void* ptr = JS_VALUE_GET_PTR(value);
    if (ptr) {
        tracked_values_[ptr] = name;
    }
}

void JSValueTracker::untrack(JSValue value) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    void* ptr = JS_VALUE_GET_PTR(value);
    if (ptr) {
        tracked_values_.erase(ptr);
    }
}

void JSValueTracker::clearAll(JSContext* context) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    tracked_values_.clear(); // Just clear tracking, don't free values here
    (void)context; // Suppress unused parameter warning
}

size_t JSValueTracker::getTrackedCount() {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    return tracked_values_.size();
}

} // namespace SCXML