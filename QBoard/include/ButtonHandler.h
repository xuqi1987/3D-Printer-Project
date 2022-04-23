#pragma once
#ifndef BUTTONHANDLER_H_
#define BUTTONHANDLER_H_

#include <Arduino.h>
#include <functional>
#include <FunctionalInterrupt.h>

enum button_event
{
	BUTTON_EVENT_SINGLECLICK = 0,
	BUTTON_EVENT_DOUBLECLICK,
	BUTTON_EVENT_LONGCLICK
};
class ButtonHandler
{

public:
	ButtonHandler(uint32_t longclick_duration = 5000, uint32_t doubleclick_duration = 200);

	void setBtnEventCallback(std::function<void(const button_event)> callback);
	void setIsDownFunction(std::function<bool(void)> is_down_function);

	void setLongClickEnable(bool enable);
	void setDoubleClickEnable(bool enable);

	//当pin点评改变时调用
	void handleChange(bool down);
	void loop();

private:
	std::function<void(const button_event)> mCallback;
	std::function<bool(void)> is_down_function;

	bool mLongClicked{false}; //用来保证一次按下只能触发一次长按
	bool mDownHandled{false}; //标志着是否已经处理了该次down按下事件（比如已经触发了长按)
	// down->up，在规定时间内再次down就是双击，否则超时就是单击
	bool mWaitDoubleClick{false}; //标志着是否等待着双击事件
	uint32_t mDownTime{0};		  // ms
	uint32_t mUpTime{0};

	uint32_t mLongclickThreshold{5000};
	uint32_t mDoubleclickThreshold{200}; //按下释放后在此时间间隔内又按下认为是双击

	bool mLongclickEnable{false};
	bool mDoubleclickEnable{false};
};

#endif /* BUTTONHANDLER_H_ */
