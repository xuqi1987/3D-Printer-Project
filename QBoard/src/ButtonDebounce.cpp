#include "ButtonDebounce.h"

ButtonDebounce::ButtonDebounce(uint8_t pin, uint8_t pin_mode, uint8_t pin_down_digital, uint32_t debounce_ms)
    : mPin{pin}, mPindowndigital{pin_down_digital}, mDebounceMs{debounce_ms}, mClickCallback{nullptr},
      mLongClickCallback{nullptr}, mDoubleClickCallback{nullptr}
{
    pinMode(pin, pin_mode);
    attachInterrupt(
        digitalPinToInterrupt(mPin), [this]() { this->update(); }, CHANGE);
    setHandlerCallback(std::bind(&ButtonHandler::handleChange, &mButtonhandler, std::placeholders::_1));

    mButtonhandler.setIsDownFunction(std::bind(&ButtonDebounce::checkIsDown, this));

    mButtonhandler.setBtnEventCallback([this](button_event e) {
        if ((e == BUTTON_EVENT_SINGLECLICK))
        {
            this->onBtnClick(this->mPin);
        }
        if (e == BUTTON_EVENT_DOUBLECLICK)
        {
            this->onBtnDoublePress(this->mPin);
        }
        if (e == BUTTON_EVENT_LONGCLICK)
        {
            this->onBtnLongPress(this->mPin);
        }
    });
}

void ButtonDebounce::update()
{
    bool down = readIsDown();
    update(down);
}

void ButtonDebounce::update(bool down)
{
    const uint32_t t = millis();
    // 如果间隔小于 mDebounceMs (35ms)
    if (t - mLastchangeMs < mDebounceMs)
    {
        mLastchangeMs = t; // 更新最后一次的时间
    }
    // 否则认为是有效的点击
    else
    {
        // 如果按钮状态不一致，认为有效点击
        if (mLaststateIsDown != down)
        {
            // state changed, up->down or down->up
            mLastchangeMs = t;
            mLaststateIsDown = down;
            if (mCallback)
            {
                mCallback(down, mPin); // 触发callback
            }
        }
    }
}

bool ButtonDebounce::checkIsDown()
{
    return mLaststateIsDown;
}

void ButtonDebounce::setHandlerCallback(std::function<void(const bool down, const int pin)> callback)
{
    this->mCallback = callback;
}

bool ButtonDebounce::readIsDown()
{
    // 获取按钮是否变化
    return mPindowndigital == digitalRead(mPin);
}

void ButtonDebounce::setClickCallback(std::function<void(const int)> callback)
{
    mClickCallback = callback;
}
void ButtonDebounce::setLongClickCallback(std::function<void(const int)> callback)
{
    mLongClickCallback = callback;
}
void ButtonDebounce::setDoubleClickCallback(std::function<void(const int)> callback)
{
    mDoubleClickCallback = callback;
}
void ButtonDebounce::onBtnClick(int pin)
{
    if (mClickCallback)
    {
        mClickCallback(pin);
    }
}

void ButtonDebounce::onBtnLongPress(int pin)
{
    if (mLongClickCallback)
    {
        mLongClickCallback(pin);
    }
}

void ButtonDebounce::onBtnDoublePress(int pin)
{
    if (mDoubleClickCallback)
    {
        mDoubleClickCallback(pin);
    }
}
