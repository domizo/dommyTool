#pragma once

// Test-process state only. Production code must never change the global locale.
#include <locale>
#include <string>

namespace dommy_test {

class GroupedNumbers final : public std::numpunct<char> {
public:
    GroupedNumbers(char decimal, char separator)
        : decimal_(decimal), separator_(separator) {}
private:
    char do_decimal_point() const override { return decimal_; }
    char do_thousands_sep() const override { return separator_; }
    std::string do_grouping() const override { return "\3"; }
    char decimal_, separator_;
};

class ScopedNumericLocale final {
public:
    explicit ScopedNumericLocale(const std::locale& selected)
        : selected_(selected), prior_(std::locale::global(selected_)) {}
    ScopedNumericLocale(char decimal, char separator)
        : ScopedNumericLocale(std::locale(std::locale::classic(),
                                          new GroupedNumbers(decimal, separator))) {}
    ~ScopedNumericLocale() { std::locale::global(prior_); }
    const std::locale& selected() const { return selected_; }
    ScopedNumericLocale(const ScopedNumericLocale&) = delete;
    ScopedNumericLocale& operator=(const ScopedNumericLocale&) = delete;
private:
    std::locale selected_, prior_;
};

} // namespace dommy_test
