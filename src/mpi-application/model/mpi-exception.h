//
// Created by Ricardo Evans on 2023/6/19.
//

#ifndef NS3_MPI_APPLICATION_EXCEPTION_H
#define NS3_MPI_APPLICATION_EXCEPTION_H

#include <exception>
#include <string>

class MPIException : public std::exception {
private:
    std::string message;
public:
    explicit MPIException(const std::string &message) : message(message) {}

    explicit MPIException(std::string &&message) : message(message) {}

    explicit MPIException(const MPIException &) = default;

    explicit MPIException(MPIException &&) = default;

    MPIException &operator=(const MPIException &) = default;

    MPIException &operator=(MPIException &&) = default;

    const char *what() const noexcept override {
        return message.c_str();
    }
};

class CoroutineSocketException : public MPIException {
public:
    explicit CoroutineSocketException(const std::string &message) : MPIException(message) {}

    explicit CoroutineSocketException(std::string &&message) : MPIException(message) {}

    explicit CoroutineSocketException(const CoroutineSocketException &) = default;

    explicit CoroutineSocketException(CoroutineSocketException &&exception) = default;

    CoroutineSocketException &operator=(const CoroutineSocketException &) = default;

    CoroutineSocketException &operator=(CoroutineSocketException &&) = default;
};

#endif //NS3_MPI_APPLICATION_EXCEPTION_H
