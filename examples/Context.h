#pragma once

#include <string>
#include <functional>
#include <boost/signals2.hpp>
#include <unordered_map>
#include <any>

/**
 * @brief 이벤트 컨텍스트 클래스
 * 이벤트와 함께 전달되는 컨텍스트 데이터 관리
 */
class EventContext
{
private:
    std::unordered_map<std::string, std::any> values_;

public:
    // 기본 생성자
    EventContext() = default;

    template <typename T>
    void setValue(const std::string &key, const T &value)
    {
        values_[key] = value;
    }

    template <typename T>
    T getValue(const std::string &key) const
    {
        auto it = values_.find(key);
        if (it != values_.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (const std::bad_any_cast &)
            {
                throw std::runtime_error("Type mismatch in EventContext::getValue");
            }
        }
        throw std::out_of_range("Key not found in EventContext: " + key);
    }

    bool hasValue(const std::string &key) const
    {
        return values_.find(key) != values_.end();
    }
};

/**
 * @brief 반응형 컨텍스트 클래스
 * 상태 머신에서 사용하는 모든 컨텍스트 속성 관리
 */
class Context
{
public:
    /**
     * @brief 반응형 속성 템플릿 클래스
     * 값 변경을 감지하고 알림을 발생시키는 래퍼
     */
    template <typename T>
    class Property
    {
    private:
        T value_;
        boost::signals2::signal<void(const T &, const T &)> changed_;

    public:
        Property(T initial = T{}) : value_(initial) {}

        // 값 설정 (변경 시에만 신호 발생)
        void set(const T &newValue)
        {
            if (value_ != newValue)
            {
                T oldValue = value_;
                value_ = newValue;
                changed_(oldValue, newValue);
            }
        }

        // 값 접근
        const T &get() const { return value_; }
        operator const T &() const { return value_; }

        // 변경 구독
        boost::signals2::connection onChange(
            std::function<void(const T &, const T &)> callback)
        {
            return changed_.connect(callback);
        }
    };

    // SCXML에서 정의된 컨텍스트 속성들
    Property<int> counter{0};
    Property<bool> flag{false};
    Property<std::string> currentUser{""};
    Property<bool> isActive{false};
    Property<std::string> status{""};

    // 모든 속성 초기화
    void reset()
    {
        counter.set(0);
        flag.set(false);
        currentUser.set("");
        isActive.set(false);
        status.set("");
    }
};

/**
 * @brief 가드 조건 인터페이스
 * 상태 전환 조건을 평가하는 인터페이스
 */
class Guard
{
public:
    virtual ~Guard() = default;
    virtual bool evaluate(const Context &context) const = 0;
};
