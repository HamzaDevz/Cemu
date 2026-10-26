#include "AndroidInputHelpers.h"

#include "input/api/Android/ControllerManager.h"
#include "input/ControllerFactory.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/WiimoteController.h"

#include <android/keycodes.h>

ControllerPtr CreateDefaultDeviceController()
{
	auto controller = ControllerFactory::CreateController(InputAPI::Device, "device", "device");
	controller->set_use_motion(true);
	return controller;
}

// The handheld's own pad, which gives player 1 up to any gamepad that joins
static constexpr std::string_view kBuiltInControllerName = "Odin Controller";

static bool IsBuiltIn(std::string_view name)
{
	return name == kBuiltInControllerName;
}

// The handheld's own pad reports Nintendo face buttons (A on the right), other gamepads Xbox ones (A at the bottom):
// copying mappings between them swaps A/B and X/Y so that each button keeps its place.
static uint64 SamePlaceButton(uint64 button, bool swapFaces)
{
	if (!swapFaces)
		return button;
	switch (button)
	{
	case AKEYCODE_BUTTON_A: return AKEYCODE_BUTTON_B;
	case AKEYCODE_BUTTON_B: return AKEYCODE_BUTTON_A;
	case AKEYCODE_BUTTON_X: return AKEYCODE_BUTTON_Y;
	case AKEYCODE_BUTTON_Y: return AKEYCODE_BUTTON_X;
	default: return button;
	}
}

// Maps controller to every mapping that source has on sourceController, with the same settings. Replaces
// sourceController when target is source.
static void CopyMappings(const EmulatedControllerPtr& target, const EmulatedControllerPtr& source, const ControllerPtr& sourceController, const ControllerPtr& controller)
{
	const bool swapFaces = IsBuiltIn(sourceController->display_name()) != IsBuiltIn(controller->display_name());
	std::vector<std::pair<uint64, uint64>> mappings;
	for (uint64 mapping = 0; mapping <= source->get_highest_mapping_id(); mapping++)
	{
		if (source->get_mapping_controller(mapping) == sourceController)
			mappings.emplace_back(mapping, SamePlaceButton(source->get_mapping_button(mapping), swapFaces));
	}
	if (target == source)
		target->remove_controller(sourceController);
	controller->set_settings(sourceController->get_settings());
	target->add_controller(controller);
	for (const auto& [mapping, button] : mappings)
		target->set_mapping(mapping, controller, button);
}

namespace
{
	// Players and the connected gamepad each one plays with
	struct AndroidPlayers
	{
		std::vector<ControllerManager::ControllerInfo> gamepads = ControllerManager::instance().get_controllers();
		std::array<EmulatedControllerPtr, InputManager::kMaxController> players;

		AndroidPlayers()
		{
			for (size_t i = 0; i < players.size(); i++)
				players[i] = InputManager::instance().get_controller(i);
		}

		bool isConnected(const ControllerPtr& controller) const
		{
			return controller->api() == InputAPI::Android && std::ranges::any_of(gamepads, [&](const auto& gamepad) {
				return gamepad.descriptor == controller->uuid() && gamepad.name == controller->display_name();
			});
		}

		ControllerPtr androidController(size_t player) const
		{
			if (!players[player])
				return {};
			auto controllers = players[player]->get_controllers();
			auto it = std::ranges::find_if(controllers, [&](const auto& c) { return isConnected(c); });
			if (it == controllers.end())
				it = std::ranges::find(controllers, InputAPI::Android, &ControllerBase::api);
			return it != controllers.end() ? *it : nullptr;
		}

		ControllerPtr holder(size_t player) const
		{
			auto controller = androidController(player);
			return controller && isConnected(controller) ? controller : nullptr;
		}

		bool isUsed(std::string_view descriptor) const
		{
			for (size_t i = 0; i < players.size(); i++)
			{
				if (auto controller = holder(i); controller && controller->uuid() == descriptor)
					return true;
			}
			return false;
		}

		const ControllerManager::ControllerInfo* builtIn() const
		{
			auto it = std::ranges::find_if(gamepads, [](const auto& g) { return IsBuiltIn(g.name); });
			return it != gamepads.end() ? &*it : nullptr;
		}

		// Binds gamepad to player in place of its Android controller, or mapped like player 1 when it has none. Not
		// saved: the profiles keep their own gamepad, so borrowed gamepads do not pile up in them.
		void assign(size_t player, const ControllerManager::ControllerInfo& gamepad)
		{
			auto controller = ControllerFactory::CreateController(InputAPI::Android, gamepad.descriptor, gamepad.name);
			// Calibrated now, before the press that claims it is applied: buttons held when a controller calibrates
			// stay masked, so calibrating mid-press would leave that button dead
			controller->calibrate();
			if (auto current = androidController(player))
			{
				CopyMappings(players[player], players[player], current, controller);
			}
			else
			{
				auto first = androidController(0);
				if (player == 0 || !first || players[player] || players[0]->type() == EmulatedController::Type::VPAD)
					return;
				auto& manager = EmulatedControllerManager::GetController(player);
				manager.SetType(players[0]->type());
				players[player] = manager.GetControllerPtr();
				if (!players[player])
					return;
				CopyMappings(players[player], players[0], first, controller);
			}
			cemuLog_log(LogType::Force, "Input: {} plays as player {} ({})", gamepad.name, player + 1, gamepad.descriptor);
		}
	};
} // namespace

// Profiles store the Android device descriptor, which is not stable: devices sharing vendor/product ids without a unique
// id (the Odin 3's "Odin Controller" and "ODIN Station Virtual Mouse") get the same hash plus a registration-order nonce,
// so they swap descriptors and the profile ends up bound to the mouse. Rebind such controllers to an unused gamepad of
// the same name. Player 1 goes back to the handheld's own pad when its gamepad went away; other gamepads join with a
// button press (ClaimAndroidController).
void AssignAndroidControllers()
{
	auto& inputManager = InputManager::instance();
	AndroidPlayers state;
	auto& players = state.players;
	std::unordered_set<std::string> used;
	for (size_t i = 0; i < players.size(); i++)
	{
		if (auto holder = state.holder(i))
			used.insert(holder->uuid());
	}

	for (size_t i = 0; i < players.size(); i++)
	{
		if (!players[i])
			continue;
		auto controllers = players[i]->get_controllers();
		for (const auto& controller : controllers)
		{
			if (controller->api() != InputAPI::Android || state.isConnected(controller))
				continue;
			auto gamepad = std::ranges::find_if(state.gamepads, [&](const auto& g) {
				return g.name == controller->display_name() && !used.contains(g.descriptor);
			});
			if (gamepad != state.gamepads.end())
			{
				CopyMappings(players[i], players[i], controller, ControllerFactory::CreateController(InputAPI::Android, gamepad->descriptor, gamepad->name));
				used.insert(gamepad->descriptor);
				inputManager.save(i);
				cemuLog_log(LogType::Force, "Input: player {} rebound to {} ({})", i + 1, gamepad->name, gamepad->descriptor);
			}
		}
	}

	if (const auto* builtIn = state.builtIn(); builtIn && players[0] && !state.holder(0) && !used.contains(builtIn->descriptor))
		state.assign(0, *builtIn);
}

// Like a Switch, a gamepad that plays for no one joins on a button press: as player 1 if the handheld's own pad or no
// gamepad holds it, else as the first free player. The handheld's own pad takes player 1 back when no one else plays.
void ClaimAndroidController(std::string_view descriptor)
{
	AndroidPlayers state;
	auto gamepad = std::ranges::find(state.gamepads, descriptor, &ControllerManager::ControllerInfo::descriptor);
	if (gamepad == state.gamepads.end() || state.isUsed(descriptor))
		return;
	const bool builtIn = IsBuiltIn(gamepad->name);
	bool othersFree = true;
	for (size_t i = 1; i < state.players.size(); i++)
		othersFree = othersFree && !state.holder(i);
	if (builtIn && othersFree)
	{
		state.assign(0, *gamepad);
		return;
	}
	for (size_t i = 0; i < state.players.size(); i++)
	{
		auto holder = state.holder(i);
		if (!holder || (!builtIn && IsBuiltIn(holder->display_name())))
		{
			state.assign(i, *gamepad);
			return;
		}
	}
}

// Leaving the dock hands player 1 back to the handheld's own pad, as on a Switch
void ReturnPlayerOneToBuiltIn()
{
	AndroidPlayers state;
	const auto* builtIn = state.builtIn();
	if (!builtIn || !state.players[0])
		return;
	auto holder = state.holder(0);
	if (!holder || holder->uuid() != builtIn->descriptor)
		state.assign(0, *builtIn);
}

EmulatedControllerManager::EmulatedControllerManager(size_t index) : m_index(index)
{
	m_emulatedController = InputManager::instance().get_controller(m_index);
}

EmulatedControllerManager& EmulatedControllerManager::GetController(size_t index)
{
	auto& controller = s_emulatedControllers.at(index);

	if (!controller)
	{
		controller = std::unique_ptr<EmulatedControllerManager>(new EmulatedControllerManager(index));
	}

	return *controller;
}

void EmulatedControllerManager::SetButtonValue(uint64 mappingId, bool value)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->setButtonValue(mappingId, value);
}

void EmulatedControllerManager::SetAxisValue(uint64 mappingId, float value)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->setAxisValue(mappingId, value);
}

void EmulatedControllerManager::SetType(EmulatedController::Type type)
{
	if (m_emulatedController && m_emulatedController->type() == type)
	{
		return;
	}

	m_emulatedController = InputManager::instance().set_controller(m_index, type);
}

void EmulatedControllerManager::SetMapping(uint64 mappingId, ControllerPtr controller, uint64 buttonId)
{
	if (!m_emulatedController || !controller)
	{
		return;
	}

	const auto& controllers = m_emulatedController->get_controllers();
	auto controllerIt = std::find_if(controllers.begin(), controllers.end(), [&](const ControllerPtr& c) { return c->api() == controller->api() && c->uuid() == controller->uuid(); });
	if (controllerIt == controllers.end())
		m_emulatedController->add_controller(controller);
	else
		controller = *controllerIt;
	m_emulatedController->set_mapping(mappingId, controller, buttonId);
}

std::optional<std::string> EmulatedControllerManager::GetMapping(uint64 mapping) const
{
	if (!m_emulatedController)
		return {};
	auto controller = m_emulatedController->get_mapping_controller(mapping);
	if (!controller)
		return {};
	auto mappingName = m_emulatedController->get_mapping_name(mapping);
	return fmt::format("{}: {}", controller->display_name(), mappingName);
}

std::map<uint64, std::string> EmulatedControllerManager::GetMappings() const
{
	if (!m_emulatedController)
		return {};

	std::map<uint64, std::string> mappings;
	EmulatedController::Type type = m_emulatedController->type();
	uint64 mapping = 0;
	uint64 maxMapping = 0;

	if (type == EmulatedController::Type::VPAD)
	{
		mapping = VPADController::ButtonId::kButtonId_A;
		maxMapping = VPADController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Pro)
	{
		mapping = ProController::ButtonId::kButtonId_A;
		maxMapping = ProController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Classic)
	{
		mapping = ClassicController::ButtonId::kButtonId_A;
		maxMapping = ClassicController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Wiimote)
	{
		mapping = WiimoteController::ButtonId::kButtonId_A;
		maxMapping = WiimoteController::ButtonId::kButtonId_Max;
	}

	for (; mapping < maxMapping; mapping++)
	{
		if (auto mappingName = GetMapping(mapping))
		{
			mappings[mapping] = *mappingName;
		}
	}

	return mappings;
}

void EmulatedControllerManager::SetDisabled()
{
	InputManager::instance().delete_controller(m_index, true);
	m_emulatedController.reset();
}

EmulatedControllerPtr EmulatedControllerManager::GetControllerPtr() const
{
	return m_emulatedController;
}

void EmulatedControllerManager::ClearMapping(uint64 mapping)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->delete_mapping(mapping);
}
