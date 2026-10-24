//
// Created by thedmkn on 24.10.2026.
//

#ifndef ASTRO_ASTRO_HXX
#define ASTRO_ASTRO_HXX
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <mutex>
#include <typeindex>
#include <functional>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <vector>

namespace astro {
    class event_bus final {
    public:
        using priority_t = std::uint8_t;

        struct subscription_id {
            std::uint64_t id_;
            std::type_index type_id_;
        };

        static constexpr priority_t priority_lowest = std::numeric_limits<priority_t>::min();
        static constexpr priority_t priority_highest = std::numeric_limits<priority_t>::max();

        explicit event_bus(std::string name = "root") : name_(std::move(name)) {
        }

        template<typename T, typename F>
        subscription_id subscribe_constant(F &&f, priority_t priority = priority_lowest) {
            return add<T, false>(std::forward<F>(f), priority);
        }

        template<typename T, typename F>
        subscription_id subscribe_mutable(F &&f, priority_t priority = priority_lowest) {
            return add<T, true>(std::forward<F>(f), priority);
        }

        void unsubscribe(const subscription_id &id) {
            //
            entries_ptr old; // NOLINT
            {
                std::unique_lock lock(mutex_);
                const auto it = listeners_.find(id.type_id_);
                if (it == listeners_.end()) return;

                auto next = std::make_shared<entries_t>(*it->second);
                std::erase_if(*next, [&](const listener_entry &e) { return e.id_ == id.id_; });

                old = std::move(it->second); // старый shared_ptr освобождается здесь, под локом
                if (next->empty()) listeners_.erase(it);
                else it->second = std::move(next);
            }
        }

        template<typename T>
        std::size_t post(T &&obj) const {
            using clean_t = std::remove_cvref_t<T>;
            constexpr bool const_obj =
                    std::is_const_v<std::remove_reference_t<T> > || !std::is_lvalue_reference_v<T>;

            entries_ptr snapshot;
            {
                std::shared_lock lock(mutex_);
                const auto it = listeners_.find(std::type_index(typeid(clean_t)));
                if (it == listeners_.end()) return 0;
                snapshot = it->second;
            }
            if (!snapshot) return 0;

            void *const p = const_cast<clean_t *>(std::addressof(obj));
            std::size_t fired = 0;
            for (const auto &e: *snapshot) {
                if (const_obj && e.mutable_) continue;
                e.listener_(p);
                ++fired;
            }
            return fired;
        }

        static std::shared_ptr<event_bus> make_shared(const std::string &name = "root") {
            return std::make_shared<event_bus>(name);
        }


        [[nodiscard]] std::string& name() const {
            return name_;
        }

    private:
        struct listener_entry {
            priority_t priority_;
            bool mutable_;
            std::uint64_t id_;
            std::function<void(void *)> listener_;
        };

        using entries_t = std::vector<listener_entry>;
        using entries_ptr = std::shared_ptr<const entries_t>;

        template<typename T, bool mut, typename F>
        subscription_id add(F &&f, priority_t priority) {
            using clean_t = std::remove_cvref_t<T>;
            using arg_t = std::conditional_t<mut, clean_t &, const clean_t &>;
            static_assert(std::is_invocable_v<std::decay_t<F> &, arg_t>,
                          "Listener must be invocable with the event type "
                          "T& (mutable) or const T& (constant)");

            const std::type_index key{typeid(clean_t)};
            const auto id = next_id_.fetch_add(1, std::memory_order_relaxed);

            listener_entry entry{
                priority, mut, id,
                [fn = std::forward<F>(f)](void *p) mutable {
                    fn(*static_cast<clean_t *>(p));
                }
            };

            entries_ptr old; // NOLINT
            {
                std::unique_lock lock(mutex_);
                auto &slot = listeners_[key];
                auto next = slot
                                ? std::make_shared<entries_t>(*slot)
                                : std::make_shared<entries_t>();

                const auto pos = std::upper_bound(
                    next->begin(), next->end(), priority,
                    [](priority_t p, const listener_entry &e) { return p > e.priority_; });
                next->insert(pos, std::move(entry));

                old = std::move(slot); // старый shared_ptr освобождается здесь, под локом
                slot = std::move(next);
            }
            return {id, key};
        }

        std::string name_;
        std::unordered_map<std::type_index, entries_ptr> listeners_;
        std::atomic<std::uint64_t> next_id_{1};
        mutable std::shared_mutex mutex_;
    };


    // хранит статик список листенеров для определенного типа
    template<typename D>
    struct listener_registry {
        using binder_t = void (*)(D &);

        static std::vector<binder_t> &all() {
            static std::vector<binder_t> v;
            return v;
        }

        static bool add(binder_t b) {
            all().push_back(b);
            return true;
        }
    };

    template<typename D>
    class subscriber {
    public:
        subscriber(const subscriber &) = delete;

        subscriber(subscriber &) = delete;

        subscriber(subscriber &&) = delete;

        subscriber &operator=(const subscriber &) = delete;

        subscriber &operator=(subscriber &) = delete;

        subscriber &operator=(subscriber &&) = delete;

    protected:
        using _subscriber_self_t = D;

        explicit subscriber(event_bus &bus)
            : bus_(&bus), guard_(std::make_shared<guard>()) {
            subscribe_listeners();
        }


        ///fixme: Небезопасно т.к. release идет после того как guard слишком поздно отрабатывает
        ~subscriber() { release(); }

        void subscribe_listeners() {
            for (auto b: listener_registry<D>::all())
                b(static_cast<D &>(*this));
        }

        void release() {
            for (auto &id: subs_) bus_->unsubscribe(id);
            subs_.clear();
            std::unique_lock l(guard_->m);
            guard_->dead = true;
        }

        template<typename E, bool mut, typename M>
        void bind(D *self, M method, event_bus::priority_t prio) {
            auto fn = [self, method, g = guard_](auto &e) {
                std::shared_lock l(g->m);
                if (!g->dead) (self->*method)(e);
            };
            if constexpr (mut) subs_.push_back(bus_->subscribe_mutable<E>(std::move(fn), prio));
            else subs_.push_back(bus_->subscribe_constant<E>(std::move(fn), prio));
        }

    private:
        ///fixme: deadlock maybe?
        struct guard {
            std::shared_mutex m;
            bool dead = false;
        };

        event_bus *bus_;
        std::shared_ptr<guard> guard_;
        std::vector<event_bus::subscription_id> subs_;
    };

#define event_handler_impl(E, method, mut, prio)                                  \
    static void event_bind_##method##_(_subscriber_self_t &s) {                               \
        s.template bind<E, mut>(&s, &_subscriber_self_t::method, prio);                       \
    }                                                                             \
    [[maybe_unused]] inline static const bool event_reg_##method##_ =             \
        astro::listener_registry<_subscriber_self_t>::add(&_subscriber_self_t::event_bind_##method##_)

#define subscribe_constant(E, method)          event_handler_impl(E, method, false, astro::event_bus::priority_lowest)
#define subscribe_mutable(E, method)      event_handler_impl(E, method, true,  astro::event_bus::priority_lowest)
#define subscribe_prio_constant(E, method, prio)  event_handler_impl(E, method, false, prio)
#define subscribe_prio_mutable(E, method, prio)  event_handler_impl(E, method, true, prio)


#define declare_constant_listener(name, T) [[maybe_unused]]     subscribe_constant(T,name); \
    [[maybe_unused]] void name(const T& event)
}

#define declare_mutable_listener(name, T) [[maybe_unused]]     subscribe_mutable(T,name); \
    [[maybe_unused]] void name(T& event)


#define declare_constant_listener_prio(name, T, prio) [[maybe_unused]]     subscribe_prio_constant(T,name,prio); \
    [[maybe_unused]] void name(const T& event)

#define declare_mutable_listener_prio(name, T, prio) [[maybe_unused]]     subscribe_prio_mutable(T,name,prio); \
    [[maybe_unused]] void name(T& event)


#endif //ASTRO_ASTRO_HXX
