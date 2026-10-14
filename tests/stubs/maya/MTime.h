#pragma once

class MTime
{
public:
    // Identity tags for forwarding tests; this double does not convert units.
    enum Unit { kFilm, kSeconds, kNTSCFrame };
    explicit MTime(double value = 0.0, Unit unit = kFilm) : value_(value), unit_(unit) {}

    double value() const { return value_; }
    Unit unit() const { return unit_; }

    static double& currentValue()
    {
        static double value = 0.0;
        return value;
    }

private:
    double value_;
    Unit unit_;
};
