#include "db_relative_engine.hpp"

#include <cstring>

namespace desk_buddy {

namespace {

enum class GenderRequirement : uint8_t {
    ANY_GENDER,
    MALE_ONLY,
    FEMALE_ONLY,
};
using enum GenderRequirement;

struct KinRule {
    const char *chain;
    GenderRequirement self_gender;
    const char *title;
    const char *note;
};

constexpr KinRule kKinRules[] = {
    {"f", ANY_GENDER, "爸爸", "书面称父亲"},
    {"m", ANY_GENDER, "妈妈", "书面称母亲"},
    {"xb", ANY_GENDER, "哥哥", "亦称兄长"},
    {"xd", ANY_GENDER, "弟弟", ""},
    {"xj", ANY_GENDER, "姐姐", ""},
    {"xm", ANY_GENDER, "妹妹", ""},
    {"h", FEMALE_ONLY, "老公", "书面称丈夫"},
    {"w", MALE_ONLY, "老婆", "书面称妻子"},
    {"s", ANY_GENDER, "儿子", ""},
    {"d", ANY_GENDER, "女儿", ""},

    {"f,f", ANY_GENDER, "爷爷", "书面称祖父"},
    {"f,m", ANY_GENDER, "奶奶", "书面称祖母"},
    {"f,xb", ANY_GENDER, "伯父", "口语常称大伯 / 伯伯"},
    {"f,xd", ANY_GENDER, "叔叔", "书面称叔父"},
    {"f,xj", ANY_GENDER, "姑妈", "亦称姑姑 / 大姑"},
    {"f,xm", ANY_GENDER, "姑姑", "亦称小姑 / 姑妈"},
    {"f,w", ANY_GENDER, "妈妈", "如为再婚则为继母"},
    {"f,s", MALE_ONLY, "哥哥 / 弟弟 / 自己", "视排行长幼而定"},
    {"f,s", FEMALE_ONLY, "哥哥 / 弟弟", "视排行长幼而定"},
    {"f,d", MALE_ONLY, "姐姐 / 妹妹", "视排行长幼而定"},
    {"f,d", FEMALE_ONLY, "姐姐 / 妹妹 / 自己", "视排行长幼而定"},

    {"m,f", ANY_GENDER, "外公", "北方常称姥爷"},
    {"m,m", ANY_GENDER, "外婆", "北方常称姥姥"},
    {"m,xb", ANY_GENDER, "舅舅", "亦称大舅 / 舅父"},
    {"m,xd", ANY_GENDER, "舅舅", "亦称小舅 / 舅父"},
    {"m,xj", ANY_GENDER, "姨妈", "亦称大姨 / 阿姨"},
    {"m,xm", ANY_GENDER, "小姨", "亦称阿姨 / 姨妈"},
    {"m,h", ANY_GENDER, "爸爸", "如为再婚则为继父"},
    {"m,s", MALE_ONLY, "哥哥 / 弟弟 / 自己", "视排行长幼而定"},
    {"m,s", FEMALE_ONLY, "哥哥 / 弟弟", "视排行长幼而定"},
    {"m,d", MALE_ONLY, "姐姐 / 妹妹", "视排行长幼而定"},
    {"m,d", FEMALE_ONLY, "姐姐 / 妹妹 / 自己", "视排行长幼而定"},

    {"xb,w", ANY_GENDER, "嫂子", "书面称嫂嫂"},
    {"xd,w", ANY_GENDER, "弟媳", "亦称弟妹"},
    {"xj,h", ANY_GENDER, "姐夫", ""},
    {"xm,h", ANY_GENDER, "妹夫", ""},
    {"xb,s", ANY_GENDER, "侄子", ""},
    {"xb,d", ANY_GENDER, "侄女", ""},
    {"xd,s", ANY_GENDER, "侄子", ""},
    {"xd,d", ANY_GENDER, "侄女", ""},
    {"xj,s", ANY_GENDER, "外甥", ""},
    {"xj,d", ANY_GENDER, "外甥女", ""},
    {"xm,s", ANY_GENDER, "外甥", ""},
    {"xm,d", ANY_GENDER, "外甥女", ""},
    {"xb,f", ANY_GENDER, "爸爸", ""},
    {"xb,m", ANY_GENDER, "妈妈", ""},
    {"xd,f", ANY_GENDER, "爸爸", ""},
    {"xd,m", ANY_GENDER, "妈妈", ""},
    {"xj,f", ANY_GENDER, "爸爸", ""},
    {"xj,m", ANY_GENDER, "妈妈", ""},
    {"xm,f", ANY_GENDER, "爸爸", ""},
    {"xm,m", ANY_GENDER, "妈妈", ""},

    {"w,f", MALE_ONLY, "岳父", "口语常称老丈人 / 爸爸"},
    {"w,m", MALE_ONLY, "岳母", "口语常称丈母娘 / 妈妈"},
    {"w,xb", MALE_ONLY, "大舅子", "书面称内兄 / 大舅哥"},
    {"w,xd", MALE_ONLY, "小舅子", "书面称内弟"},
    {"w,xj", MALE_ONLY, "大姨子", "书面称姨姐"},
    {"w,xm", MALE_ONLY, "小姨子", "书面称姨妹"},
    {"w,s", MALE_ONLY, "儿子", ""},
    {"w,d", MALE_ONLY, "女儿", ""},
    {"w,h", MALE_ONLY, "自己", ""},

    {"h,f", FEMALE_ONLY, "公公", "口语常称爸爸"},
    {"h,m", FEMALE_ONLY, "婆婆", "口语常称妈妈"},
    {"h,xb", FEMALE_ONLY, "大伯子", "书面称大伯哥"},
    {"h,xd", FEMALE_ONLY, "小叔子", ""},
    {"h,xj", FEMALE_ONLY, "大姑子", "亦称大姑姐"},
    {"h,xm", FEMALE_ONLY, "小姑子", ""},
    {"h,s", FEMALE_ONLY, "儿子", ""},
    {"h,d", FEMALE_ONLY, "女儿", ""},
    {"h,w", FEMALE_ONLY, "自己", ""},

    {"s,w", ANY_GENDER, "儿媳", "口语亦称儿媳妇"},
    {"d,h", ANY_GENDER, "女婿", ""},
    {"s,s", ANY_GENDER, "孙子", ""},
    {"s,d", ANY_GENDER, "孙女", ""},
    {"d,s", ANY_GENDER, "外孙", ""},
    {"d,d", ANY_GENDER, "外孙女", ""},
    {"s,f", MALE_ONLY, "自己", ""},
    {"s,f", FEMALE_ONLY, "老公", ""},
    {"s,m", MALE_ONLY, "老婆", ""},
    {"s,m", FEMALE_ONLY, "自己", ""},
    {"d,f", MALE_ONLY, "自己", ""},
    {"d,f", FEMALE_ONLY, "老公", ""},
    {"d,m", MALE_ONLY, "老婆", ""},
    {"d,m", FEMALE_ONLY, "自己", ""},

    {"f,f,f", ANY_GENDER, "曾祖父", "口语常称太爷爷 / 老太爷"},
    {"f,f,m", ANY_GENDER, "曾祖母", "口语常称太奶奶 / 老太太"},
    {"f,m,f", ANY_GENDER, "外曾祖父", "口语亦称太外公"},
    {"f,m,m", ANY_GENDER, "外曾祖母", "口语亦称太外婆"},
    {"m,f,f", ANY_GENDER, "外曾祖父", "北方常称太姥爷"},
    {"m,f,m", ANY_GENDER, "外曾祖母", "北方常称太姥姥"},
    {"m,m,f", ANY_GENDER, "外曾祖父", "外婆的父亲"},
    {"m,m,m", ANY_GENDER, "外曾祖母", "外婆的母亲"},

    {"f,f,xb", ANY_GENDER, "伯祖父", "口语常称大爷爷 / 伯公"},
    {"f,f,xd", ANY_GENDER, "叔祖父", "口语常称叔爷爷 / 叔公"},
    {"f,f,xj", ANY_GENDER, "姑奶奶", "亦称姑祖母"},
    {"f,f,xm", ANY_GENDER, "姑奶奶", "亦称姑祖母"},
    {"f,m,xb", ANY_GENDER, "舅爷爷", "亦称舅公"},
    {"f,m,xd", ANY_GENDER, "舅爷爷", "亦称舅公"},
    {"f,m,xj", ANY_GENDER, "姨奶奶", "亦称姨婆"},
    {"f,m,xm", ANY_GENDER, "姨奶奶", "亦称姨婆"},

    {"m,f,xb", ANY_GENDER, "伯外公", "北方常称大姥爷"},
    {"m,f,xd", ANY_GENDER, "叔外公", "北方常称叔姥爷"},
    {"m,f,xj", ANY_GENDER, "姑外婆", "北方常称姑姥姥"},
    {"m,f,xm", ANY_GENDER, "姑外婆", "北方常称姑姥姥"},
    {"m,m,xb", ANY_GENDER, "舅外公", "北方常称舅姥爷"},
    {"m,m,xd", ANY_GENDER, "舅外公", "北方常称舅姥爷"},
    {"m,m,xj", ANY_GENDER, "姨外婆", "北方常称姨姥姥"},
    {"m,m,xm", ANY_GENDER, "姨外婆", "北方常称姨姥姥"},

    {"f,xb,w", ANY_GENDER, "伯母", "口语常称大妈 / 大娘"},
    {"f,xd,w", ANY_GENDER, "婶婶", "亦称婶娘 / 叔母"},
    {"f,xj,h", ANY_GENDER, "姑父", "亦称姑丈"},
    {"f,xm,h", ANY_GENDER, "姑父", "亦称姑丈"},
    {"m,xb,w", ANY_GENDER, "舅妈", "书面称舅母"},
    {"m,xd,w", ANY_GENDER, "舅妈", "书面称舅母"},
    {"m,xj,h", ANY_GENDER, "姨父", "亦称姨丈"},
    {"m,xm,h", ANY_GENDER, "姨父", "亦称姨丈"},

    {"f,xb,s", ANY_GENDER, "堂哥 / 堂弟", "比自己大叫堂哥，小叫堂弟"},
    {"f,xb,d", ANY_GENDER, "堂姐 / 堂妹", "比自己大叫堂姐，小叫堂妹"},
    {"f,xd,s", ANY_GENDER, "堂哥 / 堂弟", "比自己大叫堂哥，小叫堂弟"},
    {"f,xd,d", ANY_GENDER, "堂姐 / 堂妹", "比自己大叫堂姐，小叫堂妹"},
    {"f,xj,s", ANY_GENDER, "表哥 / 表弟", "姑表亲，视长幼称表哥/表弟"},
    {"f,xj,d", ANY_GENDER, "表姐 / 表妹", "姑表亲，视长幼称表姐/表妹"},
    {"f,xm,s", ANY_GENDER, "表哥 / 表弟", "姑表亲，视长幼称表哥/表弟"},
    {"f,xm,d", ANY_GENDER, "表姐 / 表妹", "姑表亲，视长幼称表姐/表妹"},
    {"m,xb,s", ANY_GENDER, "表哥 / 表弟", "舅表亲，视长幼称表哥/表弟"},
    {"m,xb,d", ANY_GENDER, "表姐 / 表妹", "舅表亲，视长幼称表姐/表妹"},
    {"m,xd,s", ANY_GENDER, "表哥 / 表弟", "舅表亲，视长幼称表哥/表弟"},
    {"m,xd,d", ANY_GENDER, "表姐 / 表妹", "舅表亲，视长幼称表姐/表妹"},
    {"m,xj,s", ANY_GENDER, "表哥 / 表弟", "姨表亲，视长幼称表哥/表弟"},
    {"m,xj,d", ANY_GENDER, "表姐 / 表妹", "姨表亲，视长幼称表姐/表妹"},
    {"m,xm,s", ANY_GENDER, "表哥 / 表弟", "姨表亲，视长幼称表哥/表弟"},
    {"m,xm,d", ANY_GENDER, "表姐 / 表妹", "姨表亲，视长幼称表姐/表妹"},

    {"xb,s,w", ANY_GENDER, "侄媳妇", ""},
    {"xd,s,w", ANY_GENDER, "侄媳妇", ""},
    {"xb,d,h", ANY_GENDER, "侄女婿", ""},
    {"xd,d,h", ANY_GENDER, "侄女婿", ""},
    {"xj,s,w", ANY_GENDER, "外甥媳妇", ""},
    {"xm,s,w", ANY_GENDER, "外甥媳妇", ""},
    {"xj,d,h", ANY_GENDER, "外甥女婿", ""},
    {"xm,d,h", ANY_GENDER, "外甥女婿", ""},
    {"xb,s,s", ANY_GENDER, "侄孙", ""},
    {"xb,s,d", ANY_GENDER, "侄孙女", ""},
    {"xd,s,s", ANY_GENDER, "侄孙", ""},
    {"xd,s,d", ANY_GENDER, "侄孙女", ""},

    {"w,f,f", MALE_ONLY, "太岳父", "妻子的爷爷"},
    {"w,f,m", MALE_ONLY, "太岳母", "妻子的奶奶"},
    {"w,m,f", MALE_ONLY, "外太岳父", "妻子的外公"},
    {"w,m,m", MALE_ONLY, "外太岳母", "妻子的外婆"},
    {"w,xb,w", MALE_ONLY, "大舅嫂", "亦称舅嫂"},
    {"w,xd,w", MALE_ONLY, "小舅媳", "亦称弟妹"},
    {"w,xj,h", MALE_ONLY, "连襟", "口语常称大姐夫"},
    {"w,xm,h", MALE_ONLY, "连襟", "口语常称妹夫"},
    {"w,xb,s", MALE_ONLY, "内侄", "妻兄之子"},
    {"w,xb,d", MALE_ONLY, "内侄女", "妻兄之女"},
    {"w,xd,s", MALE_ONLY, "内侄", "妻弟之子"},
    {"w,xd,d", MALE_ONLY, "内侄女", "妻弟之女"},
    {"w,xj,s", MALE_ONLY, "姨外甥", "亦随妻称外甥"},
    {"w,xj,d", MALE_ONLY, "姨外甥女", "亦随妻称外甥女"},
    {"w,xm,s", MALE_ONLY, "姨外甥", "亦随妻称外甥"},
    {"w,xm,d", MALE_ONLY, "姨外甥女", "亦随妻称外甥女"},

    {"h,f,f", FEMALE_ONLY, "祖翁", "随夫称爷爷"},
    {"h,f,m", FEMALE_ONLY, "祖婆", "随夫称奶奶"},
    {"h,m,f", FEMALE_ONLY, "外祖翁", "随夫称外公 / 姥爷"},
    {"h,m,m", FEMALE_ONLY, "外祖婆", "随夫称外婆 / 姥姥"},
    {"h,xb,w", FEMALE_ONLY, "大嫂", "彼此互称妯娌"},
    {"h,xd,w", FEMALE_ONLY, "弟妹", "彼此互称妯娌"},
    {"h,xj,h", FEMALE_ONLY, "大姑父", "随夫称姐夫"},
    {"h,xm,h", FEMALE_ONLY, "小姑父", "随夫称妹夫"},
    {"h,xb,s", FEMALE_ONLY, "侄子", ""},
    {"h,xb,d", FEMALE_ONLY, "侄女", ""},
    {"h,xd,s", FEMALE_ONLY, "侄子", ""},
    {"h,xd,d", FEMALE_ONLY, "侄女", ""},
    {"h,xj,s", FEMALE_ONLY, "外甥", ""},
    {"h,xj,d", FEMALE_ONLY, "外甥女", ""},
    {"h,xm,s", FEMALE_ONLY, "外甥", ""},
    {"h,xm,d", FEMALE_ONLY, "外甥女", ""},

    {"s,w,f", ANY_GENDER, "亲家公", ""},
    {"s,w,m", ANY_GENDER, "亲家母", ""},
    {"d,h,f", ANY_GENDER, "亲家公", ""},
    {"d,h,m", ANY_GENDER, "亲家母", ""},
    {"s,s,s", ANY_GENDER, "曾孙", "亦称重孙"},
    {"s,s,d", ANY_GENDER, "曾孙女", "亦称重孙女"},
    {"s,s,w", ANY_GENDER, "孙媳妇", ""},
    {"s,d,h", ANY_GENDER, "孙女婿", ""},
    {"d,s,w", ANY_GENDER, "外孙媳妇", ""},
    {"d,d,h", ANY_GENDER, "外孙女婿", ""},
    {"d,s,s", ANY_GENDER, "外曾孙", ""},
    {"d,s,d", ANY_GENDER, "外曾孙女", ""},
};

bool hasGenderConflict(SelfGender previous_gender, KinOp op)
{
    if (previous_gender == SelfGender::MALE && op == KinOp::HUSBAND) {
        return true;
    }
    if (previous_gender == SelfGender::FEMALE && op == KinOp::WIFE) {
        return true;
    }
    return false;
}

SelfGender relativeGender(KinOp op)
{
    switch (op) {
        case KinOp::FATHER:
        case KinOp::OLDER_BROTHER:
        case KinOp::YOUNGER_BROTHER:
        case KinOp::HUSBAND:
        case KinOp::SON:
            return SelfGender::MALE;
        case KinOp::MOTHER:
        case KinOp::OLDER_SISTER:
        case KinOp::YOUNGER_SISTER:
        case KinOp::WIFE:
        case KinOp::DAUGHTER:
            return SelfGender::FEMALE;
    }
    return SelfGender::MALE;
}

std::vector<KinOp> reduceChain(const std::vector<KinOp> &chain)
{
    std::vector<KinOp> out;
    out.reserve(chain.size());
    for (KinOp op : chain) {
        if (!out.empty()) {
            const KinOp prev = out.back();
            if ((prev == KinOp::HUSBAND && op == KinOp::WIFE) ||
                (prev == KinOp::WIFE && op == KinOp::HUSBAND)) {
                out.pop_back();
                continue;
            }
            if (prev == KinOp::FATHER && op == KinOp::WIFE) {
                out.back() = KinOp::MOTHER;
                continue;
            }
            if (prev == KinOp::MOTHER && op == KinOp::HUSBAND) {
                out.back() = KinOp::FATHER;
                continue;
            }
        }
        out.push_back(op);
    }
    return out;
}

}

const char *RelativeEngine::opCode(KinOp op)
{
    switch (op) {
        case KinOp::FATHER: return "f";
        case KinOp::MOTHER: return "m";
        case KinOp::OLDER_BROTHER: return "xb";
        case KinOp::YOUNGER_BROTHER: return "xd";
        case KinOp::OLDER_SISTER: return "xj";
        case KinOp::YOUNGER_SISTER: return "xm";
        case KinOp::HUSBAND: return "h";
        case KinOp::WIFE: return "w";
        case KinOp::SON: return "s";
        case KinOp::DAUGHTER: return "d";
    }
    return "";
}

const char *RelativeEngine::opDisplayName(KinOp op)
{
    switch (op) {
        case KinOp::FATHER: return "爸爸";
        case KinOp::MOTHER: return "妈妈";
        case KinOp::OLDER_BROTHER: return "哥哥";
        case KinOp::YOUNGER_BROTHER: return "弟弟";
        case KinOp::OLDER_SISTER: return "姐姐";
        case KinOp::YOUNGER_SISTER: return "妹妹";
        case KinOp::HUSBAND: return "老公";
        case KinOp::WIFE: return "老婆";
        case KinOp::SON: return "儿子";
        case KinOp::DAUGHTER: return "女儿";
    }
    return "";
}

std::string RelativeEngine::formatPath(const std::vector<KinOp> &chain)
{
    std::string path = "我";
    for (KinOp op : chain) {
        path += "的";
        path += opDisplayName(op);
    }
    return path;
}

RelativeResult RelativeEngine::evaluate(const std::vector<KinOp> &chain, SelfGender gender)
{
    RelativeResult res{};
    if (chain.empty()) {
        res.supported = true;
        res.is_self = true;
        res.primary_title = "自己";
        res.note = "点击下方关系键开始查询";
        return res;
    }

    const auto self_gender = gender == SelfGender::MALE ? SelfGender::MALE : SelfGender::FEMALE;
    auto current_gender = self_gender;
    for (KinOp op : chain) {
        if (hasGenderConflict(current_gender, op)) {
            res.supported = false;
            res.invalid_chain = true;
            res.primary_title = "关系冲突";
            res.note = "请检查当前性别设置或关系路径";
            return res;
        }
        current_gender = relativeGender(op);
    }

    auto buildKey = [](const std::vector<KinOp> &c) {
        std::string key;
        for (size_t i = 0; i < c.size(); ++i) {
            if (i > 0) key += ',';
            key += opCode(c[i]);
        }
        return key;
    };

    const auto required_gender = self_gender == SelfGender::MALE ? MALE_ONLY : FEMALE_ONLY;

    auto lookup = [&](const std::vector<KinOp> &c) -> const KinRule * {
        const std::string key = buildKey(c);
        const KinRule *generic_match = nullptr;
        for (const auto &rule : kKinRules) {
            if (key == rule.chain) {
                if (rule.self_gender == required_gender) {
                    return &rule;
                }
                if (rule.self_gender == ANY_GENDER) {
                    generic_match = &rule;
                }
            }
        }
        return generic_match;
    };

    if (const KinRule *match = lookup(chain)) {
        res.supported = true;
        res.primary_title = match->title;
        res.note = match->note;
        res.is_self = (res.primary_title == "自己");
        return res;
    }

    std::vector<KinOp> reduced = reduceChain(chain);
    if (reduced.empty()) {
        res.supported = true;
        res.is_self = true;
        res.primary_title = "自己";
        res.note = "兜兜转转还是你自己";
        return res;
    }
    if (const KinRule *match = lookup(reduced)) {
        res.supported = true;
        res.primary_title = match->title;
        res.note = match->note;
        res.is_self = (res.primary_title == "自己");
        return res;
    }

    res.supported = false;
    res.primary_title = "暂不支持";
    res.note = "该多层亲属关系较复杂，可减少一层或直接称呼长辈/亲戚";
    return res;
}

}
