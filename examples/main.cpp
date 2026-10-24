//
// Created by thedmkn on 24.10.2026.
//

#pragma once
#include <iostream>
#include <astro/astro.hxx>

[[maybe_unused]] static void listener_a(const int &number) {
    std::cout << "Constant event: " << number << std::endl;
}

[[maybe_unused]] static void listener_b(int &number) {
    std::cout << "Mutable event: " << number << std::endl;
    number++;
    std::cout << "New value: " << number << std::endl;
}

[[maybe_unused]] static void listener_c(const int &number) {
    std::cout << "Another constant event: " << number << std::endl;
}

[[maybe_unused]] static void listener_d(const int &number) {
    std::cout << "Second another constant event: " << number << std::endl;
}


void basf() {
    astro::event_bus bus{"example"};

    const auto listener = [](const int& event) -> void {
        std::cout << "Event " << event << "\n";
    };

    bus.subscribe_constant<int>(listener);

    bus.post(5);
}

[[maybe_unused]] inline void subscribe_basic() {
    astro::event_bus bus{"subscribe_basic"};

    bus.subscribe_constant<int>(listener_a);
    bus.subscribe_mutable<int>(listener_b);

    int number = 6;

    bus.post<int>(7);
    //Output:
    // Constant event: 7

    bus.post(number);

    //Output:
    // Constant event: 6
    // Mutable event: 6
    // New value: 7
}


[[maybe_unused]] inline void subscribe_prio() {
    astro::event_bus bus{"subscribe_prio"};
    bus.subscribe_constant<int>(listener_a, 1);
    bus.subscribe_mutable<int>(listener_b, 1);
    bus.subscribe_mutable<int>(listener_d, 0); // lowest
    bus.subscribe_constant<int>(listener_c, 2); // highest

    bus.subscribe_mutable<int>([](int& event) -> void {
        std::cout << "value: " << event << std::endl;
    }, 1);

    int number = 1;
    bus.post(number);
    //Output:
    // Another constant event: 1
    // Constant event: 1
    // Mutable event: 1
    // New value: 2
    // Second another constant event: 2
}


struct my_event1 {
    std::string data = "data";
};

struct my_event2 {
    int counter = 0;
};

class example_class : public astro::subscriber<example_class> {
public:
    explicit example_class(astro::event_bus &bus) : subscriber(bus) {}


    [[maybe_unused]] void my_event_listener1(const my_event1 &event) {
        std::cout << "my_event_listener1: " << event.data << std::endl;
    }

    subscribe_constant(my_event1, my_event_listener1);

    declare_constant_listener(my_event_listener2, my_event1) {
        std::cout << "my_event_listener2: " << event.data << std::endl;
    }

    declare_constant_listener_prio(my_event_listener3, my_event1, astro::event_bus::priority_lowest) {
        std::cout << "my_event_listener3: " << event.data << std::endl;
    }

    declare_mutable_listener_prio(my_event_listener4, my_event2, astro::event_bus::priority_highest) {
        std::cout << "my_event_listener4: " << event.counter << std::endl;
        event.counter++;
    }

    declare_mutable_listener_prio(my_event_listener5, my_event2, astro::event_bus::priority_lowest) {
        std::cout << "my_event_listener4: " << event.counter << std::endl;
        event.counter+= 2;
    }
};

[[maybe_unused]] inline void subscribe_class() {

    astro::event_bus bus{"subscribe_class"};
    example_class klass{bus};

    bus.post<my_event1>({});

    my_event2 event;

    bus.post(event);

    std::cout << "value: " << event.counter << std::endl;

}
