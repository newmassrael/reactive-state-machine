// TransitionNode.cpp
#include "TransitionNode.h"
#include "common/Logger.h"
#include <algorithm>
#include <sstream>
using namespace RSM;


TransitionNode::TransitionNode(const std::string &event, const std::string &target)
    : event_(event), target_(target), guard_(""), reactive_(false), internal_(false), targetsDirty_(true)
{
    Logger::debug("TransitionNode::Constructor - Creating transition node: " +
                  (event.empty() ? "<no event>" : event) + " -> " + target);


    if (!event.empty())
    {
        events_.push_back(event);
    }
}


TransitionNode::~TransitionNode()
{
    Logger::debug("TransitionNode::Destructor - Destroying transition node: " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_);
}


const std::string &TransitionNode::getEvent() const
{
    return event_;
}


std::vector<std::string> TransitionNode::getTargets() const
{
    // 캐싱 메커니즘 - 타겟이 변경되었거나 아직 파싱되지 않았으면 파싱
    if (targetsDirty_)
    {
        parseTargets();
        targetsDirty_ = false;
    }
    return cachedTargets_;
}


void TransitionNode::addTarget(const std::string &target)
{
    Logger::debug("TransitionNode::addTarget() - Adding target to transition " +
                  (event_.empty() ? "<no event>" : event_) + ": " + target);


    if (target.empty())
    {
        return; // 빈 타겟은 추가하지 않음
    }


    if (target_.empty())
    {
        target_ = target;
    }
    else
    {
        target_ += " " + target;
    }
    targetsDirty_ = true; // 캐시 갱신 필요 표시
}


void TransitionNode::clearTargets()
{
    Logger::debug("TransitionNode::clearTargets() - Clearing targets for transition " +
                  (event_.empty() ? "<no event>" : event_));


    target_.clear();
    cachedTargets_.clear();
    targetsDirty_ = false; // 이미 캐시가 비어있으므로 갱신 불필요
}


bool TransitionNode::hasTargets() const
{
    if (!targetsDirty_ && !cachedTargets_.empty())
    {
        return true;
    }
    return !target_.empty();
}


void TransitionNode::parseTargets() const
{
    cachedTargets_.clear();


    if (target_.empty())
    {
        return;
    }


    std::istringstream ss(target_);
    std::string target;
    while (ss >> target)
    {
        cachedTargets_.push_back(target);
    }
}


void TransitionNode::setGuard(const std::string &guard)
{
    Logger::debug("TransitionNode::setGuard() - Setting guard for transition " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_ + ": " + guard);
    guard_ = guard;
}


const std::string &TransitionNode::getGuard() const
{
    return guard_;
}


void TransitionNode::addAction(const std::string &action)
{
    Logger::debug("TransitionNode::addAction() - Adding action to transition " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_ + ": " + action);
    actions_.push_back(action);
}


const std::vector<std::string> &TransitionNode::getActions() const
{
    return actions_;
}


void TransitionNode::setReactive(bool reactive)
{
    Logger::debug("TransitionNode::setReactive() - Setting reactive flag for transition " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_ + ": " +
                  (reactive ? "true" : "false"));
    reactive_ = reactive;
}


bool TransitionNode::isReactive() const
{
    return reactive_;
}


void TransitionNode::setInternal(bool internal)
{
    Logger::debug("TransitionNode::setInternal() - Setting internal flag for transition " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_ + ": " +
                  (internal ? "true" : "false"));
    internal_ = internal;
}


bool TransitionNode::isInternal() const
{
    return internal_;
}


void TransitionNode::setAttribute(const std::string &name, const std::string &value)
{
    Logger::debug("TransitionNode::setAttribute() - Setting attribute for transition " +
                  (event_.empty() ? "<no event>" : event_) + " -> " + target_ + ": " +
                  name + "=" + value);
    attributes_[name] = value;
}


std::string TransitionNode::getAttribute(const std::string &name) const
{
    auto it = attributes_.find(name);
    if (it != attributes_.end())
    {
        return it->second;
    }
    return "";
}


void TransitionNode::addEvent(const std::string &event)
{
    if (std::find(events_.begin(), events_.end(), event) == events_.end())
    {
        Logger::debug("TransitionNode::addEvent() - Adding event to transition: " + event);
        events_.push_back(event);
    }
}


const std::vector<std::string> &TransitionNode::getEvents() const
{
    return events_;
}
