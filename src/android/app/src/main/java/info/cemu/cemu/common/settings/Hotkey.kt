package info.cemu.cemu.common.settings

import android.view.KeyEvent
import kotlinx.serialization.Serializable

@Serializable
data class HotkeyCombo(
    val keys: Set<Int>
)

enum class HotkeyAction {
    QUIT,
    TOGGLE_MENU,
    SHOW_EMULATED_USB_DEVICES_DIALOG,
}

// Select+Start leaves the game, the exit combo frontends such as ES-DE expect
val DEFAULT_HOTKEY_SETTINGS = mapOf(
    HotkeyAction.QUIT to HotkeyCombo(
        setOf(KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_START)
    )
)