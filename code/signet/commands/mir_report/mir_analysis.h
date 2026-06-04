#pragma once

#include "json.hpp"

struct AudioData;

namespace mir {

nlohmann::json Analyse(const AudioData &audio,
                       int envelope_columns = 96,
                       int spectrum_bands = 32);

} // namespace mir
