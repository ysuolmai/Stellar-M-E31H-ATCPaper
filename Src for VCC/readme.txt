此目录是旧版 VCC 显示源码，仅把界面改成显示 VCC，采样仍使用 PB7，不能正确显示接在 VCC 上的锂电池电量。新版本请直接使用 Firmware/src，并执行 `make -C Firmware clean all PROFILE=lipo` 编译 VCC LiPo 固件。
