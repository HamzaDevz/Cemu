#include "AndroidInputHelpers.h"

#include "input/api/Android/ControllerManager.h"
#include "input/ControllerFactory.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/WiimoteController.h"

ControllerPtr CreateDefaultDeviceController()
{
	auto controller = ControllerFactory::CreateController(InputAPI::Device, "device", "device");
	controller->set_use_motion(true);
	return controller;
}

// Maps controller to every mapping that source has on sourceController, with the same settings. Replaces
// sourceController when target is source.
static void CopyMappings(const EmulatedControllerPtr& target, const EmulatedControllerPtr& source, const ControllerPtr& sourceController, const ControllerPtr& controller)
{
	std::vector<std::pair<uint64, uint64>> mappings;
	for (uint64 mapping = 0; mapping <= source->get_highest_mapping_id(); mapping++)
	{
		if (source->get_mapping_controller(mapping) == sourceController)
			mappings.emplace_back(mapping, source->get_mapping_button(mapping));
	}
	if (target == source)
		target->remove_controller(sourceController);
	controller->set_settings(sourceController->get_settings());
	target->add_controller(controller);
	for (const auto& [mapping, button] : mappings)
		target->set_mapping(mapping, controller, button);
}

// Profiles store the Android device descriptor, which is not stable: devices sharing vendor/product ids without a unique
// id (the Odin 3's "Odin Controller" and "ODIN Station Virtual Mouse") get the same hash plus a registration-order nonce,
// so they swap descriptors and the profile ends up bound to the mouse. Rebind such controllers to an unused gamepad of
// the same name, then give each gamepad still unused to the first player that has none connected, or to a new player
// mapped like player 1 so that multiplayer works without setting up every controller.
void AssignAndroidControllers()
{
	auto& inputManager = InputManager::instance();
	const auto gamepads = ControllerManager::instance().get_controllers();
	auto isConnected = [&](const ControllerPtr& controller) {
		return controller->api() == InputAPI::Android && std::ranges::any_of(gamepads, [&](const auto& gamepad) {
			return gamepad.descriptor == controller->uuid() && gamepad.name == controller->display_name();
		});
	};

	std::array<EmulatedControllerPtr, InputManager::kMaxController> players;
	std::unordered_set<std::string> used;
	for (size_t i = 0; i < players.size(); i++)
	{
		players[i] = inputManager.get_controller(i);
		if (!players[i])
			continue;
		for (const auto& controller : players[i]->get_controllers())
		{
			if (isConnected(controller))
				used.insert(controller->uuid());
		}
	}

	auto rebind = [&](size_t player, const ControllerPtr& stale, const ControllerManager::ControllerInfo& gamepad) {
		CopyMappings(players[player], players[player], stale, ControllerFactory::CreateController(InputAPI::Android, gamepad.descriptor, gamepad.name));
		used.insert(gamepad.descriptor);
		inputManager.save(player);
		cemuLog_log(LogType::Force, "Input: player {} rebound to {} ({})", player + 1, gamepad.name, gamepad.descriptor);
	};
	auto staleController = [&](size_t player) -> ControllerPtr {
		if (!players[player] || std::ranges::any_of(players[player]->get_controllers(), isConnected))
			return {};
		auto controllers = players[player]->get_controllers();
		auto it = std::ranges::find(controllers, InputAPI::Android, &ControllerBase::api);
		return it != controllers.end() ? *it : nullptr;
	};

	for (size_t i = 0; i < players.size(); i++)
	{
		if (!players[i])
			continue;
		auto controllers = players[i]->get_controllers();
		for (const auto& controller : controllers)
		{
			if (controller->api() != InputAPI::Android || isConnected(controller))
				continue;
			auto gamepad = std::ranges::find_if(gamepads, [&](const auto& g) {
				return g.name == controller->display_name() && !used.contains(g.descriptor);
			});
			if (gamepad != gamepads.end())
				rebind(i, controller, *gamepad);
		}
	}

	const auto& first = players[0];
	ControllerPtr firstGamepad;
	if (first && first->type() != EmulatedController::Type::VPAD)
	{
		auto controllers = first->get_controllers();
		auto it = std::ranges::find_if(controllers, isConnected);
		if (it != controllers.end())
			firstGamepad = *it;
	}

	for (const auto& gamepad : gamepads)
	{
		if (used.contains(gamepad.descriptor))
			continue;
		for (size_t i = 0; i < players.size(); i++)
		{
			if (auto stale = staleController(i))
			{
				rebind(i, stale, gamepad);
				break;
			}
			if (players[i] || !firstGamepad)
				continue;
			auto& manager = EmulatedControllerManager::GetController(i);
			manager.SetType(first->type());
			players[i] = manager.GetControllerPtr();
			if (!players[i])
				break;
			// Not saved: the next session assigns it again, so profiles of borrowed gamepads do not pile up.
			CopyMappings(players[i], first, firstGamepad, ControllerFactory::CreateController(InputAPI::Android, gamepad.descriptor, gamepad.name));
			used.insert(gamepad.descriptor);
			cemuLog_log(LogType::Force, "Input: player {} assigned to {} ({})", i + 1, gamepad.name, gamepad.descriptor);
			break;
		}
	}
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
