# 🧩 meta-magic_mount

A **KernelSU metamodule** that takes over module mounting using Magic Mount. It is the mount backend that lands [KernelNoSU](https://github.com/Bouteillepleine/kernelnosu)'s real `su` — a fork of [7a72's magic_mount](https://github.com/7a72/meta-magic_mount) with a Material Design 3 WebUI and KernelNoSU integration.

> This is an **Android root module** — a flashable `.zip`, not a desktop app. It runs on a rooted phone, not on your computer.

## 📥 Download

Grab the latest zip from the [**Releases**](https://github.com/Bouteillepleine/meta-magic_mount/releases/latest) page.

## 🛠️ Requirements

- A rooted Android device on a **KernelSU-based** root with metamodule support: **KernelSU-Next**, **SukiSU-Ultra**, or **ReSukiSU**.
- Only **one metamodule** can be active at a time. If another (e.g. `magic_mount_rs`) is installed, uninstall it and reboot before installing this one.
- Optional: the [KernelNoSU](https://github.com/Bouteillepleine/kernelnosu) module, if you want a real, consistent `su` binary — this metamodule is what mounts it.

## 🚀 Install

1. Download the zip from [Releases](https://github.com/Bouteillepleine/meta-magic_mount/releases/latest).
2. In your root manager, open **Modules → Install from storage** and select the zip.
3. **Reboot.** The metamodule takes over module mounting on the next boot.

## 📦 What it does

- Replaces the manager's built-in module mounting with a Magic Mount engine that builds a merged mount tree from `/data/adb/modules` and mounts it over the system.
- Cooperates with **KernelNoSU**: reads its `su_target`/`su_source`, disables sucompat around the mount, mounts the real `su` through the standard tree-walk, and confirms it landed.
- Ships a **Material Design 3 WebUI** (open it from the module card in your root manager):
  - **Config** — log level, try-unmount, module/temp directories, mount source, log file, extended partitions.
  - **Modules** — enable/disable each module's mount, with per-module status.

## ⚙️ Configuration

Settings live in `/data/adb/magic_mount/mm.conf` and are editable from the WebUI's **Config** tab. The mount log is written to `/data/adb/magic_mount/mm.log`.

## 🙏 Credits

- Upstream: [7a72/meta-magic_mount](https://github.com/7a72/meta-magic_mount).
- This fork: **7a72, XxxY**.

## 📄 License

Licensed under the **GNU GPL v3** — see [LICENSE](LICENSE).
