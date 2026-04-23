//
// Created by Ricardo Evans on 2023/6/12.
//

#ifndef NS3_MPI_APPLICATION_UTIL_H
#define NS3_MPI_APPLICATION_UTIL_H

#include <chrono>
#include <ratio>

#include <cxxabi.h>

#include <ns3/core-module.h>
#include <ns3/network-module.h>

namespace ns3 {
    constexpr std::string format(const Socket::SocketErrno &err) {
        switch (err) {
            case ns3::Socket::ERROR_NOTERROR:
                return "ERROR_NOTERROR";
            case ns3::Socket::ERROR_ISCONN:
                return "ERROR_ISCONN";
            case ns3::Socket::ERROR_NOTCONN:
                return "ERROR_NOTCONN";
            case ns3::Socket::ERROR_MSGSIZE:
                return "ERROR_MSGSIZE";
            case ns3::Socket::ERROR_AGAIN:
                return "ERROR_AGAIN";
            case ns3::Socket::ERROR_SHUTDOWN:
                return "ERROR_SHUTDOWN";
            case ns3::Socket::ERROR_OPNOTSUPP:
                return "ERROR_OPNOTSUPP";
            case ns3::Socket::ERROR_AFNOSUPPORT:
                return "ERROR_AFNOSUPPORT";
            case ns3::Socket::ERROR_INVAL:
                return "ERROR_INVAL";
            case ns3::Socket::ERROR_BADF:
                return "ERROR_BADF";
            case ns3::Socket::ERROR_NOROUTETOHOST:
                return "ERROR_NOROUTETOHOST";
            case ns3::Socket::ERROR_NODEV:
                return "ERROR_NODEV";
            case ns3::Socket::ERROR_ADDRNOTAVAIL:
                return "ERROR_ADDRNOTAVAIL";
            case ns3::Socket::ERROR_ADDRINUSE:
                return "ERROR_ADDRINUSE";
            case ns3::Socket::SOCKET_ERRNO_LAST:
                return "SOCKET_ERRNO_LAST";
            default:
                throw std::runtime_error("Unknown error code");
        }
    }

    template<typename T>
    std::function<void(T &&)> discard = [](T &&) {};

    template<typename R, typename P>
    ns3::Time convert(const std::chrono::duration<R, P> &duration) {
        using fs = std::chrono::duration<std::intmax_t, std::femto>;
        using ps = std::chrono::duration<std::intmax_t, std::pico>;
        using ns = std::chrono::nanoseconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using s = std::chrono::seconds;
        using m = std::chrono::minutes;
        using h = std::chrono::hours;
        using d = std::chrono::duration<std::intmax_t, std::ratio_multiply<h::period, std::ratio<24>>>;
        using y = std::chrono::duration<std::intmax_t, std::ratio_multiply<d::period, std::ratio<365>>>;
        switch (ns3::Time::GetResolution()) {
            case Time::FS:
                return ns3::Time{std::chrono::duration_cast<fs>(duration).count()};
            case Time::PS:
                return ns3::Time{std::chrono::duration_cast<ps>(duration).count()};
            case Time::NS:
                return ns3::Time{std::chrono::duration_cast<ns>(duration).count()};
            case Time::US:
                return ns3::Time{std::chrono::duration_cast<us>(duration).count()};
            case Time::MS:
                return ns3::Time{std::chrono::duration_cast<ms>(duration).count()};
            case Time::S:
                return ns3::Time{std::chrono::duration_cast<s>(duration).count()};
            case Time::MIN:
                return ns3::Time{std::chrono::duration_cast<m>(duration).count()};
            case Time::H:
                return ns3::Time{std::chrono::duration_cast<h>(duration).count()};
            case Time::D:
                return ns3::Time{std::chrono::duration_cast<d>(duration).count()};
            case Time::Y:
                return ns3::Time{std::chrono::duration_cast<y>(duration).count()};
            default:
                throw std::runtime_error("unsupported ns3 time resolution: " + std::to_string(ns3::Time::GetResolution()));
        }
    }

    template<typename R, typename P>
    std::chrono::duration<R, P> convert(const Time &duration) {
        using fs = std::chrono::duration<std::intmax_t, std::femto>;
        using ps = std::chrono::duration<std::intmax_t, std::pico>;
        using ns = std::chrono::nanoseconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using s = std::chrono::seconds;
        using m = std::chrono::minutes;
        using h = std::chrono::hours;
        using d = std::chrono::duration<std::intmax_t, std::ratio_multiply<h::period, std::ratio<24>>>;
        using y = std::chrono::duration<std::intmax_t, std::ratio_multiply<d::period, std::ratio<365>>>;
        switch (ns3::Time::GetResolution()) {
            case Time::FS:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(fs{duration.GetTimeStep()});
            case Time::PS:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(ps{duration.GetTimeStep()});
            case Time::NS:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(ns{duration.GetTimeStep()});
            case Time::US:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(us{duration.GetTimeStep()});
            case Time::MS:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(ms{duration.GetTimeStep()});
            case Time::S:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(s{duration.GetTimeStep()});
            case Time::MIN:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(m{duration.GetTimeStep()});
            case Time::H:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(h{duration.GetTimeStep()});
            case Time::D:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(d{duration.GetTimeStep()});
            case Time::Y:
                return std::chrono::duration_cast<std::chrono::duration<R, P>>(y{duration.GetTimeStep()});
            default:
                throw std::runtime_error("unsupported ns3 time resolution: " + std::to_string(ns3::Time::GetResolution()));
        }
    }

    Address retrieveIPAddress(const Address &address);

    uint16_t retrievePort(const Address &address);

    Address addressWithPort(const Address &address, uint16_t port);

    inline void log(const std::string &name, LogLevel level, const std::string &message) noexcept {
        [[maybe_unused]] LogComponent &g_log = GetLogComponent(name);
        NS_LOG(level, message);
    }

    [[maybe_unused]] inline void logError(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_ERROR, message);
    }

    [[maybe_unused]] inline void logWarn(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_WARN, message);
    }

    [[maybe_unused]] inline void logInfo(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_INFO, message);
    }

    [[maybe_unused]] inline void logFunction(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_FUNCTION, message);
    }

    [[maybe_unused]] inline void logLogic(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_LOGIC, message);
    }

    [[maybe_unused]] inline void logDebug(const std::string &name, const std::string &message) noexcept {
        log(name, LOG_DEBUG, message);
    }

    template<typename T, typename ...S>
    std::string to_string(const T &t, const S &...s) {
        return std::format("{}, {}", std::to_string(t), to_string(s...));
    }

    template<typename T>
    std::string to_string(const T &t) {
        return std::to_string(t);
    }

    constexpr std::string to_string() {
        return "";
    }

    template<typename... T>
    std::string to_string(const std::tuple<T...> &t) {
        return std::apply([](const auto &...args) { return to_string(args...); }, t);
    }

    template<typename T>
    std::string getTypename() {
        char *s = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
        std::string name{s};
        free(s);
        return name;
    }
}

#endif //NS3_MPI_APPLICATION_UTIL_H
