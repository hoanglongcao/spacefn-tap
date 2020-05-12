# spacefn-tap

SpaceFN for Linux with tapping for dedicated arrow keys especially for 60% keyboard.

SpaceFN-Tap is adapted from [abrasive/spacefn-evdev](https://github.com/abrasive/spacefn-evdev) with tapping for dedicated arrow keys inspired from [Anne Pro 2](https://medium.com/@thomaz.moura/how-the-anne-pro-2-mechanical-keyboard-completely-changed-my-workflow-e795f1f62026).

If you have an Anne Pro 2, check [this](https://www.reddit.com/r/AnnePro/comments/9es5zv/spacefn_layout_with_tap_layer/) thread.

I did not spend much time on this code so it does not look clean. 

![Image of SpaceFN-tab](/img/layout.png)


## SpaceFN history and Anne Pro 2 Tap

The story started from here: https://geekhack.org/index.php?topic=51069.0

![Image of SpaceFN](https://geekhack.org/index.php?action=dlattach;topic=51069.0;attach=44508;image)

Keys at the second layer are not the same in my code. It is personal so choose your own preference.

![Image of Anne Pro 2 Tap](/img/AnnePro2Tap.png)
## Tapping function

I use `KEY_RIGHTSHIFT`, `KEY_COMPOSE`, `KEY_RIGHTALT` and `KEY_RIGHTCTRL` for `KEY_UP`, `KEY_DOWN`, `KEY_LEFT` and `KEY_RIGHT`. All these four keys should work with their normal functionalities when being pressed and held. 

`spacefn-tap` mimics Anne Pro 2 tap mode but not exactly the same when 4 decidated arrow keys are pressed and held. So it is better to use the `KEY_LEFTCTRL` and `KEY_LEFTALT` when `KEY_RIGHTCTRL` and `KEY_RIGHTALT` are used for the tap mode. For example to perform `ALT + F4` you should do `KEY_LEFTALT + KEY_SPACE + KEY_4`. `KEY_LEFTALT + KEY_4` does not work at this moment.

Depending on your keyboard layout, you might need to swap these keys either by software or hardware (rewiring, check [this](https://www.reddit.com/r/MechanicalKeyboards/comments/71kzff/helpswapping_fn_and_alt_keys_on_magicforce_68/) thread).

Check function `key_remap()`. I change `\|` to `KEY_DELETE`. Note that I also change `~`to `KEY_ESC` to test on my TKL. Comment out this key if you have a real 60% keyboard.

## Compile
```
make

sudo ./spacefn-tab /dev/input/by-id/usb-XXX-event-kbd

```
Find your keyboard ID in `/dev/input/by-id`.
```
sudo ./spacefn-tab /dev/input/by-id/usb   then press Tab twice to list all input devices for

```

## Modification
You can customize by using `key_map()` as the original `spacefn-evdev` version.
You can also add new layers.

Check this page to find keycode for Linux system: [input-event-codes.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h).

Most of the keycodes are compatible with my keyboards. If not, detect key pressed by `sudo showkey`
