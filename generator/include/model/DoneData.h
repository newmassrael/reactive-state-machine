#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility> // for std::pair

class IDataModelItem;

/**
 * @brief <donedata> 요소에 대한 정보를 저장하는 클래스
 *
 * 이 클래스는 SCXML의 <donedata> 요소 정보를 저장합니다.
 * <donedata>는 <final> 상태가 진입될 때 반환될 데이터를 포함합니다.
 */
class DoneData
{
public:
    /**
     * @brief 기본 생성자
     */
    DoneData() = default;

    /**
     * @brief 소멸자
     */
    ~DoneData() = default;

    /**
     * @brief <content> 요소의 내용 설정
     * @param content 콘텐츠 문자열
     */
    void setContent(const std::string &content)
    {
        content_ = content;
        hasContent_ = true;
    }

    /**
     * @brief <content> 요소 내용 반환
     * @return 콘텐츠 문자열
     */
    const std::string &getContent() const
    {
        return content_;
    }

    /**
     * @brief <param> 요소 추가
     * @param name 매개변수 이름
     * @param location 데이터 모델 위치 경로
     */
    void addParam(const std::string &name, const std::string &location)
    {
        params_.push_back(std::make_pair(name, location));
    }

    /**
     * @brief <param> 요소 목록 반환
     * @return 매개변수 이름과 위치 목록
     */
    const std::vector<std::pair<std::string, std::string>> &getParams() const
    {
        return params_;
    }

    /**
     * @brief <donedata> 요소가 비어 있는지 확인
     * @return 비어 있으면 true, 아니면 false
     */
    bool isEmpty() const
    {
        return !hasContent_ && params_.empty();
    }

    /**
     * @brief <content> 요소가 있는지 확인
     * @return <content> 요소가 있으면 true, 아니면 false
     */
    bool hasContent() const
    {
        return hasContent_;
    }

    void clearParams()
    {
        params_.clear();
    }

private:
    std::string content_;                                     // <content> 요소의 내용
    std::vector<std::pair<std::string, std::string>> params_; // <param> 요소 목록 (이름, 위치)
    bool hasContent_ = false;                                 // <content> 요소 존재 여부
};
