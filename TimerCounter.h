/**
 * TimerCounter.h
 * Declaration of TimerCounter class.
 * (c) 2023 Diego Quesada
*/

#pragma once

#include <limits.h>

/**
 * A utility that tracks the last time an event occurred.
 */
class TimerCounter
{
public:
    TimerCounter(unsigned long delay) : _delay(delay) { }
    void setDelay(unsigned long delay) { _delay = delay; }
    bool IsItTime()
    {
        unsigned long currentTime = millis();
        bool needsUpdate = false;

        if (_lastTime == 0) // special case: first time
        {
            needsUpdate = true;
        }
        else if (currentTime < _lastTime) // we wrapped
        { 
            needsUpdate = (ULONG_MAX - _lastTime + 1 + currentTime) > _delay;
        }
        else
        {
            needsUpdate = (currentTime - _lastTime) > _delay;
        }

        return needsUpdate;
    }
    void Reset()
    {
        _lastTime = millis();
        if (_lastTime == 0) _lastTime++; // zero is a special case.
    }
    void Dump()
    {
        Serial.print("_delay = ");
        Serial.print(_delay);
        Serial.print(", _lastTime = ");
        Serial.print(_lastTime);
        Serial.print(", current = ");
        Serial.println(millis());
    }

private:
    unsigned long _delay = 0; /// Frequency with which the timer fires, in ms.
    unsigned long _lastTime = 0; /// Last time timer fired, in ms. 0 means it has never fired.
};
