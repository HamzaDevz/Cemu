package info.cemu.cemu.emulation.input

import android.content.Context
import android.hardware.display.DisplayManager
import android.hardware.input.InputManager
import info.cemu.cemu.common.android.inputdevice.listGameControllers
import info.cemu.cemu.common.android.inputdevice.toControllerInfo
import info.cemu.cemu.common.input.InputDeviceListener
import info.cemu.cemu.nativeinterface.NativeInput

class NativeInputDeviceListener(private val context: Context) {
    private val inputManager
        get() = context.getSystemService(Context.INPUT_SERVICE) as InputManager?

    private fun refreshControllers() {
        val gameControllerInfos = listGameControllers()
            .map { it.toControllerInfo() }
            .toTypedArray()

        NativeInput.setControllers(gameControllerInfos)
        NativeInput.assignControllers()
    }

    private val listener = object : InputDeviceListener {
        override fun onInputDeviceChanged() = refreshControllers()
    }

    private val displayManager
        get() = context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager?

    // Leaving the dock hands player 1 back to the handheld's own pad, as on a Switch
    private val displayListener = object : DisplayManager.DisplayListener {
        override fun onDisplayAdded(displayId: Int) {}

        override fun onDisplayChanged(displayId: Int) {}

        override fun onDisplayRemoved(displayId: Int) {
            if (displayManager?.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)
                    ?.isEmpty() == true
            ) {
                NativeInput.returnPlayerOneToBuiltIn()
            }
        }
    }

    fun register() {
        inputManager?.registerInputDeviceListener(listener, null)
        displayManager?.registerDisplayListener(displayListener, null)
        refreshControllers()
    }

    fun unregister() {
        inputManager?.unregisterInputDeviceListener(listener)
        displayManager?.unregisterDisplayListener(displayListener)
    }
}