#pragma once

#include <string>
#include <vector>

#include "b2b/app/Messages.hpp"
#include "b2b/core/bms/ObjectId.hpp"
#include "b2b/core/bmson/Bmson.hpp"
#include "b2b/core/slicing/SlicePlan.hpp"
#include "b2b/infra/text/TextEncoding.hpp"

namespace b2b::app {

struct NamingOptions {
    std::string folderPattern;
    std::string keyPattern = "{song:.16}_k{index:04}";
    std::string bgmPattern = "{song:.16}_b{index:04}";

    friend bool operator==(const NamingOptions&, const NamingOptions&) = default;
};

struct NamingResult {
    std::vector<std::string> paths;
    std::string folder;
    std::vector<Message> messages;

    [[nodiscard]] bool hasErrors() const;
};

[[nodiscard]] NamingResult checkNamingPatterns(const NamingOptions& options);

[[nodiscard]] NamingResult nameSounds(const core::bmson::Chart& chart, const core::SlicePlan& plan, core::IdBase base,
                                      const NamingOptions& options, const std::string& extension,
                                      infra::TextEncoding encoding);

}  // namespace b2b::app
