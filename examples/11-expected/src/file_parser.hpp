#pragma once

#include <string>
#include <expected>
#include <vector>
#include <cmath>
#include <sstream>

// A file parser service that uses std::expected for clean error handling.
// Demonstrates how C++ APIs using std::expected work naturally in Python,
// where errors become exceptions and success values pass through directly.

struct ParseError {
    int line;
    std::string message;
};

struct FileParser {
    std::string delimiter = ",";
    bool strict_mode = false;

    std::expected<std::vector<double>, std::string> parse_numbers(const std::string& input) const {
        std::vector<double> result;
        std::istringstream stream(input);
        std::string token;

        while (std::getline(stream, token, delimiter[0])) {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            if (start == std::string::npos) {
                if (strict_mode) {
                    return std::unexpected("empty token found in strict mode");
                }
                continue;
            }
            token = token.substr(start, token.find_last_not_of(" \t") - start + 1);

            try {
                double val = std::stod(token);
                result.push_back(val);
            } catch (...) {
                return std::unexpected("invalid number: '" + token + "'");
            }
        }

        if (result.empty()) {
            return std::unexpected("no numbers found in input");
        }
        return result;
    }

    std::expected<double, std::string> compute_mean(const std::string& input) const {
        auto numbers = parse_numbers(input);
        if (!numbers) {
            return std::unexpected(numbers.error());
        }

        double sum = 0.0;
        for (double n : *numbers) {
            sum += n;
        }
        return sum / static_cast<double>(numbers->size());
    }

    std::expected<double, std::string> compute_stddev(const std::string& input) const {
        auto numbers = parse_numbers(input);
        if (!numbers) {
            return std::unexpected(numbers.error());
        }
        if (numbers->size() < 2) {
            return std::unexpected("need at least 2 numbers for standard deviation");
        }

        double sum = 0.0;
        for (double n : *numbers) sum += n;
        double mean = sum / static_cast<double>(numbers->size());

        double sq_sum = 0.0;
        for (double n : *numbers) {
            double diff = n - mean;
            sq_sum += diff * diff;
        }
        return std::sqrt(sq_sum / static_cast<double>(numbers->size() - 1));
    }
};
