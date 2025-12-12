# senseshock

`senseshock` is a small userspace utility that tries to emulate a fully fledged DualShock 4 HID API with a DualSense.

The target of this utility is to provide compatibility with games such as Quantic Dream's `Detroit: Become Human` and `Heavy Rain`, which are native PS4 ports on PC. These games require a DualShock 4 plugged into USB, as they use the raw HID API. This utility will emulate a complete API so the DualShock 4 specific features such as gyro and touchpad still work in the game.

It will automatically steal the DualSense from the hid-playstation driver and claim the interface as long as the utility is running.

# Supported features
* Battery and charging statuses
* Lightbar
* Touchpad events
* Gyro
* All buttons, analog sticks and triggers
* Classic DualShock 4 style rumble on the DualSense

# TODO
* Fix the awful thread structure
* ...TBD.
