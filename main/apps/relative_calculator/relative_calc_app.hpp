#pragma once

#include <vector>
#include "apps/db_base_app.hpp"
#include "db_relative_engine.hpp"

namespace desk_buddy {

class RelativeCalcApp : public DeskBuddyApp {
public:
    RelativeCalcApp();
    ~RelativeCalcApp() override = default;

protected:
    bool run(void) override;

private:
    void buildUi();
    void refreshDisplay();

    static void onKinKeyClicked(lv_event_t *e);
    static void onGenderToggle(lv_event_t *e);
    static void onUndoClicked(lv_event_t *e);
    static void onClearClicked(lv_event_t *e);

    std::vector<KinOp> _chain;
    SelfGender _gender = SelfGender::MALE;

    lv_obj_t *_gender_lbl = nullptr;
    lv_obj_t *_path_lbl = nullptr;
    lv_obj_t *_title_lbl = nullptr;
    lv_obj_t *_note_lbl = nullptr;
};

}
