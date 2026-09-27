#include "Clock.h"

#include <cassert>
#include <cmath>

#include "AndroidOut.h"
#include "Math.h"

namespace engine {
namespace clock {

namespace {

constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour   = 60 * kSecondsPerMinute;
constexpr int kSecondsPerDay    = 24 * kSecondsPerHour;

}

Clock::Clock(int monthsPerYear, int daysPerMonth)
        : monthsPerYear_(monthsPerYear > 0 ? monthsPerYear : 12),
          daysPerMonth_(daysPerMonth > 0 ? daysPerMonth : 30),
          daysPerYear_(monthsPerYear_ * daysPerMonth_),
          totalSeconds_(0.0),
          timeScale_(1.0f),
          paused_(false) {
    cached_ = {1, 1, 1, 0, 0, 0};
}

void Clock::tick(float dtSeconds) {
    if (paused_ || dtSeconds <= 0.0f) return;
    totalSeconds_ += static_cast<double>(dtSeconds) * static_cast<double>(timeScale_);
    recompute();
}

void Clock::addSeconds(double seconds) {
    totalSeconds_ += seconds;
    recompute();
}

void Clock::pause()    { paused_ = true; }
void Clock::resume()   { paused_ = false; }
bool Clock::isPaused() const { return paused_; }

float Clock::gameDt(float realDtSeconds) const {
    if (paused_ || realDtSeconds <= 0.0f) return 0.0f;
    return realDtSeconds * timeScale_;
}

void Clock::setTimeScale(float scale) {
    assert(scale > 0.0f);
    timeScale_ = scale;
}

float Clock::timeScale() const { return timeScale_; }

void Clock::setDateTime(int year, int month, int day, int hour, int minute, int second) {
    assert(year >= 1);
    assert(month >= 1 && month <= monthsPerYear_);
    assert(day >= 1 && day <= daysPerMonth_);
    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(second >= 0 && second < 60);
    totalSeconds_ = dateTimeToSeconds(year, month, day, hour, minute, second);
    recompute();
}

void Clock::setDateTime(const DateTime& dt) {
    setDateTime(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

void Clock::setTimeOfDay(int hour, int minute, int second) {
    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(second >= 0 && second < 60);
    int dayOffset = (hour * kSecondsPerHour + minute * kSecondsPerMinute + second)
                    / kSecondsPerDay;
    int h = hour % 24;
    int m = minute;
    int s = second;
    int targetSecOfDay = h * kSecondsPerHour + m * kSecondsPerMinute + s;
    double currentSecOfDay = totalSeconds_ - std::floor(totalSeconds_ / kSecondsPerDay) * kSecondsPerDay;
    double delta = static_cast<double>(targetSecOfDay) - currentSecOfDay;
    totalSeconds_ += delta + static_cast<double>(dayOffset) * kSecondsPerDay;
    recompute();
}

const DateTime& Clock::now() const { return cached_; }

int Clock::year() const        { return cached_.year; }
int Clock::month() const       { return cached_.month; }
int Clock::day() const         { return cached_.day; }
int Clock::hour() const        { return cached_.hour; }
int Clock::minute() const      { return cached_.minute; }
int Clock::second() const      { return cached_.second; }
int Clock::dayOfYear() const   { return (cached_.month - 1) * daysPerMonth_ + cached_.day; }
int Clock::monthsPerYear() const { return monthsPerYear_; }
int Clock::daysPerMonth() const  { return daysPerMonth_; }
int Clock::daysPerYear() const   { return daysPerYear_; }

double Clock::totalSeconds() const {
    return totalSeconds_;
}

double Clock::timeOfDaySeconds() const {
    double sec = totalSeconds_ - std::floor(totalSeconds_ / kSecondsPerDay) * kSecondsPerDay;
    return sec;
}

void Clock::recompute() {
    secondsToDateTime(totalSeconds_, cached_);
}

double Clock::dateTimeToSeconds(int year, int month, int day, int hour, int minute, int second) const {
    long long yearDays  = static_cast<long long>(year - 1) * static_cast<long long>(daysPerYear_);
    long long monthDays = static_cast<long long>(month - 1) * static_cast<long long>(daysPerMonth_);
    long long dDays     = static_cast<long long>(day - 1);
    long long totalDays = yearDays + monthDays + dDays;
    return static_cast<double>(totalDays) * static_cast<double>(kSecondsPerDay)
           + hour   * kSecondsPerHour
           + minute * kSecondsPerMinute
           + second;
}

void Clock::secondsToDateTime(double seconds, DateTime& out) const {
    long long totalSec = static_cast<long long>(std::floor(seconds));
    long long secOfDay = ((totalSec % kSecondsPerDay) + kSecondsPerDay) % kSecondsPerDay;
    long long dayIndex = totalSec / kSecondsPerDay;
    if (totalSec < 0 && secOfDay != 0) dayIndex -= 1;
    long long yearCount = dayIndex / daysPerYear_;
    long long dayOfYearAbs = dayIndex - yearCount * daysPerYear_;
    if (dayOfYearAbs < 0) {
        yearCount -= 1;
        dayOfYearAbs += daysPerYear_;
    }
    long long monthIndex = dayOfYearAbs / daysPerMonth_;
    long long dayOfMonth = dayOfYearAbs - monthIndex * daysPerMonth_;
    out.year   = static_cast<int>(yearCount) + 1;
    out.month  = static_cast<int>(monthIndex) + 1;
    out.day    = static_cast<int>(dayOfMonth) + 1;
    out.hour   = static_cast<int>(secOfDay / kSecondsPerHour);
    out.minute = static_cast<int>((secOfDay % kSecondsPerHour) / kSecondsPerMinute);
    out.second = static_cast<int>(secOfDay % kSecondsPerMinute);
}

void Clock::selfTest() {
    aout << "[clock] self-test starting" << std::endl;

    {
        Clock c;
        const DateTime& t = c.now();
        assert(t.year == 1 && t.month == 1 && t.day == 1);
        assert(t.hour == 0 && t.minute == 0 && t.second == 0);
        assert(c.dayOfYear() == 1);
        assert(c.totalSeconds() == 0.0);
        assert(c.timeOfDaySeconds() == 0.0);
    }

    {
        Clock c;
        c.tick(1.0f);
        assert(math::approxEqual(c.totalSeconds(), 1.0, 1e-9));
        assert(c.now().second == 1);
    }

    {
        Clock c;
        c.tick(60.0f);
        assert(c.now().minute == 1);
        assert(c.now().second == 0);
    }

    {
        Clock c;
        c.tick(3600.0f);
        assert(c.now().hour == 1);
        assert(c.now().minute == 0);
        assert(c.now().second == 0);
    }

    {
        Clock c;
        c.tick(86400.0f);
        assert(c.now().day == 2);
        assert(c.now().hour == 0);
    }

    {
        Clock c;
        c.setTimeScale(1440.0f);
        c.tick(1.0f);
        assert(c.now().hour == 0);
        assert(c.now().minute == 24);
        assert(c.now().second == 0);
        c.tick(60.0f);
        assert(c.now().day == 2);
        assert(c.now().hour == 0);
    }

    {
        Clock c;
        c.setDateTime(2, 1, 1, 0, 0, 0);
        assert(c.now().year == 2);
        assert(c.dayOfYear() == 1);
        double expected = 360.0 * 86400.0;
        assert(math::approxEqual(c.totalSeconds(), expected, 1e-6));
    }

    {
        Clock c;
        c.setDateTime(1, 1, 30, 23, 59, 59);
        c.tick(1.0f);
        const DateTime& t = c.now();
        assert(t.year == 1);
        assert(t.month == 2);
        assert(t.day == 1);
        assert(t.hour == 0);
        assert(t.minute == 0);
        assert(t.second == 0);
    }

    {
        Clock c;
        c.setDateTime(1, 12, 30, 23, 59, 59);
        c.tick(1.0f);
        const DateTime& t = c.now();
        assert(t.year == 2);
        assert(t.month == 1);
        assert(t.day == 1);
        assert(c.dayOfYear() == 1);
    }

    {
        Clock c;
        c.setDateTime(1, 6, 15, 12, 0, 0);
        c.setTimeOfDay(18, 30, 0);
        const DateTime& t = c.now();
        assert(t.year == 1 && t.month == 6 && t.day == 15);
        assert(t.hour == 18 && t.minute == 30 && t.second == 0);
    }

    {
        Clock c;
        c.setDateTime(1, 6, 15, 23, 0, 0);
        c.setTimeOfDay(1, 0, 0);
        const DateTime& t = c.now();
        assert(t.day == 15);
        assert(t.hour == 1);
    }

    {
        Clock c;
        c.pause();
        c.setDateTime(1, 1, 1, 0, 0, 0);
        c.tick(100.0f);
        assert(c.totalSeconds() == 0.0);
        c.resume();
        c.tick(100.0f);
        assert(math::approxEqual(c.totalSeconds(), 100.0, 1e-9));
    }

    {
        Clock c(10, 10);
        assert(c.monthsPerYear() == 10);
        assert(c.daysPerMonth() == 10);
        assert(c.daysPerYear() == 100);
        c.setDateTime(3, 5, 7, 12, 0, 0);
        assert(c.now().year == 3);
        assert(c.now().month == 5);
        assert(c.now().day == 7);
        c.tick(86400.0f);
        assert(c.now().day == 8);
    }

    {
        Clock c;
        double start = c.totalSeconds();
        c.tick(0.0f);
        assert(c.totalSeconds() == start);
        c.tick(-5.0f);
        assert(c.totalSeconds() == start);
    }

    {
        Clock c;
        c.setDateTime(1, 1, 1, 3, 25, 45);
        for (int i = 0; i < 1800; ++i) {
            c.tick(86400.0f);
        }
        const DateTime& t = c.now();
        assert(t.year == 6);
        assert(t.month == 1);
        assert(t.day == 1);
        assert(t.hour == 3);
        assert(t.minute == 25);
        assert(t.second == 45);
    }

    {
        Clock c;
        c.setTimeScale(2.0f);
        c.tick(12.0f);
        assert(c.now().minute == 0);
        assert(c.now().second == 24);
    }

    {
        Clock c;
        c.pause();
        c.addSeconds(7200.0);
        assert(c.now().hour == 2);
        assert(c.now().minute == 0);
        c.resume();
        c.setTimeScale(1440.0f);
        c.addSeconds(60.0);
        assert(c.now().minute == 1);
    }

    {
        Clock c;
        assert(c.gameDt(1.0f) == 1.0f);
        c.setTimeScale(2.0f);
        assert(math::approxEqual(c.gameDt(1.0f), 2.0f, 1e-6f));
        assert(math::approxEqual(c.gameDt(0.5f), 1.0f, 1e-6f));
        c.pause();
        assert(c.gameDt(1.0f) == 0.0f);
        assert(c.gameDt(100.0f) == 0.0f);
        c.resume();
        assert(math::approxEqual(c.gameDt(1.0f), 2.0f, 1e-6f));
        assert(c.gameDt(-1.0f) == 0.0f);
        assert(c.gameDt(0.0f) == 0.0f);
    }

    aout << "[clock] self-test PASS" << std::endl;
}

}
}
