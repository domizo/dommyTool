#pragma once

class MComputation
{
public:
    MComputation() : active_(false) {}

    void beginComputation(
        bool = false, bool = true, bool = true)
    {
        active_ = true;
        ++beginCount();
    }

    bool isInterruptRequested()
    {
        return interruptRequested();
    }

    void endComputation()
    {
        if (active_) {
            active_ = false;
            ++endCount();
        }
    }

    static int& beginCount()
    {
        static int value = 0;
        return value;
    }

    static int& endCount()
    {
        static int value = 0;
        return value;
    }

    static bool& interruptRequested()
    {
        static bool value = false;
        return value;
    }

private:
    bool active_;
};
