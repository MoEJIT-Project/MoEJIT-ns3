//
// Created by Ricardo Evans on 2023/6/4.
//

#ifndef NS3_COROUTINE_OPERATION_H
#define NS3_COROUTINE_OPERATION_H

#include <coroutine>
#include <functional>
#include <optional>
#include <vector>
#include <utility>

#include <ns3/core-module.h>

#include "awaitable.h"
#include "operation-trait.h"
#include "operation-type.h"

namespace ns3 {
    template<typename R>
    class CoroutineOperation {
    private:
        struct Promise {
            bool terminated = false;
            std::size_t reference = 0;
            std::optional<R> result;
            std::optional<std::exception_ptr> exception;
            std::vector<std::function<void(std::optional<R> &, std::optional<std::exception_ptr> &)>> continuations;

            constexpr std::suspend_never initial_suspend() const noexcept {
                return {};
            }

            constexpr ConditionalAwaitable final_suspend() const noexcept {
                return reference <= 0;
            }

            CoroutineOperation get_return_object() noexcept {
                return CoroutineOperation{Coroutine::from_promise(*this)};
            }

            template<AwaitableConcept T>
            constexpr T &&await_transform(T &&t) const noexcept {
                return std::forward<T>(t);
            }

            template<CoroutineOperationConcept T>
            constexpr auto await_transform(T &&operation) const noexcept {
                return CoroutineOperationAwaitable{std::forward<T>(operation)};
            }

            template<typename U=R>
            void terminate(U &&_result) {
                if (terminated) {
                    return;
                }
                terminated = true;
                result = std::forward<U>(_result);
            }

            template<typename ...Args>
            void terminate(std::in_place_t, Args &&...args) {
                if (terminated) {
                    return;
                }
                terminated = true;
                result.emplace(std::forward<Args>(args)...);
            }

            void return_value(const R &_result) {
                if (!terminated) {
                    result = _result;
                }
                terminated = true;
                complete();
            }

            void return_value(R &&_result) {
                if (!terminated) {
                    result = std::move(_result);
                }
                terminated = true;
                complete();
            }

            void unhandled_exception() {
                if (!terminated) {
                    exception = std::current_exception();
                }
                terminated = true;
                complete();
            }

        private:
            void complete() {
                for (auto &f: continuations) {
                    f(result, exception);
                }
                continuations.clear();
            }
        };

        inline void releaseHandle() const {
            if (handle) {
                auto &promise = handle.promise();
                --promise.reference;
                if (promise.reference <= 0 && handle.done()) {
                    handle.destroy();
                }
            }
        }

        inline void checkHandle() const {
            if (!handle) {
                throw std::runtime_error("null operation");
            }
        }

    public:
        using promise_type = Promise;
        using Coroutine = std::coroutine_handle<promise_type>;

        CoroutineOperation() noexcept = default;

        explicit CoroutineOperation(const Coroutine &handle) noexcept: handle(handle) {
            if (handle) {
                ++this->handle.promise().reference;
            }
        }

        explicit CoroutineOperation(Coroutine &&handle) noexcept: handle(std::move(handle)) {
            if (handle) {
                ++this->handle.promise().reference;
            }
        }

        CoroutineOperation(CoroutineOperation &&operation) noexcept: handle(std::exchange(operation.handle, nullptr)) {}

        CoroutineOperation(const CoroutineOperation &operation) noexcept: CoroutineOperation(operation.handle) {}

        CoroutineOperation &operator=(const CoroutineOperation &operation) noexcept {
            releaseHandle();
            handle = operation.handle;
            if (handle) {
                ++handle.promise().reference;
            }
            return *this;
        }

        CoroutineOperation &operator=(CoroutineOperation &&operation) noexcept {
            releaseHandle();
            handle = std::exchange(operation.handle, nullptr);
            return *this;
        }

        constexpr inline Coroutine coroutine() const noexcept {
            return handle;
        }

        const R &result() const {
            checkHandle();
            auto &promise = handle.promise();
            if (promise.exception.has_value()) {
                std::rethrow_exception(std::exchange(promise.exception, std::nullopt).value());
            }
            return promise.result.value();
        }

        R &result() {
            checkHandle();
            auto &promise = handle.promise();
            if (promise.exception.has_value()) {
                std::rethrow_exception(std::exchange(promise.exception, std::nullopt).value());
            }
            return promise.result.value();
        }

        void onComplete(std::function<void()> &&function) const {
            checkHandle();
            onComplete([function = std::move(function)](auto &, auto &) { function(); });
        }

        void onComplete(const std::function<void()> &function) const {
            checkHandle();
            onComplete([function](auto &, auto &) { function(); });
        }

        void onComplete(std::function<void(std::optional<R> &, std::optional<std::exception_ptr> &)> &&function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function(promise.result, promise.exception);
            } else {
                promise.continuations.emplace_back(std::move(function));
            }
        }

        void onComplete(const std::function<void(std::optional<R> &, std::optional<std::exception_ptr> &)> &function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function(promise.result, promise.exception);
            } else {
                promise.continuations.emplace_back(function);
            }
        }

        bool resume() const {
            checkHandle();
            if (done()) {
                return true;
            }
            handle.resume();
            return done();
        }

        template<typename T=R>
        void terminate(T &&result) const {
            checkHandle();
            if (done()) {
                return;
            }
            handle.promise().terminate(std::forward<T>(result));
            while (!handle.done()) {
                handle.resume();
            }
        }

        template<typename ...Args>
        void terminate(std::in_place_t, Args &&...args) const {
            checkHandle();
            if (done()) {
                return;
            }
            handle.promise().terminate(std::in_place, std::forward<Args>(args)...);
            while (!handle.done()) {
                handle.resume();
            }
        }

        bool done() const {
            checkHandle();
            return handle.done() || handle.promise().terminated;
        }

        template<typename F>
        requires std::is_invocable_v<F, R &&>
        CoroutineOperation<std::invoke_result_t<F, R &&>> then(F function) const {
            checkHandle();
            co_return function(std::move(co_await *this));
        }

        virtual ~CoroutineOperation() noexcept {
            releaseHandle();
        }

    private:
        Coroutine handle;
    };

    template<>
    class CoroutineOperation<void> {
    private:
        struct Promise {
            bool terminated = false;
            std::size_t reference = 0;
            std::optional<std::exception_ptr> exception;
            std::vector<std::function<void(std::optional<std::exception_ptr> &)>> continuations;

            constexpr std::suspend_never initial_suspend() const noexcept {
                return {};
            }

            constexpr ConditionalAwaitable final_suspend() const noexcept {
                return reference <= 0;
            }

            CoroutineOperation get_return_object() noexcept {
                return CoroutineOperation{Coroutine::from_promise(*this)};
            }

            template<AwaitableConcept T>
            constexpr T &&await_transform(T &&t) const noexcept {
                return std::forward<T>(t);
            }

            template<CoroutineOperationConcept T>
            constexpr CoroutineOperationAwaitable<CoroutineOperationResultType<T>> await_transform(T &&operation) const noexcept {
                return CoroutineOperationAwaitable<CoroutineOperationResultType<T>>{std::forward<T>(operation)};
            }

            void terminate() {
                if (terminated) {
                    return;
                }
                terminated = true;
            }

            void return_void() {
                terminated = true;
                complete();
            }

            void unhandled_exception() {
                if (!terminated) {
                    exception = std::current_exception();
                }
                terminated = true;
                complete();
            }

        private:
            void complete() {
                for (auto &f: continuations) {
                    f(exception);
                }
                continuations.clear();
            }
        };

        constexpr inline void releaseHandle() const {
            if (handle) {
                auto &promise = handle.promise();
                --promise.reference;
                if (promise.reference <= 0 && handle.done()) {
                    handle.destroy();
                }
            }
        }

        constexpr inline void checkHandle() const {
            if (!handle) {
                throw std::runtime_error("null operation");
            }
        }

    public:
        using promise_type = Promise;
        using Coroutine = std::coroutine_handle<promise_type>;

        CoroutineOperation() noexcept = default;

        explicit CoroutineOperation(const Coroutine &handle) noexcept: handle(handle) {
            if (handle) {
                ++this->handle.promise().reference;
            }
        }

        explicit CoroutineOperation(Coroutine &&handle) noexcept: handle(std::move(handle)) {
            if (handle) {
                ++this->handle.promise().reference;
            }
        }

        CoroutineOperation(CoroutineOperation &&operation) noexcept: handle(std::exchange(operation.handle, nullptr)) {}

        CoroutineOperation(const CoroutineOperation &operation) noexcept: CoroutineOperation(operation.handle) {}

        CoroutineOperation &operator=(const CoroutineOperation &operation) noexcept {
            releaseHandle();
            handle = operation.handle;
            if (handle) {
                ++handle.promise().reference;
            }
            return *this;
        }

        CoroutineOperation &operator=(CoroutineOperation &&operation) noexcept {
            releaseHandle();
            handle = std::exchange(operation.handle, nullptr);
            return *this;
        }

        constexpr inline Coroutine coroutine() const noexcept {
            return handle;
        }

        void result() const {
            checkHandle();
            auto &promise = handle.promise();
            if (promise.exception.has_value()) {
                std::rethrow_exception(promise.exception.value());
            }
        }

        void onComplete(std::function<void()> &&function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function();
            } else {
                promise.continuations.emplace_back([function = std::move(function)](auto &) { function(); });
            }
        }

        void onComplete(const std::function<void()> &function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function();
            } else {
                promise.continuations.emplace_back([function](auto &) { function(); });
            }
        }

        void onComplete(std::function<void(std::optional<std::exception_ptr> &)> &&function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function(promise.exception);
            } else {
                promise.continuations.emplace_back(std::move(function));
            }
        }

        void onComplete(const std::function<void(std::optional<std::exception_ptr> &)> &function) const {
            checkHandle();
            auto &promise = handle.promise();
            if (done()) {
                function(promise.exception);
            } else {
                promise.continuations.emplace_back(function);
            }
        }

        bool resume() const {
            checkHandle();
            if (done()) {
                return true;
            }
            handle.resume();
            return done();
        }

        void terminate() const {
            checkHandle();
            if (done()) {
                return;
            }
            handle.promise().terminate();
            while (!handle.done()) {
                handle.resume();
            }
        }

        bool done() const {
            checkHandle();
            return handle.done() || handle.promise().terminated;
        }

        template<typename F>
        requires std::is_invocable_v<F, void>
        CoroutineOperation<std::invoke_result_t<F, void>> then(F function) const {
            checkHandle();
            co_return function(co_await *this);
        }

        virtual ~CoroutineOperation() noexcept {
            releaseHandle();
        }

    private:
        Coroutine handle;
    };

    template<typename T1, typename T2>
    constexpr std::strong_ordering operator<=>(const CoroutineOperation<T1> &o1, const CoroutineOperation<T2> &o2) noexcept {
        return o1.coroutine() <=> o2.coroutine();
    }

    template<typename T1, typename T2>
    constexpr bool operator==(const CoroutineOperation<T1> &o1, const CoroutineOperation<T2> &o2) noexcept {
        return o1.coroutine() == o2.coroutine();
    }

    template<typename R>
    requires std::is_default_constructible_v<R> || std::is_void_v<R>
    CoroutineOperation<R> makeCoroutineOperation() {
        co_await std::suspend_always{};
        if constexpr (std::is_void_v<R>) {
            co_return;
        } else {
            co_return R{};
        }
    }

    template<typename R>
    CoroutineOperation<R> makeCoroutineOperation(R &&placeholder) {
        co_await std::suspend_always{};
        co_return std::forward<R>(placeholder);
    }

    template<typename R, typename P>
    requires std::is_invocable_v<P> && std::is_convertible_v<std::invoke_result<P>, R>
    CoroutineOperation<R> makeCoroutineOperationWithTimeoutByProvider(R &&placeholder, P timeout_provider, Time timeout) {
        auto operation = makeCoroutineOperation(placeholder);
        Simulator::Schedule(timeout, [operation, timeout_provider]() {
            operation.terminate(timeout_provider());
        });
    }

    template<typename R>
    CoroutineOperation<R> makeCoroutineOperationWithTimeout(R &&placeholder, R &&timeout_result, Time timeout) {
        auto operation = makeCoroutineOperation(std::forward<R>(placeholder));
        Simulator::Schedule(timeout, [operation, timeout_result = std::forward<R>(timeout_result)]() {
            operation.terminate(std::move(timeout_result));
        });
        return operation;
    }

    template<typename R>
    requires std::is_invocable_v<R>
    CoroutineOperation<std::invoke_result_t<R>> makeCoroutineOperation(R provider) {
        co_await std::suspend_always{};
        co_return provider();
    }

    template<typename C, typename R>
    requires std::is_invocable_r_v<bool, C> and std::is_invocable_v<R>
    CoroutineOperation<std::invoke_result_t<R>> makeCoroutineOperation(C condition, R provider) {
        while (!condition()) {
            co_await std::suspend_always{};
        }
        co_return provider();
    }
}

#endif //NS3_COROUTINE_OPERATION_H
