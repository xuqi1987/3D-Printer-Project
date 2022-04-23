#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(uint32_t longclick_duration, uint32_t doubleclick_duration)
    : mLongclickThreshold(longclick_duration),
      mDoubleclickThreshold(doubleclick_duration)
{
}

void ButtonHandler::setBtnEventCallback(std::function<void(const button_event)> callback)
{
    this->mCallback = callback;
}

void ButtonHandler::setIsDownFunction(std::function<bool(void)> is_down_function)
{
    this->is_down_function = is_down_function;
}

void ButtonHandler::setLongClickEnable(bool enable)
{
    mLongclickEnable = enable;
}

void ButtonHandler::setDoubleClickEnable(bool enable)
{
    mDoubleclickEnable = enable;
}

void ButtonHandler::handleChange(bool down)
{
    //单击：按下释放且释放一段时间内没有第二次按下
    //双击：按下释放且释放一段时间内执行第二次按下时触发
    //长按：按下一段时间内未释放
    if (down)
    { // down
        if (mWaitDoubleClick && mDoubleclickEnable)
        {
            //规定时间内第二次down了，认为是双击
            //亲测，一般情况下我的双击up->第二次down的间隔是80~100左右
            // Serial.println(String("doubleclick->duration=") + (millis() - mUpTime));
            mDownHandled = true;
            // key2DoDoubleClick();
            if (mCallback)
            {
                mCallback(BUTTON_EVENT_DOUBLECLICK);
            }
        }
        else
        {
            //第一次按下
            mDownHandled = false;
        }
        mDownTime = millis();
        mLongClicked = false;
        mWaitDoubleClick = false;
    }
    else
    { // up
        if (!mDownHandled)
        {
            if (mDoubleclickEnable)
            {
                //在loop中延时等待第二次按下
                mUpTime = millis();
                mWaitDoubleClick = true;
            }
            else
            {
                mDownHandled = true;
                if (mCallback)
                {
                    mCallback(BUTTON_EVENT_SINGLECLICK);
                }
            }
        }
    }
}

void ButtonHandler::loop()
{
    bool down = is_down_function();
    if (down)
    {
        if (mLongclickEnable)
        {
            if (!mLongClicked && !mDownHandled)
            {
                if (millis() - mDownTime > mLongclickThreshold)
                {
                    // key2DoLongClick();
                    mLongClicked = true;
                    mDownHandled = true;
                    if (mCallback)
                    {
                        mCallback(BUTTON_EVENT_LONGCLICK);
                    }
                }
            }
        }
    }
    else
    { // up
        mLongClicked = false;
        if (mWaitDoubleClick && mDoubleclickEnable)
        {
            if (millis() - mUpTime > mDoubleclickThreshold)
            {
                mWaitDoubleClick = false;
                mDownHandled = true;
                // key2DoClick();
                if (mCallback)
                {
                    mCallback(BUTTON_EVENT_SINGLECLICK);
                }
            }
        }
    }
}