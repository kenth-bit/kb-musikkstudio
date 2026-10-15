#pragma once

namespace kb
{
enum class ConnectionState
{
    Connected,
    Lost,
    Retrying
};

enum class StatusColour
{
    Green,
    Yellow,
    Red
};

/** Ren logikk for tilkobling til lydkort eller MIDI-enhet (ingen maskinvare, lett å teste).
    Statusfargen UTLEDES alltid fra tilstanden, den er aldri en fastlåst feilmelding.

    Forløp: Tilkoblet (grønn) -> enheten forsvinner -> Mistet (rød)
            -> første forsøk -> Prøver på nytt (gul) -> lykkes -> Tilkoblet (grønn). */
class ConnectionTracker
{
public:
    ConnectionState state() const noexcept { return currentState; }

    StatusColour colour() const noexcept
    {
        switch (currentState)
        {
            case ConnectionState::Connected: return StatusColour::Green;
            case ConnectionState::Retrying:  return StatusColour::Yellow;
            case ConnectionState::Lost:      break;
        }
        return StatusColour::Red;
    }

    bool isConnected() const noexcept { return currentState == ConnectionState::Connected; }

    int attempts() const noexcept { return attemptCount; }
    double lastOutageSeconds() const noexcept { return lastOutage; }

    void onConnected (double nowSeconds) noexcept
    {
        if (currentState != ConnectionState::Connected && lostAt >= 0.0)
            lastOutage = nowSeconds - lostAt;

        currentState = ConnectionState::Connected;
        attemptCount = 0;
        lostAt = -1.0;
        lastAttemptAt = -1.0;
    }

    void onLost (double nowSeconds) noexcept
    {
        if (currentState == ConnectionState::Connected)
        {
            currentState = ConnectionState::Lost;
            lostAt = nowSeconds;
            attemptCount = 0;
            lastAttemptAt = -1.0;
        }
    }

    void onRetryStarted (double nowSeconds) noexcept
    {
        if (currentState == ConnectionState::Connected)
            return;

        currentState = ConnectionState::Retrying;
        ++attemptCount;
        lastAttemptAt = nowSeconds;
    }

    /** Et forsøk mislyktes. Tilstanden blir stående som "Prøver på nytt" (gul). */
    void onRetryFailed (double nowSeconds) noexcept
    {
        if (currentState == ConnectionState::Connected)
            return;

        currentState = ConnectionState::Retrying;
        lastAttemptAt = nowSeconds;
    }

    /** Skal vi gjøre et nytt forsøk nå? */
    bool shouldRetry (double nowSeconds, double intervalSeconds) const noexcept
    {
        if (currentState == ConnectionState::Connected)
            return false;

        return lastAttemptAt < 0.0 || (nowSeconds - lastAttemptAt) >= intervalSeconds;
    }

private:
    ConnectionState currentState = ConnectionState::Lost;
    int attemptCount = 0;
    double lostAt = -1.0;
    double lastAttemptAt = -1.0;
    double lastOutage = 0.0;
};
} // namespace kb
