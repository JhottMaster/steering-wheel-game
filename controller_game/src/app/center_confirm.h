#pragma once

struct CenterConfirmState {
  bool active = false;
  bool greenWasDown = false;
  bool redWasDown = false;
};

enum class CenterConfirmAction {
  kNone,
  kConfirm,
  kCancel,
};

inline void OpenCenterConfirm(CenterConfirmState* state, bool greenDown, bool redDown) {
  state->active = true;
  state->greenWasDown = greenDown;
  state->redWasDown = redDown;
}

inline void CloseCenterConfirm(CenterConfirmState* state) {
  state->active = false;
  state->greenWasDown = false;
  state->redWasDown = false;
}

inline CenterConfirmAction UpdateCenterConfirm(CenterConfirmState* state, bool greenDown,
                                               bool redDown) {
  if (!state->active) {
    state->greenWasDown = greenDown;
    state->redWasDown = redDown;
    return CenterConfirmAction::kNone;
  }

  const bool greenPressed = greenDown && !state->greenWasDown;
  const bool redPressed = redDown && !state->redWasDown;
  state->greenWasDown = greenDown;
  state->redWasDown = redDown;

  if (redPressed) {
    return CenterConfirmAction::kCancel;
  }
  if (greenPressed) {
    return CenterConfirmAction::kConfirm;
  }
  return CenterConfirmAction::kNone;
}
