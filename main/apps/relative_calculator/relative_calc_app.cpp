#include "relative_calc_app.hpp"

#include "services/sound_service.hpp"
#include "ui/assets/db_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
struct KeySpec {
    KinOp op;
    const char *label;
};

constexpr KeySpec kKeypad[10] = {
    {KinOp::FATHER,          "父"},
    {KinOp::MOTHER,          "母"},
    {KinOp::OLDER_BROTHER,   "兄"},
    {KinOp::YOUNGER_BROTHER, "弟"},
    {KinOp::OLDER_SISTER,    "姐"},
    {KinOp::YOUNGER_SISTER,  "妹"},
    {KinOp::HUSBAND,         "夫"},
    {KinOp::WIFE,            "妻"},
    {KinOp::SON,             "子"},
    {KinOp::DAUGHTER,        "女"},
};
}

RelativeCalcApp::RelativeCalcApp()
    : DeskBuddyApp("deskbuddy.relative_calculator", i18n::tr(i18n::StrId::APP_RELATIVE_CALC), &db_icon_relative_calc, 1)
{
}

bool RelativeCalcApp::run(void)
{
    buildUi();
    refreshDisplay();
    return true;
}

void RelativeCalcApp::buildUi()
{
    lv_obj_t *scr = appRoot();
    theme::styleRootScreen(scr);

    auto top = theme::createAppTopBar(
        scr,
        i18n::tr(i18n::StrId::APP_RELATIVE_CALC),
        theme::kAccentMintFg,
        DeskBuddyApp::onBackBtnClicked,
        this,
        _gender == SelfGender::MALE ? "我是男" : "我是女",
        onGenderToggle
    );
    lv_obj_set_width(top.action_btn, 104);
    _gender_lbl = top.action_label;

    lv_obj_t *card = theme::createCard(scr, theme::kContentWidth, 140);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_border_color(card, theme::color(0xC2EFE2), 0);
    lv_obj_set_style_pad_hor(card, 18, 0);
    lv_obj_set_style_pad_ver(card, 12, 0);

    _path_lbl = lv_label_create(card);
    lv_obj_set_width(_path_lbl, LV_PCT(100));
    lv_obj_set_height(_path_lbl, lv_font_get_line_height(theme::fontSm()));
    lv_label_set_long_mode(_path_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(_path_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_path_lbl, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(_path_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    _title_lbl = lv_label_create(card);
    lv_obj_set_width(_title_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_title_lbl, theme::fontXl(), 0);
    lv_obj_set_style_text_color(_title_lbl, theme::color(theme::kAccentMintFg), 0);
    lv_obj_align(_title_lbl, LV_ALIGN_CENTER, 0, 4);

    _note_lbl = lv_label_create(card);
    lv_obj_set_width(_note_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(_note_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_note_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_note_lbl, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(_note_lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto makeRow = [&](int start_idx, int y) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, theme::kContentWidth, 58);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 5; ++i) {
            const auto &spec = kKeypad[start_idx + i];
            lv_obj_t *b = theme::createPillButton(row, spec.label, theme::kBgCard, theme::kTextPrimary, 82, 58, theme::fontLg());
            lv_obj_set_style_radius(b, 16, 0);
            lv_obj_set_style_border_width(b, 1, 0);
            lv_obj_set_style_border_color(b, theme::color(theme::kBorderSubtle), 0);
            lv_obj_set_user_data(b, reinterpret_cast<void *>(static_cast<uintptr_t>(spec.op)));
            lv_obj_add_event_cb(b, onKinKeyClicked, LV_EVENT_CLICKED, this);
        }
    };

    makeRow(0, 208);
    makeRow(5, 278);

    lv_obj_t *ctrl = lv_obj_create(scr);
    lv_obj_remove_style_all(ctrl);
    lv_obj_set_size(ctrl, theme::kContentWidth, 58);
    lv_obj_align(ctrl, LV_ALIGN_TOP_MID, 0, 358);
    lv_obj_set_style_bg_opa(ctrl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctrl, 0, 0);
    lv_obj_set_style_pad_all(ctrl, 0, 0);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ctrl, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *undo = theme::createPillButton(ctrl, "撤销一步", theme::kBgCard, theme::kTextPrimary, theme::kColumnWidth, 58, theme::fontMd());
    lv_obj_set_style_radius(undo, 20, 0);
    lv_obj_set_style_border_width(undo, 1, 0);
    lv_obj_set_style_border_color(undo, theme::color(theme::kBorderSubtle), 0);
    lv_obj_add_event_cb(undo, onUndoClicked, LV_EVENT_CLICKED, this);

    lv_obj_t *clr = theme::createPillButton(ctrl, "全部清空", theme::kBgCard, theme::kAccentCoralFg, theme::kColumnWidth, 58, theme::fontMd());
    lv_obj_set_style_radius(clr, 20, 0);
    lv_obj_set_style_border_width(clr, 1, 0);
    lv_obj_set_style_border_color(clr, theme::color(theme::kBorderSubtle), 0);
    lv_obj_add_event_cb(clr, onClearClicked, LV_EVENT_CLICKED, this);
}

void RelativeCalcApp::refreshDisplay()
{
    const std::string path = RelativeEngine::formatPath(_chain);
    const RelativeResult res = RelativeEngine::evaluate(_chain, _gender);

    if (_path_lbl) lv_label_set_text(_path_lbl, path.c_str());
    if (_title_lbl) {
        lv_label_set_text(_title_lbl, res.primary_title.c_str());
        uint32_t col = theme::kAccentMintFg;
        if (res.invalid_chain) col = theme::kAccentCoralFg;
        else if (!res.supported) col = theme::kAccentAmberFg;
        lv_obj_set_style_text_color(_title_lbl, theme::color(col), 0);
        lv_obj_set_style_text_font(_title_lbl,
            res.primary_title.size() > 15 ? theme::fontLg() : theme::fontXl(), 0);
    }
    if (_note_lbl) {
        lv_label_set_text(_note_lbl, res.note.empty() ? "可继续叠加下一层关系" : res.note.c_str());
    }
}

void RelativeCalcApp::onKinKeyClicked(lv_event_t *e)
{
    auto *self = static_cast<RelativeCalcApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (self->_chain.size() >= RelativeEngine::MAX_CHAIN_LEN) {
        return;
    }
    const auto op = static_cast<KinOp>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    self->_chain.push_back(op);
    self->refreshDisplay();
}

void RelativeCalcApp::onGenderToggle(lv_event_t *e)
{
    auto *self = static_cast<RelativeCalcApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_gender = (self->_gender == SelfGender::MALE) ? SelfGender::FEMALE : SelfGender::MALE;
    if (self->_gender_lbl) {
        lv_label_set_text(self->_gender_lbl, self->_gender == SelfGender::MALE ? "我是男" : "我是女");
    }
    self->refreshDisplay();
}

void RelativeCalcApp::onUndoClicked(lv_event_t *e)
{
    auto *self = static_cast<RelativeCalcApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (!self->_chain.empty()) {
        self->_chain.pop_back();
        self->refreshDisplay();
    }
}

void RelativeCalcApp::onClearClicked(lv_event_t *e)
{
    auto *self = static_cast<RelativeCalcApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_chain.clear();
    self->refreshDisplay();
}

}
