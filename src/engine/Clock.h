#ifndef NOT_A_GAME_CLOCK_H
#define NOT_A_GAME_CLOCK_H

namespace engine {
namespace clock {

struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

class Clock {
public:
    Clock(int monthsPerYear = 12, int daysPerMonth = 30);

    void tick(float dtSeconds);
    void addSeconds(double seconds);
    void pause();
    void resume();
    bool isPaused() const;

    void setTimeScale(float scale);
    float timeScale() const;

    /*!
     * "Game time" delta: real seconds, scaled by timeScale, zeroed when
     * paused. Feed this to scene-graph / world simulation that should
     * freeze on pause and speed up with timeScale. Use the raw real-time
     * dt for player-facing things (turn timer, action duration, UI).
     */
    float gameDt(float realDtSeconds) const;

    void setDateTime(int year, int month, int day, int hour, int minute, int second);
    void setDateTime(const DateTime& dt);
    void setTimeOfDay(int hour, int minute, int second);

    const DateTime& now() const;
    int  year() const;
    int  month() const;
    int  day() const;
    int  hour() const;
    int  minute() const;
    int  second() const;
    int  dayOfYear() const;
    int  monthsPerYear() const;
    int  daysPerMonth() const;
    int  daysPerYear() const;
    double totalSeconds() const;
    double timeOfDaySeconds() const;

    static void selfTest();

private:
    void recompute();
    void secondsToDateTime(double seconds, DateTime& out) const;
    double dateTimeToSeconds(int year, int month, int day, int hour, int minute, int second) const;

    int     monthsPerYear_;
    int     daysPerMonth_;
    int     daysPerYear_;
    double  totalSeconds_;
    float   timeScale_;
    bool    paused_;
    DateTime cached_;
};

}
}

#endif
