export module VirtualController;

export namespace sds
{
	/**
	 * \brief Left thumb stick or right.
	 */
	enum class ControllerStick
	{
		LeftStick,
		RightStick
	};

	/**
	 * \brief Direction values for thumbsticks, includes diagonals.
	 */
	enum class ThumbstickDirection
	{
		Invalid,
		Up,
		UpRight,
		Right,
		RightDown,
		Down,
		DownLeft,
		Left,
		LeftUp
	};

	/**
	 * \brief	All known/supported controller button/functionality identifiers usable with one or more pollers.
	 * \remarks Use these with the ButtonDescription mappings to attach them to a specific virtual key--one that is supported by the chosen poller.
	 */
	enum class VirtualButtons
	{
		// TODO might separate these into xbox and ps5, etc.
		NotSet, // Invalid value.

		Square,
		Circle,
		Triangle,
		X,
		A,
		B,
		Y,

		LeftTrigger, // Xbox trigger
		RightTrigger, // Xbox trigger

		ShoulderLeft, // top button on PS5
		ShoulderRight, // top button on PS5

		ShoulderBottomLeft, // PS5
		ShoulderBottomRight, // PS5
		LeftPill, // PS5
		RightPill, // PS5
		PS5Logo, // PS5
		ShiftSwitch, // PS5

		LeftStickClick,
		RightStickClick,

		Start,
		Back,

		DpadUp,
		DpadDown,
		DpadLeft,
		DpadRight,

		LeftThumbstickUp,
		LeftThumbstickUpRight,
		LeftThumbstickRight,
		LeftThumbstickDownRight,
		LeftThumbstickDown,
		LeftThumbstickDownLeft,
		LeftThumbstickLeft,
		LeftThumbstickUpLeft,

		RightThumbstickUp,
		RightThumbstickUpRight,
		RightThumbstickRight,
		RightThumbstickDownRight,
		RightThumbstickDown,
		RightThumbstickDownLeft,
		RightThumbstickLeft,
		RightThumbstickUpLeft
	};
}