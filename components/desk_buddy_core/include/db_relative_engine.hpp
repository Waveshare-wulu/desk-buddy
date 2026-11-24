#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace desk_buddy {

enum class KinOp : uint8_t {
    FATHER = 0,
    MOTHER,
    OLDER_BROTHER,
    YOUNGER_BROTHER,
    OLDER_SISTER,
    YOUNGER_SISTER,
    HUSBAND,
    WIFE,
    SON,
    DAUGHTER,
};

enum class SelfGender : uint8_t {
    MALE = 0,
    FEMALE,
};

struct RelativeResult {
    bool supported;
    std::string primary_title;
    std::string note;              // Regional aliases, age-order candidates, or explanation
    bool is_self;
    bool invalid_chain;            // e.g. male->husband or female->wife
};

class RelativeEngine {
public:
    static constexpr size_t MAX_CHAIN_LEN = 4;

    static const char *opCode(KinOp op);
    static const char *opDisplayName(KinOp op);

    /** Format breadcrumb like "我 → 爸爸 → 姐姐". */
    static std::string formatPath(const std::vector<KinOp> &chain);

    static RelativeResult evaluate(const std::vector<KinOp> &chain, SelfGender gender);
};

}
