# Installation · ziviSynthi AKS 3.9.6

[Português](INSTALL.md) · [Notices and credits](../../NOTICES.md#english)

## Try without the SMK

Open Web Studio on GitHub Pages, or open `zivisynthi-aks-editor.html` from `artefatos/web-studio/` in Chrome or Edge. Click **Try Web Studio** to start audio and go to the virtual keyboard. Play by mouse, touch or the **A W S E D F T G Y H U J K** keys. The local synth supports pin and patch editing, JSON patch import and export, and **KS** phrase playback. For a factory example, select a preset from **KS sequences** and press **Play** in KS. The **SEQ** grid transport controls the linked SMK.

The HTML bundles the synth, presets and interface. If the browser blocks audio when you open the file directly, run `python3 -m http.server 8000 --bind 127.0.0.1` in its directory and open [the editor on localhost](http://127.0.0.1:8000/zivisynthi-aks-editor.html).

## Install on the SMK‑37 Pro

The `zivisynthi-felucca-smk37-3.9.6.fwsc` image targets the **M‑VAVE SMK‑37 Pro**. The `zivisynthi-fm1-host-linux-x86_64.tar.gz` archive provides a desktop simulator. The expected USB identity after flashing is `SMK-37 Pro_99500`.

1. On the project’s GitHub Pages home, choose **Install firmware** to open Web Studio with the embedded package and downloads. For a local install, get the editor and FWSC from **Releases** or the **zivisynthi-aks-smk37-pro** Actions artifact. Run `sha256sum -c SHA256SUMS.txt` in the extracted download on Linux.
2. Export any personal patches and sequences you want to keep. Verify the original firmware and review [compatibility and recovery](SMK37_FELUCCA_BANCADA.en.md#installation-and-recovery). Switching from the Open branch requires a recovery procedure specific to the Pro because the flash layouts differ.
3. Open the Studio in Chrome or Edge. To serve a local release copy, open the directory containing the editor, run `python3 -m http.server 8000 --bind 127.0.0.1` and visit [the local editor](http://127.0.0.1:8000/zivisynthi-aks-editor.html).
4. Connect by USB, open **Firmware**, grant USB MIDI access and identify the device. Select the embedded 3.9.6 package or the downloaded FWSC. Confirm and wait for the Studio to identify the restarted device.

The Studio accepts recognized, SHA‑256-verified files for rollback. The default Actions build embeds 3.9.6. Select the recognized 3.9.5 file separately or rebuild the editor with `FIRMWARE_ROLLBACK` to embed it. Follow the [M‑VAVE factory recovery procedure](https://github.com/m-vave/smk37_open#updates-and-recovery) for the Pro.

The simulator, firmware image and updater protocol have been verified. Use the [hardware validation procedure](../development/HARDWARE_VALIDATION_PRO.md#english) to record flash and audio results on an instrument.
