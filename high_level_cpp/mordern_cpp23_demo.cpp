/*
 * cpp_modern_demo.cpp  —  C++23 / C++26 feature showcase
 *
 * Features demonstrated:
 *  ① std::print / std::println      (C++23 — shim provided for older stdlibs)
 *  ② std::format                    (C++20)
 *  ③ std::expected<T,E>             (C++23) — replaces throw/catch for errors
 *  ④ std::optional  monadic ops     (C++23) — .transform() .and_then() .or_else()
 *  ⑤ std::ranges + views pipeline   (C++20/23)
 *  ⑥ std::span                      (C++20) — non-owning array view
 *  ⑦ RAII                           (always ♥) — FileGuard
 *  ⑧ Smart pointers                 (C++11/14) — unique, shared, weak
 *  ⑨ vtable / virtual dispatch      — polymorphic Shape hierarchy
 *  ⑩ Virtual inheritance            — Diamond solved with virtual base
 *
 * Compile (GCC 14+ / Clang 17+):
 *   g++ -std=c++26 -Wall -Wextra -o demo cpp_modern_demo.cpp
 *
 * Compile (GCC 13 / Clang 16 with compat shim for std::print):
 *   g++ -std=c++23 -Wall -Wextra -o demo cpp_modern_demo.cpp
 */

// ── ① Compatibility shim: std::print/println for pre-GCC-14 ─────────────────
// Remove this block once your toolchain ships <print> (GCC 14+, MSVC 19.37+).
#if __has_include(<print>)
  #include <print>
#else
  #include <cstdio>
  #include <format>
  namespace std {
      template<typename... Args>
      void print(std::format_string<Args...> fmt, Args&&... args) {
          std::fputs(std::format(fmt, std::forward<Args>(args)...).c_str(), stdout);
      }
      template<typename... Args>
      void println(std::format_string<Args...> fmt, Args&&... args) {
          std::puts(std::format(fmt, std::forward<Args>(args)...).c_str());
      }
      inline void println() { std::putchar('\n'); }
  }
#endif

#include <algorithm>
#include <expected>    // C++23
#include <format>      // C++20
#include <fstream>
#include <memory>
#include <optional>    // monadic ops: C++23
#include <ranges>      // C++20/23
#include <span>        // C++20
#include <string>
#include <string_view>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// ⑦  RAII  —  FileGuard
//     Wraps an ofstream so the file is *always* closed on scope exit.
//     Returns std::expected instead of throwing (③).
// ─────────────────────────────────────────────────────────────────────────────
class FileGuard {
public:
    // Factory — returns the guard OR an error message; never throws.
    [[nodiscard]]
    static std::expected<FileGuard, std::string>
    open(std::string path) noexcept
    {
        FileGuard g;
        g.path_ = std::move(path);
        g.file_.open(g.path_, std::ios::out | std::ios::trunc);
        if (!g.file_.is_open())
            return std::unexpected(std::format("Cannot open '{}'", g.path_));
        std::println("[RAII] FileGuard acquired: {}", g.path_);
        return g;
    }

    // Non-copyable, movable
    FileGuard(const FileGuard&)            = delete;
    FileGuard& operator=(const FileGuard&) = delete;
    FileGuard(FileGuard&&)                 = default;
    FileGuard& operator=(FileGuard&&)      = default;

    ~FileGuard() {
        if (file_.is_open()) {
            file_.close();
            std::println("[RAII] FileGuard released: {}", path_);
        }
    }

    std::ofstream& stream() { return file_; }

private:
    FileGuard() = default;   // only the factory constructs
    std::string   path_;
    std::ofstream file_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ⑨  vtable / Virtual Dispatch  —  Shape hierarchy
// ─────────────────────────────────────────────────────────────────────────────
class Shape {
public:
    explicit Shape(std::string_view name) : name_(name) {}
    virtual ~Shape() { std::println("[vtable] ~Shape({})", name_); }

    virtual double      area()     const = 0;
    virtual std::string kind()     const = 0;
    virtual void        describe() const {
        // ② std::format inside std::println
        std::println("[vtable] {:>12s}  area = {:8.3f}", kind(), area());
    }

protected:
    std::string name_;
};

struct Circle : Shape {
    explicit Circle(double r) : Shape("Circle"), r_(r) {}
    double      area() const override { return 3.14159265358979 * r_ * r_; }
    std::string kind() const override { return std::format("Circle(r={:.1f})", r_); }
private:
    double r_;
};

struct Rectangle : Shape {
    Rectangle(double w, double h) : Shape("Rectangle"), w_(w), h_(h) {}
    double      area() const override { return w_ * h_; }
    std::string kind() const override { return std::format("Rect({}×{})", w_, h_); }
private:
    double w_, h_;
};

struct Triangle : Shape {
    Triangle(double b, double h) : Shape("Triangle"), b_(b), h_(h) {}
    double      area() const override { return 0.5 * b_ * h_; }
    std::string kind() const override { return std::format("Tri(b={},h={})", b_, h_); }
private:
    double b_, h_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ⑩  Virtual Inheritance  —  Diamond problem
//
//            Device   (virtual base — lives once)
//           /        \
//       Wireless    Sensor
//           \        /
//          SmartSensor
// ─────────────────────────────────────────────────────────────────────────────
struct Device {
    explicit Device(std::string id) : id_(std::move(id)) {
        std::println("[Diamond] Device ctor  id={}", id_);
    }
    virtual ~Device() { std::println("[Diamond] Device dtor  id={}", id_); }

    virtual void status() const {
        std::println("  Device id={}", id_);
    }
protected:
    std::string id_;
};

struct Wireless : virtual Device {         // ← virtual keyword = shared base
    Wireless(const std::string& id, int freq)
        : Device(id), freq_(freq)
    { std::println("[Diamond] Wireless ctor  freq={} MHz", freq_); }

    void status() const override {
        Device::status();
        std::println("  Wireless freq={} MHz", freq_);
    }
protected:
    int freq_;
};

struct Sensor : virtual Device {           // ← virtual keyword = shared base
    Sensor(const std::string& id, std::string type)
        : Device(id), type_(std::move(type))
    { std::println("[Diamond] Sensor ctor  type={}", type_); }

    void status() const override {
        Device::status();
        std::println("  Sensor type={}", type_);
    }
protected:
    std::string type_;
};

// Most-derived class must call the virtual base constructor directly;
// the Device(id) calls in Wireless/Sensor are ignored during construction
// of SmartSensor — the compiler ensures only ONE Device exists.
struct SmartSensor : Wireless, Sensor {
    SmartSensor(std::string id, int freq, std::string type)
        : Device(id)                       // ← sole construction of Device
        , Wireless(id, freq)
        , Sensor(id, std::move(type))
    { std::println("[Diamond] SmartSensor ctor"); }

    void status() const override {
        std::println("[SmartSensor] id={}  freq={} MHz  type={}", id_, freq_, type_);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ③  std::expected helper — safe integer parsing
// ─────────────────────────────────────────────────────────────────────────────
std::expected<int, std::string> parse_int(std::string_view s) {
    int result{};
    for (char c : s) {
        if (c < '0' || c > '9')
            return std::unexpected(std::format("'{}' is not a digit in '{}'", c, s));
        result = result * 10 + (c - '0');
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ⑧  Smart pointer / weak_ptr  — Observer pattern
// ─────────────────────────────────────────────────────────────────────────────
struct EventBus {
    std::vector<std::string> log;
    void post(std::string msg) {
        log.push_back(msg);
        std::println("[EventBus] {}", msg);
    }
    ~EventBus() { std::println("[EventBus] destroyed  ({} events logged)", log.size()); }
};

struct Subscriber {
    std::string          name;
    std::weak_ptr<EventBus> bus;   // weak — doesn't extend lifetime

    void tryPost(std::string_view msg) const {
        if (auto sp = bus.lock())
            sp->post(std::format("{}: {}", name, msg));
        else
            std::println("[Subscriber] {} — bus is gone, dropping '{}'", name, msg);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Pretty section header
// ─────────────────────────────────────────────────────────────────────────────
void header(std::string_view title) {
    std::println("\n╔══════════════════════════════════════════════╗");
    std::println("║  {:44} ║", title);
    std::println("╚══════════════════════════════════════════════╝");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {

    // ──────────────────────────────────────────────────────────────────────
    // ① ② ③  RAII + std::expected (no exceptions!)
    // ──────────────────────────────────────────────────────────────────────
    header("① RAII  +  ③ std::expected (no throw/catch)");
    {
        auto result = FileGuard::open("/tmp/raii_demo.txt");
        if (result) {
            result->stream() << "Written safely via RAII\n";
            // guard destructs here → file closed automatically
        } else {
            std::println("[RAII] error: {}", result.error());
        }

        // expected: chaining with .transform() (④)
        auto bad = FileGuard::open("/no/such/path/file.txt");
        auto msg = bad
            .transform([](FileGuard&)  { return std::string("opened!"); })
            .or_else ([](std::string e){ return std::expected<std::string,std::string>(e + " [handled]"); });
        std::println("[expected] result: {}", *msg);
    }

    // ──────────────────────────────────────────────────────────────────────
    // ③  std::expected — safe parsing pipeline
    // ──────────────────────────────────────────────────────────────────────
    header("③ std::expected — error propagation without exceptions");
    for (std::string_view s : {"42", "3x7", "100"}) {
        auto r = parse_int(s)
            .transform([](int n) { return n * n; });   // square if ok
        if (r)  std::println("  parse_int({:>5}) → {}² = {}", s, std::string(s), *r);
        else    std::println("  parse_int({:>5}) → ✗ {}", s, r.error());
    }

    // ──────────────────────────────────────────────────────────────────────
    // ④  std::optional monadic ops (C++23)
    // ──────────────────────────────────────────────────────────────────────
    header("④ std::optional monadic ops (C++23)");
    {
        auto lookup = [](int key) -> std::optional<std::string> {
            if (key == 1) return "Alice";
            if (key == 2) return "Bob";
            return std::nullopt;
        };

        for (int key : {1, 2, 3}) {
            auto greeting = lookup(key)
                .transform([](const std::string& n) { return "Hello, " + n + "!"; })
                .or_else([]() -> std::optional<std::string> { return "Hello, stranger!"; });
            std::println("  key={} → {}", key, *greeting);
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // ⑤  std::ranges + views pipeline (C++20/23)
    // ──────────────────────────────────────────────────────────────────────
    header("⑤ std::ranges + views pipeline");
    {
        namespace rv = std::ranges::views;

        // Lazy pipeline: generate → filter → transform — nothing computed yet
        auto pipeline =
            rv::iota(1, 20)
            | rv::filter([](int n){ return n % 3 == 0; })      // divisible by 3
            | rv::transform([](int n){ return n * n; })         // square
            | rv::take(5);                                       // first 5

        std::print("  first 5 squares of multiples of 3: ");
        std::println("{}", pipeline);

        // ranges::fold_left (C++23)
        std::vector<int> nums{1, 2, 3, 4, 5};
        int sum = std::ranges::fold_left(nums, 0, std::plus{});
        std::println("  fold_left sum of 1..5 = {}", sum);

        // collect a view into a vector (ranges::to<> needs GCC 14 / C++23 full)
        auto sq_view = rv::iota(1, 6) | rv::transform([](int n){ return n*n; });
        std::vector<int> squares_vec(sq_view.begin(), sq_view.end());
        std::print("  squares 1..5 collected: ");
        std::println("{}", squares_vec);
    }

    // ──────────────────────────────────────────────────────────────────────
    // ⑥  std::span — non-owning view over contiguous data
    // ──────────────────────────────────────────────────────────────────────
    header("⑥ std::span — non-owning array view");
    {
        auto print_span = [](std::span<const double> s, std::string_view label) {
            std::print("  {} [{}]: ", label, s.size());
            std::println("{}", s);
        };

        double raw[]  = {1.1, 2.2, 3.3, 4.4, 5.5};
        std::vector<double> vec = {9.9, 8.8, 7.7};

        print_span(raw, "C-array");
        print_span(vec, "vector ");
        print_span(std::span{raw}.subspan(1, 3), "subspan");  // no copy!
    }

    // ──────────────────────────────────────────────────────────────────────
    // ⑨  vtable — polymorphic Shape collection
    // ──────────────────────────────────────────────────────────────────────
    header("⑨ vtable / virtual dispatch");
    {
        std::vector<std::unique_ptr<Shape>> shapes;
        shapes.push_back(std::make_unique<Circle>(5.0));
        shapes.push_back(std::make_unique<Rectangle>(4.0, 6.0));
        shapes.push_back(std::make_unique<Triangle>(3.0, 8.0));

        for (const auto& s : shapes) s->describe();   // vtable dispatch

        // Total area via ranges (⑤ + ⑨ combined)
        double total = std::ranges::fold_left(
            shapes | std::ranges::views::transform([](const auto& s){ return s->area(); }),
            0.0, std::plus{});
        std::println("  total area = {:.3f}", total);
    }   // unique_ptrs call virtual destructors here ↑

    // ──────────────────────────────────────────────────────────────────────
    // ⑩  Virtual inheritance — Diamond
    // ──────────────────────────────────────────────────────────────────────
    header("⑩ Virtual inheritance — Diamond (one Device, not two)");
    {
        SmartSensor ss("dev-42", 2400, "temperature");
        std::println("\n-- polymorphic call via Device* --");
        Device* d = &ss;         // vtable routes to SmartSensor::status
        d->status();
    }   // SmartSensor dtor → Device dtor runs ONCE ↑

    // ──────────────────────────────────────────────────────────────────────
    // ⑧  Smart pointers — unique / shared / weak
    // ──────────────────────────────────────────────────────────────────────
    header("⑧ Smart pointers — unique / shared / weak");
    {
        // unique_ptr: sole ownership, move-only
        auto u1 = std::make_unique<Circle>(3.0);
        std::println("[unique_ptr] area = {:.3f}", u1->area());
        auto u2 = std::move(u1);                       // transfer ownership
        std::println("[unique_ptr] after move: u1={}, u2={:.3f}",
                     u1 ? "valid" : "null", u2->area());

        // shared_ptr: reference counted
        auto bus = std::make_shared<EventBus>();

        Subscriber alice{"Alice"}, bob{"Bob"};
        alice.bus = bus;   // weak_ptr — no ref bump
        bob.bus   = bus;

        std::println("[shared_ptr] use_count = {}", bus.use_count());
        alice.tryPost("Hello from Alice");
        bob.tryPost("Hello from Bob");

        std::println("[shared_ptr] dropping the last strong owner …");
        bus.reset();                                   // EventBus destroyed here

        alice.tryPost("Is anyone still there?");       // weak_ptr expired → null
    }

    std::println("\n✓ All done.\n");
}