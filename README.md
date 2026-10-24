<div align="center">

# Astro

###  Event system for C++20

</div>

### Features:
 - Thread safety
 - Event priories
 - Constant & Mutable events

### Usage:

```c++
// 1. Create an event bus
astro::event_bus bus{"example"};

// 2. Define a listener (function or lambda)
const auto listener = [](const int& event) -> void {
   std::cout << "Event " << event << "\n";  
};

// 3. Subscribe
bus.subscribe_constant<int>(listener);

// 4. Post an event
bus.post(5);
```

```c++
struct event{ int counter = 0; };

class example_listener : public astro::subscriber<example_listener> {
public:
    explicit example_listener(astro::event_bus &bus) : subscriber(bus) {}
    
    declare_mutable_listener(on_event, event) {
        std::cout << "Event: " << event.counter << std::endl;
        event.counter++;
    }
};

int main() {
    astro::event_bus bus{"example_listener"};
    example_listener example{bus};
    event e;
    bus.post(e);
}
```
### More examples: [example.hxx](https://github.com/thedmkn/astro/blob/master/examples/main.cpp)

#### Inspired by: https://github.com/zeromemes/alpine