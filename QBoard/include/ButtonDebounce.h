#pragma once
#ifndef BUTTON_DEBOUNCE_H_
#define BUTTON_DEBOUNCE_H_

#include "ButtonHandler.h"
#include "Device.h"
#include <Arduino.h>
#include <FunctionalInterrupt.h> //attachInterrupt可以传function
#include <functional>

/**
 * @brief 按钮去抖
 *
 */
class ButtonDebounce : public Device
{
  public:
    /**
     * @brief 配置按钮的pin和输入模式，和初始值
     *
     * @param pin D8/7/6/5
     * @param pin_mode INPUT_PULLUP
     * @param pin_down_digital LOW，默认该pin下拉低
     * @param debounce_ms
     */
    ButtonDebounce(uint8_t pin, uint8_t pin_mode, uint8_t pin_down_digital, uint32_t debounce_ms = 35);

    /**
     * @brief update函数会去调用这个注册的callback
     *
     * @param callback
     */
    void setHandlerCallback(std::function<void(const bool down, const int pin)> callback);

    /**
     * @brief 更新当前的按钮状态
     *
     */
    void update();
    void update(bool down);

    //返回当前debouce后的按钮状态
    bool checkIsDown();

    bool readIsDown();

    void setClickCallback(std::function<void(const int)> callback);
    void setLongClickCallback(std::function<void(const int)> callback);
    void setDoubleClickCallback(std::function<void(const int)> callback);

    virtual void setup(){};
    virtual void loop(){};

  private:
    void onBtnClick(int pin);

    void onBtnLongPress(int pin);

    void onBtnDoublePress(int pin);

  private:
    int mPin{0};              // 当前按钮的pin
    int mPindowndigital{LOW}; // 按钮初始状态
    std::function<void(const bool, const int)> mCallback;
    uint32_t mLastchangeMs{0};
    uint32_t mDebounceMs{35};
    bool mLaststateIsDown{false}; // true is down;
    ButtonHandler mButtonhandler;
    std::function<void(int)> mClickCallback;
    std::function<void(int)> mLongClickCallback;
    std::function<void(int)> mDoubleClickCallback;
};

#endif
